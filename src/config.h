// All tuning constants live here. If you're typing a number into a logic file, stop.
#pragma once
#include "raylib.h"

// Compile-time debug helpers: F1 overlay, F2 skip wave, F3 powerup, F4 modifier, F5 kill-all-but-one
#define DEBUG_KEYS 1

namespace cfg {

// ---- canvas ----
inline constexpr int   kCanvasW = 800;   // logical coordinates all code draws in
inline constexpr int   kCanvasH = 950;
inline constexpr int   kSupersample = 2; // canvas rasterized at 2x for crisp window scaling
inline constexpr float kWindowFit = 0.90f; // initial window height as fraction of monitor
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

// ---- falling invaders ----
inline constexpr float kFallChance  = 0.10f;   // killed invader tumbles instead of exploding
inline constexpr float kFallGravity = 480.0f;  // px/s^2 (debris uses 500)
inline constexpr float kFallInitVy  = 40.0f;   // initial downward kick
inline constexpr float kFallDriftX  = 50.0f;   // max sideways drift px/s
inline constexpr float kFallSpinMin = 140.0f;  // tumble deg/s
inline constexpr float kFallSpinMax = 360.0f;
inline constexpr float kFallQuipDur = 2.6f;

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

// ---- colors (neon palette, saturated arcade) ----
inline constexpr Color kColBg        = {6, 7, 20, 255};    // deep space blue-violet
inline constexpr Color kColPlayer    = {80, 255, 170, 255};
inline constexpr Color kColRow[5]    = {{255, 60, 190, 255},   // executives (top) - hot magenta
                                        {70, 220, 255, 255},   // managers - cyan
                                        {120, 150, 255, 255},  // managers - electric azure
                                        {255, 210, 70, 255},   // interns - gold
                                        {255, 140, 60, 255}};  // interns - amber orange
inline constexpr Color kColOverachiever = {255, 60, 70, 255};
inline constexpr Color kColUfo       = {200, 110, 255, 255};
inline constexpr Color kColBunker    = {70, 240, 200, 255};
inline constexpr Color kColShot      = {235, 255, 255, 255};
inline constexpr Color kColBomb      = {255, 130, 70, 255};
inline constexpr Color kColHud       = {170, 185, 215, 255};  // kept below bloom threshold
inline constexpr Color kColAccent    = {255, 70, 190, 255};
inline constexpr bool  kScanlines    = true;

// consolidated literals (were scattered across draw files)
inline constexpr Color kColHudBar    = {13, 14, 30, 255};
inline constexpr Color kColBeamlet   = {255, 80, 100, 255};
inline constexpr Color kColClipboard = {235, 225, 185, 255};
inline constexpr Color kColCompliment= {255, 150, 205, 255};
inline constexpr Color kColBrick     = {210, 70, 70, 255};
inline constexpr Color kColHurt      = {255, 90, 100, 255};   // boss/hit red
inline constexpr Color kColHair      = {255, 220, 150, 255};  // Karen's haircut
inline constexpr Color kColSign      = {235, 225, 185, 255};  // protest signs
inline constexpr Color kColProducer  = {96, 98, 128, 255};    // producer chassis
inline constexpr Color kColProducerScreen = {18, 22, 46, 255};
inline constexpr Color kColScopeCreep= {255, 170, 90, 255};   // producer's creep beam
inline constexpr Color kColConfetti[4] = {{255, 60, 190, 255}, {70, 220, 255, 255},
                                          {255, 210, 70, 255}, {80, 255, 170, 255}};

// per-archetype invader detail colors (index: 0 squid, 1 crab, 2 octo)
struct InvaderPalette { Color accent; Color eye; };
inline constexpr InvaderPalette kInvPal[3] = {
    {{255, 235, 160, 255}, {30, 12, 40, 255}},   // squid: warm accent, dark eyes
    {{235, 255, 255, 255}, {20, 30, 60, 255}},   // crab: white highlight, navy eyes
    {{255, 255, 210, 255}, {40, 20, 20, 255}},   // octo: pale accent, dark eyes
};

// ---- glow helper tuning ----
inline constexpr float kGlowHalo      = 6.0f;   // outer halo px
inline constexpr float kGlowHaloA     = 0.12f;  // outer halo alpha
inline constexpr float kGlowMidA      = 0.28f;  // inner halo alpha
inline constexpr float kGlowCircleOut = 1.9f;   // outer radius multiple
inline constexpr float kGlowCircleMid = 1.25f;  // mid radius multiple

// ---- bloom post-process ----
inline constexpr float kBloomThreshold = 0.66f; // luminance cutoff for bright-pass
inline constexpr float kBloomSoftKnee  = 0.28f; // soft threshold rolloff
inline constexpr float kBloomIntensity = 1.15f; // additive bloom strength
inline constexpr int   kBloomPasses    = 4;     // separable blur iterations (H+V each)
inline constexpr int   kBloomDownscale = 2;     // bloom RT = canvas / this

// ---- CRT composite ----
inline constexpr float kCrtScanline   = 0.10f;  // scanline darkening depth
inline constexpr float kCrtAberration = 0.0016f;// chromatic aberration (uv units)
inline constexpr float kCrtBarrel     = 0.020f; // barrel distortion strength
inline constexpr float kCrtVignette   = 0.32f;  // edge darkening
inline constexpr float kCrtScanCount  = 340.0f; // number of scanline bands

// ---- background / starfield ----
inline constexpr int   kStarLayers    = 3;
inline constexpr int   kStarsPerLayer = 80;
inline constexpr Color kColNebula[3]  = {{120, 30, 140, 255},   // magenta cloud
                                         {30, 70, 160, 255},    // blue cloud
                                         {20, 110, 130, 255}};  // teal cloud

} // namespace cfg
