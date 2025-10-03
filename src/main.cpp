#include "emulator.hpp"
#include<iostream>

int main() {
    Emulator emu;                            // エミュレータ本体を作成
    emu.loadROM("../roms/cpu_instrs.gb");           // ROMを読み込み+CPU初期化

    std::cout << "First byte of ROM:"
              << std::hex << (int)emu.readByte(0x0000) << std::endl;
    emu.run();                               // メインループ開始
    return 0;
}
