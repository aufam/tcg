#include <mup/html.h>
#include "main.h"

using namespace mup::html;
constexpr mup::Attr data_label = "data-label";


std::string State::Index() const {
    return html<(lang = "en")>(
        head<>(
            meta<(charset = "utf-8")>,
            meta<(name_ = "viewport", content_ = "width=device-width, initial-scale=1")>,
            meta<(name_ = "htmx-config", R"(content='{"allowNestedOobSwaps": false}')")>,

            link_<(rel = "preconnect", href = "https://fonts.googleapis.com")>,
            link_<(rel = "preconnect", href = "https://fonts.gstatic.com", crossorigin)>,
            link_<(
                rel  = "stylesheet",
                href = "https://fonts.googleapis.com/"
                       "css2?family=Big+Shoulders+Display:wght@700;900&family=IBM+Plex+Sans:wght@400;500;600&display=swap"
            )>,
            link_<(rel = "stylesheet", href = "style.css")>,

            title<>{"TCG Dark Era"},
            script<(defer, src = "https://cdn.jsdelivr.net/npm/htmx.org@4.0.1/dist/htmx.min.js")>{},
            script<(defer, src = "https://cdn.jsdelivr.net/npm/htmx.org@4.0.1/dist/ext/hx-ws.min.js")>{},
            script<(defer, src = "animations.js")>{}
        ),

        body<R"(hx-ws:connect=ws/game)">(
            header<(class_ = "masthead")>{
                h1<>{"Dark Era"},
                span<(class_ = "sub")>{"Trading Card Game"},
                PhaseBar(),
            },

            opp ? Board() : DeckSelect(),

            section<(id = "hand", class_ = "hand", hx_swap_oob = "true")>{},

            aside<(id = "log", class_ = "log", hx_swap_oob = "true", aria_live = "polite")>{},
            div_<(id = "toast", class_ = "toast", hx_swap_oob = "true")>{},
            div_<(id = "modal", hx_swap_oob = "true")>{}
        )
    );
}


std::string State::PhaseBar() const {
    const std::string ph = "phase";

    using namespace mup::html;
    return div_<(id = "phase-bar", class_ = "phase-bar", hx_swap_oob = "true")>(mup::when(opp == nullptr && !cards.empty(), []() {
        return span<(class_ = "wait")>{"Waiting for opponent..."};
    }));
}


std::string State::DeckSelect() const {
    return main_<(id = "screen", hx_swap_oob = "true")>{section<(class_ = "deck-select")>(
        h2<>{"Pick Your Starter Deck"},

        div_<(class_ = "decks")>{mup::each(decks, [](Deck &d) {
            return form<"hx-ws:send">(
                (class_ = "deck " + d.css),

                input<(type = "hidden", name_ = "action", value = "choose_deck")>,
                input<(type = "hidden", name_ = "deck_id")>(value = d.id),

                h3<>{d.name},
                p<>{d.blurb},

                button<(type = "button", class_ = "ghost", hx_target = "#modal")>{
                    (hx_get = "decks/" + d.id),
                    "View cards",
                },
                button<(type = "submit")>{"Play " + d.name}
            );
        })}
    )};
}


std::string State::Board() const {
    auto &opp = *this->opp;
    auto &me  = *this;

    return main_<(id = "screen", class_ = "board", hx_swap_oob = "true")>(

        section<(class_ = "side opp")>(
            div_<(class_ = "pile deck-pile")>{
                div_<(class_ = "stack")>{},
                b<>{opp.cards.size()},
            },

            div_<(class_ = "center")>(
                div_<(class_ = "zone victory", id = "opp-victory", data_label = "Victory")>{
                    mup::each(opp.victory, [&](Card &c) { return c.Render("mini", "", false); })
                },

                div_<(class_ = "zone battle", id = "opp-battle", data_label = "Battle")>{
                    mup::when(opp.battle.has_value(), [&]() { return opp.battle->card.Render("", "", opp.battle->face_down); })
                }
            ),

            div_<(class_ = "pile grave", id = "opp-grave")>{
                div_<(class_ = "stack")>{opp.graveyard.size()},
                small<>{"Graveyard"},
            }
        ),

        div_<(class_ = "divider")>{
            span<>{opp.name + "— " + std::to_string(opp.victory.size()) + "won"},
        },

        section<(class_ = "side me")>(
            div_<(class_ = "pile grave", id = "my-grave")>{
                div_<(class_ = "stack")>{me.graveyard.size()},
                small<>{"Graveyard"},
            },

            div_<(class_ = "center")>(
                div_<(class_ = "zone battle", id = "my-battle", R"(data-label="Battle")")>{
                    mup::when(me.battle.has_value(), [&]() { return me.battle->card.Render("", "", me.battle->face_down); })
                },

                div_<(class_ = "zone victory", id = "my-victory", R"(data-label="Victory")")>{
                    mup::each(me.victory, [&](Card &c) { return c.Render("mini", "", false); })
                }
            ),

            div_<(class_ = "pile deck-pile")>{
                div_<(class_ = "stack")>{},
                b<>{me.cards.size()},
            }
        ),

        div_<(class_ = "divider")>{
            span<>{me.name + "— " + std::to_string(me.victory.size()) + " won"},
        },

        Hand()
    );
}


std::string State::Hand() const {
    return section<(id = "hand", class_ = "hand", hx_swap_oob = "true")>{
        mup::each(hand, [&](Card &c) -> std::string {
            return form<"hx-ws:send", (hx_trigger = "click")>(
                (class_ = "hand-slot" + std::string(c.type == "spell" || battle.has_value() ? " blocked" : "")),

                input<(type = "hidden", name_ = "action")>(value = c.type == "spell" ? "place_spell" : "place_card"),
                input<(type = "hidden", name_ = "card_id")>(value = c.id),

                c.Render()
            );
        }),
    };
}
