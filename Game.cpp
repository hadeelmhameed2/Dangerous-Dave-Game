/**
 * @file Game.cpp
 * @brief Dangerous Dave one-stage rebuild — tile-based platformer with ECS components.
 */

#include "Game.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace dave {

namespace {

constexpr float kGravity      = 0.55f;
constexpr float kMaxFallSpeed = 12.0f;
constexpr float kJumpVel      = -10.5f;
constexpr float kMoveSpeed    = 3.4f;
constexpr float kBulletSpeed  = 9.0f;
constexpr int   kBulletTTL    = 80;
constexpr int   kMaxPlayerBullets = 1;
constexpr int   kRespawnDelay = 50;
constexpr int   kEndStateFrames = 150;
constexpr int   kDoorAnimFrames = 48;
constexpr int   kPlayerW = 28;
constexpr int   kPlayerH = 32;
constexpr int   kEnemyW = 32;
constexpr int   kEnemyH = 32;
constexpr int   kSpriteRenderW = 32;   // most sprites render at one-tile size
constexpr int   kSpriteRenderH = 32;

// Level layout: 14 rows × 100 cols. Symbols:
//   ' ' = empty / sky
//   'G' = ground tile (solid)
//   'B' = brick tile (solid)
//   'f' = fire tile (animated, hazard)
//   'w' = water tile (animated, hazard)
//   '*' = blue gem pickup    (50 pts)
//   'o' = sphere pickup      (150 pts)
//   '^' = red diamond pickup (300 pts)
//   'g' = gun pickup
//   'T' = trophy             (1000 pts, unlocks door)
//   'D' = door top-left      (2 tiles tall, only top tile is marked)
//   'E' = enemy spawn
//   'S' = player spawn
//
// Each row literal must be at most kLevelCols (100) chars; shorter rows are padded with spaces.
    static const char* kLevel1[kLevelRows] = {
        //   0         1         2         3         4         5         6         7         8         9
        //   0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
        "                                                                                                    ", // 0
        "                                                                                  ^                 ", // 1
        "                                                                                 BBB                ", // 2
        "                                                                        ^       B   B               ", // 3
        "             ^                                                         BBB     B     B       T      ", // 4
        "            BBB                 o                                     B   B   B       B     BBB     ", // 5
        "                               BBB             *                     B     B B         B            ", // 6
        "          *         g                         BBB             *     B       B           B           ", // 7
        "        BBBB       BBB                                       BBB                                    ", // 8
        "                          BBB        *                                               E          D   ", // 9
        "    o           E                   BBB             E    BB         o               BBB        BBB  ", // 10
        "   BBB         BBB                                 BBB             BBB                            B ", // 11
        "S       f f    BBB    w w w w w            ff             w w w           f f            f f      B ", // 12
        "GGGGGGGGGGGGGGGGGGwwwwwwwwwwwwwGGGGGGGGGGGGGGGGGGGGGGGGGwwwwwwwwwGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG", // 13
    };
inline float worldToScreenX(float wx, float camX) { return wx - camX; }
inline float worldToScreenY(float wy)              { return wy + kPlayfieldOffsetY; }

inline bool aabb(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

}  // namespace

Game::Game(SDL_Renderer* ren, const TextureRegistry& tex)
    : m_renderer(ren), m_tex(tex)
{
    resetLevel();
}

// ---------- Level construction ----------

void Game::buildLevelFromAscii()
{
    for (int row = 0; row < kLevelRows; ++row)
    {
        for (int col = 0; col < kLevelCols; ++col)
        {
            m_tiles[row][col] = TileKind::EMPTY;
        }
    }

    int startCol = 1;
    int startRow = 12;

    for (int row = 0; row < kLevelRows; ++row)
    {
        const char* line = kLevel1[row];
        const int len = static_cast<int>(std::strlen(line));
        for (int col = 0; col < kLevelCols; ++col)
        {
            const char c = (col < len) ? line[col] : ' ';
            switch (c)
            {
                case 'G': m_tiles[row][col] = TileKind::GROUND; break;
                case 'B': m_tiles[row][col] = TileKind::BRICK;  break;
                case 'f': m_tiles[row][col] = TileKind::FIRE;   break;
                case 'w': m_tiles[row][col] = TileKind::WATER;  break;

                case 'S': startCol = col; startRow = row; break;

                case '*': m_pickups.push_back(spawnPickup(col, row, PickupKind::GEM)); break;
                case 'o': m_pickups.push_back(spawnPickup(col, row, PickupKind::SPHERE)); break;
                case '^': m_pickups.push_back(spawnPickup(col, row, PickupKind::RED_DIAMOND)); break;
                case 'g': m_pickups.push_back(spawnPickup(col, row, PickupKind::GUN)); break;
                case 'T': {
                    bagel::ent_type t = spawnPickup(col, row, PickupKind::TROPHY);
                    m_trophy = t;
                    m_pickups.push_back(t);
                    break;
                }
                case 'D': m_door = spawnDoor(col, row); break;
                case 'E': m_enemies.push_back(spawnEnemy(col, row)); break;

                default: break;
            }
        }
    }

    spawnPlayer(startCol, startRow);
}

void Game::resetLevel()
{
    m_pickups.clear();
    m_enemies.clear();
    m_bullets.clear();
    m_trophy = bagel::ent_type{ {-1} };
    m_door   = bagel::ent_type{ {-1} };
    m_player = bagel::ent_type{ {-1} };

    m_camX = 0;
    m_animTick = 0;
    m_state = GameState::PLAYING;
    m_stateTimer = 0;
    m_prevJumpHeld = false;
    m_prevShoot = false;

    buildLevelFromAscii();
}

void Game::spawnPlayer(int col, int row)
{
    bagel::ent_type ent = bagel::World::createEntity();

    bagel::Storage<Position>::type::add(ent,  {(float)(col * kTileSize), (float)(row * kTileSize)});
    bagel::Storage<Velocity>::type::add(ent,  {0, 0});
    bagel::Storage<Box>::type::add(ent,       {(float)kPlayerW, (float)kPlayerH});

    Sprite spr;
    spr.tex = m_tex.daveStand;
    spr.z = 10;
    bagel::Storage<Sprite>::type::add(ent, spr);

    Anim a;
    a.frameCount = 0;
    a.delay = 6;
    a.loop = true;
    a.playing = false;
    bagel::Storage<Anim>::type::add(ent, a);

    PlayerData p;
    p.spawnCol = col;
    p.spawnRow = row;
    bagel::Storage<PlayerData>::type::add(ent, p);

    m_player = ent;
}

bagel::ent_type Game::spawnPickup(int col, int row, PickupKind kind)
{
    bagel::ent_type ent = bagel::World::createEntity();

    bagel::Storage<Position>::type::add(ent, {(float)(col * kTileSize), (float)(row * kTileSize)});
    bagel::Storage<Box>::type::add(ent,      {(float)kTileSize, (float)kTileSize});
    bagel::Storage<Velocity>::type::add(ent, {0, 0});

    Sprite spr;
    spr.z = 5;
    PickupData pd;
    pd.kind = kind;

    Anim a;
    a.frameCount = 0;
    a.delay = 8;
    a.playing = false;

    switch (kind)
    {
        case PickupKind::GEM:         spr.tex = m_tex.gem;        pd.score = 50;   break;
        case PickupKind::SPHERE:      spr.tex = m_tex.sphere;     pd.score = 150;  break;
        case PickupKind::RED_DIAMOND: spr.tex = m_tex.redDiamond; pd.score = 300;  break;
        case PickupKind::GUN:         spr.tex = m_tex.gun;        pd.score = 200;  break;
        case PickupKind::TROPHY:
            pd.score = 1000;
            a.frameCount = 5;
            for (int i = 0; i < 5; ++i) a.frames[i] = m_tex.cup[i];
            a.playing = true;
            spr.tex = a.frames[0] ? a.frames[0] : m_tex.cup[0];
            break;
    }

    bagel::Storage<Sprite>::type::add(ent, spr);
    bagel::Storage<Anim>::type::add(ent, a);
    bagel::Storage<PickupData>::type::add(ent, pd);
    return ent;
}

bagel::ent_type Game::spawnEnemy(int col, int row)
{
    bagel::ent_type ent = bagel::World::createEntity();

    bagel::Storage<Position>::type::add(ent, {(float)(col * kTileSize), (float)(row * kTileSize)});
    bagel::Storage<Box>::type::add(ent,      {(float)kEnemyW, (float)kEnemyH});
    bagel::Storage<Velocity>::type::add(ent, {0, 0});

    Sprite spr;
    spr.tex = m_tex.enemy;
    spr.z = 6;
    bagel::Storage<Sprite>::type::add(ent, spr);

    Anim a;
    a.frameCount = 0;
    a.playing = false;
    bagel::Storage<Anim>::type::add(ent, a);

    EnemyData ed;
    ed.patrolMin = (float)((col - 4) * kTileSize);
    ed.patrolMax = (float)((col + 4) * kTileSize);
    ed.speed = 1.4f;
    ed.dir = -1;
    bagel::Storage<EnemyData>::type::add(ent, ed);
    return ent;
}

bagel::ent_type Game::spawnDoor(int col, int row)
{
    bagel::ent_type ent = bagel::World::createEntity();

    bagel::Storage<Position>::type::add(ent, {(float)(col * kTileSize), (float)(row * kTileSize)});
    bagel::Storage<Box>::type::add(ent,      {(float)kTileSize, (float)(kTileSize * 2)});
    bagel::Storage<Velocity>::type::add(ent, {0, 0});

    Sprite spr;
    spr.tex = m_tex.doorClosed;
    spr.z = 8;
    bagel::Storage<Sprite>::type::add(ent, spr);

    Anim a;
    a.frameCount = 0;
    a.playing = false;
    bagel::Storage<Anim>::type::add(ent, a);

    DoorData d;
    d.closedTex = m_tex.doorClosed;
    d.openTex   = m_tex.doorOpen ? m_tex.doorOpen : m_tex.doorClosed;
    bagel::Storage<DoorData>::type::add(ent, d);
    return ent;
}

bagel::ent_type Game::spawnBullet(float x, float y, int dir, bool fromPlayer)
{
    bagel::ent_type ent = bagel::World::createEntity();

    bagel::Storage<Position>::type::add(ent, {x, y});
    bagel::Storage<Box>::type::add(ent,      {12.0f, 8.0f});
    bagel::Storage<Velocity>::type::add(ent, {kBulletSpeed * dir, 0});

    Sprite spr;
    spr.tex = fromPlayer ? m_tex.bullet : m_tex.monsterBullet;
    spr.z = 12;
    spr.flipH = (dir < 0);
    bagel::Storage<Sprite>::type::add(ent, spr);

    Anim a;
    a.frameCount = 0;
    a.playing = false;
    bagel::Storage<Anim>::type::add(ent, a);

    BulletData b;
    b.dir = dir;
    b.ttl = kBulletTTL;
    b.fromPlayer = fromPlayer;
    bagel::Storage<BulletData>::type::add(ent, b);

    m_bullets.push_back(ent);
    return ent;
}

// ---------- Tile queries ----------

TileKind Game::tileAt(int col, int row) const
{
    if (col < 0 || col >= kLevelCols || row < 0 || row >= kLevelRows) return TileKind::EMPTY;
    return m_tiles[row][col];
}

bool Game::tileSolid(int col, int row) const
{
    const TileKind k = tileAt(col, row);
    return k == TileKind::GROUND || k == TileKind::BRICK;
}

bool Game::tileHazard(int col, int row) const
{
    const TileKind k = tileAt(col, row);
    return k == TileKind::FIRE || k == TileKind::WATER;
}

// ---------- Input ----------

void Game::readInput()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_EVENT_QUIT) m_running = false;
    }

    bool left = false, right = false, jump = false, shoot = false, exitKey = false;
    bool jumpEdge = false, shootEdge = false;
