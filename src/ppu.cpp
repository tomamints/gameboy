#include "ppu.hpp"
#include "memory.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>

// Game Boy仕様
// CPU::step()は1dot（Tサイクル）単位。1ライン=456dot
constexpr int SCANLINE_CYCLES = 456;
constexpr int MODE2_LENGTH    = 80;
constexpr int MODE3_START     = MODE2_LENGTH;
constexpr int MODE0_START     = MODE2_LENGTH + 172;
constexpr int VBLANK_START    = 144;
constexpr int TOTAL_LINES     = 154;

PPU::PPU(Memory& mem)
    : memory(mem)
{
    reset();
}

void PPU::reset() {
    dotCounter            = 0;
    currentLine           = 0;
    windowLineCounter     = 0;
    windowLineStarted     = false;
    windowActive          = false;
    windowEnabledThisLine = false;
    spriteCount           = 0;
    bgFifo.clear();
    scxDiscard            = 0;
    fetcherState          = 0;
    fetcherDotCounter     = 0;
    mode                  = 2;
    memory.LY             = 0;
    for (int y = 0; y < 144; ++y) {
        for (int x = 0; x < 160; ++x) {
            framebuffer[y * 160 + x] = 0x00000000;  // 灰色で塗りつぶす例
        }
}

}

void PPU::step(int cycles) {
    if ((memory.LCDC & 0x80) == 0) {
        mode = 0;
        dotCounter = 0;
        currentLine = 0;
        windowLineCounter = 0;
        windowLineStarted = false;
        windowActive = false;
        windowEnabledThisLine = false;
        spriteCount = 0;
        bgFifo.clear();
        scxDiscard = 0;
        fetcherState = 0;
        fetcherDotCounter = 0;
        memory.LY = 0;
        memory.vramLocked = false;
        memory.oamLocked = false;
        updateCoincidence();
        return;
    }

    for (int i = 0; i < cycles; ++i) {
        // 可視ライン中（LY=0..143）
        updateCoincidence(); // LYC=LY割り込みチェック
        if (currentLine < VBLANK_START) {
            // ── 行内のモード切替を dotCounter でスイッチ ──
            switch (dotCounter) {
                case 0:
                    enterMode2();
                    break;  // 行頭: OAM検索開始 (Mode2)
                case MODE3_START:
                    enterMode3(); break;  // dot=80: 描画開始 (Mode3)
                case MODE0_START:
                    enterMode0(); break;  // dot=252: HBlankへ (Mode0)
                default: break;
            }

            if (dotCounter >= MODE3_START && dotCounter < MODE0_START) {
                stepMode3(dotCounter);
            }
        }
        // VBlankライン（LY=144..153）
        else {
            // VBlankにまだ入っていなければ一度だけ入る
            if (mode != 1 && dotCounter == 0) {
                enterVBlank();           // Mode1 + IF(VBlank) 立てる
            }
        }

        // ── ドット進行 ──
        ++dotCounter;

        // ── 行末処理 ──
        if (dotCounter == SCANLINE_CYCLES) { // 456dot ちょうどで行送り
            dotCounter = 0;

            // Window行カウンタの更新（その行で一度でもWindow描画したら）
            if (currentLine < VBLANK_START && windowEnabledThisLine && windowLineStarted) {
                ++windowLineCounter;
            }
            windowLineStarted = false;
            windowActive = false;

            // 次の行へ
            currentLine++;

            // LY/STAT更新
            memory.LY = currentLine;

            // VBlank開始/フレーム終了
            if (currentLine == VBLANK_START) {
                enterVBlank();   // 念のため境界でも一度だけ
            } else if (currentLine >= TOTAL_LINES) {
                currentLine = 0;
                memory.LY = 0;
                windowLineCounter = 0;
                // 次行の dot=0 ケースで enterMode2() が呼ばれる
            }
        }
    }


    return;
}


void PPU::setMode(uint8_t newMode) {
    newMode &= 0x03;
    if (mode == newMode) {
        uint8_t stat = (memory.STAT & 0xF8) | (coincidence ? 0x04 : 0) | mode;
        memory.STAT = stat;
        return;
    }

    uint8_t prev = memory.STAT;
    bool requestStat = false;
    if (newMode == 0 && (prev & 0x08)) requestStat = true;
    if (newMode == 1 && (prev & 0x10)) requestStat = true;
    if (newMode == 2 && (prev & 0x20)) requestStat = true;

    mode = newMode;
    memory.STAT = (prev & 0xF8) | (coincidence ? 0x04 : 0) | mode;
    if (requestStat) memory.if_reg |= 0x02;  // STAT割り込み
}

