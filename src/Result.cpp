#include <mup/html.h>
#include "main.h"

using namespace mup::html;

std::string State::Result(std::string log_line) const {
    std::string ret;
    ret += div_<(id = "my-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{};
    ret += div_<(id = "opp-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{};
    ret += div_<(id = "my-spells", R"(data-label="Spells")", class_ = "zone spell", hx_swap_oob = "true")>{};
    ret += div_<(id = "opp-spells", R"(data-label="Spells")", class_ = "zone spell", hx_swap_oob = "true")>{};

    ret += div_<(id = "my-victory", R"(data-label="Victory")", class_ = "zone victory", hx_swap_oob = "true")>{
        mup::each(victory, [&](Card &c) { return c.Render("mini"); })
    };

    ret += div_<(id = "opp-victory", R"(data-label="Victory")", class_ = "zone victory", hx_swap_oob = "true")>{
        mup::each(opp->victory, [&](Card &c) { return c.Render("mini"); })
    };

    ret += div_<(id = "my-grave", class_ = "pile grave", hx_swap_oob = "true")>{
        div_<(class_ = "stack")>{graveyard.size()},
        small<>{"Graveyard"},
    };

    ret += div_<(id = "opp-grave", class_ = "pile grave", hx_swap_oob = "true")>{
        div_<(class_ = "stack")>{opp->graveyard.size()},
        small<>{"Graveyard"},
    };

    ret += Log(log_line);

    if (game_over)
        ret += div_<(id = "modal", class_ = "modal", hx_swap_oob = "true")>{div_<(class_ = "modal-box")>(
            h3<>{
                winner == "me"    ? "Victory"
                : winner == "opp" ? "Defeat"
                                  : "Draw"
            },
            p<>{std::to_string(victory.size()) + " to " + std::to_string(opp->victory.size())},
            form<"hx-ws:send">{
                input<(type = "hidden", name_ = "action", value = "play_again")>,
                button<(type = "submit")>{"Play again"},
            }
        )};
    else
        ret += div_<(id = "modal", hx_swap_oob = "true")>(form<"hx-ws:send", (hx_trigger = "load delay:1200ms")>{
            input<(type = "hidden", name_ = "action", value = "reveal_next")>,
        });

    return ret;
}

std::string State::Log(std::string log_line) const {
    return aside<(id = "log", hx_swap_oob = "beforeend")>{
        p<>{log_line},
    };
}
