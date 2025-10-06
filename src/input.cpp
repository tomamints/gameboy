#include "input.hpp"
#include <iostream>

Input::Input()
    : buttonStates(0xFF),       // ボタンは1=未押下で初期化
      directionStates(0xFF),    // 方向キーも1=未押下で初期化
      joypadRegister(0x0F)      // 下位4bit=1、bit4,5=1(どちらも未選択)
{}

void Input::poll() {
    // SDL2等のイベント処理はEmulatorまたはDisplayクラス側で行う
    // ここでは入力の状態を保持するのみ
}

void Input::setButtonPressed(JoypadButton button, bool pressed) {
    // A,B,Select,Start はボタン系
    if (button == JoypadButton::A || button == JoypadButton::B ||
        button == JoypadButton::SELECT || button == JoypadButton::START) {

        // 下位4bit: bit0=A, bit1=B, bit2=Select, bit3=Start
        uint8_t bitPos = static_cast<uint8_t>(button);
        if (pressed) {
            buttonStates &= ~(1 << bitPos);   // 押されたら0
        } else {
            buttonStates |=  (1 << bitPos);   // 離されたら1
        }
    }
    // 方向キー系
    else {
        // 下位4bit: bit0=Right, bit1=Left, bit2=Up, bit3=Down
        uint8_t bitPos = 0;
        if      (button == JoypadButton::RIGHT) bitPos = 0;
        else if (button == JoypadButton::LEFT)  bitPos = 1;
        else if (button == JoypadButton::UP)    bitPos = 2;
        else if (button == JoypadButton::DOWN)  bitPos = 3;

        if (pressed) {
            directionStates &= ~(1 << bitPos); // 押されたら0
        } else {
            directionStates |=  (1 << bitPos); // 離されたら1
        }
    }
}

void Input::setJoypadRegister(uint8_t value) {
    // CPU が P1 レジスタ(0xFF00)に書いた値を保持
    joypadRegister = value;
}

uint8_t Input::getJoypadState() const {
    // bit7,6は常に1、bit5,4はCPUが書いた選択ビットを保持
    uint8_t result = 0xC0 | (joypadRegister & 0x30);

    // bit5=0 → ボタン選択, bit4=0 → 方向キー選択
    bool selectButtons    = !(joypadRegister & 0x20);
    bool selectDirections = !(joypadRegister & 0x10);

    if (selectButtons) {
        // ボタン系を返す
        result &= 0xF0;
        result |= (buttonStates & 0x0F);
    }
    else if (selectDirections) {
        // 方向キー系を返す
        result &= 0xF0;
        result |= (directionStates & 0x0F);
    }
    else {
        // どちらも選択されていない場合は下位4bitを1にする
        result |= 0x0F;
    }

    return result;
}
