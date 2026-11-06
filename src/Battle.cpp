#include <mup/html.h>
#include "main.h"

using namespace mup::html;
constexpr mup::Attr data_label = "data-label";


std::string State::Battle(
    const Card &mine,
    const Card &theirs,
    std::string outcome,
    std::string reason,
    bool        can_spell,
    std::string role,
    const Card &revealed
) const {
    auto my_fx  = "reveal " + std::string(outcome == "win" ? "won" : outcome == "lose" ? "lost" : "");
    auto opp_fx = "reveal " + std::string(outcome == "win" ? "lost" : outcome == "lose" ? "won" : "");
    auto info   = outcome == "win" ? "You win this clash" : outcome == "lose" ? "You lose this clash" : "Clash cancelled";

    std::string ret;

    ret += div_<(id = "my-battle", data_label = "Battle", class_ = "zone battle", hx_swap_oob = "true")>{
        mine.Render("", my_fx),
    };

    ret += div_<(id = "opp-battle", data_label = "Battle", class_ = "zone battle", hx_swap_oob = "true")>{
        theirs.Render("", opp_fx),
    };

    ret += div_<(id = "toast", class_ = "toast show", hx_swap_oob = "true")>{
        info,
        small<>{reason},
    };

    // TODO
    // if (can_spell) {
    //     ret += SpellPrompt(role, revealed);
    // } else {
    //     ret += div_<(id = "modal", hx_swap_oob = "true")>{form<"hx-ws:send", (hx_trigger = "load delay:2500ms")>{
    //         input<(type = "hidden", name_ = "action", value = "reveal_next")>
    //     }};
    // }

    return ret;
}
