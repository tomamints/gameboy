#include "memory.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

Memory::Memory()
    : rom(),
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
    rom.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (rom.empty()) {
        std::cerr << "ROM file is empty: " << path << std::endl;
        return;
    }

    if (rom.size() % 0x4000 != 0) {
        size_t padded = ((rom.size() + 0x3FFF) / 0x4000) * 0x4000;
        rom.resize(padded, 0xFF);
    }

    romBankCount = rom.size() / 0x4000;
    if (romBankCount == 0) {
        romBankCount = 1;
    }
    romBankLower = 1;
    romBankUpper = 0;
    mbc1Mode = 0;

    std::cout << "ROM loaded: " << path << std::endl;
    std::cout << "Total ROM size: " << rom.size() << " bytes (" << romBankCount << " banks)" << std::endl;
}

uint8_t Memory::readByte(uint16_t addr) const { // メモリからバイトを読み込む
    if (addr < 0x8000) {
        return readROM(addr);
    } else if (addr < 0xA000) {
        if (vramLocked) return 0xFF;
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
        if (oamLocked) return 0xFF;
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
    if (addr < 0x2000) {
        // RAM有効化コマンド（未使用）
    } else if (addr < 0x4000) {
        romBankLower = val & 0x1F;
        if ((romBankLower & 0x1F) == 0) {
            romBankLower = 1;
        }
    } else if (addr < 0x6000) {
        romBankUpper = val & 0x03;
    } else if (addr < 0x8000) {
        mbc1Mode = val & 0x01;
        // RAMバンク未実装のため実質ROMバンクモードのみ
    } else if (addr < 0xA000) {
        if (vramLocked) return;
        vram[addr - 0x8000] = val;
    } else if (addr < 0xC000) {
        // カートリッジRAM未実装
    } else if (addr < 0xE000) {
        wram[addr - 0xC000] = val;
    } else if (addr < 0xFE00) {
        wram[addr - 0xE000] = val;         // Echo RAM
    } else if (addr < 0xFEA0) {
        if (oamLocked) return;
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

size_t Memory::currentROMBank() const {
    if (romBankCount == 0) {
        return 0;
    }

    uint8_t bank = (romBankUpper << 5) | (romBankLower & 0x1F);

    if ((bank & 0x1F) == 0 && romBankCount > 1) {
        bank |= 0x01;
    }

    bank %= static_cast<uint8_t>(romBankCount);
    if (bank >= romBankCount && romBankCount > 0) {
        bank = static_cast<uint8_t>(romBankCount - 1);
    }

    if (bank == 0 && romBankCount > 1) {
        bank = 1;
    }

    return bank;
}

uint8_t Memory::readROM(uint16_t addr) const {
    if (rom.empty()) {
        return 0xFF;
    }

    if (addr < 0x4000) {
        size_t index = addr;
        if (index < rom.size()) {
            return rom[index];
        }
        return 0xFF;
    }

    size_t bank = currentROMBank();
    size_t offset = addr - 0x4000;
    size_t base = bank * 0x4000;
    size_t index = base + offset;
    if (index < rom.size()) {
        return rom[index];
    }

    return 0xFF;
}