void PPU::updateCoincidence() {
    bool match = (memory.LY == memory.LYC);
    uint8_t prev = memory.STAT;
    if (match != coincidence) {
        coincidence = match;
        if (coincidence && (prev & 0x40)) {
            memory.if_reg |= 0x02;          // LYC=LY割り込み
        }
    }
    memory.STAT = (prev & 0xF8) | (coincidence ? 0x04 : 0) | mode;
}



void PPU::enterMode2() { //OAM find
    setMode(2);
    // OAMをロック、スプライト探索など
    memory.oamLocked = true;
    memory.vramLocked = false;

    bgFifo.clear();
    fetcherState = 0;
    fetcherX = 0;
    fetcherDotCounter = 0;
    fetchUsingWindow = false;  // 各ライン開始時にリセット

    scxDiscard = memory.SCX & 0x07;

    bgLineY = static_cast<uint8_t>(memory.SCY + currentLine);

    //windorwの行頭判定
    windowActive = false;
    windowLineStarted = false;


    windowEnabledThisLine =
        ((memory.LCDC & 0x20) != 0) && ((memory.LCDC & 0x01) != 0) && currentLine >= memory.WY && memory.WX <= 166;


    // windowTriggerXの計算
    if (!windowEnabledThisLine) {
        windowTriggerX = -1;  // Window無効時は-1
    } else {
        // WX→画面Xの変換
        int trig = static_cast<int>(memory.WX) - 7;
        if (trig < 0) trig = 0;
        if (trig > 159) trig = 159;
        windowTriggerX = trig;

        // WX=0の特殊処理
        if (memory.WX == 0) {
            windowTriggerX = -1;  // 画面外なので実質的に発火しない
            windowEnabledThisLine = false;  // WX=0は事実上無効扱い
        }
    }

    spriteCount = 0;
    gatherSprites();

}

void PPU::enterMode3() {
    setMode(3);  // setMode()を使ってSTAT割り込み処理
    // VRAMもロックして描画開始
    gatherSprites(); //Mode3の直前にも飛ぶ＝タイミング補正
    memory.oamLocked = true;
    memory.vramLocked = true;
    fetcherDotCounter = 0;  // Mode3開始時にフェッチャーカウンタリセット
}

void PPU::enterMode0() {
    setMode(0);  // setMode()を使ってSTAT割り込み処理
    // ロック解除
    memory.oamLocked = false;
    memory.vramLocked = false;
}

void PPU::enterVBlank() {
    setMode(1);  // setMode()を使ってSTAT割り込み処理
    // ロック解除＋VBlank割り込みをIFにセット
    memory.oamLocked = false;
    memory.vramLocked = false;
    memory.if_reg |= 0x01; // VBlank割り込み要求
}


