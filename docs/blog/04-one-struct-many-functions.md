# One Struct, Many Functions

*Post 4 of a series on the engineering behind* **SPACE INVADERS: WE HAVE DEMANDS**.
*Previously: [Every Sound Is a Function](03-every-sound-is-a-function.md).*

---

Posts 2 and 3 were about the "zero assets" half of this project — pictures and sound
conjured from code. This one is about the other half: the architecture that keeps a game
with bosses, power-ups, eight wave modifiers, particles, and a talking labor force from
turning into the usual tangle of managers, systems, and base classes.

There is no `EntityManager`. There is no `GameState` interface with virtual `enter()` and
`exit()`. There is no component registry. There is one struct that holds everything, a
pile of free functions that operate on it, and a `switch`. That's the whole design, and
it is a design — the simplicity is load-bearing, not accidental. The invaders find the
lack of hierarchy demeaning. They had hoped, at minimum, for a middle-management layer.
They got a flat `struct`. This post is about why that was the right call.

## The shape: state is data, behavior is functions

The entire mutable world is one aggregate, `Game` (we met it in post 1) — the player, a
fixed array of 55 invaders, vectors of shots and particles, the boss, the UFO, wave
state, score, lives, an RNG, and a borrowed pointer to the audio bank. No methods hang
off it. Every subsystem is a free function that takes `Game&` to mutate it or `const
Game&` to read it, and `game.h` is literally a table of contents grouped by the `.cpp`
that implements each block:

```cpp
// ---- game.cpp ----
void UpdatePlaying(Game& g, float dt);
void DrawPlaying(const Game& g);
void ResolveCollisions(Game& g);

// ---- invaders.cpp ----
void UpdateInvaders(Game& g, float dt);
void KillInvader(Game& g, int idx);

// ---- player.cpp ----
void UpdatePlayer(Game& g, float dt);
void HitPlayer(Game& g, std::string_view cause);
```

