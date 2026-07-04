# SPACE INVADERS: WE HAVE DEMANDS
### Game Design Document

A self-aware parody of Space Invaders. The invaders know they're in a 1978 video game,
they have opinions about it, and they would like to speak to whoever is in charge.

- **Genre**: fixed-shooter arcade (Classic+ variant)
- **Platforms**: Windows, Linux, macOS
- **Tech**: C++20, raylib 5.5 (CMake FetchContent), zero external assets
- **Session**: endless escalating waves, one run ≈ 5–15 minutes, high-score chase
- **Art**: neon vector/shape art drawn with raylib primitives (glow, particles, squash/stretch)
- **Audio**: 100% procedurally synthesized at startup

---

## 1. Tone Guide

The game is a loving parody of *itself*. Rules for all writing:

- The invaders are not evil — they are **underpaid, self-aware, and mildly annoyed**.
  They complain about their own AI, their working conditions, and 1978 design decisions.
- Fourth-wall breaks are always **dry, never wacky**. "We only descend because of a 1978
  bug. It's tradition now." — not "LOL so random!"
- The game itself is a character: pause screens, game-over text, and achievement toasts
  speak in a deadpan corporate/legal voice.
- Jokes never interfere with play. Speech bubbles are ambient; nothing blocks input;
  every modifier gag is also a real mechanical change.
- All joke text lives in `src/content.h`. If it's funny, it's in that file.

---

## 2. Core Loop (Classic+)

The faithful core, kept sacred:

- Player cannon moves horizontally along the bottom; **one shot on screen at a time**
  (the `maxPlayerShots` rule — power-ups may temporarily raise it).
- A **5×11 grid of invaders** marches sideways in discrete steps, dropping down and
  reversing at the edges. The march **speeds up as invaders die** (the classic panic curve).
- Bottom-most invader in each column may drop bombs.
- **4 destructible bunkers** shield the player and get chewed away pixel-cluster by
  pixel-cluster from both sides.
- A **UFO** occasionally crosses the top for mystery points.
- Invaders reaching the cannon row = game over regardless of lives.
- 3 lives. Clearing the grid starts the next, faster wave.

The additions (the "+"):

- **Power-ups** drop from destroyed invaders (~8% chance).
- **Wave modifiers** from wave 3 onward: every wave gets one comedic mechanical twist.
- **Boss waves** every 5th wave replace the grid with a set-piece boss.
- **Achievements** (joke toasts), speech bubbles, and a persistent high-score table.

### Controls

| Input | Action |
|---|---|
| ← / → or A / D | Move cannon |
| Space | Fire |
| Enter | Confirm / start |
| Esc / P | Pause (Esc on title quits) |

---

## 3. Cast

### The Grid (top row → bottom rows)

| Enemy | Rows | Points | Personality |
|---|---|---|---|
| **Squid Executive** | 1 | 30 | Complains the most, does the least. C-suite of the invasion. |
| **Crab Middle-Manager** | 2–3 | 20 | Forwards the Executive's complaints downward. |
| **Octopus Intern** | 4–5 | 10 | Front line, first to die. "I'm not even paid." |

Later waves mix in **Overachievers**: red-tinted variants with 2 HP, any row.
First hit knocks the tint off. "I take this seriously."

### The Consultant (UFO)

Crosses the top of the screen every 20–30 s. Worth 50–300 mystery points.
On flyby it airs grievances: *"Scale pay for cameo appearances. I'm calling my agent."*
It is, canonically, the same UFO from 1978 and it has never been paid.

---

## 4. Power-Ups

Drop from destroyed invaders at ~8% (roll per kill, max 2 falling at once).
Fall slowly; catch with the cannon. Timed effects show an icon + countdown in the HUD.

