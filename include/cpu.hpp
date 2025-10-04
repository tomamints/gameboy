#pragma once
#include <cstdint>
#include "memory.hpp"
#include "ppu.hpp"

// ---------------------------
// Fレジスタ用ビットマスク定義
// ---------------------------
const uint8_t FLAG_Z = 0x80; // 1000 0000  Zeroフラグ
const uint8_t FLAG_N = 0x40; // 0100 0000  Subtractフラグ
const uint8_t FLAG_H = 0x20; // 0010 0000  Half Carryフラグ
const uint8_t FLAG_C = 0x10; // 0001 0000  Carryフラグ

class CPU {
public:
    CPU(Memory* mem, PPU* ppu);
    void reset();      // CPUを初期化する
    int step();       // 1命令を実行する

private:
    Memory* memory;
    PPU* ppu;

    // レジスタ
    uint8_t A, F;      // A: アキュムレータ, F: フラグ
    uint8_t B, C;
    uint8_t D, E;
    uint8_t H, L;

    uint16_t PC;       // プログラムカウンタ
    uint16_t SP;       // スタックポインタ
    int cycles;

    bool ime = false; //割り込みフラグ
    bool halted = false; // HALT状態フラグ
    bool ei_delay = false; // EI命令の遅延フラグ
    void handleInterrupts();
    void executeCB();
    uint8_t& getRegisterRef(uint8_t code);
};
