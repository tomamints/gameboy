#include "ppu.hpp"
#include "memory.hpp"
#include <iostream>

// Game Boy仕様
constexpr int SCANLINE_CYCLES = 456;
constexpr int VBLANK_START    = 144;
constexpr int TOTAL_LINES     = 154;

PPU::PPU(Memory& mem)
    : memory(mem)
{
    reset();
}

void PPU::reset() {
    scanlineCounter = SCANLINE_CYCLES;
    currentLine     = 0;
    memory.LY       = 0;
}

void PPU::step(int cycles) {
    scanlineCounter -= cycles;

    if (scanlineCounter <= 0) {
        scanlineCounter += SCANLINE_CYCLES;
        currentLine++;

        if (currentLine >= TOTAL_LINES) {
            currentLine = 0;
        }

        memory.LY = currentLine;          // ← 直接代入でOK

        // VBlank開始（144ライン目）
        if (currentLine == VBLANK_START) {
            std::cout << " VBlank interruput" << std::endl;;
            memory.if_reg |= 0x01;        // VBlank割り込み要求
        }
    }
}

