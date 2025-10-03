#include "memory.hpp"
#include <fstream>
#include <iostream>

Memory::Memory()
    : rom(0x8000, 0),
      vram(0x2000, 0),
      wram(0x2000, 0),
      oam(0xA0, 0),
      hram(0x7F, 0),
      ie(0)
{ // 64KBをゼロ初期化
}

void Memory::loadROM(const std::string& path) { // メモリにROMを読み込む
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return;
    }
    file.read(reinterpret_cast<char*>(&rom[0]), rom.size()); // ROM領域へコピー
    std::cout << "ROM loaded: " << path << std::endl;
    std::cout << "I am Toma " << std::endl;
    std::cout << " rom.size " << rom.size() << std::endl;
}

uint8_t Memory::readByte(uint16_t addr) const { // メモリからバイトを読み込む
    if (addr < 0x8000) {
        return rom[addr];
    } else if (addr < 0xA000) {
        return vram[addr - 0x8000]; // ビデオRAMを返す
    } else if (addr < 0xC000) {
        // カートリッジRAM未実装
        return 0;
    } else if (addr < 0xE000) {
        return wram[addr - 0xC000]; // 内部RAMを返す
    } else if (addr < 0xFE00) {
        // Echo RAM →WRAMを返す
        return wram[addr - 0xE000];
    } else if (addr < 0xFEA0) {
        return oam[addr - 0xFE00]; // オブジェクト属性メモリを返す
    } else if (addr < 0xFF00) {
        // 未実装領域
        return 0;
    } else if (addr == 0xFF01) {   // SB
        return SB;
    } else if (addr == 0xFF02) {   // SC
        return SC;
    } else if (addr == 0xFF44) {
        return LY;  // グローバルまたはクラス変数で管理
    } else if (addr == 0xFF0F) {
        return if_reg;
    } else if (addr < 0xFF80) {
        return 0; //I/Oポート未実装
    } else if (addr < 0xFFFF) {
        return hram[addr - 0xFF80]; // ハイレジスタを返す
    } else if (addr == 0xFFFF) {
        return ie; // 割り込みイネーブルレジスタを返す
    } else {
        return 0;
    }
}

void Memory::writeByte(uint16_t addr, uint8_t val) {
    if (addr < 0x8000) {
        // ROMは書き込めない
    } else if (addr < 0xA000) {
        vram[addr - 0x8000] = val;
    } else if (addr < 0xC000) {
        // カートリッジRAM未実装
    } else if (addr < 0xE000) {
        wram[addr - 0xC000] = val;
    } else if (addr < 0xFE00) {
        wram[addr - 0xE000] = val;         // Echo RAM
    } else if (addr < 0xFEA0) {
        oam[addr - 0xFE00] = val;
    } else if (addr < 0xFF00) {
        // 未使用
    } else if (addr == 0xFF01) {        // SB
        SB = val;
    } else if (addr == 0xFF02) {        // SC
        SC = val;
    } else if (addr == 0xFF44){
        // LYはCPUから書き込めないので無視
    } else if (addr == 0xFF0F){
        if_reg = val;
    } else if (addr < 0xFF80) {
        // I/Oレジスタ未実装
    } else if (addr < 0xFFFF) {
        hram[addr - 0xFF80] = val;
    } else if (addr == 0xFFFF) {
        ie = val;
    }
}
