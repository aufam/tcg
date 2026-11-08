#include <mup/html.h>
#include "main.h"

using namespace mup::html;


std::string State::Hand() const {
    return section<(id = "hand", class_ = "hand", hx_swap_oob = "true")>{
        mup::each(hand, [&](const Card &c) -> std::string {
            return form<"hx-ws:send", (hx_trigger = "click")>(
                (class_ = "hand-slot " + std::string(c.type == "spell" || battle.has_value() ? "blocked" : "")),

                input<(type = "hidden", name_ = "action")>(value = c.type == "spell" ? "place_spell" : "place_card"),
                input<(type = "hidden", name_ = "card_id")>(value = c.id),

                c.Render()
            );
        }),
    };
}