#ifdef _WIN32
    // GetAsyncKeyState: high bit 0x8000 = currently held; low bit 0x0001 = pressed since
    // last call. Including the low bit catches taps that occurred between frame polls,
    // so rapid jump-spam while moving doesn't silently drop inputs.
    auto poll = [](int vk, bool& held, bool& edge) {
        const SHORT s = GetAsyncKeyState(vk);
        if (s & 0x8000) held = true;
        if (s & 0x0001) edge = true;
    };
    bool _drop = false;

    poll(VK_LEFT,    left,  _drop); _drop = false;
    poll('A',        left,  _drop); _drop = false;
    poll(VK_RIGHT,   right, _drop); _drop = false;
    poll('D',        right, _drop); _drop = false;

    poll(VK_SPACE,   jump,  jumpEdge);
    poll(VK_UP,      jump,  jumpEdge);
    poll('W',        jump,  jumpEdge);

    poll(VK_CONTROL, shoot, shootEdge);
    poll('S',        shoot, shootEdge);
    poll('F',        shoot, shootEdge);

    exitKey = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
#endif

    m_input.left  = left;
    m_input.right = right;
    m_input.jumpHeld    = jump;
    m_input.jumpPressed = jumpEdge || (jump && !m_prevJumpHeld);
    m_input.shoot       = shoot;
    m_input.shootPressed = shootEdge || (shoot && !m_prevShoot);
    m_input.exit  = exitKey;
    m_prevJumpHeld = jump;
    m_prevShoot    = shoot;
}

