#include "content.h"
#include "game.h"
#include "render.h"
#include <cmath>

namespace {

// sizes
constexpr float kKarenW = 150, kKarenH = 54, kKarenY = 160;
constexpr float kSaucerW = 70, kSaucerH = 26, kSignW = 78, kSignH = 42;
constexpr float kProdW = 200, kProdH = 140, kProdY = 200;

float ScaleForWave(int wave) {
    int round = (wave / cfg::kBossEvery - 1) / 4;  // HP scales up every four boss rounds
    return 1.0f + 0.35f * (float)round;
}

BossKind KindForWave(int wave) {
    int slot = (wave / cfg::kBossEvery - 1) % 4;  // Karen / Local / Producer / Lawyer
    return (BossKind)slot;
}

void BossSay(Game& g, std::string_view line) {
    PushBubble(g, kBubbleAnchorBoss, line, 3.5f);
    PlaySfx(*g.audio, Sfx::BossRoar, 1.2f);
}

void CheckDialogue(Game& g) {
    Boss& b = g.boss;
    float f = b.hp / b.maxHp;
    const char *l75 = nullptr, *l50 = nullptr, *l25 = nullptr;
    switch (b.kind) {
    case BossKind::Karen:     l75 = content::kKaren75; l50 = content::kKaren50; l25 = content::kKaren25; break;
    case BossKind::Local1978: l75 = content::kLocal75; l50 = content::kLocal50; l25 = content::kLocal25; break;
    case BossKind::Producer:  l75 = content::kProd75;  l50 = content::kProd50;  l25 = content::kProd25;  break;
    case BossKind::Lawyer:    l75 = content::kLawyer75; l50 = content::kLawyer50; l25 = content::kLawyer25; break;
    }
    bool crossed = false;
    if (f <= 0.75f && !b.saidAt75) { b.saidAt75 = true; BossSay(g, l75); crossed = true; }
    if (f <= 0.50f && !b.saidAt50) { b.saidAt50 = true; BossSay(g, l50); crossed = true; }
    if (f <= 0.25f && !b.saidAt25) { b.saidAt25 = true; BossSay(g, l25); crossed = true; }
    if (crossed) g.hitStop = fmaxf(g.hitStop, cfg::kHitStopBossPhase);
}

void BossDefeated(Game& g) {
    Boss& b = g.boss;
    b.active = false;
    g.stats.bossesDefeated++;
    AddScore(g, cfg::kBossBonusPer * g.wave.number);
    SpawnConfetti(g, b.pos, 120);
    SpawnExplosion(g, b.pos, cfg::kColUfo, 60);
    g.shake = 14.0f;
    g.hitStop = fmaxf(g.hitStop, cfg::kHitStopBossKill);
    PlaySfx(*g.audio, Sfx::BossRoar, 0.7f);

    const char* death = content::kKarenDeath;
    if (b.kind == BossKind::Local1978) death = content::kLocalDeath;
    if (b.kind == BossKind::Producer) death = content::kProdDeath;
    if (b.kind == BossKind::Lawyer) death = content::kLawyerDeath;
    PushToast(g, death);

    if (b.kind == BossKind::Karen) TryAward(g, Ach::SpeakToTheManager);
    if (b.kind == BossKind::Local1978 && g.lives == b.livesAtStart)
        TryAward(g, Ach::UnionBuster);

    // clear leftover boss projectiles kindly
    std::erase_if(g.shots, [](const Shot& s) { return !s.fromPlayer; });
    g.wave.clearing = true;
    g.wave.intermission = cfg::kIntermission + 1.0f;
    OfferMemos(g);  // sign one perk-with-a-catch before the next wave
}

void EnemyShot(Game& g, Vector2 pos, Vector2 vel, ShotKind kind) {
    Shot s;
    s.pos = pos;
    s.vel = vel;
    s.kind = kind;
    s.fromPlayer = false;
    s.owner = kBubbleAnchorBoss;  // all boss/minion fire files under management
    if (kind == ShotKind::Clipboard) s.spin = g.rng.range(0, 360);
    g.shots.push_back(s);
}

// ---------- KAREN ----------
void UpdateKaren(Game& g, float dt) {
    Boss& b = g.boss;
    b.timer += dt;

    switch (b.phase) {
    case 0: {  // sweep + occasional pot shots
        b.pos.x = cfg::kCanvasW / 2.0f + sinf(b.timer * 0.8f) * 240.0f;
        b.pos.y = kKarenY + sinf(b.timer * 2.1f) * 12.0f;
        b.attackTimer -= dt;
        if (b.attackTimer <= 0) {
            b.attackTimer = 1.1f;
            EnemyShot(g, {b.pos.x, b.pos.y + kKarenH / 2},
                      {g.rng.range(-40, 40), cfg::kBombSpeed * 1.1f}, ShotKind::Beamlet);
        }
        if (b.timer > 3.5f) {
            b.timer = 0;
            b.attackCycle = (b.attackCycle % 3) + 1;
            b.phase = b.attackCycle;
            if (b.phase == 1) {  // arm the laser over the player
                b.laser.active = true;
                b.laser.x = g.player.pos.x;
                b.laser.t = 0;
                b.laser.telegraph = 0.9f;
                b.laser.width = 26.0f;
            }
        }
        break;
    }
    case 1: {  // laser: telegraph then fire
        BeamAttack& L = b.laser;
        L.t += dt;
        b.pos.x += (L.x - b.pos.x) * fminf(1.0f, dt * 6.0f);
        if (L.t > L.telegraph && L.t < L.telegraph + 0.6f) {
            if (g.player.alive && fabsf(g.player.pos.x - L.x) < L.width / 2 + cfg::kPlayerW * 0.3f)
                HitPlayer(g, "laser", kBubbleAnchorBoss);
            CarveBunkers(g, {L.x, cfg::kBunkerY + 20}, cfg::kCarveBoss * dt * 8.0f);
            g.shake = fmaxf(g.shake, 3.0f);
        }
        if (L.t >= L.telegraph + 0.6f) {
            L.active = false;
            b.phase = 0;
            b.timer = 0;
            b.attackTimer = 0.8f;
        }
        break;
    }
    case 2: {  // hire assistants
        int alive = 0;
        for (const auto& m : b.minions)
            if (m.alive) alive++;
        if (alive < 6 && b.timer < 2.0f) {
            if (b.attackTimer <= 0) {
                Minion m;
                m.alive = true;
                m.basePos = {b.pos.x + g.rng.range(-120, 120), b.pos.y + 60};
                m.pos = m.basePos;
                m.t = g.rng.range(0, 6.28f);
                b.minions.push_back(m);
                b.attackTimer = 0.3f;
            }
            b.attackTimer -= dt;
        }
        if (b.timer > 2.5f) { b.phase = 0; b.timer = 0; b.attackTimer = 1.0f; }
        break;
    }
    case 3: {  // the ram ("I'm coming down there")
        float targetY = cfg::kBunkerY - 120.0f;
        if (b.timer < 1.4f) {
            b.pos.x += (g.player.pos.x - b.pos.x) * fminf(1.0f, dt * 3.0f);
            b.pos.y += (targetY - b.pos.y) * fminf(1.0f, dt * 2.4f);
        } else {
            b.pos.y += (kKarenY - b.pos.y) * fminf(1.0f, dt * 2.0f);
            if (b.timer > 2.8f) { b.phase = 0; b.timer = 0; b.attackTimer = 1.0f; }
        }
        break;
    }
    }

    // minions swoop and snipe
    for (auto& m : b.minions) {
        if (!m.alive) continue;
        m.t += dt;
        m.pos.x = m.basePos.x + sinf(m.t * 2.2f) * 90.0f;
        m.pos.y = m.basePos.y + sinf(m.t * 3.1f) * 40.0f + m.t * 6.0f;
        if (g.rng.chance(0.004f))
            EnemyShot(g, m.pos, {0, cfg::kBombSpeed}, ShotKind::Beamlet);
        if (m.pos.y > cfg::kBunkerY - 40) m.alive = false;  // drifts off shift
    }
    std::erase_if(b.minions, [](const Minion& m) { return !m.alive; });
}

// ---------- LOCAL 1978 ----------
void UpdateLocal(Game& g, float dt) {
    Boss& b = g.boss;
    b.timer += dt;
    b.pos.x = cfg::kCanvasW / 2.0f + sinf(b.timer * 0.7f) * 150.0f;
    b.pos.y = kKarenY + sinf(b.timer * 1.7f) * 16.0f;

    b.attackTimer -= dt;
    if (b.attackTimer <= 0) {
        b.attackTimer = g.rng.range(1.4f, 2.2f);
        // a random member of the local throws a clipboard at the player
        int order[3] = {g.rng.irange(0, 2), 0, 0};
        order[1] = (order[0] + 1) % 3;
        order[2] = (order[0] + 2) % 3;
        for (int i = 0; i < 3; i++) {
            const Saucer& s = b.saucers[order[i]];
            if (!s.alive) continue;
            Vector2 at = {b.pos.x + s.offset.x, b.pos.y + s.offset.y};
            Vector2 d = {g.player.pos.x - at.x, g.player.pos.y - at.y};
            float len = sqrtf(d.x * d.x + d.y * d.y);
            if (len < 1) len = 1;
            float sp = 250.0f;
            EnemyShot(g, at, {d.x / len * sp, d.y / len * sp}, ShotKind::Clipboard);
            break;
        }
    }
}

// ---------- PRODUCER ----------
void UpdateProducer(Game& g, float dt) {
    Boss& b = g.boss;
    b.timer += dt;
    b.pos.x = cfg::kCanvasW / 2.0f + sinf(b.timer * 0.4f) * 120.0f;
    b.pos.y = kProdY;

    // DEADLINE bricks
    b.brickTimer -= dt;
    if (b.brickTimer <= 0) {
        b.brickTimer = g.rng.range(2.0f, 3.2f);
        float x = g.rng.range(cfg::kPlayfieldMargin + 40, cfg::kCanvasW - cfg::kPlayfieldMargin - 40);
        EnemyShot(g, {x, b.pos.y + kProdH / 2}, {0, 240.0f}, ShotKind::Brick);
    }

    // scope-creep beam
    BeamAttack& c = b.creep;
    if (!c.active) {
        b.attackTimer -= dt;
        if (b.attackTimer <= 0) {
            c.active = true;
            c.t = 0;
            c.telegraph = 1.0f;
            c.x = g.rng.range(cfg::kPlayfieldMargin + 60, cfg::kCanvasW - cfg::kPlayfieldMargin - 60);
            c.width = 8.0f;
        }
    } else {
        c.t += dt;
        if (c.t > c.telegraph) {
            c.width = 8.0f + (c.t - c.telegraph) * 46.0f;  // the scope creeps
            if (g.player.alive && fabsf(g.player.pos.x - c.x) < c.width / 2)
                HitPlayer(g, "scope creep", kBubbleAnchorBoss);
            if (c.t > c.telegraph + 2.5f) {
                c.active = false;
                b.attackTimer = g.rng.range(2.5f, 4.0f);
            }
        }
    }
}

// ---------- THE LAWYER ----------
constexpr float kLawyerW = 150, kLawyerH = 54;

void UpdateLawyer(Game& g, float dt) {
    Boss& b = g.boss;
    b.timer += dt;

    // once below half health, table a destructible settlement briefcase
    if (!b.settlementUsed && b.settlementHp <= 0 && b.hp <= b.maxHp * 0.5f) {
        b.settlementHp = 10.0f * ScaleForWave(g.wave.number);
        b.settlementPos = {g.rng.range(cfg::kPlayfieldMargin + 90, cfg::kCanvasW - cfg::kPlayfieldMargin - 90),
                           kKarenY + 240.0f};
        PushToast(g, "Settlement tabled. Shoot to accept.");
    }

    switch (b.phase) {
    case 0: {  // sweep + cease-and-desist fans (paper planes)
        b.pos.x = cfg::kCanvasW / 2.0f + sinf(b.timer * 0.9f) * 220.0f;
        b.pos.y = kKarenY + sinf(b.timer * 1.9f) * 12.0f;
        b.attackTimer -= dt;
        if (b.attackTimer <= 0) {
            b.attackTimer = 1.8f;
            for (int i = -2; i <= 2; i++) {  // five-way downward spread
                float ang = 1.5708f + (float)i * 0.25f;
                float sp = cfg::kBombSpeed * 0.95f;
                EnemyShot(g, {b.pos.x, b.pos.y + kLawyerH / 2},
                          {cosf(ang) * sp, sinf(ang) * sp}, ShotKind::PaperPlane);
            }
        }
        if (b.timer > 3.5f) { b.timer = 0; b.phase = 1; b.attackTimer = 0.6f; }
        break;
    }
    case 1: {  // homing subpoenas: slow, tanky, one player-hit to destroy
        b.pos.x += (g.player.pos.x - b.pos.x) * fminf(1.0f, dt * 1.5f);
        b.attackTimer -= dt;
        if (b.attackTimer <= 0) {
            b.attackTimer = 1.2f;
            Vector2 at = {b.pos.x, b.pos.y + kLawyerH / 2};
            Vector2 d = {g.player.pos.x - at.x, g.player.pos.y - at.y};
            float len = sqrtf(d.x * d.x + d.y * d.y);
            if (len < 1) len = 1;
            Shot s;
            s.pos = at;
            s.vel = {d.x / len * 140.0f, d.y / len * 140.0f};
            s.kind = ShotKind::Subpoena;
            s.fromPlayer = false;
            s.owner = kBubbleAnchorBoss;
            s.hp = 2;  // takes two player shots to quash
            g.shots.push_back(s);
        }
        if (b.timer > 3.0f) { b.timer = 0; b.phase = 2; b.objection = false; b.objectionT = 0; }
        break;
    }
    case 2: {  // OBJECTION: brief invulnerability, then a radial counter-burst
        b.pos.x += (cfg::kCanvasW / 2.0f - b.pos.x) * fminf(1.0f, dt * 2.0f);
        b.objection = true;
        b.objectionT += dt;
        if (b.objectionT > 1.2f) {
            for (int i = 0; i < 12; i++) {  // sustained; overruled
                float ang = (float)i * (6.2831f / 12.0f);
                EnemyShot(g, b.pos, {cosf(ang) * 180.0f, sinf(ang) * 180.0f}, ShotKind::Beamlet);
            }
            b.objection = false;
            b.phase = 0;
            b.timer = 0;
            b.attackTimer = 1.0f;
        }
        break;
    }
    }

    // subpoena homing: a limited turn toward the player each frame
    for (auto& s : g.shots) {
        if (s.fromPlayer || s.kind != ShotKind::Subpoena) continue;
        Vector2 d = {g.player.pos.x - s.pos.x, g.player.pos.y - s.pos.y};
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 1) len = 1;
        Vector2 want = {d.x / len * 140.0f, d.y / len * 140.0f};
        float steer = fminf(1.0f, dt * 1.2f);
        s.vel.x += (want.x - s.vel.x) * steer;
        s.vel.y += (want.y - s.vel.y) * steer;
        s.spin += 200.0f * dt;
    }
}

} // namespace