void PPU::stepMode3(int dotCounter) {
  // フェッチャーカウンタ更新
  fetcherDotCounter++;

  // 画面上のX座標計算（ウィンドウ切り替え判定用）
  int screenX = dotCounter - MODE3_START -8 ;

  // リアルタイムWXチェック: WXが変更されたらwindowEnabledThisLineを再計算
  static uint8_t cachedWX = memory.WX;
  if (memory.WX != cachedWX) {
    cachedWX = memory.WX;

    // windowEnabledThisLineを再計算
    windowEnabledThisLine = ((memory.LCDC & 0x20) != 0) && ((memory.LCDC & 0x01) != 0)
                            && currentLine >= memory.WY && memory.WX <= 166;

    if (memory.WX == 0) {
      windowEnabledThisLine = false;  // WX=0は事実上無効扱い
    }

    windowTriggerX = std::min(159, std::max(0, static_cast<int>(memory.WX) - 7));
  }


  // ウィンドウ切り替え処理
  // Window判定は描画位置より早めに行う（フェッチャー遅延を考慮）
  int windowCheckX = dotCounter - MODE3_START;

  if (!windowActive && windowEnabledThisLine && windowCheckX >= windowTriggerX) {
    windowActive = true;
    windowLineStarted = true;
    fetcherX = 0;
    fetcherState = 0;
    scxDiscard = 0;
    fetcherDotCounter = 0;
  }

  const bool bgEnabled = (memory.LCDC & 0x01) != 0;

  if (bgFifo.size() < 8) {
    switch (fetcherState) {
      case 0: { // タイル番号取得
        fetchUsingWindow = windowActive && windowEnabledThisLine;

        // タイルマップ基底アドレス
        uint16_t tileMapBase = fetchUsingWindow
            ? ((memory.LCDC & 0x40) ? 0x9C00 : 0x9800)
            : ((memory.LCDC & 0x08) ? 0x9C00 : 0x9800);

        // タイル行・列
        uint8_t tileRow = fetchUsingWindow
            ? static_cast<uint8_t>((windowLineCounter >> 3) & 0x1F)
            : static_cast<uint8_t>((bgLineY >> 3) & 0x1F);
        uint8_t tileCol = fetchUsingWindow
            ? static_cast<uint8_t>(fetcherX & 0x1F)
            : static_cast<uint8_t>(((memory.SCX >> 3) + fetcherX) & 0x1F);

        uint16_t tileMapAddr = tileMapBase + tileRow * 32 + tileCol;

        fetchTileNumber = readPPUByte(tileMapAddr);
        fetcherState = 1; // ウェイト1
        break;
      }
      case 1:
        fetcherState = 2; // Low取得へ
        break;

      case 2: { // タイルラインLow取得
        bool unsignedIndex = (memory.LCDC & 0x10) != 0; // 1:0x8000, 0:0x8800(符号付き)
        int16_t tileIndex = unsignedIndex
            ? static_cast<int16_t>(fetchTileNumber)
            : static_cast<int8_t>(fetchTileNumber); // 符号拡張

        uint16_t tileAddrBase = unsignedIndex ? 0x8000 : 0x9000;

        uint8_t lineInTile = fetchUsingWindow
            ? static_cast<uint8_t>(windowLineCounter & 0x07)
            : static_cast<uint8_t>(bgLineY & 0x07);

        fetchTileAddr = static_cast<uint16_t>(tileAddrBase + tileIndex * 16 + lineInTile * 2);
        fetchDataLow = readPPUByte(fetchTileAddr);
        fetcherState = 3; // ウェイト
        break;
      }
      case 3:
        fetcherState = 4; // High取得へ
        break;

      case 4: // タイルラインHigh取得
        fetchDataHigh = readPPUByte(fetchTileAddr + 1);
        fetcherState = 5; // ウェイト
        break;

      case 5:
        fetcherState = 6; // pushへ
        break;

      case 6: { // 8px を FIFO へ
        for (int bit = 7; bit >= 0; --bit) {
          uint8_t color = 0;
          if (bgEnabled) {
            color = static_cast<uint8_t>(((fetchDataHigh >> bit) & 0x01) << 1 |
                                         ((fetchDataLow  >> bit) & 0x01));
          }
          bgFifo.push_back(color);
        }
        fetcherX = static_cast<uint8_t>(fetcherX + 1);
        fetcherState = 0;
        break;
      }
    }
  }

  // ---- ここから1px出力 ----

  // FIFOが空なら描けない
  if (bgFifo.empty()) return;

  // 左端スクロール捨て処理（Windowではスクロール無効）
  if (scxDiscard > 0 && !windowActive) {
    bgFifo.pop_front();
    --scxDiscard;
    return;
  }

  // 画面範囲＆可視ライン内のみ描く
  if (screenX < 0 || screenX >= 160 || currentLine >= 144) {
    // ピクセルを消費はする（Mode3の進行を模すなら pop してもよい）
    bgFifo.pop_front();
    return;
  }

  // BGの2bit色 → DMGパレット(BGP)で 4階調ARGBへ
  uint8_t bgColorId = bgFifo.front();
  bgFifo.pop_front();

  uint32_t pixel = decodeDMGColor(memory.BGP, bgColorId);
  uint8_t finalColor = bgColorId;
  bool bgOpaque = (bgColorId != 0);

  // スプライト処理 (リアルタイムパレット対応)
  int spriteScreenX = screenX + 2;  // スプライト用座標をscreenXと同じにする

  // スプライトパレット変更検出
  static uint8_t cachedOBP0 = memory.OBP0;
  static uint8_t cachedOBP1 = memory.OBP1;

  if (memory.OBP0 != cachedOBP0 || memory.OBP1 != cachedOBP1) {
    cachedOBP0 = memory.OBP0;
    cachedOBP1 = memory.OBP1;
  }

  for (int s = 0; s < spriteCount; ++s) {
    const SpriteLine& spr = spriteLineBuffer[s];

    // line 42-49 (0x2A-0x31) のスプライトデバッグ
    if ((currentLine >= 0x2A && currentLine <= 0x31) && screenX >= 0 && screenX < 160) {
      // 最初のspriteだけ、または重要な位置でのみ詳細表示
      if (s == 0 || (spriteScreenX >= spr.x && spriteScreenX < spr.x + 8)) {
        std::cout << "LINE" << std::hex << (int)currentLine << std::dec
                  << "[" << std::setw(3) << screenX << "] "
                  << "SPR[" << s << "] "
                  << "sprX=" << std::setw(3) << spr.x
                  << " scrX=" << std::setw(3) << spriteScreenX
                  << " tile=" << std::hex << (int)spr.tile << std::dec
                  << " attr=" << std::hex << (int)spr.attr << std::dec
                  << " prio=" << spr.priority;

        if (spriteScreenX >= spr.x && spriteScreenX < spr.x + 8) {
          int pixelIndex = spriteScreenX - spr.x;
          uint8_t spriteColor = spr.pixels[pixelIndex];
          std::cout << " px[" << pixelIndex << "]=" << (int)spriteColor;
        }
        std::cout << " sprCnt=" << spriteCount << std::endl;
      }
    }

    // X座標範囲チェック
    if (spriteScreenX < spr.x || spriteScreenX >= spr.x + 8) {
      continue;
    }

    // スプライトピクセル取得
    uint8_t spriteColor = spr.pixels[spriteScreenX - spr.x];
    if (spriteColor == 0) {  // 透明ピクセル
      continue;
    }

    // Priority処理：スプライトが背景より後ろ && 背景が不透明なら表示しない
    if (spr.priority && bgOpaque) {
      continue;
    }

    // リアルタイムパレット使用 (gatherSprites時のパレットではなく現在のパレット)
    uint8_t currentPalette = (spr.attr & 0x10) ? memory.OBP1 : memory.OBP0;
    pixel = decodeDMGColor(currentPalette, spriteColor);
    finalColor = spriteColor;
    bgOpaque = true;
    break;  // 最初に見つかったスプライトを採用（優先度順）
  }

  framebuffer[currentLine * 160 + screenX] = pixel;

  // LINE 0x58と0x59で実際に描画された色を比較
  if ((currentLine == 0x58 || currentLine == 0x59) && screenX < 160) {
    if (finalColor != 0) {  // 背景色以外が描画された時
      std::cout << "DRAW LINE" << std::hex << (int)currentLine << std::dec
                << "[" << std::setw(3) << screenX << "] color=" << (int)finalColor
                << " pixel=" << std::hex << pixel << std::dec << std::endl;
    }
  }


}


