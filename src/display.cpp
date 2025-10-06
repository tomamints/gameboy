#include "display.hpp"
#include "input.hpp"
#include <iostream>

Display::Display() = default;

Display::~Display() {
    close();
}

bool Display::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL init error: " << SDL_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow(
        "Game Boy Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Window creation error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation error: " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        GB_WIDTH,
        GB_HEIGHT
    );

    if (!texture) {
        std::cerr << "Texture creation error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL2 display initialized successfully" << std::endl;
    return true;
}

void Display::updateFrame(const uint32_t* framebuffer) {
    if (!texture || !renderer) return;

    void* pixels;
    int pitch;

    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) == 0) {
        uint32_t* dest = static_cast<uint32_t*>(pixels);

        for (int y = 0; y < GB_HEIGHT; ++y) {
            for (int x = 0; x < GB_WIDTH; ++x) {
                dest[y * GB_WIDTH + x] = framebuffer[y * GB_WIDTH + x];
            }
        }

        SDL_UnlockTexture(texture);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

bool Display::handleEvents(Input* input) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }

        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);

            // ESCキーで終了
            if (event.key.keysym.sym == SDLK_ESCAPE && pressed) {
                return false;
            }

            // Game Boyキーマッピング (input が null でない場合のみ)
            if (input) {
                switch (event.key.keysym.sym) {
                    case SDLK_z:      // A button
                    case SDLK_k:      // K = A button
                        input->setButtonPressed(JoypadButton::A, pressed);
                        break;
                    case SDLK_x:      // B button
                    case SDLK_j:      // J = B button
                        input->setButtonPressed(JoypadButton::B, pressed);
                        break;
                    case SDLK_RETURN: // START
                        input->setButtonPressed(JoypadButton::START, pressed);
                        break;
                    case SDLK_RSHIFT: // SELECT
                    case SDLK_LSHIFT:
                        input->setButtonPressed(JoypadButton::SELECT, pressed);
                        break;
                    case SDLK_UP:     // UP
                    case SDLK_w:      // W = UP
                        input->setButtonPressed(JoypadButton::UP, pressed);
                        break;
                    case SDLK_DOWN:   // DOWN
                    case SDLK_s:      // S = DOWN
                        input->setButtonPressed(JoypadButton::DOWN, pressed);
                        break;
                    case SDLK_LEFT:   // LEFT
                    case SDLK_a:      // A = LEFT
                        input->setButtonPressed(JoypadButton::LEFT, pressed);
                        break;
                    case SDLK_RIGHT:  // RIGHT
                    case SDLK_d:      // D = RIGHT
                        input->setButtonPressed(JoypadButton::RIGHT, pressed);
                        break;
                }
            }
        }
    }
    return true;
}

void Display::close() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}