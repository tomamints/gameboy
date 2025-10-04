#include "emulator.hpp"
#include <iostream>
#include <iomanip>

Emulator::Emulator()
    : ppu(memory),
      cpu(&memory, &ppu),
      timer(&memory) {}

void Emulator::loadROM(const std::string& path) {
    memory.loadROM(path);
    cpu.reset(); // ROMロード後にCPUを初期化
    timer.reset(); // タイマーも初期化
}

void Emulator::run() {
    std::cout << "Emulator running...\n";

    // 0x0100番地（プログラム開始地点）の内容を確認
    std::cout << "\n=== Memory dump from 0x0100 (start) ===\n";
    for (uint16_t addr = 0x0100; addr <= 0x010F; addr++) {
        uint8_t val = memory.readByte(addr);
        std::cout << std::hex << std::setfill('0') << std::setw(4) << addr
                  << ": " << std::setw(2) << (int)val << std::dec << std::endl;
    }
    std::cout << "=====================================\n";

    // 0xC7D0周辺のメモリ内容を確認
    std::cout << "\n=== Memory dump around 0xC7D0 ===\n";
    for (uint16_t addr = 0xC7D0; addr <= 0xC7DF; addr++) {
        uint8_t val = memory.readByte(addr);
        std::cout << std::hex << std::setfill('0') << std::setw(4) << addr
                  << ": " << std::setw(2) << (int)val << std::dec << std::endl;
    }
    std::cout << "================================\n\n";

    int totalCycles = 0;
    long long stepCount = 0;
    const int MAX_CYCLES = 2000000000;
    std::string output;                     // ★ここに文字を貯める

    int serialCount = 0;
    int serialLimit = 100;                    // ★10回で停止
    uint8_t lastSC = 0;
    int serialDelay = 0;                      // シリアル転送遅延カウンタ

    while (totalCycles < MAX_CYCLES) {
        int cycles = cpu.step();
        ++stepCount;
        ppu.step(cycles);
        timer.step(cycles);
        totalCycles += cycles;

        uint8_t sc = memory.readByte(0xFF02);

        // シリアル転送の遅延処理
        if (serialDelay > 0) {
            serialDelay--;
            if (serialDelay == 0) {
                // 転送完了 → SC.bit7を0に戻す
                memory.writeByte(0xFF02, 0x00);
                std::cout << " [Transfer complete]\n";
            }
        }

        // SCを再読み取り（遅延処理後の正しい値を取得）
        sc = memory.readByte(0xFF02);

        // デバッグ: SCが変わったら表示
        if (sc != lastSC && (sc == 0x80 || sc == 0x81)) {
            std::cout << "[DEBUG] SC changed: "
                      << std::hex << (int)lastSC << " -> "
                      << (int)sc << std::dec << "\n";
        }
        lastSC = sc;

        // シリアル出力チェック（新規転送開始の検出）
        if (sc == 0x81 && serialDelay == 0) {  // 新しい転送開始
            char c = static_cast<char>(memory.readByte(0xFF01));
            std::cout << c << std::flush;  // デバッグ表示を簡素化

            // ★受け取った文字を蓄積
            output += c;
            std::cout<<"output = " << output << std::endl;

            // 成功パターン: "Passed"が確認されたら終了
            if (output.find("Passed") != std::string::npos) {
                std::cout << "\n[INFO] 'Passed' が検出されました！テスト完了。\n";
                break;
            }

            // 失敗パターン: 完全なエラーメッセージが確認されたら終了
            if (output.find("Failed") != std::string::npos) {
                std::cout << "\n[INFO] テスト失敗またはエラーが検出されました。\n";
                break;
            }

            // 転送遅延を設定（CPUが確認できるように数サイクル待つ）
            serialDelay = 100;  // 100サイクル後に転送完了

            // ★フォールバック: 上限回数で終了
            serialCount++;
            if (serialCount >= serialLimit) {
                std::cout << "\n[INFO] シリアル転送が "
                          << serialLimit << " 回発生したので終了します。\n";
                break;
            }
        }
    }

    // ★ここでこれまで受け取った文字列をまとめて表示
    std::cout << "\n[INFO] 受信したシリアル文字列: \"" << output << "\"\n";

    std::cout << "[INFO] 最終サイクル数: " << totalCycles << "\n";
    std::cout << "[INFO] 70000サイクル換算ステップ数: "
              << (totalCycles / 70000) << "\n";
    std::cout << "[INFO] 命令実行ステップ数: " << stepCount << "\n";
}
