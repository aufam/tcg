#include <mup/html.h>
#include "main.h"

using namespace mup::html;


std::string State::Hand() const {
    return section<(id = "hand", class_ = "hand", hx_swap_oob = "true")>{
        mup::each(hand, [&](Card &c) -> std::string {
            if (battle.has_value()) {
                return div_<>{
                    (class_ = "hand-slot " + std::string(placed_ids.count(c.id) ? "placed" : "")),
                    c.Render(),
                };
            }

            if (phase == "main" && !placed_ids.count(c.id)) {
                return form<"hx-ws:send", (hx_trigger = "click")>(
                    (class_ = "hand-slot " + std::string(c.type != "spell" && monster_placed ? "blocked" : "")),

                    input<(type = "hidden", name_ = "action")>(value = c.type == "spell" ? "place_spell" : "place_card"),

                    input<(type = "hidden", name_ = "card_id")>(value = c.id),

                    c.Render()
                );
            } else {
                return div_<>{
                    (class_ = "hand-slot " + std::string(placed_ids.count(c.id) ? "placed" : "")),
                    c.Render(),
                };
            }
        }),
    };
}
