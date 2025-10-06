#pragma once
#include <cstdint>
#include <vector>
#include <string>

class Input;  // 前方宣言

class Memory {
public:
    Memory();
    void loadROM(const std::string& path);
    uint8_t readByte(uint16_t addr) const;
    void writeByte(uint16_t addr, uint8_t val);

    // OAM DMA関連
    void stepDMA();
    void startDMA(uint8_t sourcePage);

    // Input関連
    void setInputReference(Input* inputPtr);

    uint8_t LY = 0;
    uint8_t if_reg = 0x00;
    uint8_t ie     = 0x00;
    uint8_t SB = 0;      // 0xFF01 シリアルデータ
    uint8_t SC = 0;      // 0xFF02 シリアル制御
    uint8_t DIV = 0;     // 0xFF04 ディバイダレジスタ
    uint8_t TIMA = 0;    // 0xFF05 タイマカウンタ
    uint8_t TMA = 0;     // 0xFF06 タイマモジュロ
    uint8_t TAC = 0;     // 0xFF07 タイマコントロール
    uint8_t LCDC = 0x91; // 0xFF40 LCD制御（初期値）
    uint8_t STAT = 0;    // 0xFF41 LCD状態
    uint8_t SCY = 0;     // 0xFF42 スクロールY
    uint8_t SCX = 0;     // 0xFF43 スクロールX
    uint8_t LYC = 0;     // 0xFF45 LY比較値
    uint8_t BGP = 0xFC;  // 0xFF47 BGパレット
    uint8_t OBP0 = 0xFF; // 0xFF48 OBJパレット0
    uint8_t OBP1 = 0xFF; // 0xFF49 OBJパレット1
    uint8_t WY = 0;      // 0xFF4A ウィンドウY
    uint8_t WX = 0;      // 0xFF4B ウィンドウX
    uint8_t DMA = 0;     // 0xFF46 DMA転送

    bool vramLocked = false;
    bool oamLocked  = false;

    // OAM DMA関連
    bool dmaActive = false;
    uint16_t dmaSource = 0;
    uint8_t dmaCycles = 0;

    // Input関連
    Input* input = nullptr;

private:
    std::vector<uint8_t> rom; // 完全なROMデータ（バンク切り替え対応）
    std::vector<uint8_t> vram; //8kb video ram
    std::vector<uint8_t> wram; //work ram 8kb
    std::vector<uint8_t> oam; //object attribute memory 160bytes
    std::vector<uint8_t> hram; //high ram 127bytes
    size_t romBankCount = 0;
    uint8_t romBankLower = 1;
    uint8_t romBankUpper = 0;
    uint8_t mbc1Mode = 0;

    size_t currentROMBank() const;
    uint8_t readROM(uint16_t addr) const;
};
