#include "emulator.hpp"
#include <iostream>

Emulator::Emulator()
    : ppu(memory),
      cpu(&memory, &ppu) {}

void Emulator::loadROM(const std::string& path) {
    memory.loadROM(path);
    cpu.reset(); //ROM ロード後にCPUを初期化
}

void Emulator::run() {
    std::cout << "Emulator running...\n";

    for (int i = 0; i < 50'000'000; ++i) {
        cpu.step();

        // シリアル出力チェック
        uint8_t sc = memory.readByte(0xFF02);
        if (sc & 0x80) {                     // 送信開始フラグ
            char c = static_cast<char>(memory.readByte(0xFF01));
            std::cout << c;                   // コンソールに出力
            memory.writeByte(0xFF02, 0x00);    // フラグをクリア
        }
    }

    std::cout << "\n[Run finished]\n";
}
