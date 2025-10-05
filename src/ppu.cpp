#include "ppu.hpp"
#include "memory.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

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
        memory.LY = 0;
        memory.vramLocked = false;
        memory.oamLocked = false;
        updateCoincidence();
        return;
    }

    for (int i = 0; i < cycles; ++i) {
        // 可視ライン中（LY=0..143）
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
            //updateCoincidence(); // LYC=LY割り込みチェック

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

    scxDiscard = memory.SCX & 0x07;

    bgLineY = static_cast<uint8_t>(memory.SCY + currentLine);

    //windorwの行頭判定
    windowActive = false;
    windowLineStarted = false;
    windowEnabledThisLine =
        ((memory.LCDC & 0x20) != 0) && ((memory.LCDC & 0x01) != 0) && currentLine >= memory.WY && memory.WX <= 166;
    windowTriggerX = std::min(159, std::max(0, static_cast<int>(memory.WX) -7));

    spriteCount = 0;
    // スプライト処理は一旦無効化
    // TODO: 後でスプライト実装時に有効化

}

void PPU::enterMode3() {
    setMode(3);  // setMode()を使ってSTAT割り込み処理
    // VRAMもロックして描画開始
    memory.oamLocked = true;
    memory.vramLocked = true;
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
  // 画面上のX（Mode3開始からの相対）
  int screenX = dotCounter - MODE3_START;

  // ウィンドウ処理は一旦無効化
  // TODO: 後でウィンドウ実装時に有効化

  const bool bgEnabled = (memory.LCDC & 0x01) != 0;

  // ---- BG FIFO が細りすぎたら補充（<=8くらいを目安）----
  if (bgFifo.size() <= 8) {
    switch (fetcherState) {
      case 0: { // タイル番号取得
        fetchUsingWindow = false; // ウィンドウ無効化

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

  // 左端スクロール捨て処理
  if (scxDiscard > 0) {
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

  // スプライト処理は一旦無効化
  // TODO: 後でスプライト実装時に有効化

  framebuffer[currentLine * 160 + screenX] = pixel;


}


// ▼▼▼ これを ppu.cpp に追加してください（1箇所だけ） ▼▼▼
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
