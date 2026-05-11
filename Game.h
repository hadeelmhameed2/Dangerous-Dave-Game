/**
 * @file Game.h
 * @brief Components, factories, and Game class.
 */

#pragma once

#include "bagel.h"
#include <SDL3/SDL.h>
#include <box2d/box2d.h>

#include <array>

namespace dave {

// 1 meter = kTileSize px. Box2D coords = world pixels / BOX_SCALE.
constexpr float BOX_SCALE = 32.0f;
constexpr float BOX_GRAVITY = 40.0f;  // m/s^2, +Y is down on screen.
constexpr int   FPS = 60;
constexpr float BOX_STEP = 1.0f / FPS;

// Collision categories. Player vs enemy/bullet uses AABB; pickups/hazards/door use sensor events.
constexpr std::uint64_t CAT_TILE   = 0x0001;
constexpr std::uint64_t CAT_PLAYER = 0x0002;
constexpr std::uint64_t CAT_ENEMY  = 0x0004;
constexpr std::uint64_t CAT_BULLET = 0x0008;
constexpr std::uint64_t CAT_SENSOR = 0x0010;

// User-data sentinels on Box2D bodies, read by sensor_event_system:
//   null → hazard tile sensor
//   1    → player body (the visitor)
//   id+2 → entity-based sensor (pickup / door / trophy)
constexpr std::uintptr_t UDATA_PLAYER   = 1;
constexpr std::uintptr_t UDATA_ENTITY_BASE = 2;

constexpr int kTileSize    = 32;
constexpr int kLevelCols   = 100;
constexpr int kLevelRows   = 14;
constexpr int kScreenW     = 800;
constexpr int kScreenH     = 600;
constexpr int kHudH        = 40;
constexpr int kWorldW      = kLevelCols * kTileSize;  // 3200
constexpr int kWorldH      = kLevelRows * kTileSize;  // 448
constexpr int kPlayfieldOffsetY = kScreenH - kWorldH; // 152 — pad below HUD so floor sits at screen bottom.

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
struct Box      { float w = 0; float h = 0; };

struct Sprite {
    SDL_Texture* tex = nullptr;
    int z = 0;
    bool flipH = false;
    bool visible = true;
    Uint8 tintR = 255, tintG = 255, tintB = 255;  // Color mod; 255 = no tint.
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
    int  facing = 1;          // 1 = right, -1 = left.
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

enum class EnemyKind : std::uint8_t { GROUND, FLYING };

struct EnemyData {
    EnemyKind kind = EnemyKind::GROUND;
    float patrolMin = 0;
    float patrolMax = 0;
    float speed = 0;
    int dir = 1;
    bool alive = true;
    int shootCooldown = 0;
    float phase = 0;     // FLYING: sine phase so multiple flyers desync.
};

struct BulletData {
    int dir = 1;
    int ttl = 90;
    bool fromPlayer = true;
    bool alive = true;
};

// Box2D body handle for dynamic entities (player, enemies, bullets).
struct Collider { b2BodyId b; };

// Input intent. input_system writes held state; player_system reads it and does edge detection.
struct Intent {
    bool left = false;
    bool right = false;
    bool jumpHeld = false;
    bool shoot = false;
};

// Per-entity keybinds. jumpAlt is optional (SDL_SCANCODE_UNKNOWN if unused).
struct Keys {
    SDL_Scancode left, right, jump, jumpAlt, shoot;
};

}  // namespace dave

// Storage selections. Per-frame iterated components (Anim, Intent, Keys) use PackedStorage
// for cache locality; the rest are SparseStorage (lookup by id).
namespace bagel {
    template <> struct Storage<::dave::Position>   { using type = SparseStorage<::dave::Position>; };
    template <> struct Storage<::dave::Box>        { using type = SparseStorage<::dave::Box>; };
    template <> struct Storage<::dave::Sprite>     { using type = SparseStorage<::dave::Sprite>; };
    template <> struct Storage<::dave::Anim>       { using type = PackedStorage<::dave::Anim>; };
    template <> struct Storage<::dave::PlayerData> { using type = SparseStorage<::dave::PlayerData>; };
    template <> struct Storage<::dave::PickupData> { using type = SparseStorage<::dave::PickupData>; };
    template <> struct Storage<::dave::DoorData>   { using type = SparseStorage<::dave::DoorData>; };
    template <> struct Storage<::dave::EnemyData>  { using type = SparseStorage<::dave::EnemyData>; };
    template <> struct Storage<::dave::BulletData> { using type = SparseStorage<::dave::BulletData>; };
    template <> struct Storage<::dave::Collider>   { using type = SparseStorage<::dave::Collider>; };
    template <> struct Storage<::dave::Intent>     { using type = PackedStorage<::dave::Intent>; };
    template <> struct Storage<::dave::Keys>       { using type = PackedStorage<::dave::Keys>; };
}

namespace dave {

/// Textures loaded once by main.cpp, consumed read-only by Game.
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
    std::array<SDL_Texture*, 10> digit{};
    SDL_Texture* labelScore = nullptr;
    SDL_Texture* labelLives = nullptr;
    SDL_Texture* labelLevel = nullptr;
};

class Game {
public:
    Game(SDL_Renderer* ren, const TextureRegistry& tex);
    ~Game();
    void run();

private:
    void resetLevel();
    void buildLevelFromAscii();
    void buildStaticTileBodies();
    void box_system();
    void sensor_event_system();
    bool playerGrounded() const;
    void spawnPlayer(int col, int row);
    bagel::ent_type spawnPickup(int col, int row, PickupKind kind);
    bagel::ent_type spawnEnemy(int col, int row, EnemyKind kind = EnemyKind::GROUND);
    bagel::ent_type spawnDoor(int col, int row);
    bagel::ent_type spawnBullet(float x, float y, int dir, bool fromPlayer);

    void input_system();
    void update();
    void player_system();
    void enemy_system();
    void bullet_system();
    void anim_system();
    void hazard_system();
    void door_system();

    void draw_system();
    void renderBackground();
    void renderTiles();
    void renderEntity(bagel::ent_type ent);
    void renderHud();
    void renderEndBanner();

    void killPlayer();
    void camera_system();

    bool tileSolid(int col, int row) const;
    TileKind tileAt(int col, int row) const;

    SDL_Renderer* m_renderer;
    TextureRegistry m_tex;

    std::array<std::array<TileKind, kLevelCols>, kLevelRows> m_tiles{};

    bagel::ent_type m_player{ {-1} };
    bagel::ent_type m_door{ {-1} };
    bagel::ent_type m_trophy{ {-1} };

    b2WorldId m_world = b2_nullWorldId;
    float m_camX = 0;
    int m_animTick = 0;          // Global tick for tile/sprite animations.

    bool m_prevJumpHeld = false;
    bool m_prevShoot = false;
    int  m_jumpBuffer = 0;     // Buffered jump auto-fires on landing within N frames.

    GameState m_state = GameState::PLAYING;
    int  m_stateTimer = 0;
    bool m_running = true;
    int  m_levelNumber = 1;
};

}  // namespace dave
