# Behavior as Data

*Post 5 of a series on the engineering behind* **SPACE INVADERS: WE HAVE DEMANDS**.
*Previously: [One Struct, Many Functions](04-one-struct-many-functions.md).*

---

Post 4 was about keeping *behavior* out of class hierarchies. This one is about keeping it
out of code entirely — as far as that can reasonably go. Two things in this game that a
lesser codebase would express as control flow are instead expressed as data: the **wave
modifiers** (the comedic per-wave twists) and, more quietly, all the **tuning** and all
the **jokes**. A modifier is a row in a table. The difficulty curve is a header of
constants. Every line the invaders say is a `constexpr` array. Logic reads that data at
fixed hook points and never hard-codes any of it.

The invaders regard this as the final indignity. Their entire personality — their
grievances, their labor actions, their one (1) approved smaller war — is a `const`
initializer list. They are, quite literally, table stakes. Here's how that table works.

## A modifier is a plain struct of flags

The wave modifiers are the marquee example. "Opposite Day" reverses your controls;
"Budget Cuts" makes the invaders invisible; "Tiny Wave" shrinks them. There are eight of
them, and not one is a class. A `Modifier` is a POD — a bag of flags and multipliers with
a name and a tagline:

```cpp
struct Modifier {
    ModifierId  id   = ModifierId::None;
    std::string_view name    = "";
    std::string_view tagline = "";
    bool  invertInput       = false;
    bool  invisibleInvaders = false;
    bool  complimentBombs   = false;
    bool  discoHue          = false;
    int   startRowsLower    = 0;
    float scoreMult         = 1.0f;
    float scale             = 1.0f;
    bool  wobbly            = false;
    bool  mirrorCannon      = false;
};
```

That's the whole vocabulary of "what a wave modifier can do." Each field is a lever some
subsystem already knows how to pull. Adding a modifier doesn't mean writing behavior; it
means *composing existing levers* into a new row.

And the rows are exactly that — a single `const` table, indexed by `ModifierId`, with a
comment stating the design rule outright:

```cpp
// Behavior is data checked at hook sites, not classes. Index matches ModifierId.
const Modifier kTable[(int)ModifierId::COUNT] = {
    // id, name, tagline, invert, invisible, compliments, disco, rowsLower, scoreMult, scale, wobbly, mirror
    {ModifierId::None, "", "", false, false, false, false, 0, 1.0f, 1.0f, false, false},
    {ModifierId::OppositeDay, "OPPOSITE DAY",
     "Your keyboard has unionized.", true, false, false, false, 0, 1.0f, 1.0f, false, false},
    {ModifierId::BudgetCuts, "BUDGET CUTS",
     "We laid off the rendering department.", false, true, false, false, 0, 1.0f, 1.0f, false, false},
    {ModifierId::PassiveAggression, "PASSIVE AGGRESSION",
     "They mean it. That's worse.", false, false, true, false, 0, 1.0f, 1.0f, false, false},
    {ModifierId::SpeedrunAnyPercent, "SPEEDRUN ANY%",
     "The invaders skipped the cutscene.", false, false, false, false, 3, 1.5f, 1.0f, false, false},
    {ModifierId::TinyWave, "TINY WAVE",
     "Budget approved for one (1) smaller war.", false, false, false, false, 0, 1.0f, 0.6f, false, false},
    // ... Disco Inferno, The Understudies, Mirror Match ...
};
```

You can read every modifier's entire behavior *and* its writing on one line. "Speedrun
Any%" starts the grid three rows lower and pays 1.5× — `startRowsLower = 3, scoreMult =
1.5`. "Tiny Wave" is `scale = 0.6`. The joke and the mechanic sit in the same literal,
which is exactly how the design doc wants it: comedy and balance edited in one place.

## The behavior lives at the hook sites

The table is inert. What makes it *do* anything is that subsystems read the current
modifier's flags at the precise point those flags matter. There's a one-liner accessor —
`CurrentMod(g)` — and then each system checks the field it cares about. Look how local
these hooks are.

Reversed controls, in the player's input code:

```cpp
if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) dir -= 1.0f;
if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dir += 1.0f;
if (CurrentMod(g).invertInput) dir = -dir;      // Opposite Day
```

The score multiplier, in `AddScore`:

```cpp
void AddScore(Game& g, int points) {
    g.score += (int)((float)points * CurrentMod(g).scoreMult);
    if (g.score > g.hiScore) g.hiScore = g.score;
}
```

The shrink factor, exposed as its own tiny function so invaders don't each reach into the
table:

```cpp
float GridScale(const Game& g) { return CurrentMod(g).scale; }   // Tiny Wave
```

The "compliment bombs," at the exact spot an invader decides what to drop:

```cpp
if (m.complimentBombs) {                         // Passive Aggression
    s.kind = ShotKind::Compliment;
    s.vel  = {0, cfg::kBombSpeed * 0.8f};
    s.label = content::kCompliments[g.rng.irange(0, content::kComplimentCount - 1)];
} else {
    s.kind = ShotKind::Bomb;
    s.vel  = {0, cfg::kBombSpeed};
}
```

And the purely-visual flags land in `DrawPlaying` — `invisibleInvaders` drops the alpha
except in the brief flash after a step or a hit, `discoHue` cycles the tint, `wobbly` gets
passed straight into `DrawInvaderArt`:

```cpp
if (m.discoHue) tint = HueCycle(tint, g.time + row * 0.2f);
if (m.invisibleInvaders)
    alpha = (g.stepFlash > 0 || v.hitFlash > 0) ? 0.9f : 0.05f;