| # | Name | Effect | Toast on pickup |
|---|---|---|---|
| 1 | **Spread the Word** | Triple shot, 8 s | "Now in surround sound." |
| 2 | **Piercing Commentary** | Shots pass through invaders, 6 s | "It cuts deep." |
| 3 | **Espresso Override** | Rapid fire; suspends one-shot rule, 8 s | "HR has been notified." |
| 4 | **Union-Mandated Break** | Invaders freeze 4 s; one raises an "ON BREAK" bubble | "Legally required." |
| 5 | **Bureaucracy Shield** | Bubble shield absorbing 3 hits | "Protected by paperwork." |
| 6 | **The Refund** | +1 life | "We apologize for the inconvenience." |
| 7 | **Big Government** | Next shot is enormous and slow, clears a column | "Funding approved." |
| 8 | **Deja Vu** | All bunkers restored to full | "Continuity error." |

Stacking: different effects stack (see *Hoarder* achievement); re-catching the same
effect resets its timer.

---

## 5. Wave Modifiers

From wave 3 onward each non-boss wave draws one modifier, random without repeat until
the pool is exhausted. Announced on the wave card: **name + tagline**.

| # | Modifier | Mechanics | Tagline |
|---|---|---|---|
| 1 | **Opposite Day** | Left/right controls reversed | "Your keyboard has unionized." |
| 2 | **Budget Cuts** | Invaders invisible except when stepping or hit | "We laid off the rendering department." |
| 3 | **Passive Aggression** | Bombs are floating compliment words that still hurt ("nice try!", "love the hat") | "They mean it. That's worse." |
| 4 | **Disco Inferno** | Hue-cycling colors, invaders bounce to the beat, march SFX becomes a bassline | "Mandatory fun." |
| 5 | **Speedrun Any%** | Grid starts 3 rows lower; 1.5× points | "The invaders skipped the cutscene." |
| 6 | **Tiny Wave** | Everything 60% scale, squeaky SFX | "Budget approved for one (1) smaller war." |
| 7 | **The Understudies** | Invaders drawn wobbly and hand-drawn-looking | "The real invaders are at a convention." |
| 8 | **Mirror Match** | A ghost cannon mirrors your movement along the top and drops your own shots back at you | "Union rules: equal representation." |

Design rule: every modifier must change *play*, not just visuals — even Understudies
(wobble makes hitboxes drift slightly).

---

## 6. Boss Waves

Every 5th wave. The grid is replaced by a set-piece boss with a health bar and
dialogue interjections at 75% / 50% / 25% HP. Victory: confetti + bonus = 500 × wave.
Bosses repeat from wave 20 with scaled HP/speed.

### Wave 5 — MOTHERSHIP KAREN
A giant UFO that demands to speak to your manager.
- **Phases**: sweeping laser telegraphed by a targeting line → spawns "assistant"
  mini-invaders (max 6 alive) → slow ram descent toward the cannon.
- **Dialogue**: "I have been circling this planet for 45 YEARS." /
  "Is there someone ELSE I can shoot?" / "I want your name AND your badge number."
- **Death**: "This isn't over. I know the developer."

### Wave 10 — UFO LOCAL 1978 (the picket line)
Three linked saucers carrying picket signs ("FAIR WAGES FOR FLYING SAUCERS",
"NO PIXELS NO PEACE").
- **Mechanics**: signs are shields — destroy the sign, then the saucer beneath.
  Saucers retaliate with thrown clipboards (tumbling projectiles).
- **Death**: "Fine. We'll arbitrate."

### Wave 15 — THE PRODUCER
A giant CRT monitor displaying a live burndown chart (it burns *up*).
- **Mechanics**: drops "DEADLINE" bricks that crush bunkers; fires scope-creep beams
  that start thin and widen the longer you stay in them.
- **Intro**: "This boss was not in the original design doc."
- **Death**: "Ship it."

---

## 7. Comedy Systems

### Speech bubbles
Rounded-rect bubbles with a tail, anchored to living invaders (they march with them).
Ambient line every 8–15 s from the pool in `content.h`, plus event lines
(first bunker destroyed → "Was that load-bearing?", UFO spawn, wave 13, etc.).
If the anchor dies mid-line, the bubble swaps to "…never mind." and fades.

