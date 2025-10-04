#pragma once
#include <string>
#include "cpu.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "input.hpp"
#include "timer.hpp"
#include "display.hpp"

class Emulator {
public:
    Emulator();
    void loadROM(const std::string& path);
    uint8_t readByte(uint16_t addr) const { return memory.readByte(addr); }
    void run();
    void runWithDisplay(); // SDL2ウィンドウ付き実行

private:
    Memory memory;
    CPU cpu;
    PPU ppu;
    Input input;
    Timer timer;
    Display display;
};
