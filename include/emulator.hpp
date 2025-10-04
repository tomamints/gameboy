#pragma once
#include <string>
#include "cpu.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "input.hpp"
#include "timer.hpp"

class Emulator {
public:
    Emulator();
    void loadROM(const std::string& path);
    uint8_t readByte(uint16_t addr) const { return memory.readByte(addr); }
    void run();
    
private:
    Memory memory;
    CPU cpu;
    PPU ppu;
    Input input;
    Timer timer;
};
