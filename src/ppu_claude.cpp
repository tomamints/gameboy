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
    coincidence           = false;
    memory.LY             = 0;
    memory.vramLocked     = false;
    memory.oamLocked      = false;

    uint8_t stat = memory.STAT & 0xF8;
    memory.STAT = stat | (mode & 0x03);
    updateCoincidence();
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
        updateCoincidence();
        if (currentLine < VBLANK_START) {
            if (dotCounter == 0) {
                enterMode2();
            } else if (dotCounter == MODE3_START) {
                enterMode3();
            } else if (dotCounter == MODE0_START) {
                enterMode0();
            }
        } else if (mode != 1) {
            enterVBlank();
        }

        if (mode == 3) {
            stepMode3();
        }

        dotCounter++;

        if (dotCounter >= SCANLINE_CYCLES) {
            dotCounter = 0;

        if (currentLine < VBLANK_START && windowEnabledThisLine && windowLineStarted) {
            windowLineCounter++;
        }
        windowLineStarted = false;
        windowActive = false;

            currentLine++;

            if (currentLine == VBLANK_START) {
                enterVBlank();
            } else if (currentLine >= TOTAL_LINES) {
                currentLine = 0;
                windowLineCounter = 0;
            }

            memory.LY = currentLine;
        }
    }
}

void PPU::setMode(uint8_t newMode) {
    newMode &= 0x03;
    if (mode == newMode) {
        // 下位ビットの再同期のみ
        uint8_t stat = memory.STAT & 0xF8;
        memory.STAT = stat | (coincidence ? 0x04 : 0) | mode;
        return;
    }

    uint8_t prevStat = memory.STAT;
    bool requestStatInterrupt = false;

    if (newMode == 0 && (prevStat & 0x08)) {
        requestStatInterrupt = true;
    } else if (newMode == 1 && (prevStat & 0x10)) {
        requestStatInterrupt = true;
    } else if (newMode == 2 && (prevStat & 0x20)) {
        requestStatInterrupt = true;
    }

    mode = newMode;
    uint8_t stat = (prevStat & 0xF8) | (coincidence ? 0x04 : 0) | mode;
    memory.STAT = stat;

    if (requestStatInterrupt) {
        memory.if_reg |= 0x02;
    }
}

void PPU::updateCoincidence() {
    bool match = (memory.LY == memory.LYC);
    uint8_t prevStat = memory.STAT;
    if (match != coincidence) {
        coincidence = match;
        if (coincidence && (prevStat & 0x40)) {
            memory.if_reg |= 0x02;
        }
    }

    uint8_t stat = (prevStat & 0xF8) | (coincidence ? 0x04 : 0) | mode;
    memory.STAT = stat;
}

void PPU::enterMode2() {
    setMode(2);
    memory.oamLocked = true;
    memory.vramLocked = false;

    spriteCount = 0;
    gatherSprites();

    if (currentLine == memory.WY) {
        windowLineCounter = 0;
    }

    bgFifo.clear();
    fetcherX = 0;
    fetcherState = 0;
    scxDiscard = memory.SCX & 0x07;
    bgLineY = static_cast<uint8_t>(memory.SCY + currentLine);
    windowActive = false;
    windowLineStarted = false;
    windowEnabledThisLine = ((memory.LCDC & 0x20) != 0) && ((memory.LCDC & 0x01) != 0)
                             && currentLine >= memory.WY && memory.WX <= 166;
    windowTriggerX = std::min(159, std::max(0, static_cast<int>(memory.WX) - 7));
}

void PPU::enterMode3() {
    setMode(3);
    memory.oamLocked = true;
    memory.vramLocked = true;
    bgFifo.clear();
    fetcherX = 0;
    fetcherState = 0;
}

void PPU::enterMode0() {
    setMode(0);
    memory.oamLocked = false;
    memory.vramLocked = false;
}

void PPU::enterVBlank() {
    setMode(1);
    memory.oamLocked = false;
    memory.vramLocked = false;
    memory.if_reg |= 0x01;
}

void PPU::gatherSprites() {
    if ((memory.LCDC & 0x02) == 0) {
        spriteCount = 0;
        return;
    }

    int spriteHeight = (memory.LCDC & 0x04) ? 16 : 8;
    for (int i = 0; i < 40 && spriteCount < 10; ++i) {
        uint16_t base = 0xFE00 + i * 4;
        uint8_t y = readPPUByte(base);
        uint8_t x = readPPUByte(base + 1);
        uint8_t tile = readPPUByte(base + 2);
        uint8_t attr = readPPUByte(base + 3);

        int spriteY = static_cast<int>(y) - 16;
        int spriteX = static_cast<int>(x) - 8;

        if (spriteY <= static_cast<int>(currentLine) && static_cast<int>(currentLine) < spriteY + spriteHeight) {
            SpriteLine& info = spriteLineBuffer[spriteCount++];
            info.x = spriteX;
            info.priority = (attr & 0x80) != 0;
            info.palette = (attr & 0x10) ? memory.OBP1 : memory.OBP0;

            int lineIndex = static_cast<int>(currentLine) - spriteY;
            if (attr & 0x40) {
                lineIndex = spriteHeight - 1 - lineIndex;
            }

            if (spriteHeight == 16) {
                tile &= 0xFE;
            }

            uint16_t tileAddr = 0x8000 + tile * 16 + lineIndex * 2;
            uint8_t low = readPPUByte(tileAddr);
            uint8_t high = readPPUByte(tileAddr + 1);

            for (int px = 0; px < 8; ++px) {
                int bit = (attr & 0x20) ? px : (7 - px);
                info.pixels[px] = static_cast<uint8_t>(((high >> bit) & 0x01) << 1 | ((low >> bit) & 0x01));
            }
        }
    }
}

