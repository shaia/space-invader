#include "content.h"
#include "game.h"
#include "render.h"
#include <cmath>

void PlayerFire(Game& g) {
    Player& p = g.player;
    if (!p.alive || p.fireCooldown > 0.0f) return;

    int maxShots = g.fx.rapid > 0 ? 4 : 1;
    int mine = 0;
    for (const auto& s : g.shots)
        if (s.fromPlayer) mine++;
    if (mine >= maxShots) return;

    Vector2 muzzle = {p.pos.x, p.pos.y - cfg::kPlayerH};

    if (g.fx.bigShotArmed) {
        g.fx.bigShotArmed = false;
        g.stats.shotsFired++;
        Shot s;
        s.pos = muzzle;
        s.vel = {0, -140.0f};
        s.kind = ShotKind::BigShot;
        s.fromPlayer = true;
        s.pierce = true;
        g.shots.push_back(s);
        PlaySfx(*g.audio, Sfx::BigShot);
    } else {
        int n = g.fx.spread > 0 ? 3 : 1;
        g.stats.shotsFired += n;
        for (int i = 0; i < n; i++) {
            float ang = (i - (n - 1) / 2.0f) * 0.16f;
            Shot s;
            s.pos = muzzle;
            s.vel = {sinf(ang) * cfg::kShotSpeed, -cosf(ang) * cfg::kShotSpeed};
            s.kind = ShotKind::PlayerShot;
            s.fromPlayer = true;
            s.pierce = g.fx.pierce > 0;
            g.shots.push_back(s);
        }
        PlaySfx(*g.audio, Sfx::Shoot, g.rng.range(0.95f, 1.05f));
    }

    SpawnMuzzle(g, muzzle, g.fx.pierce > 0 ? cfg::kColAccent : cfg::kColShot);
    p.fireCooldown = g.fx.rapid > 0 ? 0.13f : 0.05f;
    p.squash = 0.5f;
    g.noShootTimer = -1000.0f;  // pacifist window closed for this run

    if (g.fx.freeze > 0) TryAward(g, Ach::CeasefireViolation);

    // Mirror Match: the ghost cannon files a countersuit
    if (CurrentMod(g).mirrorCannon) {
        Shot s;
        s.pos = {cfg::kCanvasW - p.pos.x, cfg::kHudTopH + 26.0f};
        s.vel = {0, cfg::kBombSpeed * 1.4f};
        s.kind = ShotKind::Bomb;
        s.fromPlayer = false;
        g.shots.push_back(s);
    }
}

void UpdatePlayer(Game& g, float dt) {
    Player& p = g.player;
    if (p.fireCooldown > 0) p.fireCooldown -= dt;
    if (p.invuln > 0) p.invuln -= dt;
    if (p.squash > 0) p.squash = fmaxf(0.0f, p.squash - dt * 4.0f);

    if (!p.alive) {
        p.deathTimer -= dt;
        if (p.deathTimer <= 0 && g.lives > 0) {
            p.alive = true;
            p.pos.x = cfg::kCanvasW / 2.0f;
            p.invuln = cfg::kRespawnInvuln;
        }
        return;
    }

    float dir = 0.0f;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) dir -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dir += 1.0f;
    if (CurrentMod(g).invertInput) dir = -dir;

    p.pos.x += dir * cfg::kPlayerSpeed * dt;
    float half = cfg::kPlayerW / 2.0f + 6.0f;
    if (p.pos.x < half) p.pos.x = half;
    if (p.pos.x > cfg::kCanvasW - half) p.pos.x = cfg::kCanvasW - half;

    if (IsKeyDown(KEY_SPACE)) PlayerFire(g);
}

void HitPlayer(Game& g, std::string_view cause, int anchor) {
    Player& p = g.player;
    if (!p.alive || p.invuln > 0) return;

    // any hit that lands breaks the productivity streak
    if (g.combo.tier >= 1) {
        PushToast(g, content::kComboReset);
        g.comboBroken = true;
    }
    g.combo.chain = 0; g.combo.tier = 0; g.combo.timer = 0;

    if (p.shieldHits > 0) {
        p.shieldHits--;
        p.invuln = 0.5f;
        SpawnExplosion(g, p.pos, cfg::kColUfo, 12);
        PlaySfx(*g.audio, Sfx::Crunch, 1.3f);
        if (p.shieldHits == 0) PushToast(g, "Paperwork expired.");
        return;
    }

    if (cause == "compliment")
        TryAward(g, Ach::FriendlyFire);

    p.alive = false;
    p.deathTimer = cfg::kDeathFreeze;
    g.lives--;
    g.shake = fmaxf(g.shake, 12.0f);
    SpawnExplosion(g, p.pos, cfg::kColPlayer, 40);
    SpawnDebris(g, p.pos, cfg::kColPlayer, 14);
    PlaySfx(*g.audio, Sfx::PlayerDie);

    g.stats.incidents++;
    if (g.lives > 0) {
        // the killer files an exit interview during the death freeze; if there's no
        // identifiable killer (debug, mirror cannon) a survivor just shrugs
        if (anchor != kBubbleAnchorFixed) {
            PushBubble(g, anchor,
                       content::kExitInterview[g.rng.irange(0, content::kExitInterviewCount - 1)],
                       2.4f);
        } else {
            int idx = RandomAliveInvader(g);
            if (idx >= 0) PushBubble(g, idx, content::kPlayerDown, 2.5f);
        }
        PushToast(g, TextFormat("INCIDENT REPORT #%d FILED", g.stats.incidents));
    } else {
        g.gameOver = true;
    }
}
