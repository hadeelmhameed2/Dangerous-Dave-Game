/**
 * @file sdl_compat.cpp
 * @brief Runtime loader implementation for SDL3 and SDL3_image on Windows.
 */

#include "sdl_compat.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>

namespace
{
#ifdef _WIN32
    using SDL_Init_fn = bool (*)(SDL_InitFlags);
    using SDL_Quit_fn = void (*)(void);
    using SDL_CreateWindowAndRenderer_fn = bool (*)(const char*, int, int, SDL_WindowFlags, SDL_Window**, SDL_Renderer**);
    using SDL_DestroyWindow_fn = void (*)(SDL_Window*);
    using SDL_DestroyRenderer_fn = void (*)(SDL_Renderer*);
    using SDL_DestroyTexture_fn = void (*)(SDL_Texture*);
    using SDL_SetRenderDrawColor_fn = bool (*)(SDL_Renderer*, std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t);
    using SDL_RenderClear_fn = bool (*)(SDL_Renderer*);
    using SDL_RenderTexture_fn = bool (*)(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*);
    using SDL_RenderTextureRotated_fn = bool (*)(SDL_Renderer*, SDL_Texture*, const SDL_FRect*, const SDL_FRect*, double, const SDL_FPoint*, SDL_FlipMode);
    using SDL_RenderFillRect_fn = bool (*)(SDL_Renderer*, const SDL_FRect*);
    using SDL_SetRenderDrawBlendMode_fn = bool (*)(SDL_Renderer*, std::uint32_t);
    using SDL_SetTextureColorMod_fn = bool (*)(SDL_Texture*, std::uint8_t, std::uint8_t, std::uint8_t);
    using SDL_SetTextureAlphaMod_fn = bool (*)(SDL_Texture*, std::uint8_t);
    using SDL_GetTextureSize_fn = bool (*)(SDL_Texture*, float*, float*);
    using SDL_RenderDebugText_fn = bool (*)(SDL_Renderer*, float, float, const char*);
    using SDL_RenderPresent_fn = void (*)(SDL_Renderer*);
    using SDL_Delay_fn = void (*)(std::uint32_t);
    using SDL_GetTicks_fn = std::uint64_t (*)(void);
    using SDL_PollEvent_fn = bool (*)(SDL_Event*);
    using IMG_LoadTexture_fn = SDL_Texture* (*)(SDL_Renderer*, const char*);

    struct Api
    {
        HMODULE sdl = nullptr;
        HMODULE img = nullptr;
        SDL_Init_fn SDL_Init = nullptr;
        SDL_Quit_fn SDL_Quit = nullptr;
        SDL_CreateWindowAndRenderer_fn SDL_CreateWindowAndRenderer = nullptr;
        SDL_DestroyWindow_fn SDL_DestroyWindow = nullptr;
        SDL_DestroyRenderer_fn SDL_DestroyRenderer = nullptr;
        SDL_DestroyTexture_fn SDL_DestroyTexture = nullptr;
        SDL_SetRenderDrawColor_fn SDL_SetRenderDrawColor = nullptr;
        SDL_RenderClear_fn SDL_RenderClear = nullptr;
        SDL_RenderTexture_fn SDL_RenderTexture = nullptr;
        SDL_RenderTextureRotated_fn SDL_RenderTextureRotated = nullptr;
        SDL_RenderFillRect_fn SDL_RenderFillRect = nullptr;
        SDL_SetRenderDrawBlendMode_fn SDL_SetRenderDrawBlendMode = nullptr;
        SDL_SetTextureColorMod_fn SDL_SetTextureColorMod = nullptr;
        SDL_SetTextureAlphaMod_fn SDL_SetTextureAlphaMod = nullptr;
        SDL_GetTextureSize_fn SDL_GetTextureSize = nullptr;
        SDL_RenderDebugText_fn SDL_RenderDebugText = nullptr;
        SDL_RenderPresent_fn SDL_RenderPresent = nullptr;
        SDL_Delay_fn SDL_Delay = nullptr;
        SDL_GetTicks_fn SDL_GetTicks = nullptr;
        SDL_PollEvent_fn SDL_PollEvent = nullptr;
        IMG_LoadTexture_fn IMG_LoadTexture = nullptr;
    };