void StartBoss(Game& g) {
    Boss& b = g.boss;
    b = Boss{};
    b.active = true;
    b.kind = KindForWave(g.wave.number);
    b.livesAtStart = g.lives;
    b.pos = {cfg::kCanvasW / 2.0f, kKarenY};
    b.attackTimer = 2.0f;
    b.brickTimer = 2.5f;
    float mult = ScaleForWave(g.wave.number);

    const char* intro = content::kKarenIntro;
    switch (b.kind) {
    case BossKind::Karen:
        b.maxHp = 60.0f * mult;
        intro = content::kKarenIntro;
        break;
    case BossKind::Local1978: {
        float signHp = 8.0f * mult, hullHp = 10.0f * mult;
        b.saucers[0] = {{-190, 0}, signHp, hullHp, true};
        b.saucers[1] = {{0, -14}, signHp, hullHp, true};
        b.saucers[2] = {{190, 0}, signHp, hullHp, true};
        b.maxHp = 3 * (signHp + hullHp);
        intro = content::kLocalIntro;
        break;
    }
    case BossKind::Producer:
        b.maxHp = 90.0f * mult;
        b.pos.y = kProdY;
        intro = content::kProdIntro;
        break;
    case BossKind::Lawyer:
        b.maxHp = 80.0f * mult;
        intro = content::kLawyerIntro;
        break;
    }
    b.hp = b.maxHp;
    Announce(g, "MANAGEMENT", intro, 3.2f);
    PlaySfx(*g.audio, Sfx::BossRoar);
}

