/**
 * @file Game.h
 * @brief Dangerous Dave — clean ECS rebuild. Components, factories, and Game class.
 */

#pragma once

#include "bagel.h"
#include "sdl_compat.h"

#include <array>
#include <string>
#include <vector>

namespace dave {

constexpr int kTileSize    = 32;
constexpr int kLevelCols   = 100;
constexpr int kLevelRows   = 14;
constexpr int kScreenW     = 800;
constexpr int kScreenH     = 600;
constexpr int kHudH        = 40;
constexpr int kWorldW      = kLevelCols * kTileSize;  // 3200
constexpr int kWorldH      = kLevelRows * kTileSize;  // 448
constexpr int kPlayfieldOffsetY = kScreenH - kWorldH; // 152 — pad below HUD so floor sits at bottom

enum class TileKind : std::uint8_t {
    EMPTY = 0,
    GROUND,
    BRICK,
    FIRE,
    WATER,
};

enum class PickupKind : std::uint8_t {
    GEM,
    SPHERE,
    RED_DIAMOND,
    GUN,
    TROPHY,
};

enum class GameState : std::uint8_t {
    PLAYING,
    DEAD,
    LEVEL_COMPLETE,
    GAME_OVER,
};

// --- Components ---

struct Position { float x = 0; float y = 0; };
struct Velocity { float vx = 0; float vy = 0; };
struct Box      { float w = 0; float h = 0; };

struct Sprite {
    SDL_Texture* tex = nullptr;
    int z = 0;
    bool flipH = false;
    bool visible = true;
};

struct Anim {
    std::array<SDL_Texture*, 8> frames{};
    int frameCount = 0;
    int idx = 0;
    int delay = 8;
    int counter = 0;
    bool loop = true;
    bool playing = true;
};

struct PlayerData {
    int  score = 0;
    int  lives = 3;
    bool grounded = false;
    bool hasTrophy = false;
    bool hasGun = false;
    int  ammo = 0;
    int  facing = 1;          // 1 right, -1 left
    bool moving = false;
    int  walkAnimTimer = 0;
    int  walkAnimIdx = 0;
    int  spawnCol = 1;
    int  spawnRow = 12;
};

struct PickupData {
    PickupKind kind = PickupKind::GEM;
    int score = 0;
    bool collected = false;
};

struct DoorData {
    bool unlocked = false;
    bool animating = false;
    int  animFrames = 0;
    SDL_Texture* closedTex = nullptr;
    SDL_Texture* openTex = nullptr;
};

struct EnemyData {
    float patrolMin = 0;
    float patrolMax = 0;
    float speed = 0;
    int dir = 1;
    bool alive = true;
    int shootCooldown = 0;
};

struct BulletData {
    int dir = 1;
    int ttl = 90;
    bool fromPlayer = true;
    bool alive = true;
};

}  // namespace dave

// Storage selections. Most components are sparse-by-id since we use small entity counts and
// look them up by id more often than we iterate. Velocity/Anim are updated per-frame for many
// movers so we use packed storage for cache locality.
namespace bagel {
    template <> struct Storage<::dave::Position>   { using type = SparseStorage<::dave::Position>; };
    template <> struct Storage<::dave::Velocity>   { using type = PackedStorage<::dave::Velocity>; };
    template <> struct Storage<::dave::Box>        { using type = SparseStorage<::dave::Box>; };
    template <> struct Storage<::dave::Sprite>     { using type = SparseStorage<::dave::Sprite>; };
    template <> struct Storage<::dave::Anim>       { using type = PackedStorage<::dave::Anim>; };
    template <> struct Storage<::dave::PlayerData> { using type = SparseStorage<::dave::PlayerData>; };
    template <> struct Storage<::dave::PickupData> { using type = SparseStorage<::dave::PickupData>; };
    template <> struct Storage<::dave::DoorData>   { using type = SparseStorage<::dave::DoorData>; };
    template <> struct Storage<::dave::EnemyData>  { using type = SparseStorage<::dave::EnemyData>; };
    template <> struct Storage<::dave::BulletData> { using type = SparseStorage<::dave::BulletData>; };
}

