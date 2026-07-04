// All tuning constants live here. If you're typing a number into a logic file, stop.
#pragma once
#include "raylib.h"

// Compile-time debug helpers: F1 overlay, F2 skip wave, F3 powerup, F4 modifier, F5 kill-all-but-one
#define DEBUG_KEYS 1

namespace cfg {

// ---- canvas ----
inline constexpr int   kCanvasW = 800;
inline constexpr int   kCanvasH = 950;
inline constexpr float kDtClamp = 1.0f / 30.0f;

// ---- playfield layout ----
inline constexpr float kPlayfieldMargin = 34.0f;  // march reversal margin
inline constexpr float kHudTopH        = 58.0f;
inline constexpr float kPlayerY        = 872.0f;
inline constexpr float kLoseLineY      = 830.0f;  // invaders past this = overrun
inline constexpr float kBunkerY        = 762.0f;

// ---- player ----
inline constexpr float kPlayerSpeed      = 340.0f;
inline constexpr float kPlayerW          = 44.0f;
inline constexpr float kPlayerH          = 24.0f;
inline constexpr float kShotSpeed        = 720.0f;
inline constexpr float kShotW            = 4.0f;
inline constexpr float kShotH            = 14.0f;
inline constexpr int   kStartLives       = 3;
inline constexpr int   kMaxLives         = 5;
inline constexpr float kRespawnInvuln    = 2.2f;
inline constexpr float kDeathFreeze      = 1.4f;   // world pause after player death

// ---- invader grid ----
inline constexpr int   kGridCols       = 11;
inline constexpr int   kGridRows       = 5;
inline constexpr int   kGridCount      = kGridCols * kGridRows;
inline constexpr float kGridSpacingX   = 54.0f;
inline constexpr float kGridSpacingY   = 44.0f;
inline constexpr float kGridTopY       = 150.0f;
inline constexpr float kInvaderW       = 36.0f;
inline constexpr float kInvaderH       = 24.0f;
inline constexpr float kMarchStepX     = 10.0f;
inline constexpr float kMarchDropY     = 26.0f;
inline constexpr float kMarchSlowest   = 0.80f;    // full grid
inline constexpr float kMarchFastest   = 0.06f;    // last invader
inline constexpr float kBombSpeed      = 190.0f;
inline constexpr float kBombBaseRate   = 0.55f;    // bombs per second, wave 1, full grid
inline constexpr float kBombRateWave   = 0.08f;    // +8% per wave
inline constexpr float kWaveSpeedMult  = 1.06f;    // global speed per wave
inline constexpr float kOverachieverFrac = 0.12f;  // fraction of 2-HP invaders from wave 4

// ---- UFO ----
inline constexpr float kUfoY        = 92.0f;
inline constexpr float kUfoSpeed    = 130.0f;
inline constexpr float kUfoW        = 52.0f;
inline constexpr float kUfoH        = 20.0f;
inline constexpr float kUfoMinGap   = 18.0f;   // seconds between flybys
inline constexpr float kUfoMaxGap   = 30.0f;

// ---- bunkers ----
inline constexpr int   kBunkerCount = 4;
inline constexpr int   kBunkerCols  = 24;
inline constexpr int   kBunkerRows  = 18;
inline constexpr float kBunkerCell  = 3.0f;
inline constexpr float kCarveShot   = 7.0f;    // carve radius: player shot
inline constexpr float kCarveBomb   = 10.0f;   // invader bomb
inline constexpr float kCarveBoss   = 26.0f;   // boss attacks

// ---- power-ups ----
inline constexpr float kDropChance   = 0.08f;
inline constexpr int   kMaxFalling   = 2;
inline constexpr float kPickupFall   = 110.0f;
inline constexpr float kPickupSize   = 26.0f;
inline constexpr float kFxSpread     = 8.0f;   // seconds
inline constexpr float kFxPierce     = 6.0f;
inline constexpr float kFxRapid      = 8.0f;
inline constexpr float kFxFreeze     = 4.0f;
inline constexpr int   kShieldHits   = 3;

// ---- scoring ----
inline constexpr int kPtsRow[5]     = {30, 20, 20, 10, 10}; // by grid row, top first
inline constexpr int kPtsMinion     = 25;
inline constexpr int kWaveBonusPer  = 100;  // x wave number
inline constexpr int kBossBonusPer  = 500;  // x wave number

// ---- waves / bosses / modifiers ----
inline constexpr int   kFirstModifierWave = 3;
inline constexpr int   kBossEvery         = 5;
inline constexpr float kWaveCardDur       = 2.6f;
inline constexpr float kIntermission      = 1.2f;

// ---- particles / fx ----
inline constexpr int   kMaxParticles  = 1000;
inline constexpr float kShakeDecay    = 4.5f;
inline constexpr float kBubbleAmbientMin = 8.0f;
inline constexpr float kBubbleAmbientMax = 15.0f;
inline constexpr int   kMaxToasts     = 3;

// ---- audio ----
inline constexpr int   kSampleRate  = 44100;
inline constexpr float kMasterSfx   = 0.65f;
inline constexpr float kMasterMusic = 0.35f;

// ---- colors (neon palette) ----
inline constexpr Color kColBg        = {8, 8, 16, 255};
inline constexpr Color kColPlayer    = {80, 255, 140, 255};
inline constexpr Color kColRow[5]    = {{255, 90, 200, 255},   // executives (top)
                                        {90, 200, 255, 255},   // managers
                                        {90, 200, 255, 255},
                                        {255, 220, 90, 255},   // interns
                                        {255, 220, 90, 255}};
inline constexpr Color kColOverachiever = {255, 80, 80, 255};
inline constexpr Color kColUfo       = {200, 120, 255, 255};
inline constexpr Color kColBunker    = {80, 255, 140, 255};
inline constexpr Color kColShot      = {240, 255, 255, 255};
inline constexpr Color kColBomb      = {255, 140, 90, 255};
inline constexpr Color kColHud       = {180, 190, 210, 255};
inline constexpr Color kColAccent    = {255, 90, 200, 255};
inline constexpr bool  kScanlines    = true;

} // namespace cfg
