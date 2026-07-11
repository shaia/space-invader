// All sounds are synthesized here, once, at startup. Single-threaded by design.
#include "audio.h"
#include "config.h"
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

constexpr float kTau = 6.28318530f;

enum class Osc { Sine, Square, Saw, Noise };

// deterministic noise so synthesis needs no shared rng state
float NoiseAt(int i) {
    unsigned n = (unsigned)i * 2654435761u;
    n ^= n >> 13;
    n *= 2246822519u;
    n ^= n >> 16;
    return ((float)(n & 0xFFFF) / 32768.0f) - 1.0f;
}

struct Buf {
    std::vector<float> s;
    explicit Buf(float seconds) : s((size_t)(seconds * cfg::kSampleRate), 0.0f) {}
    float dur() const { return (float)s.size() / cfg::kSampleRate; }
};

// Add one voice: freq glides f0->f1, amplitude has attack/release ramps.
void Voice(Buf& b, float t0, float dur, Osc osc, float f0, float f1, float amp,
           float attack = 0.004f, float release = 0.05f, float vibHz = 0.0f, float vibAmt = 0.0f) {
    int start = (int)(t0 * cfg::kSampleRate);
    int n = (int)(dur * cfg::kSampleRate);
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = start + i;
        if (idx < 0 || idx >= (int)b.s.size()) break;
        float t = (float)i / cfg::kSampleRate;
        float u = t / dur;
        float f = f0 + (f1 - f0) * u;
        if (vibHz > 0) f += sinf(t * kTau * vibHz) * vibAmt;
        phase += f / cfg::kSampleRate;
        float v = 0.0f;
        switch (osc) {
        case Osc::Sine:   v = sinf(phase * kTau); break;
        case Osc::Square: v = (fmodf(phase, 1.0f) < 0.5f) ? 1.0f : -1.0f; break;
        case Osc::Saw:    v = 2.0f * fmodf(phase, 1.0f) - 1.0f; break;
        case Osc::Noise:  v = NoiseAt(idx); break;
        }
        float env = 1.0f;
        if (t < attack) env = t / attack;
        float tail = dur - t;
        if (tail < release) env *= tail / release;
        b.s[idx] += v * amp * env;
    }
}

Sound Bake(const Buf& b, float gain) {
    // normalize into int16 (mixing in float means sums can exceed 1)
    float peak = 0.0001f;
    for (float v : b.s)
        peak = fmaxf(peak, fabsf(v));
    float k = gain / peak;
    if (k > gain) k = gain;  // don't amplify silence

    int n = (int)b.s.size();
    short* data = (short*)RL_MALLOC(n * sizeof(short));
    for (int i = 0; i < n; i++) {
        float v = b.s[i] * k;
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        data[i] = (short)(v * 32000.0f);
    }
    Wave w{};
    w.frameCount = (unsigned)n;
    w.sampleRate = cfg::kSampleRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}