void PPU::stepMode3() {
    int screenX = dotCounter - MODE3_START-8;

    if (!windowActive && windowEnabledThisLine && screenX >= windowTriggerX) {
        windowActive = true;
        windowLineStarted = true;
        fetcherX = 0;
        fetcherState = 0;
        scxDiscard = 0;
        bgFifo.clear();
    }

    bool bgEnabled = (memory.LCDC & 0x01) != 0;

    if (bgFifo.size() <= 8) {
        switch (fetcherState) {
            case 0: {
                fetchUsingWindow = windowActive && windowEnabledThisLine;
                uint16_t tileMapBase = fetchUsingWindow
                    ? ((memory.LCDC & 0x40) ? 0x9C00 : 0x9800)
                    : ((memory.LCDC & 0x08) ? 0x9C00 : 0x9800);

                uint8_t tileRow = fetchUsingWindow
                    ? static_cast<uint8_t>((windowLineCounter >> 3) & 0x1F)
                    : static_cast<uint8_t>((bgLineY >> 3) & 0x1F);
                uint8_t tileCol = fetchUsingWindow
                    ? static_cast<uint8_t>(fetcherX & 0x1F)
                    : static_cast<uint8_t>(((memory.SCX >> 3) + fetcherX) & 0x1F);
                uint16_t tileMapAddr = tileMapBase + tileRow * 32 + tileCol;

                fetchTileNumber = readPPUByte(tileMapAddr);
                fetcherState = 1;
                break;
            }
            case 1:
                fetcherState = 2;
                break;
            case 2: {
                bool unsignedIndex = (memory.LCDC & 0x10) != 0;
                int16_t tileIndex = unsignedIndex
                    ? static_cast<int16_t>(fetchTileNumber)
                    : static_cast<int8_t>(fetchTileNumber);
                uint16_t tileAddrBase = unsignedIndex ? 0x8000 : 0x9000;
                uint8_t lineInTile = fetchUsingWindow
                    ? static_cast<uint8_t>(windowLineCounter & 0x07)
                    : static_cast<uint8_t>(bgLineY & 0x07);
                fetchTileAddr = static_cast<uint16_t>(tileAddrBase + tileIndex * 16 + lineInTile * 2);
                fetchDataLow = readPPUByte(fetchTileAddr);
                fetcherState = 3;
                break;
            }
            case 3:
                fetcherState = 4;
                break;
            case 4:
                fetchDataHigh = readPPUByte(fetchTileAddr + 1);
                fetcherState = 5;
                break;
            case 5:
                fetcherState = 6;
                break;
            case 6: {
                for (int bit = 7; bit >= 0; --bit) {
                    uint8_t color = 0;
                    if (bgEnabled) {
                        color = static_cast<uint8_t>(((fetchDataHigh >> bit) & 0x01) << 1 | ((fetchDataLow >> bit) & 0x01));
                    }
                    bgFifo.push_back(color);
                }
                fetcherX = static_cast<uint8_t>(fetcherX + 1);
                fetcherState = 0;
                break;
            }
        }
    }

    if (bgFifo.empty()) {
        return;
    }

    if (scxDiscard > 0 && !windowActive) {
        bgFifo.pop_front();
        --scxDiscard;
        return;
    }

    uint8_t bgColor = bgFifo.front();
    bgFifo.pop_front();

    if (screenX < 0 || screenX >= 160 || currentLine >= 144) {
        return;
    }

    uint32_t pixel = decodeDMGColor(memory.BGP, bgColor);
    uint8_t finalColor = bgColor;
    bool bgOpaque = bgColor != 0;

    for (int s = 0; s < spriteCount; ++s) {
        const SpriteLine& spr = spriteLineBuffer[s];
        if (screenX < spr.x || screenX >= spr.x + 8) {
            continue;
        }

        uint8_t spriteColor = spr.pixels[screenX - spr.x];
        if (spriteColor == 0) {
            continue;
        }

        if (spr.priority && bgOpaque) {
            continue;
        }

        pixel = decodeDMGColor(spr.palette, spriteColor);
        finalColor = spriteColor;
        bgOpaque = true;
        break;
    }

    framebuffer[currentLine * 160 + screenX] = pixel;
    bgLineColor[screenX] = finalColor;
}

uint8_t PPU::readPPUByte(uint16_t addr) {
    bool vramPrev = memory.vramLocked;
    bool oamPrev = memory.oamLocked;
    memory.vramLocked = false;
    memory.oamLocked = false;
    uint8_t value = memory.readByte(addr);
    memory.vramLocked = vramPrev;
    memory.oamLocked = oamPrev;
    return value;
}

uint32_t PPU::decodeDMGColor(uint8_t palette, uint8_t colorId) const {
    static constexpr uint32_t COLORS[4] = {
        0xFFFFFFFF,
        0xFFBFBFBF,
        0xFF7F7F7F,
        0xFF1F1F1F
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
