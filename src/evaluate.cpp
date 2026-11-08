#include "main.h"

// TODO
std::pair<int, std::string> evaluate(const State &a, const State &b) {
    // const Slot *mine   = a.battle ? &*a.battle : nullptr;
    // const Slot *theirs = b.battle ? &*b.battle : nullptr;
    //
    // if (!mine && !theirs)
    //     return {0, "Both fields empty — clash cancelled"};
    // if (!mine)
    //     return {-1, "No creature to answer with"};
    // if (!theirs)
    //     return {+1, "Opponent has no creature"};
    //
    // Side me   = effective_side(*mine, a.spell_slots.size());
    // Side them = effective_side(*theirs, b.spell_slots.size());
    //
    // // spells apply in reveal order regardless of who played them
    // for (const auto &event : a.clash) {
    //     const auto &spell      = event.first;
    //     const bool  mine_spell = event.second;
    //
    //     if (spell.effect.find("Trample") != std::string::npos) {
    //         if (mine_spell)
    //             me.level += effect_number(spell.effect, "Trample");
    //         else
    //             them.level += effect_number(spell.effect, "Trample");
    //     } else if (spell.effect.find("Weaken") != std::string::npos) {
    //         const int n = effect_number(spell.effect, "Weaken"); // negative value
    //         if (mine_spell)
    //             them.level += n;
    //         else
    //             me.level += n;
    //     } else if (spell.effect.find("Mirror") != std::string::npos) {
    //         std::swap(me.level, them.level);
    //     } else if (spell.effect.find("Attune") != std::string::npos) {
    //         if (mine_spell)
    //             me.type = beats(them.type);
    //         else
    //             them.type = beats(me.type);
    //     }
    // }
    //
    // if (me.level == 1 && them.level == 9)
    //     return {+1, "Level 1 topples Level 9"};
    // if (them.level == 1 && me.level == 9)
    //     return {-1, "Level 1 topples Level 9"};
    //
    // if (me.type != them.type) {
    //     if (beats(me.type) == them.type)
    //         return {+1, me.type + " beats " + them.type};
    //     return {-1, them.type + " beats " + me.type};
    // }
    //
    // if (me.level != them.level) {
    //     const int w = std::max(me.level, them.level);
    //     const int l = std::min(me.level, them.level);
    //     return {me.level > them.level ? +1 : -1, "Level " + std::to_string(w) + " beats Level " + std::to_string(l)};
    // }
    //
    // return {0, "Same level — clash cancelled"};
}
