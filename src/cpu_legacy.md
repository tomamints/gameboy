#include "cpu.hpp"
#include <iostream>
#include <iomanip>


CPU::CPU(Memory* mem, PPU* ppu)
    : memory(mem), ppu(ppu),
      A(0),B(0),C(0),D(0),E(0),H(0),L(0),F(0),
      PC(0),SP(0),cycles(0) {}

void CPU::reset() {
    // 後で初期化コードを入れる
    PC = 0x0100; // ゲームボーイでは通常ROM開始アドレス
    SP = 0xFFFE;
    A = 0;
    F = 0;
    std::cout << "CPU reset done.\n";
}

void CPU::step() {
    // 1. 命令をフェッチ
    uint8_t opcode = memory->readByte(PC++);

    // 2. デコードして実行
    switch (opcode) {
        // --------- ロード系 ----------
        case 0x00:  // NOP
        cycles += 4;
        break;

        case 0x01:  // LD BC,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            B = high;
            C = low;
            cycles += 12;
            std::cout << "LD BC, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x06:  // LD B,d8
        {
            uint8_t val = memory->readByte(PC++);
            B = val;
            cycles += 8;
            std::cout << "LD B, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x0A:  // LD A,(BC)
        {
        A = memory->readByte((B << 8) | C);
        cycles += 8;   // メモリアクセスあり
        std::cout << "LD A,(BC) → " << std::hex << (int)A << "\n";
        break;
        }

        case 0x0E:  // LD C,d8
        {
            uint8_t val = memory->readByte(PC++);
            C = val;
            cycles += 8;
            std::cout << "LD C, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x11:  // LD DE,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            D = high;
            E = low;
            cycles += 12;
            std::cout << "LD DE, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x16:  // LD D,d8
        {
            uint8_t val = memory->readByte(PC++);
            D = val;
            cycles += 8;
            std::cout << "LD D, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x1E:  // LD E,d8
        {
            uint8_t val = memory->readByte(PC++);
            E = val;
            cycles += 8;
            std::cout << "LD E, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x21:  // LD HL,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            H = high;
            L = low;
            cycles += 12;
            std::cout << "LD HL, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x26:  // LD H,d8
        {
            uint8_t val = memory->readByte(PC++);
            H = val;
            cycles += 8;
            std::cout << "LD H, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x2E:  // LD L,d8
        {
            uint8_t val = memory->readByte(PC++);
            L = val;
            cycles += 8;
            std::cout << "LD L, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x31:  // LD SP,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            SP = (high << 8) | low;
            cycles += 12;
            std::cout << "LD SP, " << std::hex << SP << "\n";
            break;
        }

        case 0x3E:  // LD A,d8
        {
            uint8_t value = memory->readByte(PC++);
            A = value;
            cycles += 8;
            std::cout << "LD A, " << std::hex << (int)value << "\n";
            break;
        }

        case 0x6A:  // LD L,D
        L = D;
        cycles += 4;
        std::cout << "LD L,D → " << std::hex << (int)L << "\n";
        break;

        case 0x7C:  // LD A,H
        A = H;
        cycles += 4;
        std::cout << "LD A,H → " << std::hex << (int)A << "\n";
        break;

        case 0x7D:  // LD A,L
        A = L;
        cycles += 4;
        std::cout << "LD A,L → " << std::hex << (int)A << "\n";
        break;

        // --------- INC / DEC 系 ----------
        case 0x04:  // INC B
        B++;
        F &= FLAG_C;
        if (B == 0)  F |= FLAG_Z;
        F &= ~FLAG_N;
        if ((B & 0x0F) == 0) F |= FLAG_H;
        cycles += 4;
        std::cout << "INC B → " << std::hex << (int)B << "\n";
        break;

        case 0x05:  // DEC B
        B--;
        F &= FLAG_C;
        if (B == 0)  F |= FLAG_Z;
        F |= FLAG_N;
        if ((B & 0x0F) == 0x0F) F |= FLAG_H;
        cycles += 4;
        std::cout << "DEC B → " << std::hex << (int)B << "\n";
        break;

        case 0x0B:  // DEC BC
        {
            uint16_t bc = (B << 8) | C;
            bc--;
            B = (bc >> 8) & 0xFF;
            C = bc & 0xFF;
            cycles += 8;
            std::cout << "DEC BC → " << std::hex << bc << "\n";
            break;
        }

        case 0x0D:  // DEC C
        C--;
        F &= FLAG_C;
        if (C == 0)  F |= FLAG_Z;
        F |= FLAG_N;
        if ((C & 0x0F) == 0x0F) F |= FLAG_H;
        cycles += 4;
        std::cout << "DEC C → " << std::hex << (int)C << "\n";
        break;

        case 0x3B:  // DEC SP
        SP--;
        cycles += 8;
        std::cout << "DEC SP → " << std::hex << SP << "\n";
        break;

        // --------- 論理系 ----------
        case 0xA3:  // AND E
        A = A & E;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        F |= FLAG_H;
        cycles += 4;
        std::cout << "AND E → " << std::hex << (int)A << "\n";
        break;

        case 0xB2:  // OR D
        A = A | D;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 4;
        std::cout << "OR D → " << std::hex << (int)A << "\n";
        break;

        case 0x2F:  // CPL
        A = ~A;
        F |= (FLAG_N | FLAG_H);
        cycles += 4;
        std::cout << "CPL → " << std::hex << (int)A << "\n";
        break;

        // --------- ジャンプ / CALL / RET 系 ----------
        case 0x18:  // JR r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            PC = PC + offset;
            cycles += 12;
            std::cout << "JR to " << std::hex << PC << " (offset=" << (int)offset << ")\n";
            break;
        }

        case 0xC3:  // JP a16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;
            PC = addr;
            cycles += 16;
            std::cout << "JP to " << std::hex << addr << "\n";
            break;
        }

        case 0xCD:  // CALL a16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;

            uint8_t pcLow  = PC & 0xFF;
            uint8_t pcHigh = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHigh);
            SP--; memory->writeByte(SP, pcLow);

            PC = addr;
            cycles += 24;
            std::cout << "CALL " << std::hex << addr
                    << " (push return addr=" << std::hex
                    << ((pcHigh<<8)|pcLow) << ")\n";
            break;
        }

        case 0xC9:  // RET
        {
            uint8_t low  = memory->readByte(SP++);
            uint8_t high = memory->readByte(SP++);
            PC = (high << 8) | low;
            cycles += 16;
            std::cout << "RET → return to " << std::hex << PC << "\n";
            break;
        }

        // -------- 割り込み制御 --------
        case 0xF3:  // DI
            std::cout << "DI (disable interrupts)\n";
            cycles += 4;
            break;

        // -------- ロード命令 --------
        case 0xEA:  // LD (a16),A
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;
            memory->writeByte(addr, A);
            std::cout << "LD (" << std::hex << addr << "), A=" << (int)A << "\n";
            cycles += 16;
            break;
        }

        case 0xE0:  // LDH (a8),A
        {
            uint8_t offset = memory->readByte(PC++);
            uint16_t addr = 0xFF00 + offset;
            memory->writeByte(addr, A);
            std::cout << "LDH (" << std::hex << addr << "), A=" << (int)A << "\n";
            cycles += 12;
            break;
        }

        // -------- スタック操作 --------
        case 0xF5:  // PUSH AF
        {
            SP--; memory->writeByte(SP, A);  // 上位Aをpush
            SP--; memory->writeByte(SP, F);  // 下位Fをpush
            cycles += 16;
            std::cout << "PUSH AF (A=" << std::hex << (int)A
                    << ", F=" << std::hex << (int)F << ")\n";
            break;
        }

        // -------- 比較命令 --------
        case 0xFE:  // CP d8
        {
            uint8_t value = memory->readByte(PC++);
            uint8_t result = A - value;

            F = 0;
            if (result == 0) F |= FLAG_Z;                // ゼロならZ=1
            F |= FLAG_N;                                 // 減算なのでN=1
            if ((A & 0x0F) < (value & 0x0F)) F |= FLAG_H; // 借り発生でH=1
            if (A < value) F |= FLAG_C;                   // キャリー（借り）発生でC=1

            cycles += 8;
            std::cout << "CP " << std::hex << (int)value
                    << " (A=" << std::hex << (int)A << ")\n";
            break;
        }

        // -------- 条件付きCALL --------
        case 0xC4:  // CALL NZ,a16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;

            if (!(F & FLAG_Z)) {  // Zフラグが0ならCALL実行
                uint8_t pcLow  = PC & 0xFF;
                uint8_t pcHigh = (PC >> 8) & 0xFF;
                SP--; memory->writeByte(SP, pcHigh);
                SP--; memory->writeByte(SP, pcLow);

                PC = addr;
                cycles += 24;
                std::cout << "CALL NZ to " << std::hex << addr << "\n";
            } else {
                cycles += 12; // 条件不成立時は12クロック消費のみ
                std::cout << "CALL NZ skipped (Z=1)\n";
            }
            break;
        }

        // -------- 算術命令 --------
        case 0xD6:  // SUB d8
        {
            uint8_t value = memory->readByte(PC++);
            uint16_t result = A - value;

            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;          // ゼロフラグ
            F |= FLAG_N;                                   // 減算なのでN=1
            if ((A & 0x0F) < (value & 0x0F)) F |= FLAG_H;   // 借りが発生でH=1
            if (A < value) F |= FLAG_C;                     // キャリー（借り）でC=1

            A = static_cast<uint8_t>(result);
            cycles += 8;
            std::cout << "SUB " << std::hex << (int)value
                    << " → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x80:  // ADD A,B
        {
            uint16_t result = A + B;

            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;          // ゼロフラグ
            if (((A & 0x0F) + (B & 0x0F)) > 0x0F) F |= FLAG_H; // 下位4bitでキャリー
            if (result > 0xFF) F |= FLAG_C;                  // キャリー発生

            A = static_cast<uint8_t>(result);
            cycles += 4;
            std::cout << "ADD A,B → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x02:  // LD (BC),A
        {
            uint16_t addr = (B << 8) | C;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (" << std::hex << addr << "),A=" << (int)A << "\n";
            break;
        }
        case 0xE5:  // PUSH HL
        {
            SP--; memory->writeByte(SP, H);
            SP--; memory->writeByte(SP, L);
            cycles += 16;
            std::cout << "PUSH HL (H=" << std::hex << (int)H
                    << ", L=" << std::hex << (int)L << ")\n";
            break;
        }

        case 0xE1:  // POP HL
        {
            L = memory->readByte(SP++);
            H = memory->readByte(SP++);
            cycles += 12;
            std::cout << "POP HL → H=" << std::hex << (int)H
                    << ", L=" << std::hex << (int)L << "\n";
            break;
        }

        case 0x23:  // INC HL
        {
            uint16_t hl = (H << 8) | L;
            hl++;
            H = (hl >> 8) & 0xFF;
            L = hl & 0xFF;
            cycles += 8;
            std::cout << "INC HL → " << std::hex << hl << "\n";
            break;
        }

        case 0x2A:  // LD A,(HL+)
        {
            uint16_t addr = (H << 8) | L;
            A = memory->readByte(addr);
            addr++;
            H = (addr >> 8) & 0xFF;
            L = addr & 0xFF;
            cycles += 8;
            std::cout << "LD A,(HL+) → A=" << std::hex << (int)A
                      << ", HL=" << std::hex << addr << "\n";
            break;
        }

        case 0xF1:  // POP AF
        {
            F = memory->readByte(SP++);
            A = memory->readByte(SP++);
            // Fの下位4ビットは常に0にする必要あり
            F &= 0xF0;
            cycles += 12;
            std::cout << "POP AF → A=" << std::hex << (int)A
                    << ", F=" << std::hex << (int)F << "\n";
            break;
        }


        case 0xC5:  // PUSH BC
        {
            SP--; memory->writeByte(SP, B);
            SP--; memory->writeByte(SP, C);
            cycles += 16;
            std::cout << "PUSH BC (B=" << std::hex << (int)B
                    << ", C=" << std::hex << (int)C << ")\n";
            break;
        }

        case 0xC1:  // POP BC
        {
            C = memory->readByte(SP++);
            B = memory->readByte(SP++);
            cycles += 12;
            std::cout << "POP BC → B=" << std::hex << (int)B
                    << ", C=" << std::hex << (int)C << "\n";
            break;
        }

        case 0x03:  // INC BC
        {
            uint16_t bc = (B << 8) | C;
            bc++;
            B = (bc >> 8) & 0xFF;
            C = bc & 0xFF;
            cycles += 8;
            std::cout << "INC BC → " << std::hex << bc << "\n";
            break;
        }

        case 0x78:  // LD A,B
        {
        A = B;
        cycles += 4;
        std::cout << "LD A,B → " << std::hex << (int)A << "\n";
        break;
        }

        case 0xB1:  // OR C
        {
        A = A | C;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 4;
        std::cout << "OR C → " << std::hex << (int)A << "\n";
        break;
        }
        case 0x28:  // JR Z,r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            if (F & FLAG_Z) {
                PC += offset;
                cycles += 12;
                std::cout << "JR Z taken → to " << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "JR Z not taken (Z=0)\n";
            }
            break;
        }
        case 0x20:  // JR NZ,r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            if (!(F & FLAG_Z)) {
                PC += offset;
                cycles += 12;
                std::cout << "JR NZ taken → to " << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "JR NZ not taken (Z=1)\n";
            }
            break;
        }
        case 0x44:  // LD B,H
        {
        B = H;
        cycles += 4;
        std::cout << "LD B,H → " << std::hex << (int)B << "\n";
        break;
        }

        case 0xF0:  // LDH A,(a8)
        {
            uint8_t offset = memory->readByte(PC++);
            uint16_t addr = 0xFF00 + offset;
            A = memory->readByte(addr);
            cycles += 12;
            std::cout << "LDH A,(" << std::hex << addr << ") → A=" << (int)A << "\n";
            break;
        }
        case 0xFB:  // EI
        ime = true;           // ※本当は1命令遅延だが簡易
        cycles += 4;
        std::cout << "EI (enable interrupts)\n";
        break;

        case 0x77:  // LD (HL),A
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (HL),A → [" << std::hex << addr << "]=" << (int)A << "\n";
            break;
        }

        case 0x2C:  // INC L
        {
            L++;
            F &= FLAG_C;           // キャリーフラグは維持
            if (L == 0)  F |= FLAG_Z;
            F &= ~FLAG_N;          // Nは0（加算）
            if ((L & 0x0F) == 0) F |= FLAG_H;
            cycles += 4;
            std::cout << "INC L → " << std::hex << (int)L << "\n";
            break;
        }
        case 0x24:  // INC H
        {
            H++;
            F &= FLAG_C;           // キャリーフラグ維持
            if (H == 0)  F |= FLAG_Z;
            F &= ~FLAG_N;          // Nは0（加算）
            if ((H & 0x0F) == 0) F |= FLAG_H;
            cycles += 4;
            std::cout << "INC H → " << std::hex << (int)H << "\n";
            break;
        }
        case 0x47: // LD B,A
            B = A;
            cycles += 4;
            std::cout << "LD B,A → " << std::hex << (int)B << "\n";
            break;

        case 0x4F: // LD C,A
            C = A;
            cycles += 4;
            std::cout << "LD C,A → " << std::hex << (int)C << "\n";
            break;

        case 0x57: // LD D,A
            D = A;
            cycles += 4;
            std::cout << "LD D,A → " << std::hex << (int)D << "\n";
            break;

        case 0x5F: // LD E,A
            E = A;
            cycles += 4;
            std::cout << "LD E,A → " << std::hex << (int)E << "\n";
            break;

        case 0x79: // LD A,C
            A = C;
            cycles += 4;
            std::cout << "LD A,C → " << std::hex << (int)A << "\n";
            break;

        case 0x7A: // LD A,D
            A = D;
            cycles += 4;
            std::cout << "LD A,D → " << std::hex << (int)A << "\n";
            break;

        case 0x7B: // LD A,E
            A = E;
            cycles += 4;
            std::cout << "LD A,E → " << std::hex << (int)A << "\n";
            break;

        //---------------------- PUSH系 ----------------------
        case 0xD5: // PUSH DE
        SP--; memory->writeByte(SP, D);
        SP--; memory->writeByte(SP, E);
        cycles += 16;
        std::cout << "PUSH DE (D=" << std::hex << (int)D
                << ", E=" << std::hex << (int)E << ")\n";
        break;

        //---------------------- LD系 ------------------------
        case 0x46: // LD B,(HL)
        {
        uint16_t addr = (H << 8) | L;
        B = memory->readByte(addr);
        cycles += 8;
        std::cout << "LD B,(HL) → B=" << std::hex << (int)B << "\n";
        break;
        }
        case 0x4E: // LD C,(HL)
        {
        uint16_t addr = (H << 8) | L;
        C = memory->readByte(addr);
        cycles += 8;
        std::cout << "LD C,(HL) → C=" << std::hex << (int)C << "\n";
        break;
        }
        case 0x56: // LD D,(HL)
        {
        uint16_t addr = (H << 8) | L;
        D = memory->readByte(addr);
        cycles += 8;
        std::cout << "LD D,(HL) → D=" << std::hex << (int)D << "\n";
        break;
        }

        //---------------------- DEC --------------------------
        case 0x2D: // DEC L
        L--;
        F &= FLAG_C;
        if (L == 0) F |= FLAG_Z;
        F |= FLAG_N;
        if ((L & 0x0F) == 0x0F) F |= FLAG_H;
        cycles += 4;
        std::cout << "DEC L → " << std::hex << (int)L << "\n";
        break;

        //---------------------- 論理・算術 --------------------
        case 0xAE: // XOR (HL)
        {
        uint16_t addr = (H << 8) | L;
        A ^= memory->readByte(addr);
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 8;
        std::cout << "XOR (HL) → A=" << std::hex << (int)A << "\n";
        break;
        }
        case 0xB8: // CP B
        {
        uint8_t result = A - B;
        F = FLAG_N;
        if (result == 0) F |= FLAG_Z;
        if ((A & 0x0F) < (B & 0x0F)) F |= FLAG_H;
        if (A < B) F |= FLAG_C;
        cycles += 4;
        std::cout << "CP B (A=" << std::hex << (int)A << ", B=" << (int)B << ")\n";
        break;
        }
        case 0x83: // ADD A,E
        {
        uint16_t result = A + E;
        F = 0;
        if ((result & 0xFF) == 0) F |= FLAG_Z;
        if (((A & 0x0F) + (E & 0x0F)) > 0x0F) F |= FLAG_H;
        if (result > 0xFF) F |= FLAG_C;
        A = (uint8_t)result;
        cycles += 4;
        std::cout << "ADD A,E → A=" << std::hex << (int)A << "\n";
        break;
        }
        case 0xEE: // XOR d8
        {
        uint8_t value = memory->readByte(PC++);
        A ^= value;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 8;
        std::cout << "XOR " << std::hex << (int)value
                << " → A=" << (int)A << "\n";
        break;
        }

        //---------------------- 分岐 --------------------------
        case 0x30: // JR NC,r8
        {
        int8_t offset = (int8_t)memory->readByte(PC++);
        if (!(F & FLAG_C)) {
            PC += offset;
            cycles += 12;
            std::cout << "JR NC taken → to " << std::hex << PC << "\n";
        } else {
            cycles += 8;
            std::cout << "JR NC not taken (C=1)\n";
        }
        break;
        }
        case 0x38: // JR C,r8
        {
        int8_t offset = (int8_t)memory->readByte(PC++);
        if (F & FLAG_C) {
            PC += offset;
            cycles += 12;
            std::cout << "JR C taken → to " << std::hex << PC << "\n";
        } else {
            cycles += 8;
            std::cout << "JR C not taken (C=0)\n";
        }
        break;
        }

        //---------------------- その他 ------------------------
        case 0x10: // STOP
        std::cout << "STOP (treated as NOP)\n";
        cycles += 4;
        break;

        //---------------------- フラグ操作 -------------------
        case 0x37: // SCF (Set Carry Flag)
        F &= FLAG_Z;       // Zフラグは維持
        F &= ~FLAG_N;      // Nフラグをクリア
        F &= ~FLAG_H;      // Hフラグをクリア
        F |= FLAG_C;       // Cフラグをセット
        cycles += 4;
        std::cout << "SCF → F=" << std::hex << (int)F << "\n";
        break;

        //---------------------- 算術系 -----------------------
        case 0x3C: // INC A
        A++;
        F &= FLAG_C;                // Cは保持
        if (A == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
        F &= ~FLAG_N;
        if ((A & 0x0F) == 0x00) F |= FLAG_H;
        cycles += 4;
        std::cout << "INC A → A=" << std::hex << (int)A << "\n";
        break;

        //---------------------- 論理演算 ---------------------
        case 0xE6: // AND d8
        {
        uint8_t value = memory->readByte(PC++);
        A &= value;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        F |= FLAG_H;                 // AND は常にH=1
        cycles += 8;
        std::cout << "AND " << std::hex << (int)value
                << " → A=" << (int)A << "\n";
        break;
        }

        //---------------------- CBプレフィックス -------------
        case 0xCB: // CBプレフィックスは未実装。ひとまずNOP扱い
        {
        uint8_t cbcode = memory->readByte(PC++);
        std::cout << "CB prefix opcode 0x"
                << std::hex << (int)cbcode
                << " (not implemented, treated as NOP)\n";
        cycles += 8; // 仮のサイクル
        break;
        }

        //---------------------- DEC H ----------------------
        case 0x25: // DEC H
        H--;
        F &= FLAG_C;                    // Cは保持
        if (H == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
        F |= FLAG_N;                    // デクリメントなので N=1
        if ((H & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
        cycles += 4;
        std::cout << "DEC H → H=" << std::hex << (int)H << "\n";
        break;

        //---------------------- RRA ------------------------
        case 0x1F: // RRA (Rotate A right through Carry)
        {
        uint8_t carry_in  = (F & FLAG_C) ? 0x80 : 0x00;  // Cを上位ビットへ
        uint8_t carry_out = A & 0x01;                    // LSB が次のC
        A = (A >> 1) | carry_in;

        F = 0;
        if (carry_out) F |= FLAG_C;                      // 新しいC
        if (A == 0)     F |= FLAG_Z;                      // Zフラグ設定
        cycles += 4;
        std::cout << "RRA → A=" << std::hex << (int)A
                << " C=" << ((F & FLAG_C) ? 1 : 0) << "\n";
        break;
        }

        case 0xB7:  // OR A,A
        A = A | A;                      // 値は変わらない
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 4;
        std::cout << "OR A,A → A=" << std::hex << (int)A << "\n";
        break;

        case 0x22:  // LD (HL+),A
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, A);
            L++;
            if (L == 0) H++;                 // 桁上がり
            cycles += 8;
            std::cout << "LD (HL+),A → [" << std::hex << addr
                    << "]=" << (int)A
                    << " HL→" << ((H<<8)|L) << "\n";
            break;
        }

        case 0x72:  // LD (HL),D
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, D);
            cycles += 8;
            std::cout << "LD (HL),D → [" << std::hex << addr
                    << "]=" << (int)D << "\n";
            break;
        }
        case 0x71:  // LD (HL),C
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, C);
            cycles += 8;
            std::cout << "LD (HL),C → [" << std::hex << addr
                    << "]=" << (int)C << "\n";
            break;
        }
        case 0x70:  // LD (HL),B
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, B);
            cycles += 8;
            std::cout << "LD (HL),B → [" << std::hex << addr
                    << "]=" << (int)B << "\n";
            break;
        }

        case 0xD1:  // POP DE
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            D = hi;
            E = lo;
            cycles += 12;
            std::cout << "POP DE → D=" << std::hex << (int)D
                    << " E=" << (int)E << "\n";
            break;
        }


        default:
            std::cout << "Unknown opcode: 0x"
                      << std::hex << (int)opcode
                      << " at PC=" << PC-1 << "\n";
            break;
    }
    handleInterrupts();
    memory->currentLY = (memory->currentLY + 1) % 154;
}