Sound MakeSfx(Sfx id) {
    switch (id) {
    case Sfx::Shoot: {
        Buf b(0.14f);
        Voice(b, 0, 0.12f, Osc::Square, 900, 260, 0.7f, 0.002f, 0.04f);
        return Bake(b, 0.9f);
    }
    case Sfx::Pop: {
        Buf b(0.18f);
        Voice(b, 0, 0.10f, Osc::Noise, 0, 0, 0.5f, 0.002f, 0.06f);
        Voice(b, 0, 0.15f, Osc::Sine, 420, 70, 0.9f, 0.002f, 0.05f);
        return Bake(b, 0.9f);
    }
    case Sfx::PlayerDie: {
        Buf b(0.9f);
        Voice(b, 0, 0.85f, Osc::Saw, 320, 50, 0.8f, 0.004f, 0.2f, 9.0f, 20.0f);
        Voice(b, 0, 0.4f, Osc::Noise, 0, 0, 0.25f, 0.002f, 0.3f);
        return Bake(b, 0.95f);
    }
    case Sfx::UfoWarble: {
        Buf b(1.1f);
        Voice(b, 0, 1.05f, Osc::Sine, 620, 620, 0.7f, 0.01f, 0.2f, 11.0f, 90.0f);
        return Bake(b, 0.8f);
    }
    case Sfx::Pickup: {
        Buf b(0.34f);
        Voice(b, 0.00f, 0.10f, Osc::Square, 523, 523, 0.5f);
        Voice(b, 0.09f, 0.10f, Osc::Square, 659, 659, 0.5f);
        Voice(b, 0.18f, 0.14f, Osc::Square, 784, 784, 0.5f, 0.004f, 0.08f);
        return Bake(b, 0.85f);
    }
    case Sfx::Expire: {
        Buf b(0.3f);
        Voice(b, 0.00f, 0.12f, Osc::Square, 659, 659, 0.4f);
        Voice(b, 0.12f, 0.16f, Osc::Square, 440, 415, 0.4f, 0.004f, 0.1f);
        return Bake(b, 0.8f);
    }
    case Sfx::Crunch: {
        Buf b(0.16f);
        Voice(b, 0, 0.13f, Osc::Noise, 0, 0, 0.8f, 0.002f, 0.08f);
        Voice(b, 0, 0.10f, Osc::Sine, 160, 60, 0.5f, 0.002f, 0.06f);
        return Bake(b, 0.85f);
    }
    case Sfx::Scream: {
        Buf b(1.0f);
        Voice(b, 0, 0.9f, Osc::Saw, 780, 140, 0.55f, 0.005f, 0.25f, 12.0f, 35.0f);
        return Bake(b, 0.85f);
    }
    case Sfx::BossHit: {
        Buf b(0.12f);
        Voice(b, 0, 0.10f, Osc::Square, 180, 120, 0.8f, 0.002f, 0.05f);
        return Bake(b, 0.85f);
    }
    case Sfx::BossRoar: {
        Buf b(0.8f);
        Voice(b, 0, 0.7f, Osc::Saw, 95, 38, 0.8f, 0.01f, 0.25f, 6.0f, 8.0f);
        Voice(b, 0, 0.5f, Osc::Noise, 0, 0, 0.2f, 0.01f, 0.3f);
        return Bake(b, 0.95f);
    }
    case Sfx::Blip: {
        Buf b(0.07f);
        Voice(b, 0, 0.05f, Osc::Square, 660, 660, 0.5f);
        return Bake(b, 0.8f);
    }
    case Sfx::Ding: {
        Buf b(0.5f);
        Voice(b, 0, 0.45f, Osc::Sine, 1319, 1319, 0.5f, 0.002f, 0.35f);
        Voice(b, 0, 0.35f, Osc::Sine, 1760, 1760, 0.3f, 0.002f, 0.3f);
        return Bake(b, 0.8f);
    }
    case Sfx::BigShot: {
        Buf b(0.6f);
        Voice(b, 0, 0.55f, Osc::Sine, 70, 28, 1.0f, 0.005f, 0.2f);
        Voice(b, 0, 0.2f, Osc::Noise, 0, 0, 0.3f, 0.002f, 0.15f);
        return Bake(b, 1.0f);
    }
    case Sfx::March0:
    case Sfx::March1:
    case Sfx::March2:
    case Sfx::March3: {
        // the iconic descending four-note bassline
        const float notes[4] = {110.0f, 98.0f, 87.3f, 82.4f};
        float f = notes[(int)id - (int)Sfx::March0];
        Buf b(0.11f);
        Voice(b, 0, 0.09f, Osc::Square, f, f * 0.96f, 0.9f, 0.003f, 0.04f);
        return Bake(b, 0.85f);
    }
    default: {
        Buf b(0.05f);
        return Bake(b, 0.1f);
    }
    }
}

Sound MakeMusic(float& lenOut) {
    // 120 BPM step sequencer, 64 eighth-note steps = 16 s loop
    const float step = 0.25f;
    const int steps = 64;
    Buf b(steps * step + 0.3f);

    // A minor-ish: bass roots per 8-step bar
    const float bars[8] = {110.0f, 110.0f, 87.3f, 87.3f, 98.0f, 98.0f, 82.4f, 110.0f};
    for (int i = 0; i < steps; i++) {
        float t = i * step;
        float root = bars[(i / 8) % 8];
        if (i % 2 == 0)
            Voice(b, t, 0.20f, Osc::Square, root, root, 0.30f, 0.004f, 0.08f);
        // hats
        if (i % 2 == 1)
            Voice(b, t, 0.04f, Osc::Noise, 0, 0, 0.10f, 0.002f, 0.02f);
        // sparse arpeggio: root / minor third / fifth, up two octaves
        const float arp[4] = {4.0f, 4.756f, 6.0f, 8.0f};
        if (i % 4 == 2 || i % 8 == 5)
            Voice(b, t, 0.18f, Osc::Sine, root * arp[i % 4], root * arp[i % 4], 0.16f,
                  0.006f, 0.1f);
    }
    lenOut = steps * step;
    return Bake(b, 0.8f);
}

} // namespace

void InitAudioBank(AudioBank& a) {
    if (!IsAudioDeviceReady()) return;
    for (int i = 0; i < (int)Sfx::COUNT; i++) {
        a.sfx[i] = MakeSfx((Sfx)i);
        SetSoundVolume(a.sfx[i], cfg::kMasterSfx);
    }
    a.music = MakeMusic(a.musicLen);
    SetSoundVolume(a.music, cfg::kMasterMusic);
    a.ready = true;
}

void UnloadAudioBank(AudioBank& a) {
    if (!a.ready) return;
    for (auto& s : a.sfx) UnloadSound(s);
    UnloadSound(a.music);
    a.ready = false;
}

void PlaySfx(const AudioBank& a, Sfx id, float pitch) {
    if (!a.ready) return;
    const Sound& s = a.sfx[(int)id];
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void TickMusic(AudioBank& a) {
    if (!a.ready) return;
    double now = GetTime();
    if (a.musicStartedAt < 0 || now - a.musicStartedAt >= (double)a.musicLen - 0.02) {
        PlaySound(a.music);
        a.musicStartedAt = now;
    }
}