uint8_t PPU::readPPUByte(uint16_t addr) {
    bool vramPrev = memory.vramLocked;
    bool oamPrev  = memory.oamLocked;
    memory.vramLocked = false;   // PPU内部読み込みなので一時的にロック解除
    memory.oamLocked  = false;
    uint8_t v = memory.readByte(addr);
    memory.vramLocked = vramPrev;
    memory.oamLocked  = oamPrev;
    return v;
}

uint32_t PPU::decodeDMGColor(uint8_t palette, uint8_t colorId) const {
    static constexpr uint32_t COLORS[4] = {
        0xFFFFFFFF, 0xFFBFBFBF, 0xFF7F7F7F, 0xFF1F1F1F
    };
    uint8_t shade = (palette >> (colorId * 2)) & 0x03;
    return COLORS[shade];
}

void PPU::saveFramePPM(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[PPU] Failed to open frame dump: " << path << std::endl;
        return;
    }

    out << "P6\n160 144\n255\n";
    for (int y = 0; y < 144; ++y) {
        const uint32_t* line = &framebuffer[y * 160];
        for (int x = 0; x < 160; ++x) {
            uint32_t pixel = line[x];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            out.put(static_cast<char>(r));
            out.put(static_cast<char>(g));
            out.put(static_cast<char>(b));
        }
    }
}