void CPU::handleInterrupts() {
    // 割り込みが無効なら何もしない
    if (!ime) return;

    // IE と IF の両方を確認
    uint8_t req = memory->if_reg & memory->ie;
    if (req == 0) return;   // 割り込みなし

    // 割り込みを受け付けたら IME をクリア
    ime = false;

    // 優先順位: V-Blank → LCD → Timer → Serial → Joypad
    uint16_t vector = 0;
    if (req & 0x01) {            // V-Blank
        vector = 0x40;
        memory->if_reg &= ~0x01;
        std::cout << "[INT] VBlank\n";
    } else if (req & 0x02) {     // LCD STAT
        vector = 0x48;
        memory->if_reg &= ~0x02;
        std::cout << "[INT] LCD STAT\n";
    } else if (req & 0x04) {     // Timer
        vector = 0x50;
        memory->if_reg &= ~0x04;
        std::cout << "[INT] Timer\n";
    } else if (req & 0x08) {     // Serial
        vector = 0x58;
        memory->if_reg &= ~0x08;
        std::cout << "[INT] Serial\n";
    } else if (req & 0x10) {     // Joypad
        vector = 0x60;
        memory->if_reg &= ~0x10;
        std::cout << "[INT] Joypad\n";
    } else {
        return;  // どれも無ければ戻る
    }

    // 現在のPCをスタックへ退避
    uint8_t pcLow  = PC & 0xFF;
    uint8_t pcHigh = (PC >> 8) & 0xFF;
    SP--; memory->writeByte(SP, pcHigh);
    SP--; memory->writeByte(SP, pcLow);

    // ベクタへジャンプ
    PC = vector;
    cycles += 20; // 割り込み処理のオーバーヘッド
}

