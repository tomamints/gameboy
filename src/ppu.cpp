#include "ppu.hpp"
#include "memory.hpp"

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
    currentLine = 0;
    // 初期化時にLYも0にしておく
    memory.writeByte(0xFF44, 0x00);
}

void PPU::step(int cycles) {
    scanlineCounter -= cycles;

    if (scanlineCounter <= 0) {
        // ラインが終わったら次へ
        currentLine++;
        scanlineCounter += SCANLINE_CYCLES;

        // LYレジスタを更新
        memory.writeByte(0xFF44, currentLine);

        // VBlank開始
        if (currentLine == VBLANK_START) {
            // TODO: VBlank割り込み要求
        }

        // 1フレーム終了
        if (currentLine >= TOTAL_LINES) {
            currentLine = 0;
            memory.writeByte(0xFF44, currentLine);
        }
    }
}
