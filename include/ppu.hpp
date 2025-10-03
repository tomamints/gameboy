#pragma once
#include <cstdint>

class Memory;

class PPU {
public:
    PPU(Memory& mem);
    void step(int cycles);
    void reset();
    const uint32_t* getFrameBuffer() const{ return framebuffer;}


private:
    Memory& memory;
    int scanlineCounter = 456;
    uint8_t currentLine = 0;
    uint32_t framebuffer[160*144];//出力先ピクセル

};