    template <typename T>
    T load_symbol(HMODULE module, const char* name)
    {
        return reinterpret_cast<T>(GetProcAddress(module, name));
    }

    Api& api()
    {
        static Api value = [] {
            Api a;
            a.sdl = LoadLibraryA("SDL3.dll");
            a.img = LoadLibraryA("SDL3_image.dll");

            if (a.sdl)
            {
                a.SDL_Init = load_symbol<SDL_Init_fn>(a.sdl, "SDL_Init");
                a.SDL_Quit = load_symbol<SDL_Quit_fn>(a.sdl, "SDL_Quit");
                a.SDL_CreateWindowAndRenderer = load_symbol<SDL_CreateWindowAndRenderer_fn>(a.sdl, "SDL_CreateWindowAndRenderer");
                a.SDL_DestroyWindow = load_symbol<SDL_DestroyWindow_fn>(a.sdl, "SDL_DestroyWindow");
                a.SDL_DestroyRenderer = load_symbol<SDL_DestroyRenderer_fn>(a.sdl, "SDL_DestroyRenderer");
                a.SDL_DestroyTexture = load_symbol<SDL_DestroyTexture_fn>(a.sdl, "SDL_DestroyTexture");
                a.SDL_SetRenderDrawColor = load_symbol<SDL_SetRenderDrawColor_fn>(a.sdl, "SDL_SetRenderDrawColor");
                a.SDL_RenderClear = load_symbol<SDL_RenderClear_fn>(a.sdl, "SDL_RenderClear");
                a.SDL_RenderTexture = load_symbol<SDL_RenderTexture_fn>(a.sdl, "SDL_RenderTexture");
                a.SDL_RenderTextureRotated = load_symbol<SDL_RenderTextureRotated_fn>(a.sdl, "SDL_RenderTextureRotated");
                a.SDL_RenderFillRect = load_symbol<SDL_RenderFillRect_fn>(a.sdl, "SDL_RenderFillRect");
                a.SDL_SetRenderDrawBlendMode = load_symbol<SDL_SetRenderDrawBlendMode_fn>(a.sdl, "SDL_SetRenderDrawBlendMode");
                a.SDL_SetTextureColorMod = load_symbol<SDL_SetTextureColorMod_fn>(a.sdl, "SDL_SetTextureColorMod");
                a.SDL_SetTextureAlphaMod = load_symbol<SDL_SetTextureAlphaMod_fn>(a.sdl, "SDL_SetTextureAlphaMod");
                a.SDL_GetTextureSize = load_symbol<SDL_GetTextureSize_fn>(a.sdl, "SDL_GetTextureSize");
                a.SDL_RenderDebugText = load_symbol<SDL_RenderDebugText_fn>(a.sdl, "SDL_RenderDebugText");
                a.SDL_RenderPresent = load_symbol<SDL_RenderPresent_fn>(a.sdl, "SDL_RenderPresent");
                a.SDL_Delay = load_symbol<SDL_Delay_fn>(a.sdl, "SDL_Delay");
                a.SDL_GetTicks = load_symbol<SDL_GetTicks_fn>(a.sdl, "SDL_GetTicks");
                a.SDL_PollEvent = load_symbol<SDL_PollEvent_fn>(a.sdl, "SDL_PollEvent");
            }

            if (a.img)
            {
                a.IMG_LoadTexture = load_symbol<IMG_LoadTexture_fn>(a.img, "IMG_LoadTexture");
            }

            return a;
        }();

        return value;
    }

    bool ready()
    {
        static const bool ok = [] {
            const Api& a = api();
            return a.sdl && a.img && a.SDL_Init && a.SDL_Quit && a.SDL_CreateWindowAndRenderer &&
                   a.SDL_DestroyWindow && a.SDL_DestroyRenderer && a.SDL_DestroyTexture &&
                   a.SDL_SetRenderDrawColor && a.SDL_RenderClear && a.SDL_RenderTexture &&
                   a.SDL_RenderPresent && a.SDL_Delay && a.IMG_LoadTexture;
        }();
        return ok;
    }
#else
    bool ready() { return false; }
#endif
}

#ifdef _WIN32
#define SDL_CALL(fn, ...) (ready() && api().fn ? api().fn(__VA_ARGS__) : false)
#define SDL_VOID(fn, ...) do { if (ready() && api().fn) api().fn(__VA_ARGS__); } while (0)
#else
#define SDL_CALL(fn, ...) false
#define SDL_VOID(fn, ...) ((void)0)
#endif

