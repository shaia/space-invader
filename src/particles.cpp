#include "game.h"
#include "render.h"
#include <cmath>

namespace {
void Push(Game& g, const Particle& p) {
    if ((int)g.particles.size() >= cfg::kMaxParticles) return;
    g.particles.push_back(p);
}
} // namespace

// brighten a color toward white by t (0..1) for hot spark cores
static Color Hotten(Color c, float t) {
    auto mix = [&](unsigned char v) { int x = (int)(v + (255 - v) * t); return (unsigned char)(x > 255 ? 255 : x); };
    return {mix(c.r), mix(c.g), mix(c.b), c.a};
}

void SpawnShockwave(Game& g, Vector2 pos, Color c, float radius) {
    Particle p;
    p.pos = pos;
    p.maxLife = p.life = 0.35f;
    p.size = radius;     // base radius; expands in draw
    p.color = c;
    p.kind = ParticleKind::Shockwave;
    Push(g, p);
}

void SpawnScorePop(Game& g, Vector2 pos, int points, Color c) {
    Particle p;
    p.pos = pos;
    p.vel = {0, -46.0f};
    p.maxLife = p.life = 0.9f;
    p.color = c;
    p.value = points;
    p.kind = ParticleKind::ScorePop;
    Push(g, p);
}

void SpawnMuzzle(Game& g, Vector2 pos, Color c) {
    // a bright flash plus a few upward sparks
    SpawnShockwave(g, pos, c, 5.0f);
    for (int i = 0; i < 6; i++) {
        Particle p;
        p.pos = pos;
        float a = -1.5708f + g.rng.range(-0.6f, 0.6f);
        float sp = g.rng.range(120, 260);
        p.vel = {cosf(a) * sp, sinf(a) * sp};
        p.maxLife = p.life = g.rng.range(0.12f, 0.28f);
        p.size = g.rng.range(1.5f, 3.0f);
        p.color = Hotten(c, 0.5f);
        p.kind = ParticleKind::Spark;
        Push(g, p);
    }
}

void SpawnExplosion(Game& g, Vector2 pos, Color c, int count) {
    SpawnShockwave(g, pos, c, 6.0f + count * 0.15f);
    for (int i = 0; i < count; i++) {
        float a = g.rng.range(0, 6.2831f);
        float sp = g.rng.range(40, 300);
        Particle p;
        p.pos = pos;
        p.vel = {cosf(a) * sp, sinf(a) * sp};
        p.maxLife = p.life = g.rng.range(0.25f, 0.75f);
        p.size = g.rng.range(1.5f, 4.2f);
        // hotter, whiter cores near the center; a few smoke puffs drifting up
        p.color = Hotten(c, g.rng.range(0.0f, 0.6f));
        p.kind = ParticleKind::Spark;
        Push(g, p);
    }
    int smoke = count / 4;
    for (int i = 0; i < smoke; i++) {
        Particle p;
        p.pos = pos;
        p.vel = {g.rng.range(-30, 30), g.rng.range(-60, -10)};
        p.maxLife = p.life = g.rng.range(0.5f, 1.1f);
        p.size = g.rng.range(4.0f, 8.0f);
        p.color = c;
        p.kind = ParticleKind::Smoke;
        Push(g, p);
    }
}

void SpawnDebris(Game& g, Vector2 pos, Color c, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = pos;
        p.vel = {g.rng.range(-120, 120), g.rng.range(-220, -40)};
        p.gravity = 500.0f;
        p.maxLife = p.life = g.rng.range(0.5f, 1.1f);
        p.size = g.rng.range(2.0f, 5.0f);
        p.color = c;
        p.kind = ParticleKind::Debris;
        Push(g, p);
    }
}

void SpawnConfetti(Game& g, Vector2 pos, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = pos;
        p.vel = {g.rng.range(-160, 160), g.rng.range(-320, -80)};
        p.gravity = 260.0f;
        p.maxLife = p.life = g.rng.range(0.9f, 1.9f);
        p.size = g.rng.range(3.0f, 6.0f);
        p.color = cfg::kColConfetti[g.rng.irange(0, 3)];
        p.kind = ParticleKind::Confetti;
        Push(g, p);
    }
}

void SpawnTrail(Game& g, Vector2 pos, Color c) {
    Particle p;
    p.pos = {pos.x + g.rng.range(-2, 2), pos.y};
    p.vel = {0, g.rng.range(20, 60)};
    p.maxLife = p.life = 0.25f;
    p.size = 2.0f;
    p.color = c;
    p.kind = ParticleKind::Trail;
    Push(g, p);
}

void UpdateParticles(Game& g, float dt) {
    for (auto& p : g.particles) {
        p.life -= dt;
        p.vel.y += p.gravity * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
    }
    std::erase_if(g.particles, [](const Particle& p) { return p.life <= 0.0f; });
}

void DrawParticles(const Game& g) {
    // additive pass: hot light (sparks, trails, smoke glow, shockwaves) feeds the bloom
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : g.particles) {
        float a = p.life / p.maxLife;
        switch (p.kind) {
        case ParticleKind::Spark:
        case ParticleKind::Trail:
            DrawCircleV(p.pos, p.size * a, WithAlpha(p.color, a));
            break;
        case ParticleKind::Smoke:
            DrawCircleV(p.pos, p.size * (1.4f - a), WithAlpha(p.color, a * 0.18f));
            break;
        case ParticleKind::Shockwave: {
            float t = 1.0f - a;                       // 0 -> 1 as it expands
            float rad = p.size * (0.4f + t * 3.2f);
            float thick = 1.0f + 3.0f * a;
            DrawRing(p.pos, rad - thick, rad, 0, 360, 40, WithAlpha(p.color, a * 0.7f));
            break;
        }
        default:
            break;
        }
    }
    EndBlendMode();

    // normal pass: solid debris / confetti / floating score numbers
    for (const auto& p : g.particles) {
        float a = p.life / p.maxLife;
        switch (p.kind) {
        case ParticleKind::Confetti: {
            float rot = p.life * 540.0f;
            DrawRectanglePro({p.pos.x, p.pos.y, p.size, p.size * 0.6f},
                             {p.size / 2, p.size * 0.3f}, rot, WithAlpha(p.color, a));
            break;
        }
        case ParticleKind::Debris:
            DrawRectangleRec({p.pos.x, p.pos.y, p.size, p.size}, WithAlpha(p.color, a));
            break;
        case ParticleKind::ScorePop: {
            const char* t = TextFormat("%d", p.value);
            int w = MeasureText(t, 16);
            GlowText(t, (int)p.pos.x - w / 2, (int)p.pos.y, 16, WithAlpha(p.color, a));
            break;
        }
        default:
            break;
        }
    }
}
