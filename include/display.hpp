#pragma once
#include <SDL2/SDL.h>
#include <cstdint>

class Input;  // 前方宣言

class Display {
public:
    Display();
    ~Display();

    bool init();
    void updateFrame(const uint32_t* framebuffer);
    bool handleEvents(Input* input = nullptr); // false if quit requested
    void close();

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    static constexpr int WINDOW_WIDTH = 160 * 3;  // 3倍拡大
    static constexpr int WINDOW_HEIGHT = 144 * 3; // 3倍拡大
    static constexpr int GB_WIDTH = 160;
    static constexpr int GB_HEIGHT = 144;
};