namespace dave {

/// All textures loaded by main.cpp once and consumed read-only by the Game.
struct TextureRegistry {
    SDL_Texture* daveStand = nullptr;
    std::array<SDL_Texture*, 4> daveWalk{};   // dave1..dave4
    SDL_Texture* daveJump = nullptr;          // dave5 (or fall back to dave4)
    SDL_Texture* ground = nullptr;
    SDL_Texture* brick = nullptr;
    std::array<SDL_Texture*, 4> fire{};
    std::array<SDL_Texture*, 4> water{};
    SDL_Texture* gem = nullptr;
    SDL_Texture* sphere = nullptr;
    SDL_Texture* redDiamond = nullptr;
    SDL_Texture* gun = nullptr;
    std::array<SDL_Texture*, 5> cup{};
    SDL_Texture* doorClosed = nullptr;
    SDL_Texture* doorOpen = nullptr;
    SDL_Texture* enemy = nullptr;
    SDL_Texture* bullet = nullptr;
    SDL_Texture* monsterBullet = nullptr;
    SDL_Texture* cloud = nullptr;
    SDL_Texture* cloud2 = nullptr;
    SDL_Texture* background = nullptr;
    std::array<SDL_Texture*, 10> digit{};
    SDL_Texture* labelScore = nullptr;
    SDL_Texture* labelLives = nullptr;
    SDL_Texture* labelLevel = nullptr;
    SDL_Texture* labelGun = nullptr;
};

class Game {
public:
    Game(SDL_Renderer* ren, const TextureRegistry& tex);
    void run();

private:
    void resetLevel();
    void buildLevelFromAscii();
    void spawnPlayer(int col, int row);
    bagel::ent_type spawnPickup(int col, int row, PickupKind kind);
    bagel::ent_type spawnEnemy(int col, int row);
    bagel::ent_type spawnDoor(int col, int row);
    bagel::ent_type spawnBullet(float x, float y, int dir, bool fromPlayer);

    void readInput();
    void update();
    void updatePlayer();
    void updateEnemies();
    void updateBullets();
    void updateAnimations();
    void resolveTileCollisions(Position& pos, Velocity& vel, const Box& box, bool& grounded, bool& diedToHazard);
    void handlePickups();
    void checkHazards();
    void checkDoor();

    void render();
    void renderBackground();
    void renderTiles();
    void renderEntity(bagel::ent_type ent);
    void renderHud();
    void renderEndBanner();

    void killPlayer();
    void cameraUpdate();

    bool tileSolid(int col, int row) const;
    bool tileHazard(int col, int row) const;
    TileKind tileAt(int col, int row) const;

    SDL_Renderer* m_renderer;
    TextureRegistry m_tex;

    std::array<std::array<TileKind, kLevelCols>, kLevelRows> m_tiles{};

    bagel::ent_type m_player{ {-1} };
    bagel::ent_type m_door{ {-1} };
    bagel::ent_type m_trophy{ {-1} };
    std::vector<bagel::ent_type> m_pickups;
    std::vector<bagel::ent_type> m_enemies;
    std::vector<bagel::ent_type> m_bullets;

    float m_camX = 0;
    int m_animTick = 0;          // global tick for tile animations

    struct Input {
        bool left = false;
        bool right = false;
        bool jumpHeld = false;
        bool jumpPressed = false;
        bool shoot = false;
        bool shootPressed = false;
        bool exit = false;
    } m_input;
    bool m_prevJumpHeld = false;
    bool m_prevShoot = false;
    int  m_jumpBuffer = 0;     // frames remaining where a queued jump will auto-fire on landing

    GameState m_state = GameState::PLAYING;
    int  m_stateTimer = 0;
    bool m_running = true;
    int  m_levelNumber = 1;
};

}  // namespace dave
