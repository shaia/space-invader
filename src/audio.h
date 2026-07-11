// Procedural audio: everything is synthesized into Sound buffers at startup.
// The game stays single-threaded — never use SetAudioStreamCallback.
#pragma once
#include "raylib.h"
#include <array>

enum class Sfx : int {
    Shoot, Pop, PlayerDie, UfoWarble, Pickup, Expire, Crunch,
    Scream, BossHit, BossRoar, Blip, Ding, BigShot,
    March0, March1, March2, March3,
    COUNT
};

struct AudioBank {
    std::array<Sound, (int)Sfx::COUNT> sfx{};
    Sound music{};
    float musicLen = 0.0f;    // seconds
    double musicStartedAt = -1.0;
    bool ready = false;
};

void InitAudioBank(AudioBank& a);
void UnloadAudioBank(AudioBank& a);
void PlaySfx(const AudioBank& a, Sfx id, float pitch = 1.0f);
void TickMusic(AudioBank& a);   // main-thread loop restart
