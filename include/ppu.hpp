#pragma once
#include <cstdint>
#include <deque>
#include <string>

class Memory;

class PPU {
public:
    PPU(Memory& mem);
    void step(int cycles);
    void reset();
    const uint32_t* getFrameBuffer() const{ return framebuffer;}
    void saveFramePPM(const std::string& path) const;


private:
    struct SpriteLine {
        int x;
        uint8_t palette;
        bool priority;
        uint8_t pixels[8];
    };

    Memory& memory;
    uint8_t currentLine = 0;
    uint8_t mode = 2;           // LCDモード (0: HBlank, 1: VBlank, 2: OAM, 3: Transfer)
    bool coincidence = false;   // LYC=LY フラグ
    uint32_t framebuffer[160*144];//出力先ピクセル
    uint8_t bgLineColor[160]{};  // 背景/ウィンドウの色番号（スプライト優先判定用）
    int dotCounter = 0;         // 現在のライン内ドット位置

    std::deque<uint8_t> bgFifo;
    uint8_t fetchTileNumber = 0;
    uint8_t fetchDataLow = 0;
    uint8_t fetchDataHigh = 0;
    uint8_t fetcherX = 0;
    uint16_t fetchTileAddr = 0;
    bool fetchUsingWindow = false;
    int fetcherState = 0;      // 0: tile, 2: low, 4: high, 6: push
    uint8_t bgLineY = 0;
    uint8_t windowLineCounter = 0;
    bool windowLineStarted = false;
    bool windowActive = false;
    bool windowEnabledThisLine = false;
    int windowTriggerX = 0;
    uint8_t scxDiscard = 0;

    SpriteLine spriteLineBuffer[10];
    int spriteCount = 0;

    void setMode(uint8_t newMode);
    void updateCoincidence();
    void enterMode2();
    void enterMode3();
    void enterMode0();
    void enterVBlank();
    void stepMode3();
    void gatherSprites();
    uint8_t readPPUByte(uint16_t addr);
    uint32_t decodeDMGColor(uint8_t palette, uint8_t colorId) const;
};