void PPU::gatherSprites() {
    // スプライト無効なら何もしない
    if ((memory.LCDC & 0x02) == 0) {
        spriteCount = 0;
        return;
    }

    int spriteHeight = (memory.LCDC & 0x04) ? 16 : 8;  // 8x8 or 8x16
    spriteCount = 0;

    // OAMから40個のスプライトをチェック（最大10個まで）
    for (int i = 0; i < 40 && spriteCount < 10; ++i) {
        uint16_t oamAddr = 0xFE00 + i * 4;
        uint8_t y = readPPUByte(oamAddr);
        uint8_t x = readPPUByte(oamAddr + 1);
        uint8_t tile = readPPUByte(oamAddr + 2);
        uint8_t attr = readPPUByte(oamAddr + 3);

        // スプライト位置調整（GBはY-16, X-8）
        int spriteY = static_cast<int>(y) - 16;
        int spriteX = static_cast<int>(x) - 8;

        // 現在ラインと重なるかチェック
        if (spriteY <= static_cast<int>(currentLine) &&
            static_cast<int>(currentLine) < spriteY + spriteHeight) {

            SpriteLine& info = spriteLineBuffer[spriteCount++];
            info.x = spriteX;
            info.priority = (attr & 0x80) != 0;  // OBJ-to-BG Priority
            info.palette = (attr & 0x10) ? memory.OBP1 : memory.OBP0;
            info.attr = attr;
            info.tile = tile;

            // スプライト内での行位置計算
            int lineInSprite = static_cast<int>(currentLine) - spriteY;

            // Y-flip処理
            if (attr & 0x40) {
                lineInSprite = spriteHeight - 1 - lineInSprite;
            }

            // 8x16モードでは下位1bitを0にする
            if (spriteHeight == 16) {
                tile &= 0xFE;
            }

            // タイルデータ読み込み（スプライトは常に0x8000-0x8FFF）
            uint16_t tileAddr = 0x8000 + tile * 16 + lineInSprite * 2;
            uint8_t low = readPPUByte(tileAddr);
            uint8_t high = readPPUByte(tileAddr + 1);

            // LINE 0x58と0x59でタイルデータを確認
            if (currentLine == 0x58 || currentLine == 0x59) {
                if (tile == 0x0C || tile == 0x0D) {
                    std::cout << "[TILE] LINE" << std::hex << (int)currentLine
                              << " dot=" << std::dec << dotCounter
                              << " sprY=" << spriteY
                              << " tile=" << std::hex << (int)tile
                              << " addr=" << tileAddr
                              << " low=" << (int)low << " high=" << (int)high
                              << " lineInSpr=" << std::dec << lineInSprite
                              << " height=" << spriteHeight
                              << " (gathered at Mode2)" << std::endl;
                }
            }

            // 8ピクセル分のデータを準備
            for (int px = 0; px < 8; ++px) {
                int bit = (attr & 0x20) ? px : (7 - px);  // X-flip処理
                info.pixels[px] = static_cast<uint8_t>(
                    ((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01)
                );
            }
        }
    }

    // スプライト優先度ソート（X座標優先、同じ場合はOAMインデックス優先）
    std::sort(spriteLineBuffer, spriteLineBuffer + spriteCount,
              [](const SpriteLine& a, const SpriteLine& b) {
                  if (a.x != b.x) return a.x < b.x;  // X座標で優先
                  // X座標が同じ場合はOAMインデックス（収集順）で優先
                  return false;  // 安定ソートで元の順序を保持
              });

    // LINE 42-49 (0x2A-0x31) でのsprite収集結果を確認
    if (currentLine >= 0x2A && currentLine <= 0x31) {
        std::cout << "[GATHER] LINE " << std::hex << (int)currentLine << std::dec
                  << " found " << spriteCount << " sprites (sorted by X):";
        for (int i = 0; i < spriteCount; ++i) {
            std::cout << " [" << i << "]x=" << spriteLineBuffer[i].x
                      << "/tile=" << std::hex << (int)spriteLineBuffer[i].tile << std::dec
                      << "/prio=" << spriteLineBuffer[i].priority;
        }
        std::cout << std::endl;
    }
}
