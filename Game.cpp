/**
 * @file Game.cpp
 * @brief Systems, spawn factories, and rendering.
 */

#include "Game.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace dave {

namespace {

constexpr int   kBulletTTL        = 80;
constexpr int   kMaxPlayerBullets = 1;
constexpr int   kRespawnDelay     = 50;
constexpr int   kEndStateFrames   = 150;
constexpr int   kDoorAnimFrames   = 48;
constexpr int   kPlayerW = 28;
constexpr int   kPlayerH = 32;
constexpr int   kEnemyW  = 32;
constexpr int   kEnemyH  = 32;

// Level layout: 14 rows x 100 cols. Symbols:
//   ' ' empty   'G' ground   'B' brick   'f' fire   'w' water
//   '*' gem (50)   'o' sphere (150)   '^' red diamond (300)
//   'g' gun   'T' trophy (1000, unlocks door)   'D' door (top tile of a 2-tall pair)
//   'E' ground enemy   'F' flying enemy   'S' player spawn
// Rows shorter than kLevelCols are padded with spaces.
    static const char* kLevel1[kLevelRows] = {
        //   0         1         2         3         4         5         6         7         8         9
        //   0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
        "                                                                                                    ", // 0
        "                                    ^                                             ^                 ", // 1
        "                                   BBB                                           BBB                ", // 2
        "                          *                                  o          ^       B   B               ", // 3
        "             ^             F                  *             BBB        BBB     B     B       T      ", // 4
        "            BBB                 o            BBB                     B   B   B       B     BBB     ", // 5
        "                               BBB             *                F    B     B B         B            ", // 6
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
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.f, BOX_GRAVITY};
    m_world = b2CreateWorld(&worldDef);

    resetLevel();
}

Game::~Game()
{
    if (b2World_IsValid(m_world))
        b2DestroyWorld(m_world);
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

                case '*': spawnPickup(col, row, PickupKind::GEM); break;
                case 'o': spawnPickup(col, row, PickupKind::SPHERE); break;
                case '^': spawnPickup(col, row, PickupKind::RED_DIAMOND); break;
                case 'g': spawnPickup(col, row, PickupKind::GUN); break;
                case 'T': m_trophy = spawnPickup(col, row, PickupKind::TROPHY); break;
                case 'D': m_door = spawnDoor(col, row); break;
                case 'E': spawnEnemy(col, row, EnemyKind::GROUND); break;
                case 'F': spawnEnemy(col, row, EnemyKind::FLYING); break;

                default: break;
            }
        }
    }

    buildStaticTileBodies();

    spawnPlayer(startCol, startRow);
}

void Game::buildStaticTileBodies()
{
    // Static body per solid tile (GROUND/BRICK); static sensor body per hazard tile (FIRE/WATER).
    // Hazard sensor events are handled by sensor_event_system.
    const float half = (kTileSize * 0.5f) / BOX_SCALE;
    for (int row = 0; row < kLevelRows; ++row)
    {
        for (int col = 0; col < kLevelCols; ++col)
        {
            const TileKind k = m_tiles[row][col];
            if (k == TileKind::EMPTY) continue;

            b2BodyDef bd = b2DefaultBodyDef();
            bd.type = b2_staticBody;
            bd.position = {
                ((col * kTileSize) + kTileSize * 0.5f) / BOX_SCALE,
                ((row * kTileSize) + kTileSize * 0.5f) / BOX_SCALE
            };
            // User data null = tile hazard sensor.
            b2BodyId body = b2CreateBody(m_world, &bd);

            b2ShapeDef sd = b2DefaultShapeDef();
            sd.material.friction = 0.0f;
            b2Polygon poly = b2MakeBox(half, half);

            if (k == TileKind::GROUND || k == TileKind::BRICK) {
                sd.filter.categoryBits = CAT_TILE;
                sd.filter.maskBits = CAT_PLAYER | CAT_ENEMY;
                b2CreatePolygonShape(body, &sd, &poly);
            } else {
                // Hazard tile: sensor shape.
                sd.isSensor = true;
                sd.enableSensorEvents = true;
                sd.filter.categoryBits = CAT_SENSOR;
                sd.filter.maskBits = CAT_PLAYER;
                b2CreatePolygonShape(body, &sd, &poly);
            }
        }
    }
}

