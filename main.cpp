/**
 * @file main.cpp
 * @brief Bootstrap: SDL init, texture loading, run the game.
 */

#include "Game.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef DAVE_PROJECT_SOURCE_DIR
#define DAVE_PROJECT_SOURCE_DIR "."
#endif

namespace
{

#ifdef _WIN32
std::string executableDirectory()
{
    char buffer[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (size == 0 || size >= MAX_PATH) return std::string(DAVE_PROJECT_SOURCE_DIR);
    std::string path(buffer, buffer + size);
    const std::size_t pos = path.find_last_of("\\/");
    return pos == std::string::npos ? std::string(".") : path.substr(0, pos);
}
#else
std::string executableDirectory() { return std::string(DAVE_PROJECT_SOURCE_DIR); }
#endif

std::string joinPath(const std::string& base, const std::string& relative)
{
    if (base.empty()) return relative;
    const char last = base.back();
    if (last == '/' || last == '\\') return base + relative;
    return base + "/" + relative;
}

SDL_Texture* loadOne(SDL_Renderer* renderer, const std::string& exeDir, const std::string& relPath, bool warnIfMissing = true)
{
    const std::vector<std::string> candidates = {
        relPath,
        joinPath(exeDir, relPath),
        joinPath(DAVE_PROJECT_SOURCE_DIR, relPath),
    };
    for (const auto& p : candidates)
    {
        if (SDL_Texture* t = IMG_LoadTexture(renderer, p.c_str())) return t;
    }
    if (warnIfMissing) std::cerr << "Failed to load " << relPath << '\n';
    return nullptr;
}

void loadAll(SDL_Renderer* renderer, dave::TextureRegistry& reg)
{
    const std::string exeDir = executableDirectory();
    auto L = [&](const std::string& name, bool warn = true) {
        return loadOne(renderer, exeDir, "res/" + name, warn);
    };

    reg.daveStand = L("Dave.png");
    reg.daveWalk[0] = L("dave1.png", false);
    reg.daveWalk[1] = L("dave2.png", false);
    reg.daveWalk[2] = L("dave3.png", false);
    reg.daveWalk[3] = L("dave4.png", false);
    reg.daveJump  = L("dave5.png", false);
    if (!reg.daveJump) reg.daveJump = reg.daveWalk[3] ? reg.daveWalk[3] : reg.daveStand;
    for (auto& w : reg.daveWalk) if (!w) w = reg.daveStand;

    reg.background = L("blackbackround.png", false);
    reg.ground = L("ground.png");
    reg.brick  = L("brick.png", false);

    reg.fire[0] = L("fire1.png", false);
    reg.fire[1] = L("fire2.png", false);
    reg.fire[2] = L("fire3.png", false);
    reg.fire[3] = L("fire4.png", false);
    SDL_Texture* fireFallback = L("fire.png", false);
    for (auto& f : reg.fire) if (!f) f = fireFallback;

    reg.water[0] = L("water1.png", false);
    reg.water[1] = L("water2.png", false);
    reg.water[2] = L("water3.png", false);
    reg.water[3] = L("water4.png", false);
    for (auto& w : reg.water) if (!w) w = reg.water[0];

    reg.gem        = L("gem.png");
    reg.sphere     = L("sphere.png");
    reg.redDiamond = L("redDiamonds.png", false);
    if (!reg.redDiamond) reg.redDiamond = L("diamonds.png", false);
    if (!reg.redDiamond) reg.redDiamond = reg.gem;
    reg.gun = L("gun.png", false);

    reg.cup[0] = L("cup1.png", false);
    reg.cup[1] = L("cup2.png", false);
    reg.cup[2] = L("cup3.png", false);
    reg.cup[3] = L("cup4.png", false);
    reg.cup[4] = L("cup5.png", false);
    if (!reg.cup[0]) reg.cup[0] = L("prize.png", false);
    for (auto& c : reg.cup) if (!c) c = reg.cup[0];

    reg.doorClosed = L("door.png");

    reg.enemy         = L("monster.png", false);
    reg.explosion[0]  = L("explosion1.png", false);
    reg.explosion[1]  = L("explosion2.png", false);
    if (!reg.explosion[0]) reg.explosion[0] = reg.explosion[1];
    if (!reg.explosion[1]) reg.explosion[1] = reg.explosion[0];
    reg.bullet        = L("weapon.png", false);
    reg.monsterBullet = L("monsterBullet.png", false);
    if (!reg.monsterBullet) reg.monsterBullet = reg.bullet;

    for (int i = 0; i < 10; ++i)
    {
        reg.digit[i] = L(std::to_string(i) + ".png", false);
    }

    reg.labelScore = L("score.png", false);
    reg.labelLives = L("lives.png", false);
    reg.labelLevel = L("levelanddaves.png", false);
}

}  // namespace

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init failed\n";
        return -1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Dangerous Dave", 800, 600, 0, &window, &renderer))
    {
        std::cerr << "Could not create window/renderer\n";
        SDL_Quit();
        return -1;
    }

    dave::TextureRegistry reg;
    loadAll(renderer, reg);

    {
        dave::Game game(renderer, reg);
        game.run();
    }

    // Skip per-texture cleanup — process exit reclaims them, and many slots share textures.
    (void)window;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
