#pragma once
#include <cstdint>

enum class JoypadButton {
    A = 0,      // bit 0
    B = 1,      // bit 1
    SELECT = 2, // bit 2
    START = 3,  // bit 3
    RIGHT = 0,  // bit 0 (direction)
    LEFT = 1,   // bit 1 (direction)
    UP = 2,     // bit 2 (direction)
    DOWN = 3    // bit 3 (direction)
};

class Input {
public:
    Input();
    void poll();

    // Joypad状態管理
    void setButtonPressed(JoypadButton button, bool pressed);
    uint8_t getJoypadState() const;
    void setJoypadRegister(uint8_t value);  // 0xFF00 P1レジスタ設定

private:
    uint8_t buttonStates = 0xFF;     // ボタン状態 (bit0-3: A,B,Select,Start)
    uint8_t directionStates = 0xFF;  // 方向キー状態 (bit0-3: Right,Left,Up,Down)
    uint8_t joypadRegister = 0x0F;   // P1レジスタ (0xFF00)
};
