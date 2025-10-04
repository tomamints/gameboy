#include "emulator.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    Emulator emu;                            // エミュレータ本体を作成

    std::string romPath = "../roms/cpu_instrs.gb";
    if (argc > 1) {
        romPath = argv[1];
    }

    std::cout << "Loading ROM: " << romPath << std::endl;
    emu.loadROM(romPath);                    // ROMを読み込み+CPU初期化

    emu.runWithDisplay();                    // SDL2ウィンドウ付きメインループ開始
    return 0;
}
