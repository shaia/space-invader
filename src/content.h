// ALL joke text lives here. Tone: deadpan, dry, self-aware. See docs/GAME_DESIGN.md §1.
#pragma once
#include <cstddef>

namespace content {

// ---- ambient invader speech bubbles ----
inline constexpr const char* kAmbient[] = {
    "Why do we march SIDEWAYS?\nWho wrote this pathfinding?",
    "We only descend because of a\n1978 bug. It's tradition now.",
    "I've been the leftmost invader\nfor 40 years. No promotion.",
    "Does anyone actually LIKE\nthe bunkers?",
    "One of us is worth 30 points.\nNobody knows why.",
    "My cousin was in Galaga.\nWon't shut up about it.",
    "We outnumber him 55 to 1\nand we're LOSING?",
    "Whose idea was single-file\ncolumn formation?",
    "I asked for a raise. They\ngave me a second frame.",
    "The cannon respawns.\nWE don't. Think about it.",
    "Technically we're just\nnegotiating aggressively.",
    "HR said the descending is\n'part of the culture'.",
};
inline constexpr int kAmbientCount = (int)(sizeof(kAmbient) / sizeof(kAmbient[0]));

// ---- event lines ----
inline constexpr const char* kBunkerDown      = "Was that load-bearing?";
inline constexpr const char* kAnchorDied      = "...never mind.";
inline constexpr const char* kFirstBlood      = "He shot the INTERN?";
inline constexpr const char* kTenLeft         = "Okay. Staff meeting. NOW.";
inline constexpr const char* kOneLeft         = "I demand hazard pay.";
inline constexpr const char* kPlayerDown      = "Take five, everyone.";
inline constexpr const char* kFreezeBreak     = "ON BREAK";

// ---- UFO flyby lines ----
inline constexpr const char* kUfoLines[] = {
    "Scale pay for cameo\nappearances. Calling my agent.",
    "STILL not unionized up here.",
    "50 to 300 points? Who does\nyour actuarial tables?",
    "I've crossed this screen\nsince 1978. No residuals.",
};
inline constexpr int kUfoLineCount = (int)(sizeof(kUfoLines) / sizeof(kUfoLines[0]));

// ---- compliments (Passive Aggression bombs) ----
inline constexpr const char* kCompliments[] = {
    "nice try!", "love the hat", "so brave", "great effort",
    "you got this", "bless", "A for effort", "solid attempt",
};
inline constexpr int kComplimentCount = (int)(sizeof(kCompliments) / sizeof(kCompliments[0]));

// ---- power-up toasts ----
inline constexpr const char* kToastSpread   = "SPREAD THE WORD - now in surround sound.";
inline constexpr const char* kToastPierce   = "PIERCING COMMENTARY - it cuts deep.";
inline constexpr const char* kToastRapid    = "ESPRESSO OVERRIDE - HR has been notified.";
inline constexpr const char* kToastFreeze   = "UNION-MANDATED BREAK - legally required.";
inline constexpr const char* kToastShield   = "BUREAUCRACY SHIELD - protected by paperwork.";
inline constexpr const char* kToastLife     = "THE REFUND - we apologize for the inconvenience.";
inline constexpr const char* kToastBigShot  = "BIG GOVERNMENT - funding approved.";
inline constexpr const char* kToastDejaVu   = "DEJA VU - continuity error.";

// ---- achievements ----
struct AchDef { const char* name; const char* desc; };
inline constexpr AchDef kAch[] = {
    {"PACIFIST RUN (7 SECONDS)", "You didn't shoot. The invaders noticed."},
    {"FRIENDLY FIRE",            "Killed by a compliment. It meant it."},
    {"UNION BUSTER",             "Beat the picket line without losing a life."},
    {"SPEAK TO THE MANAGER",     "Karen has been escalated."},
    {"HOARDER",                  "Three effects at once. Seek help."},
    {"CEASEFIRE VIOLATION",      "You shot during the break. Noted."},
    {"THIS IS FINE",             "Wave cleared. Bunkers: none."},
    {"1978",                     "Your score ends in 1978. Nobody planned this."},
};

// ---- boss dialogue ----
inline constexpr const char* kKarenIntro   = "MOTHERSHIP KAREN\nwould like to speak to your manager.";
inline constexpr const char* kKaren75      = "I have been circling this\nplanet for 45 YEARS.";
inline constexpr const char* kKaren50      = "Is there someone ELSE\nI can shoot?";
inline constexpr const char* kKaren25      = "I want your name AND\nyour badge number.";
inline constexpr const char* kKarenDeath   = "This isn't over. I know\nthe developer.";

inline constexpr const char* kLocalIntro   = "UFO LOCAL 1978\nNO PIXELS. NO PEACE.";
inline constexpr const char* kLocal75      = "You can't fire us.\nWe're already unpaid.";
inline constexpr const char* kLocal50      = "The clipboards are\nself-defense.";
inline constexpr const char* kLocal25      = "We are PREPARED\nto arbitrate.";
inline constexpr const char* kLocalDeath   = "Fine. We'll arbitrate.";

inline constexpr const char* kProdIntro    = "THE PRODUCER\nThis boss was not in the original design doc.";
inline constexpr const char* kProd75       = "We're adding features.\nMid-fight. Deal with it.";
inline constexpr const char* kProd50       = "The burndown chart\nis burning up.";
inline constexpr const char* kProd25       = "Let's circle back\nto you losing.";
inline constexpr const char* kProdDeath    = "Ship it.";

// ---- system voice ----
inline constexpr const char* kPauseLines[] = {
    "PAUSED. The invaders are also taking five.",
    "PAUSED. Union rules: the war resumes when you do.",
    "PAUSED. The UFO kept flying. It's contractual.",
};
inline constexpr int kPauseLineCount = (int)(sizeof(kPauseLines) / sizeof(kPauseLines[0]));

inline constexpr const char* kGameOver     = "The invaders thank you for your service\nand your quarter.";
inline constexpr const char* kOverrun      = "The invaders have landed. They are\nalready complaining about the weather.";

inline constexpr const char* kTitleFlavor[] = {
    "A 1978 labor dispute, remastered.",
    "Now with 30% more self-awareness.",
    "The invaders read the reviews.",
    "No aliens were paid in the making of this game.",
    "Descending since 1978. Legally, it's a tradition.",
};
inline constexpr int kTitleFlavorCount = (int)(sizeof(kTitleFlavor) / sizeof(kTitleFlavor[0]));

// ---- wave card extras ----
inline constexpr const char* kBossCardSmall  = "Management has arrived.";
inline constexpr const char* kPlainWaveSmall[] = {
    "Same invaders. New attitude.",
    "They've had coffee.",
    "Morale is somehow worse.",
    "This one's personal. Allegedly.",
};
inline constexpr int kPlainWaveSmallCount = (int)(sizeof(kPlainWaveSmall) / sizeof(kPlainWaveSmall[0]));

// ---- default high scores ----
struct DefaultScore { int score; const char* name; };
inline constexpr DefaultScore kDefaultScores[] = {
    {10000, "CROW"}, {9000, "AAA"}, {8000, "DAD"}, {7000, "DEV"},
    {6000, "COIN"}, {5000, "SUE"}, {4000, "UFO"}, {3000, "HAL"},
    {2000, "BOB"}, {1978, "TAITO"},
};

} // namespace content