// ---------- Collision resolution against tile grid ----------

void Game::resolveTileCollisions(Position& pos, Velocity& vel, const Box& box, bool& grounded, bool& diedToHazard)
{
    grounded = false;
    diedToHazard = false;

    // Horizontal sweep
    pos.x += vel.vx;

    // Clamp to world bounds
    if (pos.x < 0) { pos.x = 0; vel.vx = 0; }
    if (pos.x + box.w > kWorldW) { pos.x = kWorldW - box.w; vel.vx = 0; }

    {
        const int colA = (int)std::floor(pos.x / kTileSize);
        const int colB = (int)std::floor((pos.x + box.w - 1) / kTileSize);
        const int rowA = (int)std::floor(pos.y / kTileSize);
        const int rowB = (int)std::floor((pos.y + box.h - 1) / kTileSize);

        for (int r = rowA; r <= rowB; ++r)
        {
            for (int c = colA; c <= colB; ++c)
            {
                if (tileSolid(c, r))
                {
                    if (vel.vx > 0)      pos.x = (float)(c * kTileSize) - box.w;
                    else if (vel.vx < 0) pos.x = (float)((c + 1) * kTileSize);
                    vel.vx = 0;
                }
                else if (tileHazard(c, r))
                {
                    diedToHazard = true;
                }
            }
        }
    }

    // Vertical sweep
    pos.y += vel.vy;

    {
        const int colA = (int)std::floor(pos.x / kTileSize);
        const int colB = (int)std::floor((pos.x + box.w - 1) / kTileSize);
        const int rowA = (int)std::floor(pos.y / kTileSize);
        const int rowB = (int)std::floor((pos.y + box.h - 1) / kTileSize);

        for (int r = rowA; r <= rowB; ++r)
        {
            for (int c = colA; c <= colB; ++c)
            {
                if (tileSolid(c, r))
                {
                    if (vel.vy > 0)
                    {
                        pos.y = (float)(r * kTileSize) - box.h;
                        grounded = true;
                    }
                    else if (vel.vy < 0)
                    {
                        pos.y = (float)((r + 1) * kTileSize);
                    }
                    vel.vy = 0;
                }
                else if (tileHazard(c, r))
                {
                    diedToHazard = true;
                }
            }
        }
    }

    // Falling out the bottom of the world also kills.
    if (pos.y > kWorldH + 64) diedToHazard = true;
}

