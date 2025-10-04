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
    } else if (addr >= 0xFF00 && addr < 0xFF80) {
        switch(addr) {
            case 0xFF01: return SB;
            case 0xFF02: return SC;
            case 0xFF04: return DIV;
            case 0xFF05: return TIMA;
            case 0xFF06: return TMA;
            case 0xFF07: return TAC;
            case 0xFF0F: return if_reg;
            case 0xFF40: return LCDC;
            case 0xFF41: return STAT;
            case 0xFF42: return SCY;
            case 0xFF43: return SCX;
            case 0xFF44: return LY;
            case 0xFF45: return LYC;
            case 0xFF47: return BGP;
            case 0xFF48: return OBP0;
            case 0xFF49: return OBP1;
            case 0xFF4A: return WY;
            case 0xFF4B: return WX;
            default: return 0xFF;  // 未実装は0xFFを返す
        }
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
    } else if (addr >= 0xFF00 && addr < 0xFF80) {
        switch(addr) {
            case 0xFF01: // SB
                std::cout << "\n\033[1;33;41m"   // 黄文字・赤背景
                          << "[SERIAL] SB ← 0x" << std::hex << (int)val
                          << " (" << std::dec << (char)val << ")\033[0m\n";
                SB = val;
                break;
            case 0xFF02: // SC
                std::cout << "\n\033[1;36;44m"   // 水色文字・青背景
                          << "[SERIAL] SC ← 0x" << std::hex << (int)val
                          << "\033[0m\n";
                SC = val;
                break;
            case 0xFF04: DIV = 0; break;  // DIVへの書き込みは0にリセット
            case 0xFF05: TIMA = val; break;
            case 0xFF06: TMA = val; break;
            case 0xFF07: TAC = val; break;
            case 0xFF0F: if_reg = val; break;
            case 0xFF40: LCDC = val; break;
            case 0xFF41: STAT = val & 0xF8; break;  // 下位3bitは読み取り専用
            case 0xFF42: SCY = val; break;
            case 0xFF43: SCX = val; break;
            case 0xFF44: break;  // LYは書き込み不可
            case 0xFF45: LYC = val; break;
            case 0xFF47: BGP = val; break;
            case 0xFF48: OBP0 = val; break;
            case 0xFF49: OBP1 = val; break;
            case 0xFF4A: WY = val; break;
            case 0xFF4B: WX = val; break;
        }
    } else if (addr < 0xFFFF) {
        hram[addr - 0xFF80] = val;
    } else if (addr == 0xFFFF) {
        ie = val;
    }
}
