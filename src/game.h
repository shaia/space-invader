// The Game struct (all playing state) and every subsystem's interface.
// Subsystem .cpp files implement these free functions operating on Game&.
#pragma once
#include "types.h"
#include "audio.h"

struct Game {
    Player player{};
    std::array<Invader, cfg::kGridCount> invaders{};  // index = row * kGridCols + col
    int aliveCount = 0;
    float marchTimer = 0.0f;
    int marchDir = 1;
    bool descendPending = false;
    int marchFrame = 0;          // 2-frame animation parity
    int marchNoteIdx = 0;        // 4-note bassline position
    float stepFlash = 0.0f;      // Budget Cuts visibility window after a step
    float bombTimer = 0.0f;

    std::vector<Shot> shots;     // player + enemy, flag disambiguates
    std::array<Bunker, cfg::kBunkerCount> bunkers{};
    std::vector<PowerUp> pickups;
    std::vector<FallingInvader> fallers;
    uint32_t fallerIdNext = 1;
    ActiveEffects fx{};
    Ufo ufo{};
    Boss boss{};
    WaveState wave{};
    std::vector<Particle> particles;
    std::vector<Star> stars;     // parallax background, seeded in ResetRun
    UiFx uifx{};

    int score = 0;
    int hiScore = 0;
    int lives = cfg::kStartLives;
    bool gameOver = false;
    bool overrun = false;        // invaders reached the bottom
    float shake = 0.0f;
    float hitStop = 0.0f;        // brief whole-world freeze on impactful kills (seconds)
    float time = 0.0f;           // run time, drives wobble/disco
    float noShootTimer = 0.0f;   // Pacifist Run
    bool pacifistChecked = false;
    float panicTimer = 0.0f;     // seconds with exactly one invader left
    float edgeDwell = 0.0f;      // continuous seconds hugging a wall (reactive commentary)
    float lastShotAt = 0.0f;     // g.time of the last player shot
    RunStats stats{};
    Combo combo{};
    bool comboBroken = false;    // set when a hit breaks a tier>=1 streak; consumed by reactive commentary
    uint32_t memoMask = 0;       // signed boss memos
    MemoOffer memoOffer{};
    Rng rng{};

    AudioBank* audio = nullptr;  // owned by main
};

// ---- game.cpp ----
void ResetRun(Game& g);
void InitStarfield(Game& g);
void DrawBackground(const Game& g, float time);
void StartWave(Game& g, int number);
void UpdatePlaying(Game& g, float dt);
void DrawPlaying(const Game& g);
void ResolveCollisions(Game& g);
void AddScore(Game& g, int points);
void ComboKill(Game& g, Vector2 pos, int basePts, Color c);  // scores a kill through the combo multiplier
const Modifier& CurrentMod(const Game& g);
Rectangle PlayerRect(const Game& g);
bool WorldFrozen(const Game& g);  // death pause / intermission
int RandomAliveInvader(Game& g);  // random living grid index, or -1 if none

// ---- invaders.cpp ----
void SpawnGrid(Game& g);
void UpdateInvaders(Game& g, float dt);
void UpdateUfo(Game& g, float dt);
Rectangle InvaderRect(const Game& g, int idx);
float GridScale(const Game& g);   // Tiny Wave
void KillInvader(Game& g, int idx);
void SpawnFaller(Game& g, int idx);
void UpdateFallers(Game& g, float dt);

// ---- player.cpp ----
void UpdatePlayer(Game& g, float dt);
void HitPlayer(Game& g, std::string_view cause, int anchor = kBubbleAnchorFixed);
void PlayerFire(Game& g);

// ---- bunkers.cpp ----
void InitBunkers(Game& g);
void RestoreBunkers(Game& g);
bool CarveBunkers(Game& g, Vector2 hit, float radius);  // true if solid cells were hit
bool ShotHitsBunker(const Game& g, const Shot& s, Vector2& hitPoint);
bool AnyBunkerAlive(const Game& g);
void DrawBunkers(const Game& g);

// ---- powerups.cpp ----
void MaybeDropPickup(Game& g, Vector2 pos);
void UpdatePickups(Game& g, float dt);
void ActivatePickup(Game& g, PowerKind kind);
void UpdateEffects(Game& g, float dt);
void DrawPickups(const Game& g);
void DrawEffectHud(const Game& g);
int ActiveEffectCount(const Game& g);

// ---- modifiers.cpp ----
const Modifier& GetModifier(ModifierId id);
ModifierId PickNextModifier(Game& g);

// ---- memos.cpp ----
const Memo& GetMemo(MemoId id);
MemoFx CollectMemoFx(const Game& g);   // fold every signed memo's effect
void OfferMemos(Game& g);              // present three unsigned memos
void UpdateMemoOffer(Game& g, float dt);
void DrawMemoOffer(const Game& g);

// ---- boss.cpp ----
void StartBoss(Game& g);
void UpdateBoss(Game& g, float dt);
void DrawBoss(const Game& g);
bool BossShotHit(Game& g, const Shot& s);   // player shot vs boss parts; true = consumed
bool BossTouchesPlayer(const Game& g);

// ---- particles.cpp ----
void SpawnExplosion(Game& g, Vector2 pos, Color c, int count);
void SpawnDebris(Game& g, Vector2 pos, Color c, int count);
void SpawnConfetti(Game& g, Vector2 pos, int count);
void SpawnTrail(Game& g, Vector2 pos, Color c);
void SpawnShockwave(Game& g, Vector2 pos, Color c, float radius);
void SpawnScorePop(Game& g, Vector2 pos, int points, Color c, float size = 16.0f,
                   std::string_view label = {});
void SpawnMuzzle(Game& g, Vector2 pos, Color c);
void UpdateParticles(Game& g, float dt);
void DrawParticles(const Game& g);

// ---- ui_fx.cpp ----
void PushToast(Game& g, std::string_view text);
void PushBubble(Game& g, int anchor, std::string_view text, float dur = 4.0f);
void Announce(Game& g, std::string_view big, std::string_view small, float dur);
void TryAward(Game& g, Ach id);
void UpdateUiFx(Game& g, float dt);
void DrawUiFx(const Game& g);
void DrawSpeechBubbles(const Game& g);  // drawn post-bloom so they stay crisp

// ---- screens.cpp ----
struct HighScores;
struct ScoreEntryState {
    char initials[4] = {'A', 'A', 'A', '\0'};
    int cursor = 0;
};
Screen UpdateDrawTitle(Game& g, const HighScores& hs, float timer);
Screen UpdateDrawPaused(const Game& g);
Screen UpdateDrawGameOver(Game& g, const HighScores& hs, float timer, float dt);
Screen UpdateDrawPerformanceReview(Game& g, HighScores& hs, float timer, float dt);
Screen UpdateDrawHighScoreEntry(Game& g, HighScores& hs, ScoreEntryState& es, float timer);
int GradeScore(const Game& g);          // 0..100 weighted run rating
int GradeLetterIndex(int gradeScore);   // 0=F .. 5=S
