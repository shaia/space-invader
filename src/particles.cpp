#include "game.h"
#include "render.h"
#include <cmath>

namespace {
void Push(Game& g, const Particle& p) {
    if ((int)g.particles.size() >= cfg::kMaxParticles) return;
    g.particles.push_back(p);
}
} // namespace

void SpawnExplosion(Game& g, Vector2 pos, Color c, int count) {
    for (int i = 0; i < count; i++) {
        float a = g.rng.range(0, 6.2831f);
        float sp = g.rng.range(40, 260);
        Particle p;
        p.pos = pos;
        p.vel = {cosf(a) * sp, sinf(a) * sp};
        p.maxLife = p.life = g.rng.range(0.25f, 0.7f);
        p.size = g.rng.range(1.5f, 4.0f);
        p.color = c;
        p.kind = ParticleKind::Spark;
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
    const Color palette[] = {{255, 90, 200, 255}, {90, 200, 255, 255},
                             {255, 220, 90, 255}, {80, 255, 140, 255}};
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = pos;
        p.vel = {g.rng.range(-160, 160), g.rng.range(-320, -80)};
        p.gravity = 260.0f;
        p.maxLife = p.life = g.rng.range(0.9f, 1.9f);
        p.size = g.rng.range(3.0f, 6.0f);
        p.color = palette[g.rng.irange(0, 3)];
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
        default:
            DrawCircleV(p.pos, p.size * a, WithAlpha(p.color, a));
            break;
        }
    }
}