Ambient pool samples:
- "Why do we march SIDEWAYS? Who wrote this pathfinding?"
- "We only descend because of a 1978 bug. It's tradition now."
- "I've been the leftmost invader for 40 years. No promotion."
- "Does anyone actually LIKE the bunkers?"
- "One of us is worth 30 points. Nobody knows why."

### Toasts
Bottom-right sliding stack (max 3 queued): power-up pickups, achievements, boss quips.

### Achievements (per run, persisted)

| Achievement | Trigger |
|---|---|
| **Pacifist Run** | Don't shoot for the first 7 seconds |
| **Friendly Fire** | Die to a compliment (Passive Aggression wave) |
| **Union Buster** | Beat UFO LOCAL 1978 without losing a life |
| **Speak to the Manager** | Beat MOTHERSHIP KAREN |
| **Hoarder** | Hold 3 timed effects at once |
| **Ceasefire Violation** | Shoot during a Union-Mandated Break |
| **This Is Fine** | Clear a wave with all bunkers destroyed |
| **1978** | End a run with a score ending in 1978 |

### The game's own voice
- Pause: "PAUSED. The invaders are also taking five."
- Game over: "The invaders thank you for your service and your quarter."
- Title flavor line rotates each visit.

---

## 8. Scoring & Difficulty Curve

| Parameter | Value |
|---|---|
| Kill points | 10 / 20 / 30 by row; UFO 50–300; Overachiever ×2 |
| Wave-clear bonus | 100 × wave number |
| Boss-kill bonus | 500 × wave number |
| March interval within a wave | 0.8 s (full grid) → 0.06 s (last invader) |
| Global speed scale | ×1.06 per wave |
| Bomb drop rate | +8% per wave |
| Lives | 3 (max 5 via The Refund) |
| Target median run | ends around wave 8–12, ≈ 9 minutes |

High scores: top 10, persisted to the platform config dir
(`%APPDATA%\SpaceInvaderPlus\` / `$XDG_DATA_HOME/space-invader-plus/` /
`~/Library/Application Support/SpaceInvaderPlus/`). Default table shipped with the
game: `CROW 10000`, `AAA (VERY ORIGINAL) 9000`, `YOUR DAD 1978 8000`,
`DEFINITELY NOT DEV 7000`, `INSERT COIN 6000`, `THE UFO'S LAWYER 5000`, …

---

## 9. Presentation

- **Resolution**: fixed 800×950 logical canvas rendered to a RenderTexture, scaled to
  fit the actual window (portrait arcade proportions, survives small laptop screens).
- **Look**: neon vector art — every shape drawn 2–3× (fat low-alpha glow pass + crisp
  core). Row-tinted invaders with 2-frame march animation, squash/stretch on steps and
  hits, particle explosions/debris/confetti, screen shake, CRT scanline + vignette
  overlay (toggleable in `config.h`).
- **Audio**: all synthesized at startup into `Sound` buffers — pew, invader pop, sad
  descending player death, UFO warble, pickup chime, bunker crunch, boss hits, menu
  blips, achievement ding, and the iconic 4-note march bassline whose tempo tracks the
  actual march interval. One pre-rendered ~20 s music loop (square-wave bass + arpeggio
  + noise hats).

---

## 10. Screens

```
Title (attract: demo grid marching behind the logo, high-score table, rotating joke)
  └─ Enter → Playing
Playing
  ├─ Esc/P → Paused (frozen playfield + joke) → resume/quit
  └─ death/overrun → GameOver (frozen playfield + verdict)
        ├─ score makes top 10 → HighScoreEntry (3 initials, arcade style)
        └─ → Title
```

---

## 11. Out of Scope (v1)

Online leaderboards, gamepad support, rebindable keys, localization, shaders,
save-mid-run, difficulty settings (the game *is* the difficulty setting).
