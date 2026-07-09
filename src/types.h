// Shared entity structs, enums, and the tiny rng. No logic here.
#pragma once
#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"

// ---- tiny xorshift rng (no <random> ceremony, no statics) ----
struct Rng {
    uint32_t s = 0x9E3779B9u;
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }           // [0,1)
    float range(float a, float b) { return a + uniform() * (b - a); }
    int irange(int a, int b) { return a + (int)(uniform() * (float)(b - a + 1)); } // [a,b]
    bool chance(float p) { return uniform() < p; }
};

// ---- screens ----
enum class Screen { Title, Playing, Paused, GameOver, PerformanceReview, HighScoreEntry, Quit };

// ---- bubble anchor sentinels ----
// Shared: enemy shots record who fired them (for the exit interview), so these
// live above the entity structs that reference them.
inline constexpr int kBubbleAnchorFixed = -1;
inline constexpr int kBubbleAnchorBoss  = -2;
inline constexpr int kBubbleAnchorUfo   = -3;
inline constexpr int kBubbleAnchorFallerBase = -16;  // anchor = base - (int)faller.id

// ---- entities ----
struct Player {
    Vector2 pos{};            // center
    bool alive = true;
    float invuln = 0.0f;      // respawn grace
    float deathTimer = 0.0f;  // world freeze after death
    float squash = 0.0f;
    int shieldHits = 0;       // Bureaucracy Shield
    float fireCooldown = 0.0f;
};

struct Invader {
    Vector2 pos{};       // center
    bool alive = false;
    int hp = 1;          // 2 = Overachiever
    bool tough = false;  // Overachiever: red tint, double points
    float squash = 0.0f; // decaying draw-scale kick
    float hitFlash = 0.0f;
};

enum class ShotKind : uint8_t { PlayerShot, Bomb, Compliment, Clipboard, BigShot, Beamlet, Brick };

struct Shot {
    Vector2 pos{};
    Vector2 vel{};
    ShotKind kind = ShotKind::PlayerShot;
    bool fromPlayer = false;
    bool pierce = false;
    float spin = 0.0f;        // clipboards tumble
    std::string_view label{}; // compliment text (view into static kCompliments)
    int owner = kBubbleAnchorFixed; // enemy shot: who fired it (grid idx / boss / faller), for exit interview
    bool grazed = false;      // enemy shot already paid a graze bonus (once per shot)
    bool tallied = false;     // player shot already counted toward accuracy (once per shot)
    int hp = 0;               // 0 = indestructible; >0 = shootable projectile (Lawyer subpoenas)
};

struct Bunker {
    Vector2 topLeft{};
    std::array<uint8_t, cfg::kBunkerCols * cfg::kBunkerRows> cells{};
    int aliveCells = 0;
};

enum class PowerKind : uint8_t {
    Spread, Pierce, Rapid, Freeze, Shield, ExtraLife, BigShot, DejaVu, COUNT
};

struct PowerUp {
    Vector2 pos{};
    PowerKind kind = PowerKind::Spread;
    float wobble = 0.0f;
};

struct ActiveEffects {
    float spread = 0.0f;   // timers, seconds remaining
    float pierce = 0.0f;
    float rapid = 0.0f;
    float freeze = 0.0f;
    bool bigShotArmed = false;
};

// ---- combo streak (Productivity Streak) ----
struct Combo {
    int chain = 0;    // consecutive kills within the window
    int tier = 0;     // 0..3, indexes cfg::kComboMult
    float timer = 0;  // seconds left before the streak lapses
};

// ---- boss memos (sign one perk-with-a-catch after each boss) ----
enum class MemoId : uint8_t {
    EspressoBudget, PerformanceBonus, KeyPersonInsurance,
    FlexibleHours, OpenFloorPlan, StockOptions, COUNT
};

struct Memo {
    MemoId id = MemoId::EspressoBudget;
    std::string_view name, buff, drawback;
    int   extraShotCap = 0;   // + concurrent player shots
    int   lifeNow = 0;        // lives granted once on signing
    int   shieldCap = 0;      // 0 = no override; >0 caps shield hits
    float bombSpeedMult = 1.0f;
    float scoreMult = 1.0f;
    float dropMult = 1.0f;
    float marchMult = 1.0f;   // >1 = slower march
    float bombRateMult = 1.0f;
    float grazeMult = 1.0f;
    float hitboxMult = 1.0f;  // >1 = larger (easier to hit / catch pickups)
    float ufoGapMult = 1.0f;  // <1 = UFO appears more often
    float ufoPayMult = 1.0f;
};

// folded effect of every signed memo, recomputed on demand
struct MemoFx {
    int   extraShotCap = 0;
    int   shieldCap = cfg::kShieldHits;
    float bombSpeedMult = 1.0f, scoreMult = 1.0f, dropMult = 1.0f;
    float marchMult = 1.0f, bombRateMult = 1.0f, grazeMult = 1.0f, hitboxMult = 1.0f;
    float ufoGapMult = 1.0f, ufoPayMult = 1.0f;
};

struct MemoOffer {
    bool active = false;
    MemoId pick[3]{};
    int count = 0;         // valid picks (1-3)
    float timer = 0.0f;    // seconds until auto-decline
};