void UpdateBoss(Game& g, float dt) {
    Boss& b = g.boss;
    if (!b.active) return;
    if (b.hp <= 0) {  // died to something other than a direct shot (e.g. debug skip)
        BossDefeated(g);
        return;
    }
    if (b.squash > 0) b.squash = fmaxf(0.0f, b.squash - dt * 4.0f);

    switch (b.kind) {
    case BossKind::Karen: UpdateKaren(g, dt); break;
    case BossKind::Local1978: UpdateLocal(g, dt); break;
    case BossKind::Producer: UpdateProducer(g, dt); break;
    case BossKind::Lawyer: UpdateLawyer(g, dt); break;
    }
}

bool BossTouchesPlayer(const Game& g) {
    const Boss& b = g.boss;
    if (!b.active || !g.player.alive) return false;
    Rectangle pr = PlayerRect(g);
    if (b.kind == BossKind::Karen) {
        Rectangle br = {b.pos.x - kKarenW / 2, b.pos.y - kKarenH / 2, kKarenW, kKarenH};
        return CheckCollisionRecs(pr, br);
    }
    return false;  // the others don't ram
}

bool BossShotHit(Game& g, const Shot& s) {
    Boss& b = g.boss;
    if (!b.active) return false;
    float dmg = s.kind == ShotKind::BigShot ? 8.0f : 1.0f;

    // Karen's assistants take the hit for her
    if (b.kind == BossKind::Karen) {
        for (auto& m : b.minions) {
            if (!m.alive) continue;
            Rectangle mr = {m.pos.x - 14, m.pos.y - 10, 28, 20};
            if (CheckCollisionPointRec(s.pos, mr) ||
                CheckCollisionRecs({s.pos.x - 2, s.pos.y - 7, 4, 14}, mr)) {
                m.alive = false;
                ComboKill(g, {m.pos.x, m.pos.y - 14.0f}, cfg::kPtsMinion, cfg::kColRow[1]);
                SpawnExplosion(g, m.pos, cfg::kColRow[1], 10);
                PlaySfx(*g.audio, Sfx::Pop, 1.3f);
                return !s.pierce;
            }
        }
        Rectangle br = {b.pos.x - kKarenW / 2, b.pos.y - kKarenH / 2, kKarenW, kKarenH};
        if (CheckCollisionPointRec(s.pos, br)) {
            b.hp -= dmg;
            b.squash = 0.4f;
            SpawnExplosion(g, s.pos, cfg::kColUfo, 8);
            PlaySfx(*g.audio, Sfx::BossHit);
            CheckDialogue(g);
            if (b.hp <= 0) BossDefeated(g);
            return true;
        }
        return false;
    }

    if (b.kind == BossKind::Local1978) {
        for (auto& sc : b.saucers) {
            if (!sc.alive) continue;
            Vector2 at = {b.pos.x + sc.offset.x, b.pos.y + sc.offset.y};
            Rectangle signR = {at.x - kSignW / 2, at.y + kSaucerH / 2, kSignW, kSignH};
            Rectangle hullR = {at.x - kSaucerW / 2, at.y - kSaucerH / 2, kSaucerW, kSaucerH};
            if (sc.signHp > 0 && CheckCollisionPointRec(s.pos, signR)) {
                sc.signHp -= dmg;
                b.hp -= dmg;
                SpawnDebris(g, s.pos, cfg::kColSign, 4);
                PlaySfx(*g.audio, Sfx::Crunch);
                if (sc.signHp <= 0) PushToast(g, "Sign down. Grievance pending.");
                CheckDialogue(g);
                if (b.hp <= 0) BossDefeated(g);
                return true;
            }
            if (sc.signHp <= 0 && CheckCollisionPointRec(s.pos, hullR)) {
                sc.hp -= dmg;
                b.hp -= dmg;
                b.squash = 0.4f;
                SpawnExplosion(g, s.pos, cfg::kColUfo, 8);
                PlaySfx(*g.audio, Sfx::BossHit);
                if (sc.hp <= 0) {
                    sc.alive = false;
                    SpawnExplosion(g, at, cfg::kColUfo, 30);
                }
                CheckDialogue(g);
                if (b.hp <= 0) BossDefeated(g);
                return true;
            }
        }
        return false;
    }

    if (b.kind == BossKind::Lawyer) {
        // the settlement briefcase: shooting it out withdraws an objection and
        // deals a chunk of damage ("Settled out of court.")
        if (b.settlementHp > 0) {
            Rectangle sr = {b.settlementPos.x - 28, b.settlementPos.y - 20, 56, 40};
            if (CheckCollisionPointRec(s.pos, sr)) {
                b.settlementHp -= dmg;
                SpawnDebris(g, s.pos, cfg::kColClipboard, 4);
                PlaySfx(*g.audio, Sfx::Crunch);
                if (b.settlementHp <= 0) {
                    b.settlementUsed = true;
                    b.objection = false;  // motion withdrawn
                    b.hp -= b.maxHp * 0.15f;
                    SpawnExplosion(g, b.settlementPos, cfg::kColAccent, 24);
                    PushToast(g, "Settled out of court.");
                    CheckDialogue(g);
                    if (b.hp <= 0) { BossDefeated(g); return true; }
                }
                return true;
            }
        }
        Rectangle br = {b.pos.x - kLawyerW / 2, b.pos.y - kLawyerH / 2, kLawyerW, kLawyerH};
        if (CheckCollisionPointRec(s.pos, br)) {
            if (b.objection) {  // OBJECTION: evidence inadmissible, no damage
                SpawnExplosion(g, s.pos, cfg::kColAccent, 4);
                PlaySfx(*g.audio, Sfx::Blip, 0.6f);
                return true;
            }
            b.hp -= dmg;
            b.squash = 0.4f;
            SpawnExplosion(g, s.pos, cfg::kColUfo, 8);
            PlaySfx(*g.audio, Sfx::BossHit);
            CheckDialogue(g);
            if (b.hp <= 0) BossDefeated(g);
            return true;
        }
        return false;
    }

    // Producer
    Rectangle br = {b.pos.x - kProdW / 2, b.pos.y - kProdH / 2, kProdW, kProdH};
    if (CheckCollisionPointRec(s.pos, br)) {
        b.hp -= dmg;
        b.squash = 0.3f;
        SpawnExplosion(g, s.pos, cfg::kColHurt, 8);
        PlaySfx(*g.audio, Sfx::BossHit);
        CheckDialogue(g);
        if (b.hp <= 0) BossDefeated(g);
        return true;
    }
    return false;
}