// ---------- Per-frame updates ----------

void Game::updatePlayer()
{
    auto& pos  = bagel::Storage<Position>::type::get(m_player);
    auto& vel  = bagel::Storage<Velocity>::type::get(m_player);
    auto& box  = bagel::Storage<Box>::type::get(m_player);
    auto& spr  = bagel::Storage<Sprite>::type::get(m_player);
    auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);

    vel.vx = 0;
    if (m_input.left)  { vel.vx -= kMoveSpeed; pdat.facing = -1; pdat.moving = true; }
    if (m_input.right) { vel.vx += kMoveSpeed; pdat.facing =  1; pdat.moving = true; }
    if (!m_input.left && !m_input.right) pdat.moving = false;

    vel.vy += kGravity;
    if (vel.vy > kMaxFallSpeed) vel.vy = kMaxFallSpeed;

    // Jump-buffer: a fresh edge arms a 4-frame window. The jump fires the moment
    // Dave is grounded within that window — fixes "I pressed jump just before landing
    // and nothing happened" and makes rapid tap-spam reliably register.
    if (m_input.jumpPressed) m_jumpBuffer = 4;

    if (m_jumpBuffer > 0 && pdat.grounded)
    {
        vel.vy = kJumpVel;
        pdat.grounded = false;
        m_jumpBuffer = 0;
    }
    else if (m_jumpBuffer > 0)
    {
        --m_jumpBuffer;
    }

    if (m_input.shootPressed && pdat.hasGun && pdat.ammo > 0)
    {
        // count active player bullets
        int active = 0;
        for (auto b : m_bullets)
        {
            const auto& bd = bagel::Storage<BulletData>::type::get(b);
            if (bd.alive && bd.fromPlayer) ++active;
        }
        if (active < kMaxPlayerBullets)
        {
            const float bx = pdat.facing > 0 ? pos.x + box.w : pos.x - 12.0f;
            const float by = pos.y + box.h * 0.4f;
            spawnBullet(bx, by, pdat.facing, true);
            --pdat.ammo;
        }
    }

    bool died = false;
    resolveTileCollisions(pos, vel, box, pdat.grounded, died);

    if (died) { killPlayer(); return; }

    // Sprite selection
    if (!pdat.grounded)
    {
        spr.tex = m_tex.daveJump ? m_tex.daveJump : m_tex.daveStand;
    }
    else if (pdat.moving)
    {
        ++pdat.walkAnimTimer;
        if (pdat.walkAnimTimer >= 6)
        {
            pdat.walkAnimTimer = 0;
            pdat.walkAnimIdx = (pdat.walkAnimIdx + 1) % 3;
        }
        SDL_Texture* tex = m_tex.daveWalk[pdat.walkAnimIdx];
        spr.tex = tex ? tex : m_tex.daveStand;
    }
    else
    {
        pdat.walkAnimTimer = 0;
        pdat.walkAnimIdx = 0;
        spr.tex = m_tex.daveStand;
    }
    spr.flipH = (pdat.facing < 0);
}