DrawInvaderArt(v.pos, cfg::kInvaderW * s, cfg::kInvaderH * s, row, g.marchFrame,
               v.squash, WithAlpha(tint, alpha), m.wobbly, g.time, i);
```

Nine hook sites across five files, each a single `if` or multiply. That's the entire
modifier engine. There is no `Modifier::apply()`, no dispatch, no per-modifier update
step — the flag is checked where its effect belongs, by the code that already owns that
effect.

## The rule the flags enforce: change *play*, not just pixels

Notice what the flag vocabulary makes easy and what it makes hard. Seven of the eight
modifiers flip a flag that some *gameplay* system reads — input direction, bomb type, grid
scale, spawn height, score. Even the ones that look cosmetic aren't:

- **Budget Cuts** sets `invisibleInvaders`, but the consequence is mechanical — you can't
  see what you're shooting at except in the flash after it steps or takes a hit.
- **The Understudies** sets `wobbly`, and that jitter (post 2) drifts the drawn silhouette
  off the hitbox, so aiming gets genuinely harder.

The single honest exception is **Disco Inferno** (`discoHue`), which is a pure morale
event — "Mandatory fun." And that's the point of the data model: the cosmetic modifier
isn't a special case in the code, it's just a row that happens to only set a draw flag.
The design rule "every modifier changes play" is *encouraged by the table shape* — most of
the levers you can pull are gameplay levers — without being enforced by a type system. The
data makes the right thing the easy thing.

## Picking one: a shuffle-bag, also data

Which modifier a wave gets is a shuffle-bag, not a dice roll — you see all of them before
any repeats. The state is a single bitmask on `Game`, and the picker is a dozen lines:

```cpp
ModifierId PickNextModifier(Game& g) {
    const int first = (int)ModifierId::OppositeDay;
    const int count = (int)ModifierId::COUNT - first;
    uint32_t allMask = ((1u << count) - 1u);

    if ((g.wave.usedModifiers & allMask) == allMask)
        g.wave.usedModifiers = 0;               // pool exhausted, reshuffle

    for (int tries = 0; tries < 64; tries++) {
        int pick = g.rng.irange(0, count - 1);
        if (g.wave.usedModifiers & (1u << pick)) continue;
        g.wave.usedModifiers |= (1u << pick);
        return (ModifierId)(first + pick);
    }
    return ModifierId::None;                     // unreachable in practice
}
```

`usedModifiers` is one `uint32_t` in the `Game` struct — which, per post 4, means
`ResetRun`'s `g = Game{}` clears the bag for free at the start of every run. The bag rides
in the same flat state as everything else. No separate allocation, no manager, no reset
call.

## The other two data tables: tuning and jokes

The modifier table is the loud example; the same discipline runs quietly through the whole
project, enforced by two hard rules from post 1.

**All tuning is `config.h`.** The file opens with the house rule — *"If you're typing a
number into a logic file, stop"* — and it means it. Canvas size, player speed, the march
interval endpoints, the per-wave speed multiplier, bomb rates, glow alphas, bloom
thresholds, audio sample rate: every one is a named `inline constexpr` in `namespace cfg`.
The difficulty curve isn't code that ramps values; it's constants the code reads. Want a
gentler game? You edit `config.h`, not `invaders.cpp`. And because it's all `constexpr`,
the "data" costs nothing at runtime — it's baked in at compile time with no indirection.

**All joke text is `content.h`.** Every string the game shows — power-up toasts, the
ambient speech-bubble pool, boss dialogue, the game-over line, and yes the compliment
bombs — is a categorized `constexpr` array. Logic pulls from those arrays by category and
never spells a joke inline. You saw it above: the Passive Aggression hook doesn't contain
a compliment, it indexes `content::kCompliments[...]`. The payoff is that the entire voice
of the game — the thing the tone guide in the design doc is so strict about — can be read,
edited, and audited for register in *one file*, with zero risk of a stray "LOL so random"
sneaking into a logic file where nobody reviews the prose.

## Why data beats code here

Tally the wins, because they're concrete:

- **Adding a modifier is a table row plus (maybe) one hook.** New behavior that reuses an
  existing lever — a score bonus, a smaller grid — is *just data*. New behavior that needs
  a new lever is one field and one `if`, not a class, a virtual, and a registration.
- **Balance and comedy change without touching logic.** Tuning lives in `config.h`, voice
  lives in `content.h`. A balance pass or a joke rewrite never risks a gameplay bug.
- **It's trivially inspectable and testable.** The debug `F4` key from post 1 just cycles
  `ModifierId` and re-announces — because a modifier *is* an index, forcing one is
  incrementing an integer. There's no object to construct.
- **It costs nothing.** `constexpr` tables and `string_view`s into static storage mean the
  data-driven design compiles down to array lookups. Flexibility with no runtime tax.

None of this needs an "engine." It needs a struct with the right fields, a `const` table,
and the discipline to check the flags where they matter instead of branching on names
scattered through the code. That discipline is the same one from every post in this
series: put the state in data, keep the behavior in small functions that read it, and
refuse to hide anything in a class or a static.

The invaders have requested that their taglines be moved to a more prestigious header.
Management reviewed the request, noted that `content.h` is already the single source of
truth for all approved humor, and closed the ticket as *Working As Designed*.

---

*Next: [One Binary, Three Platforms](06-one-binary-three-platforms.md) — the toolchain
that ships all of this as a single self-contained executable on Windows, Linux, and macOS:
CMake and FetchContent, warnings scoped to the one target that can survive them, and the
`DEBUG_KEYS` inner loop that made the whole thing fast to build.*
