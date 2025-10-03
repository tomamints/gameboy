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
        case 0x00:
        {  // NOP (何もしない)
            cycles+=4;
            break;
        }
        // LD 16-bit immediate to register pair
        case 0x01: // LD BC,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            B = high;
            C = low;
            cycles += 12;
            std::cout << "LD BC, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x11: // LD DE,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            D = high;
            E = low;
            cycles += 12;
            std::cout << "LD DE, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x21: // LD HL,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            H = high;
            L = low;
            cycles += 12;
            std::cout << "LD HL, " << std::hex << ((high<<8)|low) << "\n";
            break;
        }

        case 0x31: // LD SP,d16
        {
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            SP = (high << 8) | low;
            cycles += 12;
            std::cout << "LD SP, " << std::hex << SP << "\n";
            break;
        }

        // LD 8-bit immediate to register
        case 0x06: // LD B,d8
        {
            uint8_t val = memory->readByte(PC++);
            B = val;
            cycles += 8;
            std::cout << "LD B, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x0E: // LD C,d8
        {
            uint8_t val = memory->readByte(PC++);
            C = val;
            cycles += 8;
            std::cout << "LD C, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x16: // LD D,d8
        {
            uint8_t val = memory->readByte(PC++);
            D = val;
            cycles += 8;
            std::cout << "LD D, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x1E: // LD E,d8
        {
            uint8_t val = memory->readByte(PC++);
            E = val;
            cycles += 8;
            std::cout << "LD E, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x26: // LD H,d8
        {
            uint8_t val = memory->readByte(PC++);
            H = val;
            cycles += 8;
            std::cout << "LD H, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x2E: // LD L,d8
        {
            uint8_t val = memory->readByte(PC++);
            L = val;
            cycles += 8;
            std::cout << "LD L, " << std::hex << (int)val << "\n";
            break;
        }

        case 0x3E:  // LD A,n  (次の1バイトをAにロード)
        {
            uint8_t value = memory->readByte(PC++);
            A = value;
            std::cout << "LD A, " << std::hex << (int)value << "\n";
            break;
        }
        case 0xC3: // JP a16
        {
            // 16bitアドレスをリトルエンディアンで読み込む
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;

            // プログラムカウンタをそのアドレスに設定
            PC = addr;

            cycles += 16;  // 実行に16クロック
            std::cout << "JP to " << std::hex << addr << "\n";
            break;
        }
        case 0xF3: //DI
        {
            //割り込み禁止フラグを設定
            //とりあえずデバッグ出力のみ
            std::cout << "DI (disable interrupts)\n";
            cycles += 4;
            break;
        }
        case 0xEA: //LD(a16), A
        {
            uint8_t low = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;
            memory->writeByte(addr, A);
            std::cout << "LD (" << std::hex << addr << "), A=" << (int)A << "\n";
            cycles += 16;
            break;
        }
        case 0xE0: // LDH (a8), A
        {
            uint8_t offset = memory->readByte(PC++);
            uint16_t addr = 0xFF00 + offset;
            memory->writeByte(addr, A);
            std::cout << "LDH (" << std::hex << addr << "), A=" << (int)A << "\n";
            cycles += 12;
            break;
        }
        // INC B
        case 0x04:
        {
            B++;
            // フラグ更新
            F &= FLAG_C;                     // キャリーだけ残す、他はクリア
            if (B == 0)  F |= FLAG_Z;        // ゼロならZ=1
            F &= ~FLAG_N;                     // N=0
            if ((B & 0x0F) == 0) F |= FLAG_H; // 下位4bitが0になったらH=1
            cycles += 4;
            std::cout << "INC B → " << std::hex << (int)B << "\n";
            break;
        }

        // DEC B
        case 0x05:
        {
            B--;
            F &= FLAG_C;                     // キャリーだけ残す
            if (B == 0)  F |= FLAG_Z;        // ゼロならZ=1
            F |= FLAG_N;                      // N=1（減算なので）
            if ((B & 0x0F) == 0x0F) F |= FLAG_H; // 借り発生ならH=1
            cycles += 4;
            std::cout << "DEC B → " << std::hex << (int)B << "\n";
            break;
        }

        // DEC C
        case 0x0D:
        {
            C--;
            F &= FLAG_C;                     // キャリーだけ残す
            if (C == 0)  F |= FLAG_Z;        // ゼロならZ=1
            F |= FLAG_N;                      // N=1
            if ((C & 0x0F) == 0x0F) F |= FLAG_H; // 借り発生ならH=1
            cycles += 4;
            std::cout << "DEC C → " << std::hex << (int)C << "\n";
            break;
        }

        // SCF
        case 0x37:
        {
            // Zフラグは変えない、NとHはクリア、C=1にセット
            F &= FLAG_Z;      // Zを残してN,H,Cをクリア
            F |= FLAG_C;      // キャリーをセット
            cycles += 4;
            std::cout << "SCF (Set Carry Flag)\n";
            break;
        }
        case 0xCD: // CALL a16
        {
            // 呼び出し先アドレスを取得
            uint8_t low  = memory->readByte(PC++);
            uint8_t high = memory->readByte(PC++);
            uint16_t addr = (high << 8) | low;

            // 現在のPCをスタックに保存（戻り先用）
            uint8_t pcLow  = PC & 0xFF; //0xFFは今後のため
            uint8_t pcHigh = (PC >> 8) & 0xFF;
            SP--; memory->writeByte(SP, pcHigh); // 上位バイト
            SP--; memory->writeByte(SP, pcLow);  // 下位バイト

            // PCを呼び出し先にジャンプ
            PC = addr;

            cycles += 24;
            std::cout << "CALL " << std::hex << addr << " (push return addr=" << std::hex << ((pcHigh<<8)|pcLow) << ")\n";
            break;
        }
        case 0x18: // JR r8
        {
            int8_t offset = static_cast<int8_t>(memory->readByte(PC++));
            PC = PC + offset;
            cycles += 12;
            std::cout << "JR to " << std::hex << PC << " (offset=" << (int)offset << ")\n";
            break;
        }

        // --------- ロード系 ----------
        case 0x7D:  // LD A,L
        A = L;
        cycles += 4;
        break;

        case 0x6A:  // LD L,D
        L = D;
        cycles += 4;
        break;

        case 0x0A:  // LD A,(BC)
        A = memory->readByte((B << 8) | C);
        cycles += 8;   // メモリアクセスありなので8クロック
        break;

        // --------- 16bit デクリメント系 ----------
        case 0x0B:  // DEC BC
        {
        uint16_t bc = (B << 8) | C;
        bc--;
        B = (bc >> 8) & 0xFF;
        C = bc & 0xFF;
        cycles += 8;
        break;
        }

        case 0x3B:  // DEC SP
        SP--;
        cycles += 8;
        break;

        // --------- 論理・ビット操作系 ----------
        case 0xA3:  // AND E
        A = A & E;
        F = 0;                      // 全クリア
        if (A == 0) F |= FLAG_Z;    // ゼロフラグ設定
        F |= FLAG_H;                // ANDは常にH=1
        cycles += 4;
        break;

        case 0xB2:  // OR D
        A = A | D;
        F = 0;
        if (A == 0) F |= FLAG_Z;
        cycles += 4;
        break;

        case 0x2F:  // CPL
        A = ~A;
        F |= (FLAG_N | FLAG_H);     // NとHをセット
        cycles += 4;
        break;



        default:
            std::cout << "Unknown opcode: 0x"
                      << std::hex << (int)opcode
                      << " at PC=" << PC-1 << "\n";
            break;
    }
}
