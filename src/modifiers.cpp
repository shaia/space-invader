#include "game.h"

namespace {
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
    {ModifierId::DiscoInferno, "DISCO INFERNO",
     "Mandatory fun.", false, false, false, true, 0, 1.0f, 1.0f, false, false},
    {ModifierId::SpeedrunAnyPercent, "SPEEDRUN ANY%",
     "The invaders skipped the cutscene.", false, false, false, false, 3, 1.5f, 1.0f, false, false},
    {ModifierId::TinyWave, "TINY WAVE",
     "Budget approved for one (1) smaller war.", false, false, false, false, 0, 1.0f, 0.6f, false, false},
    {ModifierId::Understudies, "THE UNDERSTUDIES",
     "The real invaders are at a convention.", false, false, false, false, 0, 1.0f, 1.0f, true, false},
    {ModifierId::MirrorMatch, "MIRROR MATCH",
     "Union rules: equal representation.", false, false, false, false, 0, 1.0f, 1.0f, false, true},
};
} // namespace

const Modifier& GetModifier(ModifierId id) { return kTable[(int)id]; }

ModifierId PickNextModifier(Game& g) {
    const int first = (int)ModifierId::OppositeDay;
    const int count = (int)ModifierId::COUNT - first;
    uint32_t allMask = ((1u << count) - 1u);

    if ((g.wave.usedModifiers & allMask) == allMask)
        g.wave.usedModifiers = 0;  // pool exhausted, reshuffle

    for (int tries = 0; tries < 64; tries++) {
        int pick = g.rng.irange(0, count - 1);
        if (g.wave.usedModifiers & (1u << pick)) continue;
        g.wave.usedModifiers |= (1u << pick);
        return (ModifierId)(first + pick);
    }
    return ModifierId::None;  // unreachable in practice
}