void Game::updateEnemies()
{
    for (auto e : m_enemies)
    {
        auto& ed = bagel::Storage<EnemyData>::type::get(e);
        if (!ed.alive) continue;

        auto& pos = bagel::Storage<Position>::type::get(e);
        auto& vel = bagel::Storage<Velocity>::type::get(e);
        auto& box = bagel::Storage<Box>::type::get(e);
        auto& spr = bagel::Storage<Sprite>::type::get(e);

        const float desired = ed.speed * ed.dir;
        vel.vx = desired;
        vel.vy += kGravity;
        if (vel.vy > kMaxFallSpeed) vel.vy = kMaxFallSpeed;

        bool grounded = false;
        bool died = false;  // ignored — enemy doesn't die to hazards
        resolveTileCollisions(pos, vel, box, grounded, died);
        (void)died;
        (void)grounded;

        // Patrol bounds
        if (pos.x < ed.patrolMin) { pos.x = ed.patrolMin; ed.dir = 1; }
        if (pos.x + box.w > ed.patrolMax) { pos.x = ed.patrolMax - box.w; ed.dir = -1; }

        // Wall hit zeroed our intended velocity — flip direction.
        if (desired != 0.0f && vel.vx == 0.0f) ed.dir = -ed.dir;

        spr.flipH = (ed.dir < 0);

        // Optional shoot at player
        if (ed.shootCooldown > 0) --ed.shootCooldown;
    }
}

void Game::updateBullets()
{
    for (auto b : m_bullets)
    {
        auto& bd = bagel::Storage<BulletData>::type::get(b);
        if (!bd.alive) continue;

        auto& pos = bagel::Storage<Position>::type::get(b);
        auto& vel = bagel::Storage<Velocity>::type::get(b);
        auto& box = bagel::Storage<Box>::type::get(b);

        pos.x += vel.vx;
        pos.y += vel.vy;
        --bd.ttl;

        if (bd.ttl <= 0 || pos.x < -32 || pos.x > kWorldW + 32)
        {
            bd.alive = false;
            bagel::Storage<Sprite>::type::get(b).visible = false;
            continue;
        }

        // Hit solid tile
        const int col = (int)std::floor((pos.x + box.w * 0.5f) / kTileSize);
        const int row = (int)std::floor((pos.y + box.h * 0.5f) / kTileSize);
        if (tileSolid(col, row))
        {
            bd.alive = false;
            bagel::Storage<Sprite>::type::get(b).visible = false;
            continue;
        }

        // Hit enemy
        if (bd.fromPlayer)
        {
            for (auto e : m_enemies)
            {
                auto& ed = bagel::Storage<EnemyData>::type::get(e);
                if (!ed.alive) continue;
                const auto& ep = bagel::Storage<Position>::type::get(e);
                const auto& eb = bagel::Storage<Box>::type::get(e);
                if (aabb(pos.x, pos.y, box.w, box.h, ep.x, ep.y, eb.w, eb.h))
                {
                    ed.alive = false;
                    bagel::Storage<Sprite>::type::get(e).visible = false;
                    bd.alive = false;
                    bagel::Storage<Sprite>::type::get(b).visible = false;
                    auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
                    pdat.score += 100;
                    break;
                }
            }
        }
    }

    // Compact bullet list periodically
    m_bullets.erase(
        std::remove_if(m_bullets.begin(), m_bullets.end(),
            [](bagel::ent_type b) {
                return !bagel::Storage<BulletData>::type::get(b).alive;
            }),
        m_bullets.end());
}