void DrawBoss(const Game& g) {
    const Boss& b = g.boss;
    if (!b.active) return;
    float sq = b.squash;

    switch (b.kind) {
    case BossKind::Karen: {
        DrawUfoArt(b.pos, kKarenW * (1 + sq * 0.2f), kKarenH * (1 - sq * 0.2f), cfg::kColUfo, g.time);
        // the haircut (speaks for itself)
        DrawRectangleRec({b.pos.x - 22, b.pos.y - kKarenH * 0.95f, 44, 16}, cfg::kColHair);
        DrawRectangleRec({b.pos.x - 30, b.pos.y - kKarenH * 0.75f, 18, 10}, cfg::kColHair);
        // laser telegraph / beam
        if (b.phase == 1 && b.laser.active) {
            const BeamAttack& L = b.laser;
            if (L.t <= L.telegraph) {
                float a = 0.25f + 0.5f * fmodf(L.t * 6.0f, 1.0f);
                GlowLine({L.x, b.pos.y + kKarenH / 2}, {L.x, (float)cfg::kCanvasH}, 2,
                         WithAlpha(cfg::kColHurt, a));
            } else {
                GlowLine({L.x, b.pos.y + kKarenH / 2}, {L.x, (float)cfg::kCanvasH}, L.width,
                         cfg::kColHurt);
            }
        }
        for (const auto& m : b.minions)
            if (m.alive)
                DrawInvaderArt(m.pos, 28, 20, 2, ((int)(m.t * 4)) & 1, 0, cfg::kColRow[1],
                               false, g.time, (int)(m.basePos.x));
        break;
    }
    case BossKind::Local1978: {
        for (int i = 0; i < 3; i++) {
            const Saucer& sc = b.saucers[i];
            if (!sc.alive) continue;
            Vector2 at = {b.pos.x + sc.offset.x, b.pos.y + sc.offset.y};
            DrawUfoArt(at, kSaucerW, kSaucerH, cfg::kColUfo, g.time + i);
            if (sc.signHp > 0) {
                Rectangle signR = {at.x - kSignW / 2, at.y + kSaucerH / 2, kSignW, kSignH};
                DrawRectangleRec({signR.x + 36, signR.y - 8, 4, 10}, {160, 130, 90, 255});
                GlowRect(signR, cfg::kColSign);
                const char* txt = i == 0 ? "FAIR\nWAGES" : (i == 1 ? "NO PIXELS\nNO PEACE" : "SCALE\nPAY");
                DrawText(txt, (int)signR.x + 6, (int)signR.y + 6, 10, {60, 50, 40, 255});
            }
        }
        break;
    }
    case BossKind::Producer: {
        Rectangle br = {b.pos.x - kProdW / 2 * (1 + sq * 0.15f), b.pos.y - kProdH / 2,
                        kProdW * (1 + sq * 0.15f), kProdH};
        GlowRect(br, cfg::kColProducer);
        Rectangle screen = {br.x + 14, br.y + 12, br.width - 28, br.height - 44};
        DrawRectangleRec(screen, cfg::kColProducerScreen);
        // the burndown chart burns up
        float t = fmodf(g.time * 0.25f, 1.0f);
        for (int i = 0; i < 6; i++) {
            float x0 = screen.x + 8 + i * (screen.width - 16) / 6.0f;
            float hpx = 10 + (i * 12 + t * 30);
            if (hpx > screen.height - 12) hpx = screen.height - 12;
            DrawRectangleRec({x0, screen.y + screen.height - 6 - hpx,
                              (screen.width - 16) / 7.0f, hpx}, cfg::kColHurt);
        }
        DrawText("Q3 BURNDOWN", (int)screen.x + 8, (int)screen.y + 4, 10, {150, 160, 180, 255});
        DrawRectangleRec({b.pos.x - 20, br.y + br.height, 40, 18}, cfg::kColProducer);
        // scope-creep beam
        if (b.creep.active) {
            const BeamAttack& c = b.creep;
            if (c.t <= c.telegraph) {
                float a = 0.25f + 0.5f * fmodf(c.t * 5.0f, 1.0f);
                GlowLine({c.x, b.pos.y + kProdH / 2}, {c.x, (float)cfg::kCanvasH}, 2,
                         WithAlpha(cfg::kColScopeCreep, a));
                DrawText("SCOPE", (int)c.x - 18, (int)(b.pos.y + kProdH / 2 + 8), 12, cfg::kColScopeCreep);
            } else {
                GlowLine({c.x, b.pos.y + kProdH / 2}, {c.x, (float)cfg::kCanvasH}, c.width,
                         WithAlpha(cfg::kColScopeCreep, 0.85f));
            }
        }
        break;
    }
    case BossKind::Lawyer: {
        float w = kLawyerW * (1 + sq * 0.15f), h = kLawyerH * (1 - sq * 0.1f);
        Rectangle body = {b.pos.x - w / 2, b.pos.y - h / 2, w, h};
        GlowRect(body, cfg::kColProducer);                              // briefcase-gray chassis
        DrawRectangleRec({b.pos.x - 18, body.y - 8, 36, 8}, {60, 62, 80, 255});  // handle
        DrawRectangleRec({b.pos.x - 6, b.pos.y - 6, 12, 10}, cfg::kColHurt);      // tie knot
        DrawRectangleRec({b.pos.x - 4, b.pos.y + 4, 8, h / 2}, cfg::kColHurt);    // tie
        DrawRectangleRec({b.pos.x - 30, b.pos.y - 8, 8, 8}, RAYWHITE);            // eyes
        DrawRectangleRec({b.pos.x + 22, b.pos.y - 8, 8, 8}, RAYWHITE);
        if (b.objection) {  // OBJECTION shield + banner
            GlowCircle(b.pos, w * 0.75f, WithAlpha(cfg::kColAccent, 0.18f));
            const char* obj = "OBJECTION!";
            int ow = MeasureText(obj, 26);
            GlowText(obj, (int)(b.pos.x - ow / 2), (int)(b.pos.y - h - 30), 26, cfg::kColAccent);
        }
        if (b.settlementHp > 0) {  // the destructible settlement offer
            Rectangle sr = {b.settlementPos.x - 28, b.settlementPos.y - 20, 56, 40};
            GlowRect(sr, cfg::kColClipboard);
            DrawText("SETTLE", (int)b.settlementPos.x - 22, (int)b.settlementPos.y - 6, 12, cfg::kColBg);
        }
        break;
    }
    }

    // health bar
    float frac = b.hp / b.maxHp;
    if (frac < 0) frac = 0;
    float bw = 380, bx = (cfg::kCanvasW - bw) / 2, by = cfg::kHudTopH + 8;
    DrawRectangleRec({bx - 2, by - 2, bw + 4, 14}, {30, 30, 46, 255});
    DrawRectangleRec({bx, by, bw * frac, 10},
                     frac > 0.5f ? cfg::kColAccent : cfg::kColHurt);
    const char* name = b.kind == BossKind::Karen ? "MOTHERSHIP KAREN"
                     : b.kind == BossKind::Local1978 ? "UFO LOCAL 1978"
                     : b.kind == BossKind::Producer ? "THE PRODUCER" : "THE LAWYER";
    int w = MeasureText(name, 14);
    GlowText(name, (int)(cfg::kCanvasW / 2 - w / 2), (int)by + 16, 14, cfg::kColHud);
}
