#include "emulator.hpp"
#include<iostream>

int main() {
    Emulator emu;                            // エミュレータ本体を作成
    emu.loadROM("../roms/cpu_instrs.gb");           // ROMを読み込み+CPU初期化

    emu.run();                               // メインループ開始
    return 0;
}