void Game::handlePickups()
{
    auto& ppos = bagel::Storage<Position>::type::get(m_player);
    auto& pbox = bagel::Storage<Box>::type::get(m_player);
    auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);

    for (auto pu : m_pickups)
    {
        auto& pd = bagel::Storage<PickupData>::type::get(pu);
        if (pd.collected) continue;

        const auto& pos = bagel::Storage<Position>::type::get(pu);
        const auto& box = bagel::Storage<Box>::type::get(pu);

        if (aabb(ppos.x, ppos.y, pbox.w, pbox.h, pos.x, pos.y, box.w, box.h))
        {
            pd.collected = true;
            bagel::Storage<Sprite>::type::get(pu).visible = false;
            pdat.score += pd.score;

            switch (pd.kind)
            {
                case PickupKind::GUN:
                    pdat.hasGun = true;
                    pdat.ammo += 6;
                    break;
                case PickupKind::TROPHY: {
                    pdat.hasTrophy = true;
                    if (m_door.id >= 0)
                    {
                        auto& dd = bagel::Storage<DoorData>::type::get(m_door);
                        dd.animating = true;
                        dd.animFrames = kDoorAnimFrames;
                    }
                    break;
                }
                default: break;
            }
        }
    }
}

void Game::checkHazards()
{
    auto& ppos = bagel::Storage<Position>::type::get(m_player);
    auto& pbox = bagel::Storage<Box>::type::get(m_player);

    // Hazard tiles already checked in resolveTileCollisions; here we test enemy contact.
    for (auto e : m_enemies)
    {
        const auto& ed = bagel::Storage<EnemyData>::type::get(e);
        if (!ed.alive) continue;
        const auto& pos = bagel::Storage<Position>::type::get(e);
        const auto& box = bagel::Storage<Box>::type::get(e);
        if (aabb(ppos.x, ppos.y, pbox.w, pbox.h, pos.x, pos.y, box.w, box.h))
        {
            killPlayer();
            return;
        }
    }
}

void Game::checkDoor()
{
    if (m_door.id < 0) return;

    auto& dd = bagel::Storage<DoorData>::type::get(m_door);
    auto& spr = bagel::Storage<Sprite>::type::get(m_door);

    if (dd.animating)
    {
        --dd.animFrames;
        if (dd.animFrames <= 0)
        {
            dd.animating = false;
            dd.unlocked = true;
            spr.tex = dd.openTex ? dd.openTex : dd.closedTex;
        }
        return;
    }

    if (!dd.unlocked) return;

    auto& ppos = bagel::Storage<Position>::type::get(m_player);
    auto& pbox = bagel::Storage<Box>::type::get(m_player);
    const auto& dpos = bagel::Storage<Position>::type::get(m_door);
    const auto& dbox = bagel::Storage<Box>::type::get(m_door);

    if (aabb(ppos.x, ppos.y, pbox.w, pbox.h, dpos.x, dpos.y, dbox.w, dbox.h))
    {
        m_state = GameState::LEVEL_COMPLETE;
        m_stateTimer = 0;
    }
}

void Game::updateAnimations()
{
    ++m_animTick;
    // Animate trophy frames
    if (m_trophy.id >= 0)
    {
        auto& a = bagel::Storage<Anim>::type::get(m_trophy);
        if (a.playing && a.frameCount > 0)
        {
            ++a.counter;
            if (a.counter >= a.delay)
            {
                a.counter = 0;
                a.idx = (a.idx + 1) % a.frameCount;
                auto& spr = bagel::Storage<Sprite>::type::get(m_trophy);
                if (a.frames[a.idx]) spr.tex = a.frames[a.idx];
            }
        }
    }
}

void Game::killPlayer()
{
    auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
    if (m_state != GameState::PLAYING) return;
    pdat.lives -= 1;
    if (pdat.lives <= 0)
    {
        m_state = GameState::GAME_OVER;
        m_stateTimer = 0;
        return;
    }
    m_state = GameState::DEAD;
    m_stateTimer = 0;
}

void Game::cameraUpdate()
{
    const auto& pos = bagel::Storage<Position>::type::get(m_player);
    const auto& box = bagel::Storage<Box>::type::get(m_player);
    const float center = pos.x + box.w * 0.5f;
    float target = center - kScreenW * 0.5f;
    if (target < 0) target = 0;
    if (target > kWorldW - kScreenW) target = (float)(kWorldW - kScreenW);
    m_camX = target;
}