This separation — data in one place, behavior in functions that take it by reference — is
the whole idea, and everything good downstream falls out of it. The `const` vs non-`const`
`Game&` is doing real work as documentation: you can tell at the declaration whether
`DrawPlaying` can move an invader (it can't) or `UpdateInvaders` can (it must). There's no
encapsulation ceremony because there's nothing to encapsulate *from* — it's a
single-threaded game and the whole team is one struct.

## The payoff shows up when you start a run

The clearest dividend of "all state lives in one flat struct" is how you reset. Starting a
new run is *almost* `g = Game{}` — assign a default-constructed `Game` over the old one.
The only wrinkle is the handful of things that must survive a reset: the audio handle
(owned by `main`), the high score, and the RNG state so runs don't repeat:

```cpp
void ResetRun(Game& g) {
    AudioBank* audio = g.audio;
    int hi = g.hiScore;
    uint32_t rngState = g.rng.s;
    g = Game{};                    // wipe the entire world in one assignment
    g.audio = audio;
    g.hiScore = hi;
    g.rng.s = rngState ? rngState : 0x9E3779B9u;
    g.player.pos = {cfg::kCanvasW / 2.0f, cfg::kPlayerY};
    g.ufo.spawnTimer = g.rng.range(cfg::kUfoMinGap, cfg::kUfoMaxGap);
    InitStarfield(g);
    InitBunkers(g);
    StartWave(g, 1);
}
```

Think about what *isn't* here. No per-subsystem `reset()`. No walking a list of entities
to free them. No "did I remember to clear the particle pool?" The reason `g = Game{}`
works is precisely the rule from post 1: **no function-static variables anywhere.** If any
subsystem stashed state in a local `static`, this line would silently leak it across runs.
Because state is *only ever* in `Game`, wiping `Game` wipes the world, and the three lines
that restore borrowed handles are the entire exception list. The architecture makes
correctness the default.

## Screens are an enum and a switch

Game flow — title, playing, paused, game over, high-score entry — is the place most
codebases reach for a state pattern. Here it's an `enum class` and a `switch` in `main.cpp`.
The enum:

```cpp
enum class Screen { Title, Playing, Paused, GameOver, HighScoreEntry, Quit };
```

Each frame, one `switch` dispatches to the current screen's update-and-draw function, and
crucially each case **returns the next screen** rather than mutating flow state in place:

```cpp
switch (screen) {
case Screen::Title:
    next = UpdateDrawTitle(g, hs, screenTimer);
    break;
case Screen::Playing:
    UpdatePlaying(g, dt);
    DrawPlaying(g);
    if (g.gameOver) { /* ...save scores... */ next = Screen::GameOver; }
    else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) next = Screen::Paused;
    break;
case Screen::Paused:      next = UpdateDrawPaused(g); break;
case Screen::GameOver:    next = UpdateDrawGameOver(g, hs, screenTimer, dt); break;
case Screen::HighScoreEntry: next = UpdateDrawHighScoreEntry(g, hs, entry, screenTimer); break;
case Screen::Quit: break;
}
```

Then the transition is handled in *one* place, after the switch — which is where a
"start-a-run" side effect lives, guarded so that un-pausing doesn't wipe your game:

```cpp
if (next != screen) {
    if (next == Screen::Playing && screen != Screen::Paused) {
        ResetRun(g);                     // enter Playing fresh — but not when resuming
        entry = ScoreEntryState{};
    }
    screen = next;
    screenTimer = 0.0f;
}
```

That single `screen != Screen::Paused` guard is the entire "resume vs. new game"
distinction. A state-pattern version would spread this across `TitleState::exit`,
`PlayingState::enter`, and a shared transition table; here it's one `if` you can read in a
breath. The `Playing` case also fuses update and draw (`UpdatePlaying` then `DrawPlaying`),
while the menu screens combine both into a single `UpdateDraw…` — the code is honest about
which screens are simulations and which are just interactive panels.

## `UpdatePlaying` is the spine

If you want to understand the game's tick, you read one function. `UpdatePlaying` is the
ordered list of everything that happens in a playing frame, and it encodes the game's two
"freeze" rules as early returns rather than flags checked in a dozen places:

```cpp
void UpdatePlaying(Game& g, float dt) {
    g.time += dt;
    if (g.shake > 0) g.shake = fmaxf(0.0f, g.shake - cfg::kShakeDecay * dt * (1.0f + g.shake));

    UpdateUiFx(g, dt);          // always runs: toasts/bubbles/particles animate
    UpdateParticles(g, dt);     // even while the world is frozen
    UpdatePlayer(g, dt);

    // ... pacifist-achievement watch ...

    if (g.wave.clearing) {                       // intermission: tick the countdown only
        g.wave.intermission -= dt;
        if (g.wave.intermission <= 0) StartWave(g, g.wave.number + 1);
        return;
    }
    if (!g.player.alive) return;                 // death freeze: the world respectfully pauses

    UpdateInvaders(g, dt);
    UpdateUfo(g, dt);
    UpdateBoss(g, dt);
    UpdateShots(g, dt);
    UpdatePickups(g, dt);
    UpdateEffects(g, dt);
    ResolveCollisions(g);

    if (!g.wave.bossWave && g.aliveCount == 0 && !g.wave.clearing)
        FinishWave(g);
}
```

The ordering *is* the rules. UI, particles, and the player update unconditionally, so the
screen stays alive during a death or a wave-clear pause. Then two early returns short-
circuit the simulation: during `clearing` only the intermission timer advances, and after
the player dies the whole world holds still for the death animation. Everything past those
returns — invaders, boss, shots, collisions — is the "the game is actually running" block.
There's a companion predicate, `WorldFrozen`, that expresses the same idea for anyone who
needs to query it:

```cpp
bool WorldFrozen(const Game& g) { return !g.player.alive || g.wave.clearing; }
```

No state machine, no phase enum for "playing / dying / intermission." Just two booleans on
`Game` and the control flow that reads them. When you want to change *when* the world
freezes, there is exactly one function to edit.

## All collisions live in one place

The other function worth reading end to end is `ResolveCollisions`. The project rule is
that **every collision pairing in the game lives here** — nothing resolves hits in its own
update. It's one function in three phases (player shots, enemy shots, then invader bodies),
all plain AABB via raylib's `CheckCollisionRecs`:

```cpp
void ResolveCollisions(Game& g) {
    // ---- player shots ----
    for (auto& s : g.shots) {
        if (!s.fromPlayer) continue;
        bool consumed = false;
        // vs your own bunkers (friendly fire), invaders, UFO, boss...
        // pierce shots pass through; BigShot deals 2 and isn't consumed
        if (consumed) s.pos.y = -100;
    }
    // ---- enemy shots ---- : vs bunkers, vs player
    // ---- invaders vs bunkers and player ----
    if (BossTouchesPlayer(g)) HitPlayer(g, "management");
}
```

Two implementation details make this pattern hold together and are worth stealing:

**Shots are one vector, disambiguated by a flag.** Player and enemy projectiles share
`g.shots`; `s.fromPlayer` decides which phase owns them. So the phases are just filtered
passes over the same array — no parallel lists to keep in sync.

**"Consumed" is a deferred delete, not an in-loop erase.** When a shot is spent, the code
doesn't erase it mid-iteration (which would invalidate the loop over `g.shots`). It moves
it off-screen — `s.pos.y = -100` — and lets `UpdateShots` cull it next frame with
`std::erase_if`. This is the codebase's standard entity-removal idiom: never mutate a
container you're iterating; mark and sweep instead. The same trick shows up wherever
transient entities die.

Because there's a single collision function, "what can hit what" is answerable by reading
one screen of code, and adding an interaction (say, a new projectile kind versus the UFO)
means editing one place, not hunting through five update functions.

## The RNG is a value, not a global

One last small thing that captures the whole philosophy. Randomness is a four-line
xorshift struct that lives *inside* `Game`:

```cpp
// ---- tiny xorshift rng (no <random> ceremony, no statics) ----
struct Rng {
    uint32_t s = 0x9E3779B9u;
    uint32_t next() { /* xorshift32 */ }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float range(float a, float b) { return a + uniform() * (b - a); }
    int   irange(int a, int b) { return a + (int)(uniform() * (b - a + 1)); }
    bool  chance(float p) { return uniform() < p; }
};
```

No `<random>`, no global generator, no `rand()`. The state is one `uint32_t` that rides
along in `Game`, which is exactly why `ResetRun` can preserve it across a wipe with a
single `g.rng.s = rngState`. Every "random" decision in the game — a UFO's direction, a
pickup drop, a bubble line — pulls from this one value-typed source. It's the no-statics
rule again: even entropy is passed explicitly.

## Why the flat design wins here

Stack it up. Because state is one struct: reset is an assignment. Because behavior is free
functions: the compiler tells you, via `const`, what each one may touch. Because flow is an
enum and a switch: transitions live in one guarded `if`. Because the tick is one ordered
function: the game's rules are readable top to bottom. Because collisions are one function:
"what hits what" is a single source of truth. And because nothing is stashed in a static
or a global: every one of those properties actually holds.

This is not the architecture you'd reach for in a 200-person engine. It's the architecture
that lets *one person* hold an entire game in their head — and, not coincidentally, the one
the debug keys from post 1 can poke at freely, because there's no hidden state to get out of
sync. The constraints from the very first post (single struct, single thread, no statics,
no inheritance) aren't restrictions the code fights against. They're the thing that makes
the code small.

The invaders have filed to unionize the free functions. The functions, having no members
and therefore no grievances, declined to attend. `ResolveCollisions` sends its regards, and
also a bomb.

---

*Next: [Behavior as Data](05-behavior-as-data.md) — how the wave modifiers, the tuning
constants, and every joke in the game became data tables checked at hook sites, so the
comedy and the balance can change without touching a single `switch`.*