// ---- per-run stats (drives the Performance Review + reactive commentary) ----
struct RunStats {
    int shotsFired = 0;
    int shotsHit = 0;
    int wavesCleared = 0;
    int bossesDefeated = 0;
    int bunkersLost = 0;
    int grazes = 0;
    int waveGrazes = 0;      // grazes this wave, reset in StartWave
    int powerups = 0;
    int incidents = 0;       // exit-interview counter
    int maxChain = 0;        // best combo chain reached
    int memosSigned = 0;
};

struct Ufo {
    bool active = false;
    Vector2 pos{};
    float dir = 1.0f;
    float spawnTimer = 12.0f;
};

// an invader that refused to explode; tumbles down as a hazard, quipping
struct FallingInvader {
    Vector2 pos{};
    Vector2 vel{};
    float angle = 0.0f;   // degrees
    float spin = 0.0f;    // deg/s, signed
    int row = 0;          // art mask + kColRow color
    int seed = 0;         // original grid index, keeps DrawInvaderArt deterministic
    uint32_t id = 0;      // stable handle for the bubble anchor (indices shift on erase)
};

// ---- wave modifiers ----
enum class ModifierId : uint8_t {
    None, OppositeDay, BudgetCuts, PassiveAggression, DiscoInferno,
    SpeedrunAnyPercent, TinyWave, Understudies, MirrorMatch, COUNT
};

struct Modifier {
    ModifierId id = ModifierId::None;
    std::string_view name = "";
    std::string_view tagline = "";
    bool invertInput = false;
    bool invisibleInvaders = false;
    bool complimentBombs = false;
    bool discoHue = false;
    int startRowsLower = 0;
    float scoreMult = 1.0f;
    float scale = 1.0f;
    bool wobbly = false;
    bool mirrorCannon = false;
};

// ---- boss ----
enum class BossKind : uint8_t { Karen, Local1978, Producer };

struct Minion {
    Vector2 pos{};
    Vector2 basePos{};
    float t = 0.0f;
    bool alive = false;
};

struct Saucer {
    Vector2 offset{};   // from boss pos
    float signHp = 0.0f;
    float hp = 0.0f;
    bool alive = false;
};

struct BeamAttack {
    bool active = false;
    float x = 0.0f;
    float width = 0.0f;
    float t = 0.0f;       // elapsed
    float telegraph = 0.0f;
};

struct Boss {
    bool active = false;
    BossKind kind = BossKind::Karen;
    float hp = 0.0f, maxHp = 0.0f;
    Vector2 pos{};
    float dir = 1.0f;
    int phase = 0;
    int attackCycle = 0;      // rotates through attack patterns
    float timer = 0.0f;       // generic phase timer
    float attackTimer = 0.0f;
    int livesAtStart = 3;     // for Union Buster
    float squash = 0.0f;
    bool saidAt75 = false, saidAt50 = false, saidAt25 = false;
    // Karen
    BeamAttack laser{};
    std::vector<Minion> minions;
    // Local 1978
    std::array<Saucer, 3> saucers{};
    // Producer
    BeamAttack creep{};
    float brickTimer = 0.0f;
};

// ---- waves ----
struct WaveState {
    int number = 1;
    ModifierId modifier = ModifierId::None;
    uint32_t usedModifiers = 0;    // bitmask, reset when pool exhausted
    float intermission = 0.0f;     // countdown before next wave spawns
    bool clearing = false;
    bool bossWave = false;
    bool bunkersWereGone = false;  // for This Is Fine
};

// ---- background ----
struct Star {
    Vector2 pos{};
    float speed = 20.0f;   // downward scroll px/s (parallax by layer)
    float size = 1.0f;
    float phase = 0.0f;    // twinkle phase offset
    Color tint = WHITE;
};

// ---- particles ----
enum class ParticleKind : uint8_t { Spark, Debris, Confetti, Trail, Shockwave, Smoke, ScorePop };

struct Particle {
    Vector2 pos{};
    Vector2 vel{};
    float life = 0.0f, maxLife = 1.0f;
    float size = 2.0f;
    float gravity = 0.0f;
    Color color = WHITE;
    ParticleKind kind = ParticleKind::Spark;
    int value = 0;   // ScorePop: the number to draw
    std::string_view label{};  // ScorePop: optional suffix, e.g. "HAZARD PAY" (view into a static)
};

// ---- ui fx ----
struct Bubble {
    std::string text;
    int anchor = -1;          // >=0 invader index, kBubbleAnchorBoss, kBubbleAnchorFixed
    Vector2 pos{};            // used when anchor is fixed
    float t = 0.0f, dur = 4.0f;
};

struct Toast {
    std::string text;
    float t = 0.0f;
};

struct Announcement {
    std::string big;
    std::string small;
    float t = 0.0f, dur = 0.0f;
};

enum class Ach : uint8_t {
    PacifistRun, FriendlyFire, UnionBuster, SpeakToTheManager,
    Hoarder, CeasefireViolation, ThisIsFine, Y1978, COUNT
};

struct UiFx {
    std::vector<Bubble> bubbles;
    std::vector<Toast> toasts;
    Announcement card{};
    float ambientTimer = 10.0f;
    uint32_t achAwarded = 0;         // this run
    float reactiveCooldown = 20.0f;  // gap before/between reactive commentary
    uint32_t reactiveSaid = 0;       // once-per-run reactive flags
};
