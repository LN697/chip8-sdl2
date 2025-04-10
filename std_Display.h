#pragma once

#ifndef DisplayCommon
#define DisplayCommon

#include <SDL2/SDL.h>
#include "std_Chip8Includes.h"
#include "std_CommonIncludes.h"

struct sSDL {
    SDL_Window* dispWindow = nullptr;
    SDL_Renderer* dispRenderer = nullptr;   
};

class cSDL {
    private:
        SDL_Window* _window;
        SDL_Renderer* _renderer;
        SDL_Rect rect;
        const uint32_t fg_col = FG_COLOR;
        const uint32_t bg_col = BG_COLOR;
    public:
        cSDL () {};
        cSDL (SDL_Window* window, SDL_Renderer* renderer) : _window(window), _renderer(renderer) {};

        void InitSDL () {
            if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                    nDebug::LogError("SDL could not initialize! SDL_Error");
            } else {
                _window = SDL_CreateWindow("CHIP-8 Emulator",SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    DISP_WIDTH * DISP_FACTOR,
                                    DISP_HEIGHT * DISP_FACTOR,
                                    0);
                if (_window == nullptr) {
                    nDebug::LogError("Window could not be created! SDL_Error");
                    SDL_Quit();
                }
                _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
                if (_renderer == nullptr) {
                    nDebug::LogError("Renderer could not be created! SDL_Error");
                    SDL_DestroyWindow(_window);
                    SDL_Quit();
                }
            }
            rect = {.x = 0, .y = 0, .w = DISP_FACTOR, .h = DISP_FACTOR};
        }

        void UpdateFrame (uint8_t* disp, uint32_t* color) {
            SDL_SetRenderDrawColor(_renderer, 0x00, 0x00, 0x00, 0xFF);
            SDL_RenderClear(_renderer);

            for (uint32_t i = 0; i < DISP_WIDTH * DISP_HEIGHT; i++) {
                rect.x = (i % DISP_WIDTH) * DISP_FACTOR;
                rect.y = (i / DISP_WIDTH) * DISP_FACTOR;

                if (disp[i] == 0x1) {
                    if (color[i] != FG_COLOR) {
                        color[i] = ColorLerp(fg_col, color[i]);
                    }
                    SDL_SetRenderDrawColor(_renderer, (fg_col >> 24) & 0xFF, (fg_col >> 16) & 0xFF, (fg_col >> 8) & 0xFF, (fg_col) & 0xFF);
                    SDL_RenderFillRect(_renderer, &rect);
                    if (OUTLINES) {
                        SDL_SetRenderDrawColor(_renderer, (bg_col >> 24) & 0xFF, (bg_col >> 16) & 0xFF, (bg_col >> 8) & 0xFF, (bg_col) & 0xFF);
                        SDL_RenderDrawRect(_renderer, &rect);
                    }
                } else {
                    if (color[i] != BG_COLOR) {
                        color[i] = ColorLerp(bg_col, color[i]);
                    }
                    SDL_SetRenderDrawColor(_renderer, (bg_col >> 24) & 0xFF, (bg_col >> 16) & 0xFF, (bg_col >> 8) & 0xFF, (bg_col) & 0xFF);
                    SDL_RenderFillRect(_renderer, &rect);
                }
            }

            SDL_RenderPresent(_renderer);
        }

        void QuitSDL () {
            SDL_DestroyRenderer(_renderer);
            SDL_DestroyWindow(_window);
            SDL_Quit();
        }
        
        uint32_t ColorLerp(const uint32_t start, const uint32_t end) {
            const uint8_t r = (start >> 24) + LERP_RATE * ((end >> 24) - (start >> 24));
            const uint8_t g = (start >> 16) + LERP_RATE * ((end >> 16) - (start >> 16));
            const uint8_t b = (start >> 8) + LERP_RATE * ((end >> 8) - (start >> 8));
            const uint8_t a = (start) + LERP_RATE * ((end) - (start));

            return (r << 24) | (g << 16) | (b << 8) | a;
        }
};

#endif