void Game::update()
{
    updateAnimations();

    if (m_state == GameState::PLAYING)
    {
        updatePlayer();
        if (m_state != GameState::PLAYING) return;
        updateEnemies();
        updateBullets();
        handlePickups();
        checkHazards();
        checkDoor();
        cameraUpdate();
    }
    else if (m_state == GameState::DEAD)
    {
        ++m_stateTimer;
        if (m_stateTimer >= kRespawnDelay)
        {
            auto& pos = bagel::Storage<Position>::type::get(m_player);
            auto& vel = bagel::Storage<Velocity>::type::get(m_player);
            auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
            pos.x = (float)(pdat.spawnCol * kTileSize);
            pos.y = (float)(pdat.spawnRow * kTileSize);
            vel.vx = vel.vy = 0;
            pdat.grounded = false;
            m_state = GameState::PLAYING;
            cameraUpdate();
        }
    }
    else  // LEVEL_COMPLETE or GAME_OVER
    {
        ++m_stateTimer;
        if (m_stateTimer >= kEndStateFrames) m_running = false;
    }
}

// ---------- Rendering ----------

void Game::renderBackground()
{
    // Plain solid sky — no parallax, no background texture.
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
}

void Game::renderTiles()
{
    const int colStart = std::max(0, (int)std::floor(m_camX / kTileSize) - 1);
    const int colEnd   = std::min(kLevelCols, colStart + (kScreenW / kTileSize) + 3);

    const int fireFrame  = (m_animTick / 8) % 4;
    const int waterFrame = (m_animTick / 10) % 4;

    for (int row = 0; row < kLevelRows; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const TileKind k = m_tiles[row][col];
            if (k == TileKind::EMPTY) continue;

            SDL_Texture* tex = nullptr;
            switch (k)
            {
                case TileKind::GROUND: tex = m_tex.ground; break;
                case TileKind::BRICK:  tex = m_tex.brick;  break;
                case TileKind::FIRE:   tex = m_tex.fire[fireFrame]; break;
                case TileKind::WATER:  tex = m_tex.water[waterFrame]; break;
                default: break;
            }
            if (!tex) continue;

            SDL_FRect dst{
                worldToScreenX((float)(col * kTileSize), m_camX),
                worldToScreenY((float)(row * kTileSize)),
                (float)kTileSize, (float)kTileSize
            };
            SDL_RenderTexture(m_renderer, tex, nullptr, &dst);
        }
    }
}

void Game::renderEntity(bagel::ent_type ent)
{
    if (ent.id < 0) return;
    const auto& spr = bagel::Storage<Sprite>::type::get(ent);
    if (!spr.visible || !spr.tex) return;
    const auto& pos = bagel::Storage<Position>::type::get(ent);
    const auto& box = bagel::Storage<Box>::type::get(ent);

    SDL_FRect dst{
        worldToScreenX(pos.x, m_camX),
        worldToScreenY(pos.y),
        box.w, box.h
    };

    if (spr.flipH)
    {
        SDL_RenderTextureRotated(m_renderer, spr.tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
    }
    else
    {
        SDL_RenderTexture(m_renderer, spr.tex, nullptr, &dst);
    }
}

namespace {
void drawDigit(SDL_Renderer* ren, SDL_Texture* tex, float x, float y, float w, float h)
{
    SDL_FRect dst{ x, y, w, h };
    if (tex) SDL_RenderTexture(ren, tex, nullptr, &dst);
}

void drawNumber(SDL_Renderer* ren, const std::array<SDL_Texture*, 10>& digits, int value, int minDigits, float x, float y, float dw, float dh)
{
    if (value < 0) value = 0;
    std::string s = std::to_string(value);
    while ((int)s.size() < minDigits) s = "0" + s;

    float cx = x;
    bool anyDigit = false;
    for (char c : s)
    {
        const int d = c - '0';
        if (d >= 0 && d <= 9 && digits[d])
        {
            drawDigit(ren, digits[d], cx, y, dw, dh);
            anyDigit = true;
        }
        cx += dw + 1.0f;
    }
    if (!anyDigit)
    {
        SDL_RenderDebugText(ren, x, y + 4.0f, s.c_str());
    }
}
}  // namespace

void Game::renderHud()
{
    // HUD strip background
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_FRect strip{ 0, 0, (float)kScreenW, (float)kHudH };
    SDL_RenderFillRect(m_renderer, &strip);

    // Bottom border line on HUD
    SDL_SetRenderDrawColor(m_renderer, 200, 60, 0, 255);
    SDL_FRect line{ 0, (float)kHudH - 2.0f, (float)kScreenW, 2.0f };
    SDL_RenderFillRect(m_renderer, &line);

    const auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);

    // Layout helpers
    const float labelW = 60.0f, labelH = 14.0f;
    const float digitW = 20.0f, digitH = 24.0f;

    float x = 8.0f;
    if (m_tex.labelScore)
    {
        SDL_FRect dst{ x, 8.0f, labelW, labelH };
        SDL_RenderTexture(m_renderer, m_tex.labelScore, nullptr, &dst);
        x += labelW + 4.0f;
    }
    else
    {
        SDL_RenderDebugText(m_renderer, x, 12.0f, "SCORE");
        x += 56.0f;
    }
    drawNumber(m_renderer, m_tex.digit, pdat.score, 5, x, 6.0f, digitW, digitH);
    x += digitW * 5 + 30.0f;

    if (m_tex.labelLevel)
    {
        SDL_FRect dst{ x, 8.0f, labelW, labelH };
        SDL_RenderTexture(m_renderer, m_tex.labelLevel, nullptr, &dst);
        x += labelW + 4.0f;
    }
    else
    {
        SDL_RenderDebugText(m_renderer, x, 12.0f, "LEVEL");
        x += 56.0f;
    }
    drawNumber(m_renderer, m_tex.digit, m_levelNumber, 2, x, 6.0f, digitW, digitH);
    x += digitW * 2 + 30.0f;

    if (m_tex.labelLives)
    {
        SDL_FRect dst{ x, 8.0f, labelW, labelH };
        SDL_RenderTexture(m_renderer, m_tex.labelLives, nullptr, &dst);
        x += labelW + 4.0f;
    }
    else
    {
        SDL_RenderDebugText(m_renderer, x, 12.0f, "DAVES");
        x += 56.0f;
    }
    drawNumber(m_renderer, m_tex.digit, std::max(0, pdat.lives), 1, x, 6.0f, digitW, digitH);
    x += digitW + 30.0f;

    // Trophy / gun indicators
    if (pdat.hasTrophy && m_tex.cup[0])
    {
        SDL_FRect dst{ (float)kScreenW - 80.0f, 6.0f, 28.0f, 28.0f };
        SDL_RenderTexture(m_renderer, m_tex.cup[(m_animTick / 6) % 5], nullptr, &dst);
    }
    if (pdat.hasGun)
    {
        SDL_FRect dst{ (float)kScreenW - 44.0f, 6.0f, 36.0f, 28.0f };
        SDL_RenderTexture(m_renderer, m_tex.gun ? m_tex.gun : m_tex.bullet, nullptr, &dst);
        std::string ammo = "x" + std::to_string(pdat.ammo);
        SDL_RenderDebugText(m_renderer, (float)kScreenW - 44.0f, 32.0f, ammo.c_str());
    }
}