void Game::resetLevel()
{
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
    Position pos{(float)(col * kTileSize), (float)(row * kTileSize)};
    Box      box{(float)kPlayerW, (float)kPlayerH};

    Sprite spr;
    spr.tex = m_tex.daveStand;
    spr.z = 10;

    Anim a;
    a.delay = 6;
    a.loop = true;
    a.playing = false;

    PlayerData p;
    p.spawnCol = col;
    p.spawnRow = row;

    // Dynamic body, locked rotation, zero friction (slide on walls).
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = { (pos.x + box.w * 0.5f) / BOX_SCALE,
                    (pos.y + box.h * 0.5f) / BOX_SCALE };
    bd.motionLocks.angularZ = true;
    b2BodyId body = b2CreateBody(m_world, &bd);
    b2Body_SetUserData(body, reinterpret_cast<void*>(UDATA_PLAYER));

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = 0.0f;
    sd.material.restitution = 0.0f;
    sd.enableSensorEvents = true;  // Required for hazard/pickup sensor events.
    sd.filter.categoryBits = CAT_PLAYER;
    sd.filter.maskBits = CAT_TILE | CAT_SENSOR;
    // 2 px pad avoids corner-seam catching on grid-aligned tile bodies.
    constexpr float kCollisionPad = 2.0f;
    b2Polygon poly = b2MakeBox(
        (box.w * 0.5f - kCollisionPad) / BOX_SCALE,
        (box.h * 0.5f - kCollisionPad) / BOX_SCALE);
    b2CreatePolygonShape(body, &sd, &poly);

    Intent intent;
    Keys keys{ SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
               SDL_SCANCODE_SPACE, SDL_SCANCODE_UP,   // Jump + alt jump.
               SDL_SCANCODE_LCTRL };

    bagel::Entity e = bagel::Entity::create();
    e.addAll(pos, box, spr, a, p, Collider{body}, intent, keys);
    m_player = e.entity();
}

bagel::ent_type Game::spawnPickup(int col, int row, PickupKind kind)
{
    Position pos{(float)(col * kTileSize), (float)(row * kTileSize)};
    Box      box{(float)kTileSize, (float)kTileSize};

    Sprite spr;
    spr.z = 5;
    PickupData pd;
    pd.kind = kind;

    Anim a;
    a.delay = 8;

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

    bagel::Entity e = bagel::Entity::create();
    e.addAll(pos, box, spr, a, pd);

    // Sensor body for the pickup-collection event.
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_staticBody;
    bd.position = { (pos.x + box.w * 0.5f) / BOX_SCALE,
                    (pos.y + box.h * 0.5f) / BOX_SCALE };
    b2BodyId body = b2CreateBody(m_world, &bd);
    b2Body_SetUserData(body, reinterpret_cast<void*>(UDATA_ENTITY_BASE + e.entity().id));

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.isSensor = true;
    sd.enableSensorEvents = true;
    sd.filter.categoryBits = CAT_SENSOR;
    sd.filter.maskBits = CAT_PLAYER;
    b2Polygon poly = b2MakeBox((box.w * 0.5f) / BOX_SCALE, (box.h * 0.5f) / BOX_SCALE);
    b2CreatePolygonShape(body, &sd, &poly);

    e.add(Collider{body});
    return e.entity();
}

