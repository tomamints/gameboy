#include "emulator.hpp"
#include <iostream>

Emulator::Emulator() : cpu(&memory, &ppu) {}

void Emulator::loadROM(const std::string& path) {
    memory.loadROM(path);
    cpu.reset(); //ROM ロード後にCPUを初期化
}

void Emulator::run() {
    std::cout << "Emulator running...";

    //テストように10ステップだけ実行
    for(int i=0; i<100; ++i){
        cpu.step();
    }

}
