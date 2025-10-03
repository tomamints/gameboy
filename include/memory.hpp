#pragma once
#include <cstdint>
#include <vector>
#include <string>

class Memory {
public:
    Memory();
    void loadROM(const std::string& path);
    uint8_t readByte(uint16_t addr) const;
    void writeByte(uint16_t addr, uint8_t val);

    uint8_t LY = 0;
    uint8_t if_reg = 0x00;
    uint8_t ie     = 0x00;
    uint8_t SB = 0;      // 0xFF01 シリアルデータ
    uint8_t SC = 0;      // 0xFF02 シリアル制御

private:
    std::vector<uint8_t> rom; //ROM 32kb
    std::vector<uint8_t> vram; //8kb video ram
    std::vector<uint8_t> wram; //work ram 8kb
    std::vector<uint8_t> oam; //object attribute memory 160bytes
    std::vector<uint8_t> hram; //high ram 127bytes
};