bool SDL_Init(SDL_InitFlags flags) { return SDL_CALL(SDL_Init, flags); }
void SDL_Quit(void) { SDL_VOID(SDL_Quit); }

bool SDL_CreateWindowAndRenderer(const char* title, int width, int height, SDL_WindowFlags flags, SDL_Window** window, SDL_Renderer** renderer)
{
    return SDL_CALL(SDL_CreateWindowAndRenderer, title, width, height, flags, window, renderer);
}

void SDL_DestroyWindow(SDL_Window* window)        { if (window)   SDL_VOID(SDL_DestroyWindow, window); }
void SDL_DestroyRenderer(SDL_Renderer* renderer)  { if (renderer) SDL_VOID(SDL_DestroyRenderer, renderer); }
void SDL_DestroyTexture(SDL_Texture* texture)     { if (texture)  SDL_VOID(SDL_DestroyTexture, texture); }

bool SDL_SetRenderDrawColor(SDL_Renderer* renderer, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    return SDL_CALL(SDL_SetRenderDrawColor, renderer, r, g, b, a);
}

bool SDL_RenderClear(SDL_Renderer* renderer) { return SDL_CALL(SDL_RenderClear, renderer); }

bool SDL_RenderTexture(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect* srcrect, const SDL_FRect* dstrect)
{
    return SDL_CALL(SDL_RenderTexture, renderer, texture, srcrect, dstrect);
}

bool SDL_RenderTextureRotated(SDL_Renderer* renderer, SDL_Texture* texture, const SDL_FRect* srcrect, const SDL_FRect* dstrect, double angle, const SDL_FPoint* center, SDL_FlipMode flip)
{
#ifdef _WIN32
    if (ready() && api().SDL_RenderTextureRotated)
        return api().SDL_RenderTextureRotated(renderer, texture, srcrect, dstrect, angle, center, flip);
    return SDL_RenderTexture(renderer, texture, srcrect, dstrect);
#else
    (void)renderer; (void)texture; (void)srcrect; (void)dstrect; (void)angle; (void)center; (void)flip;
    return false;
#endif
}

bool SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_FRect* rect)
{
    return SDL_CALL(SDL_RenderFillRect, renderer, rect);
}

bool SDL_SetRenderDrawBlendMode(SDL_Renderer* renderer, std::uint32_t blendMode)
{
    return SDL_CALL(SDL_SetRenderDrawBlendMode, renderer, blendMode);
}

bool SDL_SetTextureColorMod(SDL_Texture* texture, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return SDL_CALL(SDL_SetTextureColorMod, texture, r, g, b);
}

bool SDL_SetTextureAlphaMod(SDL_Texture* texture, std::uint8_t alpha)
{
    return SDL_CALL(SDL_SetTextureAlphaMod, texture, alpha);
}

bool SDL_GetTextureSize(SDL_Texture* texture, float* w, float* h)
{
    return SDL_CALL(SDL_GetTextureSize, texture, w, h);
}

bool SDL_RenderDebugText(SDL_Renderer* renderer, float x, float y, const char* text)
{
    return SDL_CALL(SDL_RenderDebugText, renderer, x, y, text);
}

void SDL_RenderPresent(SDL_Renderer* renderer) { SDL_VOID(SDL_RenderPresent, renderer); }
void SDL_Delay(std::uint32_t ms)               { SDL_VOID(SDL_Delay, ms); }

std::uint64_t SDL_GetTicks(void)
{
#ifdef _WIN32
    if (ready() && api().SDL_GetTicks) return api().SDL_GetTicks();
    return GetTickCount64();
#else
    return 0;
#endif
}

bool SDL_PollEvent(SDL_Event* event)
{
#ifdef _WIN32
    if (ready() && api().SDL_PollEvent) return api().SDL_PollEvent(event);
    return false;
#else
    (void)event; return false;
#endif
}

SDL_Texture* IMG_LoadTexture(SDL_Renderer* renderer, const char* file)
{
#ifdef _WIN32
    return ready() && api().IMG_LoadTexture ? api().IMG_LoadTexture(renderer, file) : nullptr;
#else
    (void)renderer; (void)file; return nullptr;
#endif
}