void Game::renderEndBanner()
{
    if (m_state != GameState::LEVEL_COMPLETE && m_state != GameState::GAME_OVER) return;
    const char* msg = (m_state == GameState::LEVEL_COMPLETE) ? "STAGE CLEARED" : "GAME OVER";

    // Dim overlay
    SDL_SetRenderDrawBlendMode(m_renderer, 1 /* SDL_BLENDMODE_BLEND */);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
    SDL_FRect overlay{ 0, 0, (float)kScreenW, (float)kScreenH };
    SDL_RenderFillRect(m_renderer, &overlay);

    // Text rendered with debug font, scaled visually by drawing several offsets.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
            SDL_RenderDebugText(m_renderer, kScreenW/2 - 60 + dx, kScreenH/2 - 4 + dy, msg);
        }
    SDL_SetRenderDrawColor(m_renderer, 255, 220, 80, 255);
    SDL_RenderDebugText(m_renderer, kScreenW/2 - 60, kScreenH/2 - 4, msg);
}

void Game::render()
{
    renderBackground();
    renderTiles();

    // Pickups
    for (auto pu : m_pickups) renderEntity(pu);
    // Door
    renderEntity(m_door);
    // Enemies
    for (auto e : m_enemies) renderEntity(e);
    // Bullets
    for (auto b : m_bullets) renderEntity(b);
    // Player
    renderEntity(m_player);

    renderHud();
    renderEndBanner();

    SDL_RenderPresent(m_renderer);
}

// ---------- Main loop ----------

void Game::run()
{
    const std::uint64_t frameTargetMs = 16;
    while (m_running)
    {
        const std::uint64_t frameStart = SDL_GetTicks();

        readInput();
        if (m_input.exit) m_running = false;

        update();
        render();

        const std::uint64_t elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < frameTargetMs)
        {
            SDL_Delay((std::uint32_t)(frameTargetMs - elapsed));
        }
    }

    if (m_state == GameState::LEVEL_COMPLETE) std::cout << "Stage cleared.\n";
    else if (m_state == GameState::GAME_OVER) std::cout << "Game over.\n";
}

}  // namespace dave
