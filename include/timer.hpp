#pragma once
#include <cstdint>

class Memory;  // 前方宣言でOK

class Timer {
public:
    explicit Timer(Memory* mem);
    void reset();
    void step(int cycles);  // CPUの命令実行サイクルを渡して進める

private:
    Memory* memory;
    uint16_t divCounter;    // 内部クロックカウンタ
};