bagel::ent_type Game::spawnEnemy(int col, int row, EnemyKind kind)
{
    Position pos{(float)(col * kTileSize), (float)(row * kTileSize)};
    Box      box{(float)kEnemyW, (float)kEnemyH};

    Sprite spr;
    spr.tex = m_tex.enemy;
    spr.z = 6;
    if (kind == EnemyKind::FLYING) {
        // Red tint to distinguish flyers (no separate sprite asset).
        spr.tintR = 255; spr.tintG = 80; spr.tintB = 80;
    }

    Anim a;

    EnemyData ed;
    ed.kind = kind;
    ed.patrolMin = (float)((col - 4) * kTileSize);
    ed.patrolMax = (float)((col + 4) * kTileSize);
    ed.speed = (kind == EnemyKind::FLYING) ? 2.0f : 1.4f;
    ed.dir = -1;
    ed.phase = (float)(col + row);     // Per-spawn phase so flyers desync.

    // Ground: dynamic body (gravity pulls them to the floor).
    // Flying: kinematic body (no gravity; we set velocity each frame).
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = (kind == EnemyKind::FLYING) ? b2_kinematicBody : b2_dynamicBody;
    bd.position = { (pos.x + box.w * 0.5f) / BOX_SCALE,
                    (pos.y + box.h * 0.5f) / BOX_SCALE };
    bd.motionLocks.angularZ = true;
    b2BodyId body = b2CreateBody(m_world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 1.0f;
    sd.material.friction = 0.0f;
    sd.filter.categoryBits = CAT_ENEMY;
    sd.filter.maskBits = (kind == EnemyKind::FLYING) ? 0 : CAT_TILE;  // Flyers ignore tiles.
    constexpr float kCollisionPad = 2.0f;
    b2Polygon poly = b2MakeBox(
        (box.w * 0.5f - kCollisionPad) / BOX_SCALE,
        (box.h * 0.5f - kCollisionPad) / BOX_SCALE);
    b2CreatePolygonShape(body, &sd, &poly);

    bagel::Entity e = bagel::Entity::create();
    e.addAll(pos, box, spr, a, ed, Collider{body});
    return e.entity();
}

bagel::ent_type Game::spawnDoor(int col, int row)
{
    Position pos{(float)(col * kTileSize), (float)(row * kTileSize)};
    Box      box{(float)kTileSize, (float)(kTileSize * 2)};

    Sprite spr;
    spr.tex = m_tex.doorClosed;
    spr.z = 8;

    Anim a;

    DoorData d;
    d.closedTex = m_tex.doorClosed;
    d.openTex   = m_tex.doorOpen ? m_tex.doorOpen : m_tex.doorClosed;

    bagel::Entity e = bagel::Entity::create();
    e.addAll(pos, box, spr, a, d);

    // Door-entry sensor. sensor_event_system checks DoorData::unlocked before completing the level.
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_staticBody;
    bd.position = { (pos.x + box.w * 0.5f) / BOX_SCALE,
                    (pos.y + box.h * 0.5f) / BOX_SCALE };
    b2BodyId body = b2CreateBody(m_world, &bd);
    b2Body_SetUserData(body, reinterpret_cast<void*>(UDATA_ENTITY_BASE + e.entity().id));

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.isSensor = true;
    sd.enableSensorEvents = true;
    sd.filter.categoryBits = CAT_SENSOR;
    sd.filter.maskBits = CAT_PLAYER;
    b2Polygon poly = b2MakeBox((box.w * 0.5f) / BOX_SCALE, (box.h * 0.5f) / BOX_SCALE);
    b2CreatePolygonShape(body, &sd, &poly);

    e.add(Collider{body});
    return e.entity();
}

bagel::ent_type Game::spawnBullet(float x, float y, int dir, bool fromPlayer)
{
    Position pos{x, y};
    Box      box{12.0f, 8.0f};

    Sprite spr;
    spr.tex = fromPlayer ? m_tex.bullet : m_tex.monsterBullet;
    spr.z = 12;
    spr.flipH = (dir < 0);

    Anim a;

    BulletData b;
    b.dir = dir;
    b.ttl = kBulletTTL;
    b.fromPlayer = fromPlayer;

    // Kinematic body: straight-line motion, no gravity, no physical collisions (mask = 0).
    // Tile and enemy hits are resolved via AABB in bullet_system.
    constexpr float kBulletSpeed_m = 12.0f;
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_kinematicBody;
    bd.position = { (pos.x + box.w * 0.5f) / BOX_SCALE,
                    (pos.y + box.h * 0.5f) / BOX_SCALE };
    bd.linearVelocity = { kBulletSpeed_m * dir, 0.f };
    b2BodyId body = b2CreateBody(m_world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = 0.0f;
    sd.filter.categoryBits = CAT_BULLET;
    sd.filter.maskBits = 0;
    b2Polygon poly = b2MakeBox((box.w * 0.5f) / BOX_SCALE, (box.h * 0.5f) / BOX_SCALE);
    b2CreatePolygonShape(body, &sd, &poly);

    bagel::Entity e = bagel::Entity::create();
    e.addAll(pos, box, spr, a, b, Collider{body});
    return e.entity();
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

// ---------- Input ----------

void Game::input_system()
{
    // Drain SDL events (window close / ESC).
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_EVENT_QUIT) m_running = false;
        else if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE)
            m_running = false;
    }

    // Map per-entity Keys -> Intent.
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<Keys>()
        .set<Intent>()
        .build();

    SDL_PumpEvents();
    const bool* keys = SDL_GetKeyboardState(nullptr);

    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
    {
        if (!e.test(mask)) continue;
        const auto& k = e.get<Keys>();
        auto& i = e.get<Intent>();
        i.left     = keys[k.left];
        i.right    = keys[k.right];
        i.jumpHeld = keys[k.jump] || keys[k.jumpAlt];
        i.shoot    = keys[k.shoot];
    }
}

