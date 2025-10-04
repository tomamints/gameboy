#include "timer.hpp"
#include "memory.hpp"

Timer::Timer(Memory* mem)
    : memory(mem), divCounter(0) {}

void Timer::reset() {
    divCounter = 0;
    memory->DIV = 0;
}

void Timer::step(int cycles) {
    if (memory->DIV == 0 && ((divCounter >> 8) & 0xFF) != 0) {
        divCounter = 0;
    }

    static constexpr int bitMap[4] = {9, 3, 5, 7};

    for (int i = 0; i < cycles; ++i) {
        uint16_t prevDiv = divCounter;
        ++divCounter;
        memory->DIV = static_cast<uint8_t>((divCounter >> 8) & 0xFF);

        uint8_t tac = memory->TAC;
        if ((tac & 0x04) == 0) {
            continue;
        }

        int bit = bitMap[tac & 0x03];
        bool prevBit = (prevDiv >> bit) & 0x01;
        bool currBit = (divCounter >> bit) & 0x01;

        if (prevBit && !currBit) {
            if (memory->TIMA == 0xFF) {
                memory->TIMA = memory->TMA;
                memory->if_reg |= 0x04;
            } else {
                ++memory->TIMA;
            }
        }
    }
}
