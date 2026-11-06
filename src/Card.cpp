#include <mup/html.h>
#include "main.h"

import fmt;

using namespace mup::html;
constexpr mup::Attr data_card_id = "data-card-id";

std::string Card::Render(std::string size, std::string fx, bool face_down) const {
    if (face_down)
        return article<>{
            (class_ = "card down " + size, id_ = "card-" + id, data_card_id = id),
            div_<(class_ = "back")>{span<>{"Dark Era"}},
        };

    return article<>(
        (class_ = fmt::format("card {} {} {}", type, size, fx), id_ = "card-" + id, data_card_id = id),

        div_<(class_ = "c-type")>{type},

        div_<(class_ = "c-head")>{
            span<(class_ = "c-name")>{name},
            mup::when(type != "spell", [&]() { return span<(class_ = "c-level")>{level}; }),
        },

        div_<(class_ = "c-art")>{
            img<(alt = "", loading = "lazy")>(src = art),
        },

        div_<(class_ = "c-effect")>{effect},
        div_<(class_ = "c-desc")>{description}
    );
}


std::pair<int, std::string> Card::matchup(const Card &other) const {
    int         score;
    std::string verdict;

    auto &self = *this;

    if (self.type == other.type) {
        if (self.level == 1 && other.level == 9) {
            score   = 1;
            verdict = "level 1 beats level 9";
        } else if (self.level == 9 && other.level == 1) {
            score   = -1;
            verdict = "level 1 beats level 9";
        } else if (self.level > other.level) {
            score   = 1;
            verdict = "highest level wins";
        } else if (self.level < other.level) {
            score   = -1;
            verdict = "highest level wins";
        } else {
            score   = 0;
            verdict = "Draw on level";
        }
    } else if (
        (self.type == "human" && other.type == "robot") || (self.type == "robot" && other.type == "monster") ||
        (self.type == "monster" && other.type == "human")
    ) {
        score   = 1;
        verdict = fmt::format("{} beats {}", self.type, other.type);
    } else {
        score   = -1;
        verdict = fmt::format("{} beats {}", other.type, self.type);
    }

    return {score, verdict};
}