// ---------- Per-frame updates ----------

void Game::player_system()
{
    auto& pos  = bagel::Storage<Position>::type::get(m_player);
    auto& box  = bagel::Storage<Box>::type::get(m_player);
    auto& spr  = bagel::Storage<Sprite>::type::get(m_player);
    auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
    const auto& col = bagel::Storage<Collider>::type::get(m_player);
    const auto& in  = bagel::Storage<Intent>::type::get(m_player);

    // Box2D handles gravity and vy; we drive vx each frame.
    constexpr float kMoveSpeed_m = 7.0f;
    constexpr float kJumpVel_m   = 18.0f;   // Negative-y = up.

    pdat.grounded = playerGrounded();

    // Edge detection — input_system only writes held state.
    const bool jumpPressed  = in.jumpHeld && !m_prevJumpHeld;
    const bool shootPressed = in.shoot    && !m_prevShoot;
    m_prevJumpHeld = in.jumpHeld;
    m_prevShoot    = in.shoot;

    const b2Vec2 curV = b2Body_GetLinearVelocity(col.b);
    float vx = 0.f;
    if (in.left)  { vx -= kMoveSpeed_m; pdat.facing = -1; pdat.moving = true; }
    if (in.right) { vx += kMoveSpeed_m; pdat.facing =  1; pdat.moving = true; }
    if (!in.left && !in.right) pdat.moving = false;

    if (jumpPressed) m_jumpBuffer = 4;
    float vy = curV.y;
    if (m_jumpBuffer > 0 && pdat.grounded)
    {
        vy = -kJumpVel_m;
        pdat.grounded = false;
        m_jumpBuffer = 0;
    }
    else if (m_jumpBuffer > 0)
    {
        --m_jumpBuffer;
    }
    b2Body_SetLinearVelocity(col.b, {vx, vy});

    if (shootPressed && pdat.hasGun && pdat.ammo > 0)
    {
        static const bagel::Mask bulletMask = bagel::MaskBuilder()
            .set<BulletData>().build();
        int active = 0;
        for (bagel::Entity be = bagel::Entity::first(); !be.eof(); be.next())
        {
            if (!be.test(bulletMask)) continue;
            const auto& bd = be.get<BulletData>();
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
    // Walk frames are authored facing right; idle (Dave.png) and jump (dave5) face left.
    // Invert the flip for the latter two so every state faces the direction Dave is moving.
    const bool leftFacingArt = (spr.tex == m_tex.daveJump || spr.tex == m_tex.daveStand);
    spr.flipH = leftFacingArt ? (pdat.facing > 0) : (pdat.facing < 0);
}

void Game::enemy_system()
{
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<EnemyData>()
        .set<Position>()
        .set<Box>()
        .set<Sprite>()
        .set<Collider>()
        .build();

    constexpr float kEnemySpeed_m  = 2.5f;  // Ground walker, m/s.
    constexpr float kFlyerSpeed_m  = 3.0f;  // Flyer horizontal, m/s.
    constexpr float kBobAmplitude  = 1.5f;  // Peak vertical bob, m/s.
    constexpr float kBobFreq       = 0.06f; // Bob frequency, radians/frame.

    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
    {
        if (!e.test(mask)) continue;

        auto& ed = e.get<EnemyData>();
        if (!ed.alive) continue;

        const auto& c   = e.get<Collider>();
        auto& pos = e.get<Position>();
        const auto& box = e.get<Box>();
        auto& spr = e.get<Sprite>();

        // Patrol bounds (both kinds).
        if (pos.x < ed.patrolMin)         ed.dir =  1;
        if (pos.x + box.w > ed.patrolMax) ed.dir = -1;

        if (ed.kind == EnemyKind::FLYING)
        {
            // Sine bob around spawn Y, no tile collision.
            const float bob = kBobAmplitude * std::sin(m_animTick * kBobFreq + ed.phase);
            b2Body_SetLinearVelocity(c.b, { kFlyerSpeed_m * ed.dir, bob });
        }
        else
        {
            // Wall lookahead via tile grid; flip direction on hit.
            const float chestY = pos.y + box.h * 0.4f;
            const float leadX  = (ed.dir > 0) ? pos.x + box.w + 1.f : pos.x - 1.f;
            const int probeCol = (int)std::floor(leadX / kTileSize);
            const int probeRow = (int)std::floor(chestY / kTileSize);
            if (tileSolid(probeCol, probeRow)) ed.dir = -ed.dir;

            const b2Vec2 curV = b2Body_GetLinearVelocity(c.b);
            b2Body_SetLinearVelocity(c.b, { kEnemySpeed_m * ed.dir, curV.y });
        }

        spr.flipH = (ed.dir < 0);
        if (ed.shootCooldown > 0) --ed.shootCooldown;
    }
}

void Game::bullet_system()
{
    static const bagel::Mask bulletMask = bagel::MaskBuilder()
        .set<BulletData>()
        .set<Position>()
        .set<Box>()
        .set<Collider>()
        .build();
    static const bagel::Mask enemyMask = bagel::MaskBuilder()
        .set<EnemyData>()
        .set<Position>()
        .set<Box>()
        .build();

    auto kill = [](bagel::Entity x) {
        x.get<Sprite>().visible = false;
        if (x.has<Collider>()) {
            const auto& c = x.get<Collider>();
            if (b2Body_IsValid(c.b)) b2Body_Disable(c.b);
        }
    };

    for (bagel::Entity b = bagel::Entity::first(); !b.eof(); b.next())
    {
        if (!b.test(bulletMask)) continue;

        auto& bd  = b.get<BulletData>();
        if (!bd.alive) continue;

        const auto& pos = b.get<Position>();  // Synced from Box2D by box_system.
        const auto& box = b.get<Box>();

        --bd.ttl;

        if (bd.ttl <= 0 || pos.x < -32 || pos.x > kWorldW + 32) {
            bd.alive = false; kill(b); continue;
        }

        const int col = (int)std::floor((pos.x + box.w * 0.5f) / kTileSize);
        const int row = (int)std::floor((pos.y + box.h * 0.5f) / kTileSize);
        if (tileSolid(col, row)) {
            bd.alive = false; kill(b); continue;
        }

        if (!bd.fromPlayer) continue;

        // Player bullets vs enemies.
        for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
        {
            if (!e.test(enemyMask)) continue;
            auto& ed = e.get<EnemyData>();
            if (!ed.alive) continue;
            const auto& ep = e.get<Position>();
            const auto& eb = e.get<Box>();
            if (aabb(pos.x, pos.y, box.w, box.h, ep.x, ep.y, eb.w, eb.h))
            {
                bagel::Storage<PlayerData>::type::get(m_player).score += 100;
                ed.alive = false; kill(e);
                bd.alive = false; kill(b);
                break;
            }
        }
    }
}

void Game::hazard_system()
{
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<EnemyData>()
        .set<Position>()
        .set<Box>()
        .build();

    const auto& ppos = bagel::Storage<Position>::type::get(m_player);
    const auto& pbox = bagel::Storage<Box>::type::get(m_player);

    // Hazard tiles are handled by sensor_event_system; here we only catch falls off the world.
    if (ppos.y > kWorldH + 64) { killPlayer(); return; }

    // Player vs enemies — AABB overlap check.
    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
    {
        if (!e.test(mask)) continue;
        if (!e.get<EnemyData>().alive) continue;
        const auto& pos = e.get<Position>();
        const auto& box = e.get<Box>();
        if (aabb(ppos.x, ppos.y, pbox.w, pbox.h, pos.x, pos.y, box.w, box.h))
        {
            killPlayer();
            return;
        }
    }
}

void Game::door_system()
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

    // Player-enters-door is handled in sensor_event_system.
    (void)dd;
}

void Game::anim_system()
{
    ++m_animTick;
    // Trophy frame animation.
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

void Game::camera_system()
{
    const auto& pos = bagel::Storage<Position>::type::get(m_player);
    const auto& box = bagel::Storage<Box>::type::get(m_player);
    const float center = pos.x + box.w * 0.5f;
    float target = center - kScreenW * 0.5f;
    if (target < 0) target = 0;
    if (target > kWorldW - kScreenW) target = (float)(kWorldW - kScreenW);
    m_camX = target;
}

void Game::box_system()
{
    // Step Box2D, then sync Position (top-left px) from each body center.
    static const bagel::Mask mask = bagel::MaskBuilder()
        .set<Collider>()
        .set<Position>()
        .set<Box>()
        .build();

    b2World_Step(m_world, BOX_STEP, 4);

    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
    {
        if (!e.test(mask)) continue;
        const auto& c = e.get<Collider>();
        if (!b2Body_IsValid(c.b)) continue;
        const b2Vec2 p = b2Body_GetPosition(c.b);
        auto& pos = e.get<Position>();
        const auto& box = e.get<Box>();
        pos.x = p.x * BOX_SCALE - box.w * 0.5f;
        pos.y = p.y * BOX_SCALE - box.h * 0.5f;
    }
}

void Game::sensor_event_system()
{
    // Drain sensor begin-events from the last world step.
    // Player visitor dispatch: hazard -> die, pickup -> collect, door -> finish if unlocked.
    const b2SensorEvents events = b2World_GetSensorEvents(m_world);
    for (int i = 0; i < events.beginCount; ++i)
    {
        const auto& ev = events.beginEvents[i];
        const b2BodyId visitor = b2Shape_GetBody(ev.visitorShapeId);
        if (reinterpret_cast<std::uintptr_t>(b2Body_GetUserData(visitor)) != UDATA_PLAYER)
            continue;

        const b2BodyId sensorBody = b2Shape_GetBody(ev.sensorShapeId);
        const std::uintptr_t udata = reinterpret_cast<std::uintptr_t>(b2Body_GetUserData(sensorBody));

        if (udata == 0) {
            // Hazard tile sensor.
            killPlayer();
            return;
        }

        // Entity-based sensor (pickup / door / trophy).
        const int eid = static_cast<int>(udata - UDATA_ENTITY_BASE);
        bagel::Entity ent{bagel::ent_type{eid}};

        // Door entry — only if unlocked.
        if (m_door.id == eid)
        {
            const auto& dd = bagel::Storage<DoorData>::type::get(m_door);
            if (dd.unlocked) {
                m_state = GameState::LEVEL_COMPLETE;
                m_stateTimer = 0;
                return;
            }
            continue;
        }

        // Pickup entity.
        if (!ent.has<PickupData>()) continue;
        auto& pd = ent.get<PickupData>();
        if (pd.collected) continue;

        auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
        pdat.score += pd.score;
        switch (pd.kind)
        {
            case PickupKind::GUN:
                pdat.hasGun = true;
                pdat.ammo += 6;
                break;
            case PickupKind::TROPHY:
                pdat.hasTrophy = true;
                if (m_door.id >= 0)
                {
                    auto& dd = bagel::Storage<DoorData>::type::get(m_door);
                    dd.animating = true;
                    dd.animFrames = kDoorAnimFrames;
                }
                break;
            default: break;
        }
        pd.collected = true;
        ent.get<Sprite>().visible = false;
        // Disable the body so the sensor stops firing.
        if (b2Body_IsValid(sensorBody)) b2Body_Disable(sensorBody);
    }
}

bool Game::playerGrounded() const
{
    if (m_player.id < 0) return false;
    const auto& c = bagel::Storage<Collider>::type::get(m_player);
    if (!b2Body_IsValid(c.b)) return false;

    constexpr int kMaxContacts = 8;
    b2ContactData buf[kMaxContacts];
    const int n = b2Body_GetContactData(c.b, buf, kMaxContacts);
    for (int i = 0; i < n; ++i)
    {
        // Manifold normal points A -> B. Player is grounded when the normal points down (screen +y).
        const b2BodyId other = b2Shape_GetBody(buf[i].shapeIdB);
        const bool playerIsA = !(other.index1 == c.b.index1 && other.world0 == c.b.world0
                                 && other.generation == c.b.generation);
        const float ny = buf[i].manifold.normal.y;
        if (playerIsA  && ny >  0.5f) return true;
        if (!playerIsA && ny < -0.5f) return true;
    }
    return false;
}

void Game::update()
{
    anim_system();

    if (m_state == GameState::PLAYING)
    {
        player_system();
        if (m_state != GameState::PLAYING) return;
        enemy_system();
        bullet_system();
        box_system();
        sensor_event_system();
        if (m_state != GameState::PLAYING) return;
        hazard_system();
        door_system();
        camera_system();
    }
    else if (m_state == GameState::DEAD)
    {
        ++m_stateTimer;
        if (m_stateTimer >= kRespawnDelay)
        {
            auto& pos = bagel::Storage<Position>::type::get(m_player);
            auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);
            const auto& box = bagel::Storage<Box>::type::get(m_player);
            pos.x = (float)(pdat.spawnCol * kTileSize);
            pos.y = (float)(pdat.spawnRow * kTileSize);
            pdat.grounded = false;
            // Snap the body back to spawn — Box2D owns velocity now.
            const auto& c = bagel::Storage<Collider>::type::get(m_player);
            if (b2Body_IsValid(c.b)) {
                b2Body_SetTransform(c.b,
                    { (pos.x + box.w * 0.5f) / BOX_SCALE,
                      (pos.y + box.h * 0.5f) / BOX_SCALE },
                    b2MakeRot(0.0f));
                b2Body_SetLinearVelocity(c.b, {0.f, 0.f});
            }
            m_state = GameState::PLAYING;
            camera_system();
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
    // Solid sky — no parallax, no texture.
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

    SDL_SetTextureColorMod(spr.tex, spr.tintR, spr.tintG, spr.tintB);
    if (spr.flipH)
    {
        SDL_RenderTextureRotated(m_renderer, spr.tex, nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
    }
    else
    {
        SDL_RenderTexture(m_renderer, spr.tex, nullptr, &dst);
    }
    // Reset so the next entity sharing this texture isn't tinted by accident.
    SDL_SetTextureColorMod(spr.tex, 255, 255, 255);
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
    // HUD strip.
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_FRect strip{ 0, 0, (float)kScreenW, (float)kHudH };
    SDL_RenderFillRect(m_renderer, &strip);

    // Bottom border line.
    SDL_SetRenderDrawColor(m_renderer, 200, 60, 0, 255);
    SDL_FRect line{ 0, (float)kHudH - 2.0f, (float)kScreenW, 2.0f };
    SDL_RenderFillRect(m_renderer, &line);

    const auto& pdat = bagel::Storage<PlayerData>::type::get(m_player);

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

    // Trophy and gun indicators.
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

    // Dim overlay.
    SDL_SetRenderDrawBlendMode(m_renderer, 1 /* SDL_BLENDMODE_BLEND */);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 160);
    SDL_FRect overlay{ 0, 0, (float)kScreenW, (float)kScreenH };
    SDL_RenderFillRect(m_renderer, &overlay);

    // Debug font with offset passes for visual scale.
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
        {
            SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
            SDL_RenderDebugText(m_renderer, kScreenW/2 - 60 + dx, kScreenH/2 - 4 + dy, msg);
        }
    SDL_SetRenderDrawColor(m_renderer, 255, 220, 80, 255);
    SDL_RenderDebugText(m_renderer, kScreenW/2 - 60, kScreenH/2 - 4, msg);
}

void Game::draw_system()
{
    renderBackground();
    renderTiles();

    // Layer order from back to front: pickups, door, enemies, bullets, player.
    static const bagel::Mask pickupMask = bagel::MaskBuilder()
        .set<PickupData>().set<Position>().set<Box>().set<Sprite>().build();
    static const bagel::Mask enemyMask = bagel::MaskBuilder()
        .set<EnemyData>().set<Position>().set<Box>().set<Sprite>().build();
    static const bagel::Mask bulletMask = bagel::MaskBuilder()
        .set<BulletData>().set<Position>().set<Box>().set<Sprite>().build();

    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
        if (e.test(pickupMask)) renderEntity(e.entity());
    if (m_door.id >= 0) renderEntity(m_door);
    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
        if (e.test(enemyMask)) renderEntity(e.entity());
    for (bagel::Entity e = bagel::Entity::first(); !e.eof(); e.next())
        if (e.test(bulletMask)) renderEntity(e.entity());
    if (m_player.id >= 0) renderEntity(m_player);

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

        input_system();

        update();
        draw_system();

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
