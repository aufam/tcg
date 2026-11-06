#include <mup/html.h>
#include "main.h"

using namespace mup::html;

std::string State::SpellPrompt(std::string role, const Card &reveal) const {
    return div_<(id = "modal", class_ = "modal", hx_swap_oob = "true")>(div_<(class_ = "modal-box")>{
        mup::when(
            role == "loser",
            []() {
                std::string hh = h3<>{"You are losing this clash"};
                std::string pp = p<>{"Reveal a spell to turn it around?"};
                return hh + "\n" + pp;
            }
        ),

        mup::when(
            role == "winner",
            [&]() {
                std::string hh = h3<>{"Opponent revealed a spell"};
                std::string dd = div_<(class_ = "deck-grid")>{reveal.Render("mini")};
                std::string pp = p<>{"Counter it with one of yours?"};
                return hh + "\n" + pp;
            }
        ),

        // TODO
        // div_<(class_ = "deck-grid")>{mup::each(
        //     spell_slots,
        //     [](Slot &s) {
        //         return form<(hx_trigger = "click", "hx-ws:send")>{
        //             input<(type = "hidden", name_ = "action", value = "use_spell")>,
        //             [&]() {
        //                 auto in = input<(type = "hidden", name_ = "card_id")>;
        //                 in.attributes.push_back("value=\"" + s.card.id + "\"");
        //                 return in;
        //             }(),
        //             s.card.Render("mini"),
        //         };
        //     }
        // )},

        form<"hx-ws:send">(
            input<(type = "hidden", name_ = "action", value = "skip_spell")>,
            button<(type = "submit", class_ = "ghost")>{"Don't reveal"}
        ),
    });
}
