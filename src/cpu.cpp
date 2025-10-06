#include "cpu.hpp"
#include <iostream>
#include <iomanip>


CPU::CPU(Memory* mem, PPU* ppu)
    : memory(mem), ppu(ppu),
      A(0),B(0),C(0),D(0),E(0),H(0),L(0),F(0),
      PC(0),SP(0),cycles(0) {}

      void CPU::reset() {
        PC = 0x0100;   // 実機もここから実行開始
        SP = 0xFFFE;   // スタックポインタ

        std::cout << "[CPU RESET] PC=" << std::hex << PC
                  << " SP=" << SP << std::dec << std::endl;

        // 実機の電源投入直後のレジスタ値
        A = 0x01;
        F = 0xB0;
        B = 0x00;
        C = 0x13;
        D = 0x00;
        E = 0xD8;
        H = 0x01;
        L = 0x4D;

        std::cout << "CPU reset done.\n";
    }


int CPU::step() {
    cycles = 0;  // 必ず初期化！

    // 割り込みチェックを最初に実行
    handleInterrupts();

    // HALT状態のチェック
    if (halted) {
        // 割り込み待ち
        uint8_t req = memory->if_reg & memory->ie;
        if (req != 0) {
            halted = false;  // HALT解除
        } else {
            cycles = 4;  // HALTでもサイクルを消費
            return cycles;
        }
    }

    // 1. 命令をフェッチ
    uint8_t opcode = memory->readByte(PC++);

    // 2. デコードして実行
    switch (opcode) {
        // --------- ロード系 ----------
        // 0x00: NOP
        case 0x00:
        cycles += 4;
        std::cout << "NOP\n";
        break;

        // 0x01: LD BC,d16
        case 0x01:
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            B = hi;
            C = lo;
            cycles += 12;
            std::cout << "LD BC, " << std::hex << ((hi<<8)|lo) << "\n";
            break;
        }

        // 0x02: LD (BC),A
        case 0x02:
        {
            uint16_t addr = (B << 8) | C;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (" << std::hex << addr << "),A=" << (int)A << "\n";
            break;
        }

        // 0x03: INC BC
        case 0x03:
        {
            uint16_t bc = (B << 8) | C;
            bc++;
            B = (bc >> 8) & 0xFF;
            C = bc & 0xFF;
            cycles += 8;
            std::cout << "INC BC → " << std::hex << bc << "\n";
            break;
        }

        // 0x04: INC B
        case 0x04:
            B++;
            F &= FLAG_C;                   // Cフラグ保持
            if (B == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;                  // N=0(加算)
            if ((B & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC B → " << std::hex << (int)B << "\n";
            break;

        // 0x05: DEC B
        case 0x05:
            B--;
            F &= FLAG_C;                   // Cフラグ保持
            if (B == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;                    // 減算なのでN=1
            if ((B & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC B → " << std::hex << (int)B << "\n";
            break;

        // 0x06: LD B,d8
        case 0x06:
        {
            uint8_t val = memory->readByte(PC++);
            B = val;
            cycles += 8;
            std::cout << "LD B, " << std::hex << (int)val << "\n";
            break;
        }

        // 0x07: RLCA (Rotate A left)
        case 0x07:
        {
            uint8_t carry_out = (A & 0x80) ? 1 : 0;
            A = ((A << 1) | carry_out) & 0xFF;
            F = 0;
            if (carry_out) F |= FLAG_C;
            cycles += 4;
            std::cout << "RLCA → A=" << std::hex << (int)A
                        << " C=" << carry_out << "\n";
            break;
        }

        // 0x08: LD (a16),SP
        case 0x08:
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            memory->writeByte(addr, SP & 0xFF);
            memory->writeByte(addr + 1, (SP >> 8) & 0xFF);
            cycles += 20;
            std::cout << "LD (" << std::hex << addr
                        << "),SP=" << SP << "\n";
            break;
        }

        // 0x09: ADD HL,BC
        case 0x09:
        {
            uint32_t hl = (H << 8) | L;
            uint32_t bc = (B << 8) | C;
            uint32_t result = hl + bc;

            F &= FLAG_Z;                         // Zフラグ保持
            F &= ~FLAG_N;                         // N=0
            if (((hl & 0x0FFF) + (bc & 0x0FFF)) > 0x0FFF) F |= FLAG_H; else F &= ~FLAG_H;
            if (result > 0xFFFF) F |= FLAG_C; else F &= ~FLAG_C;

            H = (result >> 8) & 0xFF;
            L = result & 0xFF;
            cycles += 8;
            std::cout << "ADD HL,BC → HL=" << std::hex << (int)result << "\n";
            break;
        }

        // 0x0A: LD A,(BC)
        case 0x0A:
        {
            uint16_t addr = (B << 8) | C;
            A = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD A,(" << std::hex << addr
                        << ") → A=" << (int)A << "\n";
            break;
        }

        // 0x0B: DEC BC
        case 0x0B:
        {
            uint16_t bc = (B << 8) | C;
            bc--;
            B = (bc >> 8) & 0xFF;
            C = bc & 0xFF;
            cycles += 8;
            std::cout << "DEC BC → " << std::hex << bc << "\n";
            break;
        }

        // 0x0C: INC C
        case 0x0C:
            C++;
            F &= FLAG_C;
            if (C == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((C & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC C → " << std::hex << (int)C << "\n";
            break;

        // 0x0D: DEC C
        case 0x0D:
            C--;
            F &= FLAG_C;
            if (C == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((C & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC C → " << std::hex << (int)C << "\n";
            break;

        // 0x0E: LD C,d8
        case 0x0E:
        {
            uint8_t val = memory->readByte(PC++);
            C = val;
            cycles += 8;
            std::cout << "LD C, " << std::hex << (int)val << "\n";
            break;
        }

        // 0x0F: RRCA (Rotate A right)
        case 0x0F:
        {
            uint8_t carry_out = (A & 0x01) ? 1 : 0;
            A = ((A >> 1) | (carry_out << 7)) & 0xFF;
            F = 0;
            if (carry_out) F |= FLAG_C;
            cycles += 4;
            std::cout << "RRCA → A=" << std::hex << (int)A
                        << " C=" << carry_out << "\n";
            break;
        }

        // 0x10: STOP
        case 0x10:
            cycles += 4;
            std::cout << "STOP (treated as NOP)\n";
            break;

        // 0x11: LD DE,d16
        case 0x11:
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            D = hi;
            E = lo;
            cycles += 12;
            std::cout << "LD DE, " << std::hex << ((hi<<8)|lo) << "\n";
            break;
        }

        // 0x12: LD (DE),A
        case 0x12:
        {
            uint16_t addr = (D << 8) | E;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (" << std::hex << addr
                        << "),A=" << (int)A << "\n";
            break;
        }

        // 0x13: INC DE
        case 0x13:
        {
            uint16_t de = (D << 8) | E;
            de++;
            D = (de >> 8) & 0xFF;
            E = de & 0xFF;
            cycles += 8;
            std::cout << "INC DE → " << std::hex << de << "\n";
            break;
        }

        // ----- 0x14〜0x27 -----

        case 0x14:  // INC D
            D++;
            F &= FLAG_C;                       // Cフラグは保持
            if (D == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;                       // 加算なのでN=0
            if ((D & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC D → " << std::hex << (int)D << "\n";
            break;

        case 0x15:  // DEC D
            D--;
            F &= FLAG_C;
            if (D == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;                        // 減算なのでN=1
            if ((D & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC D → " << std::hex << (int)D << "\n";
            break;

        case 0x16:  // LD D,d8
        {
            uint8_t val = memory->readByte(PC++);
            D = val;
            cycles += 8;
            std::cout << "LD D, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x17:  // RLA (Rotate A left through Carry)
        {
            uint8_t oldCarry = (F & FLAG_C) ? 1 : 0;
            uint8_t newCarry = (A & 0x80) ? 1 : 0;
            A = ((A << 1) | oldCarry) & 0xFF;
            F = 0;
            if (newCarry) F |= FLAG_C;
            cycles += 4;
            std::cout << "RLA → A=" << std::hex << (int)A
                      << " C=" << newCarry << "\n";
            break;
        }

        case 0x18:  // JR r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));


            PC += offset;
            cycles += 12;
            std::cout << "JR → " << std::hex << PC << " (offset=" << (int)offset << ")\n";
            break;
        }

        case 0x19:  // ADD HL,DE
        {
            uint32_t hl = (H << 8) | L;
            uint32_t de = (D << 8) | E;
            uint32_t result = hl + de;

            F &= FLAG_Z;
            F &= ~FLAG_N;
            if (((hl & 0x0FFF) + (de & 0x0FFF)) > 0x0FFF) F |= FLAG_H; else F &= ~FLAG_H;
            if (result > 0xFFFF) F |= FLAG_C; else F &= ~FLAG_C;

            H = (result >> 8) & 0xFF;
            L = result & 0xFF;
            cycles += 8;
            std::cout << "ADD HL,DE → HL=" << std::hex << (int)result << "\n";
            break;
        }

        case 0x1A:  // LD A,(DE)
        {
            uint16_t addr = (D << 8) | E;
            A = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD A,(" << std::hex << addr << ") → A=" << (int)A << "\n";
            break;
        }

        case 0x1B:  // DEC DE
        {
            uint16_t de = (D << 8) | E;
            de--;
            D = (de >> 8) & 0xFF;
            E = de & 0xFF;
            cycles += 8;
            std::cout << "DEC DE → " << std::hex << de << "\n";
            break;
        }

        case 0x1C:  // INC E
            E++;
            F &= FLAG_C;
            if (E == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((E & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC E → " << std::hex << (int)E << "\n";
            break;

        case 0x1D:  // DEC E
            E--;
            F &= FLAG_C;
            if (E == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((E & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC E → " << std::hex << (int)E << "\n";
            break;

        case 0x1E:  // LD E,d8
        {
            uint8_t val = memory->readByte(PC++);
            E = val;
            cycles += 8;
            std::cout << "LD E, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x1F:  // RRA (Rotate A right through Carry)
        {
            uint8_t oldCarry = (F & FLAG_C) ? 0x80 : 0x00;
            uint8_t newCarry = A & 0x01;
            A = (A >> 1) | oldCarry;
            F = 0;
            if (newCarry) F |= FLAG_C;
            cycles += 4;
            std::cout << "RRA → A=" << std::hex << (int)A
                      << " C=" << (int)newCarry << "\n";
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

        case 0x21:  // LD HL,d16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            H = hi; L = lo;
            cycles += 12;
            std::cout << "LD HL, " << std::hex << ((hi<<8)|lo) << "\n";
            break;
        }

        case 0x22:  // LD (HL+),A
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, A);
            L++;
            if (L == 0) H++;
            cycles += 8;
            std::cout << "LD (HL+),A → [" << std::hex << addr
                      << "]=" << (int)A
                      << " HL→" << ((H<<8)|L) << "\n";
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

        case 0x24:  // INC H
            H++;
            F &= FLAG_C;
            if (H == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((H & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC H → " << std::hex << (int)H << "\n";
            break;

        case 0x25:  // DEC H
            H--;
            F &= FLAG_C;
            if (H == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((H & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC H → " << std::hex << (int)H << "\n";
            break;

        case 0x26:  // LD H,d8
        {
            uint8_t val = memory->readByte(PC++);
            H = val;
            cycles += 8;
            std::cout << "LD H, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x27:  // DAA (Decimal Adjust A)
        {
            uint8_t correction = 0;
            if (!(F & FLAG_N)) {
                if ((F & FLAG_H) || ((A & 0x0F) > 9)) correction += 0x06;
                if ((F & FLAG_C) || (A > 0x99)) {
                    correction += 0x60;
                    F |= FLAG_C;
                }
                A += correction;
            } else {
                if (F & FLAG_H) correction += 0x06;
                if (F & FLAG_C) correction += 0x60;
                A -= correction;
            }
            F &= ~(FLAG_Z | FLAG_H);
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "DAA → A=" << std::hex << (int)A << "\n";
            break;
        }
        // ----- 0x28〜0x32 -----

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

        case 0x29:  // ADD HL,HL
        {
            uint32_t hl = (H << 8) | L;
            uint32_t result = hl + hl;

            F &= FLAG_Z;                   // Zは保持
            F &= ~FLAG_N;                   // N=0
            if (((hl & 0x0FFF) + (hl & 0x0FFF)) > 0x0FFF) F |= FLAG_H; else F &= ~FLAG_H;
            if (result > 0xFFFF) F |= FLAG_C; else F &= ~FLAG_C;

            H = (result >> 8) & 0xFF;
            L = result & 0xFF;
            cycles += 8;
            std::cout << "ADD HL,HL → HL=" << std::hex << (int)result << "\n";
            break;
        }

        case 0x2A:  // LD A,(HL+)
        {
            uint16_t addr = (H << 8) | L;
            A = memory->readByte(addr);
            L++;
            if (L == 0) H++;              // 桁上がり時にHも加算
            cycles += 8;
            std::cout << "LD A,(HL+) → A=" << std::hex << (int)A
                      << ", HL→" << ((H<<8)|L) << "\n";
            break;
        }

        case 0x2B:  // DEC HL
        {
            uint16_t hl = (H << 8) | L;
            hl--;
            H = (hl >> 8) & 0xFF;
            L = hl & 0xFF;
            cycles += 8;
            std::cout << "DEC HL → " << std::hex << hl << "\n";
            break;
        }

        case 0x2C:  // INC L
            L++;
            F &= FLAG_C;
            if (L == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((L & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC L → " << std::hex << (int)L << "\n";
            break;

        case 0x2D:  // DEC L
            L--;
            F &= FLAG_C;
            if (L == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((L & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC L → " << std::hex << (int)L << "\n";
            break;

        case 0x2E:  // LD L,d8
        {
            uint8_t val = memory->readByte(PC++);
            L = val;
            cycles += 8;
            std::cout << "LD L, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x2F:  // CPL (Aを補数化)
            A = ~A;
            F |= (FLAG_N | FLAG_H);         // NとHをセット
            cycles += 4;
            std::cout << "CPL → A=" << std::hex << (int)A << "\n";
            break;

        case 0x30:  // JR NC,r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
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

        case 0x31:  // LD SP,d16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            SP = (hi << 8) | lo;
            cycles += 12;
            std::cout << "LD SP, " << std::hex << SP << "\n";
            break;
        }

        case 0x32:  // LD (HL-),A
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, A);
            L--;
            if (L == 0xFF) H--;            // 下位桁が借りてHを減らす
            cycles += 8;
            std::cout << "LD (HL-),A → [" << std::hex << addr
                      << "]=" << (int)A
                      << " HL→" << ((H<<8)|L) << "\n";
            break;
        }
        // ----- 0x33〜0x50 -----

        case 0x33:  // INC SP
            SP++;
            cycles += 8;
            std::cout << "INC SP → " << std::hex << SP << "\n";
            break;

        case 0x34:  // INC (HL)
        {
            uint16_t addr = (H << 8) | L;
            uint8_t val = memory->readByte(addr);
            val++;
            memory->writeByte(addr, val);

            F &= FLAG_C;
            if (val == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((val & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;

            cycles += 12;
            std::cout << "INC (HL) → [" << std::hex << addr
                      << "]=" << (int)val << "\n";
            break;
        }

        case 0x35:  // DEC (HL)
        {
            uint16_t addr = (H << 8) | L;
            uint8_t val = memory->readByte(addr);
            val--;
            memory->writeByte(addr, val);

            F &= FLAG_C;
            if (val == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((val & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;

            cycles += 12;
            std::cout << "DEC (HL) → [" << std::hex << addr
                      << "]=" << (int)val << "\n";
            break;
        }

        case 0x36:  // LD (HL),d8
        {
            uint8_t val = memory->readByte(PC++);
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, val);
            cycles += 12;
            std::cout << "LD (HL), " << std::hex << (int)val
                      << " → [" << addr << "]\n";
            break;
        }

        case 0x37:  // SCF (Set Carry Flag)
            F &= FLAG_Z;           // Zは保持
            F &= ~FLAG_N;
            F &= ~FLAG_H;
            F |= FLAG_C;
            cycles += 4;
            std::cout << "SCF → F=" << std::hex << (int)F << "\n";
            break;

        case 0x38:  // JR C,r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
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

        case 0x39:  // ADD HL,SP
        {
            uint32_t hl = (H << 8) | L;
            uint32_t result = hl + SP;

            F &= FLAG_Z;
            F &= ~FLAG_N;
            if (((hl & 0x0FFF) + (SP & 0x0FFF)) > 0x0FFF) F |= FLAG_H; else F &= ~FLAG_H;
            if (result > 0xFFFF) F |= FLAG_C; else F &= ~FLAG_C;

            H = (result >> 8) & 0xFF;
            L = result & 0xFF;
            cycles += 8;
            std::cout << "ADD HL,SP → HL=" << std::hex << (int)result << "\n";
            break;
        }

        case 0x3A:  // LD A,(HL-)
        {
            uint16_t addr = (H << 8) | L;
            A = memory->readByte(addr);
            L--;
            if (L == 0xFF) H--;
            cycles += 8;
            std::cout << "LD A,(HL-) → A=" << std::hex << (int)A
                      << ", HL→" << ((H<<8)|L) << "\n";
            break;
        }

        case 0x3B:  // DEC SP
            SP--;
            cycles += 8;
            std::cout << "DEC SP → " << std::hex << SP << "\n";
            break;

        case 0x3C:  // INC A
            A++;
            F &= FLAG_C;
            if (A == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F &= ~FLAG_N;
            if ((A & 0x0F) == 0x00) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "INC A → A=" << std::hex << (int)A << "\n";
            break;

        case 0x3D:  // DEC A
            A--;
            F &= FLAG_C;
            if (A == 0) F |= FLAG_Z; else F &= ~FLAG_Z;
            F |= FLAG_N;
            if ((A & 0x0F) == 0x0F) F |= FLAG_H; else F &= ~FLAG_H;
            cycles += 4;
            std::cout << "DEC A → A=" << std::hex << (int)A << "\n";
            break;

        case 0x3E:  // LD A,d8
        {
            uint8_t val = memory->readByte(PC++);
            A = val;
            cycles += 8;
            std::cout << "LD A, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x3F:  // CCF (Complement Carry Flag)
            if (F & FLAG_C) F &= ~FLAG_C; else F |= FLAG_C;
            F &= ~FLAG_N;
            F &= ~FLAG_H;
            cycles += 4;
            std::cout << "CCF → F=" << std::hex << (int)F << "\n";
            break;

        case 0x40:  // LD B,B
            // 自分自身なので実質何もしない
            cycles += 4;
            std::cout << "LD B,B → " << std::hex << (int)B << "\n";
            break;

        case 0x41:  // LD B,C
            B = C;
            cycles += 4;
            std::cout << "LD B,C → " << std::hex << (int)B << "\n";
            break;

        case 0x42:  // LD B,D
            B = D;
            cycles += 4;
            std::cout << "LD B,D → " << std::hex << (int)B << "\n";
            break;

        case 0x43:  // LD B,E
            B = E;
            cycles += 4;
            std::cout << "LD B,E → " << std::hex << (int)B << "\n";
            break;

        case 0x44:  // LD B,H
            B = H;
            cycles += 4;
            std::cout << "LD B,H → " << std::hex << (int)B << "\n";
            break;

        case 0x45:  // LD B,L
            B = L;
            cycles += 4;
            std::cout << "LD B,L → " << std::hex << (int)B << "\n";
            break;

        case 0x46:  // LD B,(HL)
        {
            uint16_t addr = (H << 8) | L;
            B = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD B,(HL) → " << std::hex << (int)B << "\n";
            break;
        }

        case 0x47:  // LD B,A
            B = A;
            cycles += 4;
            std::cout << "LD B,A → " << std::hex << (int)B << "\n";
            break;

        case 0x48:  // LD C,B
            C = B;
            cycles += 4;
            std::cout << "LD C,B → " << std::hex << (int)C << "\n";
            break;

        case 0x49:  // LD C,C
            // 自分自身なので何もしない
            cycles += 4;
            std::cout << "LD C,C → " << std::hex << (int)C << "\n";
            break;

        case 0x4A:  // LD C,D
            C = D;
            cycles += 4;
            std::cout << "LD C,D → " << std::hex << (int)C << "\n";
            break;

        case 0x4B:  // LD C,E
            C = E;
            cycles += 4;
            std::cout << "LD C,E → " << std::hex << (int)C << "\n";
            break;

        case 0x4C:  // LD C,H
            C = H;
            cycles += 4;
            std::cout << "LD C,H → " << std::hex << (int)C << "\n";
            break;

        case 0x4D:  // LD C,L
            C = L;
            cycles += 4;
            std::cout << "LD C,L → " << std::hex << (int)C << "\n";
            break;

        case 0x4E:  // LD C,(HL)
        {
            uint16_t addr = (H << 8) | L;
            C = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD C,(HL) → " << std::hex << (int)C << "\n";
            break;
        }

        case 0x4F:  // LD C,A
            C = A;
            cycles += 4;
            std::cout << "LD C,A → " << std::hex << (int)C << "\n";
            break;

        case 0x50:  // LD D,B
            D = B;
            cycles += 4;
            std::cout << "LD D,B → " << std::hex << (int)D << "\n";
            break;
        // ----- 0x51〜0x7F -----

        case 0x51:  // LD D,C
            D = C;
            cycles += 4;
            std::cout << "LD D,C → " << std::hex << (int)D << "\n";
            break;

        case 0x52:  // LD D,D
            // 自分自身なので変化なし
            cycles += 4;
            std::cout << "LD D,D → " << std::hex << (int)D << "\n";
            break;

        case 0x53:  // LD D,E
            D = E;
            cycles += 4;
            std::cout << "LD D,E → " << std::hex << (int)D << "\n";
            break;

        case 0x54:  // LD D,H
            D = H;
            cycles += 4;
            std::cout << "LD D,H → " << std::hex << (int)D << "\n";
            break;

        case 0x55:  // LD D,L
            D = L;
            cycles += 4;
            std::cout << "LD D,L → " << std::hex << (int)D << "\n";
            break;

        case 0x56:  // LD D,(HL)
        {
            uint16_t addr = (H << 8) | L;
            D = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD D,(HL) → " << std::hex << (int)D << "\n";
            break;
        }

        case 0x57:  // LD D,A
            D = A;
            cycles += 4;
            std::cout << "LD D,A → " << std::hex << (int)D << "\n";
            break;

        case 0x58:  // LD E,B
            E = B;
            cycles += 4;
            std::cout << "LD E,B → " << std::hex << (int)E << "\n";
            break;

        case 0x59:  // LD E,C
            E = C;
            cycles += 4;
            std::cout << "LD E,C → " << std::hex << (int)E << "\n";
            break;

        case 0x5A:  // LD E,D
            E = D;
            cycles += 4;
            std::cout << "LD E,D → " << std::hex << (int)E << "\n";
            break;

        case 0x5B:  // LD E,E
            cycles += 4;
            std::cout << "LD E,E → " << std::hex << (int)E << "\n";
            break;

        case 0x5C:  // LD E,H
            E = H;
            cycles += 4;
            std::cout << "LD E,H → " << std::hex << (int)E << "\n";
            break;

        case 0x5D:  // LD E,L
            E = L;
            cycles += 4;
            std::cout << "LD E,L → " << std::hex << (int)E << "\n";
            break;

        case 0x5E:  // LD E,(HL)
        {
            uint16_t addr = (H << 8) | L;
            E = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD E,(HL) → " << std::hex << (int)E << "\n";
            break;
        }

        case 0x5F:  // LD E,A
            E = A;
            cycles += 4;
            std::cout << "LD E,A → " << std::hex << (int)E << "\n";
            break;

        case 0x60:  // LD H,B
            H = B;
            cycles += 4;
            std::cout << "LD H,B → " << std::hex << (int)H << "\n";
            break;

        case 0x61:  // LD H,C
            H = C;
            cycles += 4;
            std::cout << "LD H,C → " << std::hex << (int)H << "\n";
            break;

        case 0x62:  // LD H,D
            H = D;
            cycles += 4;
            std::cout << "LD H,D → " << std::hex << (int)H << "\n";
            break;

        case 0x63:  // LD H,E
            H = E;
            cycles += 4;
            std::cout << "LD H,E → " << std::hex << (int)H << "\n";
            break;

        case 0x64:  // LD H,H
            cycles += 4;
            std::cout << "LD H,H → " << std::hex << (int)H << "\n";
            break;

        case 0x65:  // LD H,L
            H = L;
            cycles += 4;
            std::cout << "LD H,L → " << std::hex << (int)H << "\n";
            break;

        case 0x66:  // LD H,(HL)
        {
            uint16_t addr = (H << 8) | L;
            H = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD H,(HL) → " << std::hex << (int)H << "\n";
            break;
        }

        case 0x67:  // LD H,A
            H = A;
            cycles += 4;
            std::cout << "LD H,A → " << std::hex << (int)H << "\n";
            break;

        case 0x68:  // LD L,B
            L = B;
            cycles += 4;
            std::cout << "LD L,B → " << std::hex << (int)L << "\n";
            break;

        case 0x69:  // LD L,C
            L = C;
            cycles += 4;
            std::cout << "LD L,C → " << std::hex << (int)L << "\n";
            break;

        case 0x6A:  // LD L,D
            L = D;
            cycles += 4;
            std::cout << "LD L,D → " << std::hex << (int)L << "\n";
            break;

        case 0x6B:  // LD L,E
            L = E;
            cycles += 4;
            std::cout << "LD L,E → " << std::hex << (int)L << "\n";
            break;

        case 0x6C:  // LD L,H
            L = H;
            cycles += 4;
            std::cout << "LD L,H → " << std::hex << (int)L << "\n";
            break;

        case 0x6D:  // LD L,L
            cycles += 4;
            std::cout << "LD L,L → " << std::hex << (int)L << "\n";
            break;

        case 0x6E:  // LD L,(HL)
        {
            uint16_t addr = (H << 8) | L;
            L = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD L,(HL) → " << std::hex << (int)L << "\n";
            break;
        }

        case 0x6F:  // LD L,A
            L = A;
            cycles += 4;
            std::cout << "LD L,A → " << std::hex << (int)L << "\n";
            break;

        // ---- Aレジスタとのロード ----
        case 0x70:  // LD (HL),B
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, B);
            cycles += 8;
            std::cout << "LD (HL),B → [" << std::hex << addr << "]=" << (int)B << "\n";
            break;
        }

        case 0x71:  // LD (HL),C
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, C);
            cycles += 8;
            std::cout << "LD (HL),C → [" << std::hex << addr << "]=" << (int)C << "\n";
            break;
        }

        case 0x72:  // LD (HL),D
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, D);
            cycles += 8;
            std::cout << "LD (HL),D → [" << std::hex << addr << "]=" << (int)D << "\n";
            break;
        }

        case 0x73:  // LD (HL),E
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, E);
            cycles += 8;
            std::cout << "LD (HL),E → [" << std::hex << addr << "]=" << (int)E << "\n";
            break;
        }

        case 0x74:  // LD (HL),H
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, H);
            cycles += 8;
            std::cout << "LD (HL),H → [" << std::hex << addr << "]=" << (int)H << "\n";
            break;
        }

        case 0x75:  // LD (HL),L
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, L);
            cycles += 8;
            std::cout << "LD (HL),L → [" << std::hex << addr << "]=" << (int)L << "\n";
            break;
        }

        case 0x76:  // HALT
            halted = true;
            cycles += 4;
            std::cout << "HALT\n";
            break;

        case 0x77:  // LD (HL),A
        {
            uint16_t addr = (H << 8) | L;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (HL),A → [" << std::hex << addr << "]=" << (int)A << "\n";
            break;
        }

        // ---- Aレジスタにロード ----
        case 0x78:  // LD A,B
            A = B;
            cycles += 4;
            std::cout << "LD A,B → " << std::hex << (int)A << "\n";
            break;

        case 0x79:  // LD A,C
            A = C;
            cycles += 4;
            std::cout << "LD A,C → " << std::hex << (int)A << "\n";
            break;

        case 0x7A:  // LD A,D
            A = D;
            cycles += 4;
            std::cout << "LD A,D → " << std::hex << (int)A << "\n";
            break;

        case 0x7B:  // LD A,E
            A = E;
            cycles += 4;
            std::cout << "LD A,E → " << std::hex << (int)A << "\n";
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

        case 0x7E:  // LD A,(HL)
        {
            uint16_t addr = (H << 8) | L;
            A = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD A,(HL) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x7F:  // LD A,A
            cycles += 4;
            std::cout << "LD A,A → " << std::hex << (int)A << "\n";
            break;
        // ----- 0x80〜0x8F : ADD A,r -----
        case 0x80:  // ADD A,B
        {
            uint16_t result = A + B;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (B & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,B → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x81:  // ADD A,C
        {
            uint16_t result = A + C;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (C & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,C → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x82:  // ADD A,D
        {
            uint16_t result = A + D;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (D & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,D → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x83:  // ADD A,E
        {
            uint16_t result = A + E;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (E & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,E → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x84:  // ADD A,H
        {
            uint16_t result = A + H;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (H & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,H → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x85:  // ADD A,L
        {
            uint16_t result = A + L;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (L & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,L → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x86:  // ADD A,(HL)
        {
            uint8_t val = memory->readByte((H<<8)|L);
            uint16_t result = A + val;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (val & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 8;
            std::cout << "ADD A,(HL) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x87:  // ADD A,A
        {
            uint16_t result = A + A;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F)*2) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "ADD A,A → A=" << std::hex << (int)A << "\n";
            break;
        }
        /* =========================
         * 0x88–0x8F : ADC A,r / (HL)
         * ========================= */
         case 0x88: { // ADC A,B
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + B + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (B & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,B → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x89: { // ADC A,C
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + C + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (C & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,C → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8A: { // ADC A,D
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + D + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (D & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,D → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8B: { // ADC A,E
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + E + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (E & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,E → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8C: { // ADC A,H
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + H + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (H & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,H → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8D: { // ADC A,L
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + L + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (L & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,L → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8E: { // ADC A,(HL)
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint8_t v = memory->readByte((H<<8)|L);
            uint16_t r = A + v + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (v & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 8;
            std::cout << "ADC A,(HL) → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x8F: { // ADC A,A
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A + A + c;
            F = 0;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (A & 0x0F) + c) > 0x0F) F |= FLAG_H;
            if (r > 0xFF) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "ADC A,A → A=" << std::hex << (int)A << "\n";
            break;
        }

        // ----- 0x90〜0x9F : SUB A,r -----
        case 0x90:  // SUB B
        {
            uint16_t result = A - B;
            F = FLAG_N;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (B & 0x0F)) F |= FLAG_H;
            if (A < B) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "SUB B → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0x91:  // SUB C
        {
            uint16_t result = A - C;
            F = FLAG_N;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (C & 0x0F)) F |= FLAG_H;
            if (A < C) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 4;
            std::cout << "SUB C → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x92: { // SUB D
            uint16_t r = A - D;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (D & 0x0F)) F |= FLAG_H;
            if (A < D) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SUB D → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x93: { // SUB E
            uint16_t r = A - E;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (E & 0x0F)) F |= FLAG_H;
            if (A < E) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SUB E → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x94: { // SUB H
            uint16_t r = A - H;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (H & 0x0F)) F |= FLAG_H;
            if (A < H) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SUB H → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x95: { // SUB L
            uint16_t r = A - L;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (L & 0x0F)) F |= FLAG_H;
            if (A < L) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SUB L → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x96: { // SUB (HL)
            uint8_t v = memory->readByte((H<<8)|L);
            uint16_t r = A - v;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (v & 0x0F)) F |= FLAG_H;
            if (A < v) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 8;
            std::cout << "SUB (HL) → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x97: { // SUB A
            uint16_t r = A - A;
            F = FLAG_N | FLAG_Z; // Z=1, H=0, C=0
            A = 0;
            cycles += 4;
            std::cout << "SUB A → A=0\n";
            break;
        }

        /* =========================
         * 0x98–0x9F : SBC A,r/(HL)/A
         * ========================= */
        case 0x98: { // SBC A,B
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - B - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((B & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)B + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,B → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x99: { // SBC A,C
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - C - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((C & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)C + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,C → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9A: { // SBC A,D
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - D - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((D & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)D + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,D → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9B: { // SBC A,E
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - E - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((E & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)E + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,E → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9C: { // SBC A,H
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - H - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((H & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)H + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,H → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9D: { // SBC A,L
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - L - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((L & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)L + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 4;
            std::cout << "SBC A,L → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9E: { // SBC A,(HL)
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint8_t v = memory->readByte((H<<8)|L);
            uint16_t r = A - v - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((v & 0x0F) + c)) F |= FLAG_H;
            if (A < (uint16_t)v + c) F |= FLAG_C;
            A = (uint8_t)r;
            cycles += 8;
            std::cout << "SBC A,(HL) → A=" << std::hex << (int)A << "\n";
            break;
        }
        case 0x9F: { // SBC A,A
            uint8_t c = (F & FLAG_C) ? 1 : 0;
            uint16_t r = A - A - c;
            F = FLAG_N;
            if ((r & 0xFF) == 0) F |= FLAG_Z;
            if (0 < c) F |= FLAG_H; // A&0xF < (A&0xF)+c  ⇔ c==1
            if (A < (uint16_t)A + c) F |= FLAG_C; // ⇔ c==1
            A = (uint8_t)r; // 0 or 0xFF
            cycles += 4;
            std::cout << "SBC A,A → A=" << std::hex << (int)A << "\n";
            break;
        }


        // ・・・同様にSUB D,E,H,L,(HL),A を追加
        // (省略部分も同じロジックで SUB A,xxx を実装)

        // ----- 0xA0〜0xAF : AND/XOR -----
        case 0xA0:  // AND B
            A &= B;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND B → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA1:  // AND C
            A &= C;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND C → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA2:  // AND D
            A &= D;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND D → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA3:  // AND E
            A &= E;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND E → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA4:  // AND H
            A &= H;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND H → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA5:  // AND L
            A &= L;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND L → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA6:  // AND (HL)
        {
            uint8_t val = memory->readByte((H << 8) | L);
            A &= val;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 8;
            std::cout << "AND (HL) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0xA7:  // AND A
            F = 0;
            if (A == 0) F |= FLAG_Z;
            F |= FLAG_H;
            cycles += 4;
            std::cout << "AND A → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA8:  // XOR B
            A ^= B;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR B → A=" << std::hex << (int)A << "\n";
            break;

        case 0xA9:  // XOR C
            A ^= C;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR C → A=" << std::hex << (int)A << "\n";
            break;

        case 0xAA:  // XOR D
            A ^= D;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR D → A=" << std::hex << (int)A << "\n";
            break;

        case 0xAB:  // XOR E
            A ^= E;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR E → A=" << std::hex << (int)A << "\n";
            break;

        case 0xAC:  // XOR H
            A ^= H;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR H → A=" << std::hex << (int)A << "\n";
            break;

        case 0xAD:  // XOR L
            A ^= L;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "XOR L → A=" << std::hex << (int)A << "\n";
            break;

        case 0xAE:  // XOR (HL)
        {
            uint8_t val = memory->readByte((H << 8) | L);
            A ^= val;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 8;
            std::cout << "XOR (HL) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0xAF:  // XOR A
            A = 0;
            F = FLAG_Z;
            cycles += 4;
            std::cout << "XOR A → A=0\n";
            break;

        // ----- 0xB0〜0xBF : OR/CP -----
        case 0xB0:  // OR B
            A |= B;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR B → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB1:  // OR C
            A |= C;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR C → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB2:  // OR D
            A |= D;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR D → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB3:  // OR E
            A |= E;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR E → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB4:  // OR H
            A |= H;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR H → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB5:  // OR L
            A |= L;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR L → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB6:  // OR (HL)
        {
            uint8_t val = memory->readByte((H << 8) | L);
            A |= val;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 8;
            std::cout << "OR (HL) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0xB7:  // OR A
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 4;
            std::cout << "OR A → A=" << std::hex << (int)A << "\n";
            break;

        case 0xB8:  // CP B
        {
            uint8_t res = A - B;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (B & 0x0F)) F |= FLAG_H;
            if (A < B) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP B (A=" << (int)A << ",B=" << (int)B << ")\n";
            break;
        }

        case 0xB9:  // CP C
        {
            uint8_t res = A - C;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (C & 0x0F)) F |= FLAG_H;
            if (A < C) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP C (A=" << (int)A << ",C=" << (int)C << ")\n";
            break;
        }

        case 0xBA:  // CP D
        {
            uint8_t res = A - D;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (D & 0x0F)) F |= FLAG_H;
            if (A < D) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP D (A=" << (int)A << ",D=" << (int)D << ")\n";
            break;
        }

        case 0xBB:  // CP E
        {
            uint8_t res = A - E;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (E & 0x0F)) F |= FLAG_H;
            if (A < E) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP E (A=" << (int)A << ",E=" << (int)E << ")\n";
            break;
        }

        case 0xBC:  // CP H
        {
            uint8_t res = A - H;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (H & 0x0F)) F |= FLAG_H;
            if (A < H) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP H (A=" << (int)A << ",H=" << (int)H << ")\n";
            break;
        }

        case 0xBD:  // CP L
        {
            uint8_t res = A - L;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (L & 0x0F)) F |= FLAG_H;
            if (A < L) F |= FLAG_C;
            cycles += 4;
            std::cout << "CP L (A=" << (int)A << ",L=" << (int)L << ")\n";
            break;
        }

        case 0xBE:  // CP (HL)
        {
            uint8_t val = memory->readByte((H << 8) | L);
            uint8_t res = A - val;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (val & 0x0F)) F |= FLAG_H;
            if (A < val) F |= FLAG_C;
            cycles += 8;
            std::cout << "CP (HL) (A=" << (int)A << ",val=" << (int)val << ")\n";
            break;
        }

        case 0xBF:  // CP A
            F = FLAG_N | FLAG_Z;
            cycles += 4;
            std::cout << "CP A (compare with itself)\n";
            break;

        // ----- 0xC0〜0xCF : RET/CALL/JP -----
        case 0xC0:  // RET NZ
            if (!(F & FLAG_Z)) {
                {
                    uint8_t lo = memory->readByte(SP++);
                    uint8_t hi = memory->readByte(SP++);
                    PC = (hi << 8) | lo;
                }
                cycles += 20;
                std::cout << "RET NZ → PC=" << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "RET NZ skipped (Z=1)\n";
            }
            break;

        case 0xC1:  // POP BC
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            C = lo;
            B = hi;
            cycles += 12;
            std::cout << "POP BC → B=" << std::hex << (int)B
                      << " C=" << (int)C << "\n";
            break;
        }

        case 0xC2:  // JP NZ,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (!(F & FLAG_Z)) {
                PC = addr;
                cycles += 16;
                std::cout << "JP NZ → " << std::hex << PC << "\n";
            } else {
                cycles += 12;
                std::cout << "JP NZ skipped (Z=1)\n";
            }
            break;
        }

        case 0xC3:  // JP a16
        {
            uint16_t oldPC = PC - 1;  // オペコードの位置
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;

            // Gearboy形式で出力
            std::cout << std::hex << std::setfill('0') << std::setw(2)
                      << ((oldPC >> 8) & 0xFF) << ":"
                      << std::setw(4) << oldPC << " C3 "
                      << std::setw(2) << (int)lo << " "
                      << std::setw(2) << (int)hi
                      << " -> JP $" << std::setw(4) << addr
                      << std::dec << std::endl;

            PC = addr;
            cycles += 16;
            break;
        }

        case 0xC4:  // CALL NZ,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (!(F & FLAG_Z)) {
                uint8_t pcLo = PC & 0xFF;
                uint8_t pcHi = (PC >> 8) & 0xFF;
                SP--; memory->writeByte(SP, pcHi);
                SP--; memory->writeByte(SP, pcLo);
                PC = addr;
                cycles += 24;
                std::cout << "CALL NZ → " << std::hex << addr << "\n";
            } else {
                cycles += 12;
                std::cout << "CALL NZ skipped (Z=1)\n";
            }
            break;
        }

        case 0xC5:  // PUSH BC
        {
            uint8_t pcHi = B;
            uint8_t pcLo = C;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            cycles += 16;
            std::cout << "PUSH BC (B=" << std::hex << (int)B
                      << ", C=" << (int)C << ")\n";
            break;
        }

        case 0xC6:  // ADD A,d8
        {
            uint8_t val = memory->readByte(PC++);
            uint16_t result = A + val;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (val & 0x0F)) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 8;
            std::cout << "ADD A," << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xC7:  // RST 00h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x00;
            cycles += 16;
            std::cout << "RST 00h\n";
            break;
        }

        case 0xC9:  // RET
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            PC = (hi << 8) | lo;
            cycles += 16;
            std::cout << "RET → PC=" << std::hex << PC << "\n";
            break;
        }

        case 0xC8:  // RET Z
            if (F & FLAG_Z) {
                uint8_t lo = memory->readByte(SP++);
                uint8_t hi = memory->readByte(SP++);
                PC = (hi << 8) | lo;
                cycles += 20;
                std::cout << "RET Z → PC=" << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "RET Z skipped (Z=0)\n";
            }
            break;

        case 0xCA:  // JP Z,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (F & FLAG_Z) {
                PC = addr;
                cycles += 16;
                std::cout << "JP Z → " << std::hex << PC << "\n";
            } else {
                cycles += 12;
                std::cout << "JP Z skipped (Z=0)\n";
            }
            break;
        }

        case 0xCB:
            executeCB();   // 専用処理に委譲
            break;

        case 0xCD:  // CALL a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi<<8)|lo;
            uint8_t pclo = PC & 0xFF, pchi = (PC>>8)&0xFF;
            SP--; memory->writeByte(SP, pchi);
            SP--; memory->writeByte(SP, pclo);
            PC = addr;
            cycles += 24;
            std::cout << "CALL " << std::hex << addr << "\n";
            break;
        }

        case 0xCC:  // CALL Z,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (F & FLAG_Z) {
                uint8_t pcLo = PC & 0xFF;
                uint8_t pcHi = (PC >> 8) & 0xFF;
                SP--; memory->writeByte(SP, pcHi);
                SP--; memory->writeByte(SP, pcLo);
                PC = addr;
                cycles += 24;
                std::cout << "CALL Z → " << std::hex << addr << "\n";
            } else {
                cycles += 12;
                std::cout << "CALL Z skipped (Z=0)\n";
            }
            break;
        }

        case 0xCE:  // ADC A,d8
        {
            uint8_t val = memory->readByte(PC++);
            uint8_t carry = (F & FLAG_C) ? 1 : 0;
            uint16_t result = A + val + carry;
            F = 0;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if (((A & 0x0F) + (val & 0x0F) + carry) > 0x0F) F |= FLAG_H;
            if (result > 0xFF) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 8;
            std::cout << "ADC A," << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xCF:  // RST 08h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x08;
            cycles += 16;
            std::cout << "RST 08h\n";
            break;
        }

        // ----- 0xD0〜0xDF -----
        case 0xD0:  // RET NC
            if (!(F & FLAG_C)) {
                uint8_t lo = memory->readByte(SP++);
                uint8_t hi = memory->readByte(SP++);
                PC = (hi << 8) | lo;
                cycles += 20;
                std::cout << "RET NC → PC=" << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "RET NC skipped (C=1)\n";
            }
            break;

        case 0xD1:  // POP DE
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            E = lo;
            D = hi;
            cycles += 12;
            std::cout << "POP DE → D=" << std::hex << (int)D
                      << " E=" << (int)E << "\n";
            break;
        }

        case 0xD2:  // JP NC,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (!(F & FLAG_C)) {
                PC = addr;
                cycles += 16;
                std::cout << "JP NC → " << std::hex << PC << "\n";
            } else {
                cycles += 12;
                std::cout << "JP NC skipped (C=1)\n";
            }
            break;
        }

        case 0xD4:  // CALL NC,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (!(F & FLAG_C)) {
                uint8_t pcLo = PC & 0xFF;
                uint8_t pcHi = (PC >> 8) & 0xFF;
                SP--; memory->writeByte(SP, pcHi);
                SP--; memory->writeByte(SP, pcLo);
                PC = addr;
                cycles += 24;
                std::cout << "CALL NC → " << std::hex << addr << "\n";
            } else {
                cycles += 12;
                std::cout << "CALL NC skipped (C=1)\n";
            }
            break;
        }

        case 0xD5:  // PUSH DE
        {
            uint8_t hi = D;
            uint8_t lo = E;
            SP--; memory->writeByte(SP, hi);
            SP--; memory->writeByte(SP, lo);
            cycles += 16;
            std::cout << "PUSH DE (D=" << std::hex << (int)D
                      << ", E=" << (int)E << ")\n";
            break;
        }

        case 0xD6:  // SUB d8
        {
            uint8_t val = memory->readByte(PC++);
            uint16_t result = A - val;
            F = FLAG_N;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (val & 0x0F)) F |= FLAG_H;
            if (A < val) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 8;
            std::cout << "SUB " << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xD7:  // RST 10h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x10;
            cycles += 16;
            std::cout << "RST 10h\n";
            break;
        }

        case 0xD8:  // RET C
            if (F & FLAG_C) {
                uint8_t lo = memory->readByte(SP++);
                uint8_t hi = memory->readByte(SP++);
                PC = (hi << 8) | lo;
                cycles += 20;
                std::cout << "RET C → PC=" << std::hex << PC << "\n";
            } else {
                cycles += 8;
                std::cout << "RET C skipped (C=0)\n";
            }
            break;

        case 0xD9:  // RETI
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            PC = (hi << 8) | lo;
            ime = true;
            cycles += 16;
            std::cout << "RETI → PC=" << std::hex << PC << " (IME=1)\n";
            break;
        }

        case 0xDA:  // JP C,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (F & FLAG_C) {
                PC = addr;
                cycles += 16;
                std::cout << "JP C → " << std::hex << PC << "\n";
            } else {
                cycles += 12;
                std::cout << "JP C skipped (C=0)\n";
            }
            break;
        }

        case 0xDC:  // CALL C,a16
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            if (F & FLAG_C) {
                uint8_t pcLo = PC & 0xFF;
                uint8_t pcHi = (PC >> 8) & 0xFF;
                SP--; memory->writeByte(SP, pcHi);
                SP--; memory->writeByte(SP, pcLo);
                PC = addr;
                cycles += 24;
                std::cout << "CALL C → " << std::hex << addr << "\n";
            } else {
                cycles += 12;
                std::cout << "CALL C skipped (C=0)\n";
            }
            break;
        }

        case 0xDE:  // SBC A,d8
        {
            uint8_t val = memory->readByte(PC++);
            uint8_t carry = (F & FLAG_C) ? 1 : 0;
            uint16_t result = A - val - carry;
            F = FLAG_N;
            if ((result & 0xFF) == 0) F |= FLAG_Z;
            if ((A & 0x0F) < ((val & 0x0F) + carry)) F |= FLAG_H;
            if (A < (uint16_t)val + carry) F |= FLAG_C;
            A = result & 0xFF;
            cycles += 8;
            std::cout << "SBC A," << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xDF:  // RST 18h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x18;
            cycles += 16;
            std::cout << "RST 18h\n";
            break;
        }

        // ----- 0xE0〜0xFF -----
        case 0xE0:  // LDH (a8),A
        {
            uint8_t offset = memory->readByte(PC++);
            uint16_t addr = 0xFF00 + offset;
            memory->writeByte(addr, A);
            cycles += 12;
            std::cout << "LDH (" << std::hex << addr << "),A=" << (int)A << "\n";
            break;
        }

        case 0xE1:  // POP HL
        {
            uint8_t lo = memory->readByte(SP++);
            uint8_t hi = memory->readByte(SP++);
            L = lo;
            H = hi;
            cycles += 12;
            std::cout << "POP HL → H=" << std::hex << (int)H
                      << " L=" << (int)L << "\n";
            break;
        }

        case 0xE2:  // LD (C),A
        {
            uint16_t addr = 0xFF00 + C;
            memory->writeByte(addr, A);
            cycles += 8;
            std::cout << "LD (C),A → [" << std::hex << addr << "]=" << (int)A << "\n";
            break;
        }

        case 0xE5:  // PUSH HL
        {
            uint8_t hi = H;
            uint8_t lo = L;
            SP--; memory->writeByte(SP, hi);
            SP--; memory->writeByte(SP, lo);
            cycles += 16;
            std::cout << "PUSH HL (H=" << std::hex << (int)H
                      << ", L=" << (int)L << ")\n";
            break;
        }

        case 0xE6:  // AND d8
        {
            uint8_t val = memory->readByte(PC++);
            A &= val;
            F = FLAG_H;
            if (A == 0) F |= FLAG_Z;
            cycles += 8;
            std::cout << "AND " << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xE7:  // RST 20h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x20;
            cycles += 16;
            std::cout << "RST 20h\n";
            break;
        }

        case 0xE8:  // ADD SP,r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            uint16_t result = SP + offset;

            // フラグ計算は下位8bitで無符号演算として行う
            uint8_t lowSP = SP & 0xFF;
            uint8_t lowOffset = offset & 0xFF; // 符号拡張せずに8bit値として扱う

            F = 0;
            if (((lowSP & 0x0F) + (lowOffset & 0x0F)) > 0x0F) F |= FLAG_H;
            if ((lowSP + lowOffset) > 0xFF) F |= FLAG_C;

            SP = result;
            cycles += 16;
            std::cout << "ADD SP," << std::dec << (int)offset
                      << " → SP=" << std::hex << SP << "\n";
            break;
        }

        case 0xE9:  // JP (HL)
            PC = (H << 8) | L;
            cycles += 4;
            std::cout << "JP (HL) → PC=" << std::hex << PC << "\n";
            break;

        case 0xEE:  // XOR d8
        {
            uint8_t val = memory->readByte(PC++);
            A ^= val;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 8;
            std::cout << "XOR " << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xEF:  // RST 28h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x28;
            cycles += 16;
            std::cout << "RST 28h\n";
            break;
        }

        case 0xEA:  // LD (a16),A
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi<<8)|lo;
            memory->writeByte(addr, A);
            cycles += 16;
            std::cout << "LD (" << std::hex << addr << "),A=" << (int)A << "\n";
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

        case 0xF1:  // POP AF
        {
            F = memory->readByte(SP++);
            A = memory->readByte(SP++);
            F &= 0xF0;
            cycles += 12;
            std::cout << "POP AF → A=" << (int)A << " F=" << (int)F << "\n";
            break;
        }

        case 0xF2:  // LD A,(C)
        {
            uint16_t addr = 0xFF00 + C;
            A = memory->readByte(addr);
            cycles += 8;
            std::cout << "LD A,(C) → A=" << std::hex << (int)A << "\n";
            break;
        }

        case 0xF3:  // DI
            ime = false;
            ime_enable_delay = 0;  // 保留中のEIもキャンセル
            cycles += 4;
            std::cout << "DI (Disable Interrupts)\n";
            break;

        case 0xF5:  // PUSH AF
        {
            SP--;
            memory->writeByte(SP, A);                  // Aはそのまま
            SP--;
            memory->writeByte(SP, F & 0xF0);           // 下位4bitを必ず0にマスク
            cycles += 16;
            std::cout << "PUSH AF → A=" << (int)A
                        << " F=" << (int)(F & 0xF0) << "\n";
            break;
        }


        case 0xF6:  // OR d8
        {
            uint8_t val = memory->readByte(PC++);
            A |= val;
            F = 0;
            if (A == 0) F |= FLAG_Z;
            cycles += 8;
            std::cout << "OR " << std::hex << (int)val
                      << " → A=" << (int)A << "\n";
            break;
        }

        case 0xF7:  // RST 30h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x30;
            cycles += 16;
            std::cout << "RST 30h\n";
            break;
        }

        case 0xF8:  // LD HL,SP+r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            uint16_t result = SP + offset;

            // フラグ計算は下位8bitで無符号演算として行う
            uint8_t lowSP = SP & 0xFF;
            uint8_t lowOffset = offset & 0xFF; // 符号拡張せずに8bit値として扱う

            F = 0;
            if (((lowSP & 0x0F) + (lowOffset & 0x0F)) > 0x0F) F |= FLAG_H;
            if ((lowSP + lowOffset) > 0xFF) F |= FLAG_C;

            H = (result >> 8) & 0xFF;
            L = result & 0xFF;
            cycles += 12;
            std::cout << "LD HL,SP+" << std::dec << (int)offset
                      << " → HL=" << std::hex << result << "\n";
            break;
        }

        case 0xF9:  // LD SP,HL
            SP = (H << 8) | L;
            cycles += 8;
            std::cout << "LD SP,HL → SP=" << std::hex << SP << "\n";
            break;

        case 0xFA:  // LD A,(a16)
        {
            uint8_t lo = memory->readByte(PC++);
            uint8_t hi = memory->readByte(PC++);
            uint16_t addr = (hi << 8) | lo;
            A = memory->readByte(addr);
            cycles += 16;
            std::cout << "LD A,(" << std::hex << addr << ") → A=" << (int)A << "\n";
            break;
        }

        case 0xFB:  // EI
            ime_enable_delay = 2;  // 次の命令完了後にIMEを有効化
            cycles += 4;
            std::cout << "EI (Enable Interrupts - delayed)\n";
            break;

        case 0xFF:  // RST 38h
        {
            uint8_t pcLo = PC & 0xFF;
            uint8_t pcHi = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHi);
            SP--; memory->writeByte(SP, pcLo);
            PC = 0x38;
            cycles += 16;
            std::cout << "RST 38h\n";
            break;
        }

        case 0xFE:  // CP d8
        {
            uint8_t val = memory->readByte(PC++);
            uint8_t res = A - val;
            F = FLAG_N;
            if (res == 0) F |= FLAG_Z;
            if ((A & 0x0F) < (val & 0x0F)) F |= FLAG_H;
            if (A < val) F |= FLAG_C;
            cycles += 8;
            std::cout << "CP " << (int)val << " (A=" << (int)A << ")\n";
            break;
        }


        default:
            std::cout << "Unknown opcode: 0x"
                      << std::hex << (int)opcode
                      << " at PC=" << PC-1 << "\n";
            break;
    }

    // EI命令の遅延処理（次の命令完了後にIMEを有効にする）
    if (ime_enable_delay > 0) {
        ime_enable_delay--;
        if (ime_enable_delay == 0) {
            ime = true;
            std::cout << "EI delay complete - IME enabled\n";
        }
    }

    // デバッグ：異常なサイクル値を検出
    if (cycles == 0 || cycles > 100) {
        std::cout << "[CPU] Abnormal cycles: " << cycles << " at PC=" << std::hex << (PC-1) << std::endl;
    }

    return cycles;

}

void CPU::handleInterrupts() {
    // IE と IF の両方を確認
    uint8_t req = memory->if_reg & memory->ie;
    if (req == 0)
    return;   // 割り込みなし

    // HALTからの復帰（IMEに関係なく）
    if (halted) {
        halted = false;
        if (!ime) return;  // IME無効なら割り込み処理はしない
    }

    // 割り込みが無効なら何もしない
    if (!ime)
    return;

    // 割り込みを受け付けたら IME をクリア
    ime = false;
    std::cout << "================interruput start=================" << std::endl;

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
    cycles += 20;
}

void CPU::executeCB()
{
    uint8_t cbcode = memory->readByte(PC++);          // 2nd opcode
    uint8_t code   = cbcode & 0x07;                   // 下位3bit → レジスタ選択
    bool isHL      = (code == 6);

    // 値を取得
    uint8_t val = isHL ? memory->readByte((H << 8) | L)
                       : getRegisterRef(code);

    // 書き戻しヘルパー
    auto storeVal = [&](uint8_t v) {
        if (isHL) memory->writeByte((H << 8) | L, v);
        else      getRegisterRef(code) = v;
    };

    // サイクル
    int cyc = isHL ? 16 : 8;

    switch (cbcode & 0xF8)
    {
        // -------- Rotate / Shift --------
        case 0x00: { // RLC
            uint8_t c = (val & 0x80) ? 1 : 0;
            val = (val << 1) | c;
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x08: { // RRC
            uint8_t c = (val & 0x01) ? 1 : 0;
            val = (val >> 1) | (c << 7);
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x10: { // RL (through carry)
            uint8_t c = (val & 0x80) ? 1 : 0;
            val = (val << 1) | ((F & FLAG_C) ? 1 : 0);
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x18: { // RR (through carry)
            uint8_t c = (val & 0x01) ? 1 : 0;
            val = (val >> 1) | ((F & FLAG_C) ? 0x80 : 0);
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x20: { // SLA
            uint8_t c = (val & 0x80) ? 1 : 0;
            val <<= 1;
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x28: { // SRA (保持符号ビット)
            uint8_t c = (val & 0x01) ? 1 : 0;
            uint8_t msb = val & 0x80;
            val = (val >> 1) | msb;
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        case 0x30: { // SWAP
            val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
            F = 0;
            if (val == 0) F |= FLAG_Z;
            storeVal(val);
            break;
        }

        case 0x38: { // SRL
            uint8_t c = (val & 0x01) ? 1 : 0;
            val >>= 1;
            F = 0;
            if (val == 0) F |= FLAG_Z;
            if (c)       F |= FLAG_C;
            storeVal(val);
            break;
        }

        // -------- BIT / RES / SET --------
        default: {
            // BIT b,r
            if (cbcode >= 0x40 && cbcode < 0x80) {
                int bit = (cbcode >> 3) & 0x07;
                F &= FLAG_C;          // Cは保持
                F |= FLAG_H;          // H=1
                if (((val >> bit) & 1) == 0) F |= FLAG_Z;
                cyc = isHL ? 12 : 8;   // BITのHLは12サイクル
            }
            // RES b,r
            else if (cbcode >= 0x80 && cbcode < 0xC0) {
                int bit = (cbcode >> 3) & 0x07;
                val &= ~(1 << bit);
                storeVal(val);
            }
            // SET b,r
            else if (cbcode >= 0xC0) {
                int bit = (cbcode >> 3) & 0x07;
                val |= (1 << bit);
                storeVal(val);
            }
            else {
                std::cout << "CB opcode not implemented: 0x"
                          << std::hex << (int)cbcode << "\n";
            }
            break;
        }
    }

    cycles += cyc;
}


uint8_t& CPU::getRegisterRef(uint8_t code) {
    switch(code) {
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 7: return A;
        default: throw std::runtime_error("(HL)は参照不可");
    }
}

