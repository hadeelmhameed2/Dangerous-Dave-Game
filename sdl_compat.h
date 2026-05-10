/**
 * @file sdl_compat.h
 * @brief Minimal runtime-only SDL3 compatibility declarations used by the demo.
 */

#pragma once

#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

using SDL_InitFlags = std::uint32_t;
using SDL_WindowFlags = std::uint32_t;

constexpr SDL_InitFlags SDL_INIT_VIDEO = 0x00000020u;

struct SDL_FRect { float x, y, w, h; };
struct SDL_FPoint { float x, y; };

enum SDL_FlipMode : int {
    SDL_FLIP_NONE = 0,
    SDL_FLIP_HORIZONTAL = 1,
    SDL_FLIP_VERTICAL = 2,
};

/// SDL3 SDL_Event is 128 bytes; we only inspect `type`.
struct SDL_Event {
    std::uint32_t type;
    std::uint8_t  pad[124];
};

constexpr std::uint32_t SDL_EVENT_QUIT = 0x100;

bool SDL_Init(SDL_InitFlags flags);
void SDL_Quit(void);
bool SDL_CreateWindowAndRenderer(const char* title, int width, int height, SDL_WindowFlags flags, SDL_Window** window, SDL_Renderer** renderer);
void SDL_DestroyWindow(SDL_Window* window);
void SDL_DestroyRenderer(SDL_Renderer* renderer);
void SDL_DestroyTexture(SDL_Texture* texture);
bool SDL_SetRenderDrawColor(SDL_Renderer* renderer, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
bool SDL_RenderClear(SDL_Renderer* renderer);
bool SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect* srcrect, const SDL_FRect* dstrect);
bool SDL_RenderTextureRotated(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect* srcrect, const SDL_FRect* dstrect, double angle, const SDL_FPoint* center, SDL_FlipMode flip);
bool SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_FRect* rect);
bool SDL_SetRenderDrawBlendMode(SDL_Renderer* renderer, std::uint32_t blendMode);
bool SDL_SetTextureColorMod(SDL_Texture* texture, std::uint8_t r, std::uint8_t g, std::uint8_t b);
bool SDL_SetTextureAlphaMod(SDL_Texture* texture, std::uint8_t alpha);
bool SDL_GetTextureSize(SDL_Texture* texture, float* w, float* h);
bool SDL_RenderDebugText(SDL_Renderer* renderer, float x, float y, const char* text);
void SDL_RenderPresent(SDL_Renderer* renderer);
void SDL_Delay(std::uint32_t ms);
std::uint64_t SDL_GetTicks(void);
bool SDL_PollEvent(SDL_Event* event);

SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file);
