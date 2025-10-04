#include "ppu.hpp"
#include "memory.hpp"
#include <iostream>

// Game Boy仕様
// CPU::step()はTサイクル（ドット）を返すので、1ライン=456ドット
constexpr int SCANLINE_CYCLES = 456;  // Tサイクル（ドット）単位
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

    // デバッグ：大きなサイクル数を検出
    // if (cycles > 1000) {
    //     std::cout << "[PPU] WARNING: Large cycle count: " << cycles
    //               << ", scanlineCounter=" << scanlineCounter << std::endl;
    // }

    // 複数のスキャンラインを正しく処理（中間イベントを逃さない）
    int linesProcessed = 0;
    int maxIterations = 200;  // 安全装置
    while (scanlineCounter <= 0 && maxIterations-- > 0) {
        scanlineCounter += SCANLINE_CYCLES;
        linesProcessed++;

        // 前のラインを記憶
        uint8_t prevLine = currentLine;
        currentLine++;

        if (currentLine >= TOTAL_LINES) {
            currentLine = 0;
        }

        memory.LY = currentLine;

        // 143→144の遷移の瞬間にVBlank割り込みを発火
        if (prevLine == 143 && currentLine == VBLANK_START) {
            std::cout << "[VBlank] Line 143→144 (linesProcessed=" << linesProcessed << ")" << std::endl;
            memory.if_reg |= 0x01;        // VBlank割り込み要求
        }
    }

    // デバッグ：複数ライン処理を検出
    if (linesProcessed > 2) {
        std::cout << "[PPU] WARNING: Processed " << linesProcessed
                  << " lines in single step (cycles=" << cycles
                  << ", scanlineCounter after=" << scanlineCounter << ")" << std::endl;
    }

    if (maxIterations <= 0) {
        std::cout << "[PPU] ERROR: Infinite loop detected! scanlineCounter=" << scanlineCounter << std::endl;
    }
}

