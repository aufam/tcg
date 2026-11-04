#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/uuid.hpp>
#include <mup/html.h>
#include <unordered_set>

import brb;
import brb.static_files;
import cpx;
import cpx.fmt;
import cpx.yy_json;
import fmt;

struct Card {
    std::string id, type, name, art, effect, description;
    int         level;

    static constexpr mup::Attr data_card_id = "data-card-id";

    // {#
    //   Reusable card partial.
    //   Context: card {id, type, name, level, art, effect, description}
    //            face_down (bool), size ('' | 'mini'), fx (optional: 'reveal' | 'won' | 'lost')
    //   type is one of: robot (blue) | human (green) | monster (red) | spell (yellow)
    //   `art` is an image URL, e.g. /static/art/solo_empire/iron-hound.png
    //   NEVER include real card data when face_down is true for the opponent.
    // #}
    std::string Card_(std::string size, std::string fx = "", bool face_down = false) {
        using namespace mup::html;

        if (face_down)
            return article<>{
                (class_ = "card down " + size, id_ = "card-" + id, data_card_id = id),
                div_<(class_ = "back")>{span<>{"Dark Era"}},
            };

        return article<>{
            (class_ = "card " + type + " " + size + " " + fx, id_ = "card-" + id, data_card_id = id),

            div_<(class_ = "c-type")>{type},

            div_<(class_ = "c-head")>(

                span<(class_ = "c-name")>{name},

                mup::when(type != "spell", [&]() { return span<(class_ = "c-level")>{level}; })
            ),

            div_<(class_ = "c-art")>{[&]() {
                auto im = img<(alt = "", loading = "lazy")>;
                im.attributes.push_back("src=\"" + art + "\"");
                return im;
            }()},
            div_<(class_ = "c-effect")>{effect},
            div_<(class_ = "c-desc")>{description},
        };
    }
};

struct Deck {
    std::string       name, id, css, blurb;
    std::vector<Card> cards;

    std::string GetDeck() {
        using namespace mup::html;
        return div_<(id_ = "modal", class_ = "modal", hx_swap_oob = "true")>(div_<(class_ = "modal-box wide")>(
            header<>(
                h3<>{name, small<>{std::to_string(cards.size()) + " cards"}},
                button<(class_ = "ghost", onclick = "document.getElementById('modal').innerHTML=''")>{"Close"}
            ),

            div_<(class_ = "deck-grid")>{
                mup::each(cards, [](Card &c) { return c.Card_("mini"); }),
            }
        ));
    }
};

struct Slot {
    Card card;
    bool face_down;
};

struct State {
    std::shared_ptr<brb::ws_stream> ws;
    std::shared_ptr<State>          opp;

    std::string       name, phase = "wait";
    std::vector<Deck> decks;

    size_t deck_count;

    std::vector<Card> hand;
    std::vector<Card> victory;
    std::vector<Card> graveyard;

    std::unordered_set<std::string> placed_ids;

    std::optional<Slot> battle;
    std::vector<Slot>   spell_slots;

    bool        game_over;
    std::string winner;

    std::string Index() {
        using namespace mup::html;
        return html<(lang = "en")>(
            head<>(
                meta<(charset = "utf-8")>,
                meta<(name_ = "viewport", content_ = "width=device-width, initial-scale=1")>,

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

                aside<(id = "log", class_ = "log", hx_swap_oob = "true", aria_live = "polite")>{},
                div_<(id = "toast", class_ = "toast", hx_swap_oob = "true")>{},
                div_<(id = "modal", hx_swap_oob = "true")>{}
            )
        );
    }

    // {# Sent on every phase change. phase: draw | main | battle | wait #}
    std::string PhaseBar() {
        const std::string ph = "phase";

        using namespace mup::html;
        return div_<(id = "phase-bar", class_ = "phase-bar", hx_swap_oob = "true")>(
            span<>{(class_ = ph + (phase == "draw" ? " on" : "")), "Draw"},
            span<>{(class_ = ph + (phase == "main" ? " on" : "")), "Main"},
            span<>{(class_ = ph + (phase == "battle" ? " on" : "")), "Battle"},
            mup::when(phase == "wait", []() { return span<(class_ = "wait")>{"Waiting for opponent..."}; })
        );
    }

    // {# Sent over WS when a player (re)enters the lobby. Replaces #screen. #}
    std::string DeckSelect() {
        using namespace mup::html;
        return main_<(id = "screen", hx_swap_oob = "true")>{

            section<(class_ = "deck-select")>(
                h2<>{"Pick Your Started Deck"},

                div_<(class_ = "decks")>{mup::each(decks, [](Deck &d) {
                    return form<"hx-ws:send">(
                        (class_ = "deck " + d.css),
                        input<(type = "hidden", name_ = "action", value = "choose_deck")>,
                        [&]() {
                            auto in = input<(type = "hidden", name_ = "deck_id")>;
                            in.attributes.push_back("value=\"" + d.id + "\"");
                            return in;
                        }(),
                        h3<>{d.name},
                        p<>{d.blurb},
                        button<(type = "button", class_ = "ghost", hx_target = "#modal")>{
                            (hx_get = "decks/" + d.id),
                            "View cards",
                        },
                        button<(type = "submit")>{"Play " + d.name}
                    );
                })}

            )
        };
    }

    // {# Full board. Sent once the game starts. Replaces #screen.
    //    Context: me / opp each have: name, deck_count, graveyard (list), victory (list),
    //    spell_slots (list of {card, face_down}), battle ({card, face_down} or None).
    //    Opponent face-down cards are placeholders only. #}
    std::string Board() {
        auto &opp = *this->opp;
        auto &me  = *this;

        using namespace mup::html;
        return main_<(id = "screen", class_ = "board", hx_swap_oob = "true")>(

            section<(class_ = "side opp")>(
                div_<(class_ = "pile deck-pile")>{
                    div_<(class_ = "stack")>{},
                    b<>{opp.deck_count},
                },

                div_<(class_ = "center")>(
                    div_<(class_ = "zone victory", id = "opp-victory", R"(data-label="Victory")")>{
                        mup::each(opp.victory, [&](Card &c) { return c.Card_("mini", "", false); })
                    },

                    div_<(class_ = "zone spell", id = "opp-spell", R"(data-label="Spells")")>{
                        mup::each(opp.spell_slots, [&](Slot &s) { return s.card.Card_("mini", "", s.face_down); })
                    },

                    div_<(class_ = "zone battle", id = "opp-battle", R"(data-label="Battle")")>{
                        mup::when(opp.battle.has_value(), [&]() { return opp.battle->card.Card_("", "", opp.battle->face_down); })
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
                        mup::when(me.battle.has_value(), [&]() { return me.battle->card.Card_("", "", me.battle->face_down); })
                    },

                    div_<(class_ = "zone victory", id = "my-victory", R"(data-label="Victory")")>{
                        mup::each(me.victory, [&](Card &c) { return c.Card_("mini", "", false); })
                    },

                    div_<(class_ = "zone spell", id = "my-spell", R"(data-label="Spells")")>{
                        mup::each(me.spell_slots, [&](Slot &s) { return s.card.Card_("mini", "", s.face_down); })
                    }
                ),

                div_<(class_ = "pile deck-pile")>{
                    div_<(class_ = "stack")>{},
                    b<>{me.deck_count},
                }
            ),

            div_<(class_ = "divider")>{
                span<>{me.name + "— " + std::to_string(me.victory.size()) + "won"},
            }
        );
    }

    // {# Sent on draw phase and after every placement. Replaces #hand. #}
    // {# Context: hand (cards), phase ('draw'|'main'|'battle'|'wait'), placed_ids, spells_placed (0..2), monster_placed (bool) #}
    std::string Hand(int spells_placed, bool monster_placed) {
        using namespace mup::html;
        return section<(id = "hand", class_ = "hand", hx_swap_oob = "true")>{
            mup::when(
                phase == "draw",
                [&]() {
                    return form<(class_ = "hand-action", "hx-ws:send")>{
                        input<(type = "hidden", name_ = "action", value = "draw")>,
                        button<(type = "submit")>{"Draw to 5 cards"},
                    };
                }
            ),

            mup::when(
                phase == "main",
                [&]() {
                    return form<(class_ = "hand-action", "hx-ws:send")>{
                        input<(type = "hidden", name_ = "action", value = "ready")>,
                        button<(type = "submit")>{(disabled = !monster_placed), "Ready"},
                    };
                }
            ),

            mup::each(hand, [&](Card &c) -> std::string {
                if (phase == "main" && placed_ids.count(c.id)) {
                    return form<>{
                        (class_ =
                             "hand-slot " + std::string(
                                                (c.type == "spell" && spells_placed >= 2) || (c.type != "spell" && monster_placed)
                                                    ? "blocked"
                                                    : ""
                                            )),

                        [&]() {
                            auto in = input<(type = "hidden", name_ = "action")>;
                            in.attributes.push_back(
                                "value=\"" + std::string(c.type == "spell" ? "place_spell" : "place_card") + "\""
                            );
                            return in;
                        }(),

                        [&]() {
                            auto in = input<(type = "hidden", name_ = "card_id")>;
                            in.attributes.push_back("value=\"" + c.id + "\"");
                            return in;
                        }(),

                        c.Card_(""),
                    };
                } else {
                    return div_<>{
                        (class_ = "hand-slot " + std::string(placed_ids.count(c.id) ? "placed" : "")),
                        c.Card_(""),
                    };
                }
            }),
        };
    }

    // {# Sent when a battle pair is revealed. Then, after the player's decision (or a timer),
    //    the server sends result.html.
    //    Context: mine, theirs (cards, face up), outcome ('win'|'lose'|'draw'),
    //    reason_text (e.g. "Robot beats Monster", "9 loses to 1"),
    //    can_spell (bool: I may reveal a spell now), role ('loser'|'winner'), revealed, my_spells #}
    std::string
    Battle(Card &mine, Card &theirs, std::string outcome, std::string reason, bool can_spell, std::string role, Card &revealed) {
        using namespace mup::html;

        auto my_res  = "reveal " + std::string(outcome == "win" ? "won" : outcome == "lose" ? "lost" : "");
        auto opp_res = "reveal " + std::string(outcome == "win" ? "lost" : outcome == "lose" ? "won" : "");
        auto info    = outcome == "win" ? "You win this clash" : outcome == "You lose this clash" ? "draw" : "Draw";

        std::string ret = div_<(id = "my-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{
            mine.Card_("", my_res),
        };

        ret += div_<(id = "opp-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{
            theirs.Card_("", opp_res),
        };

        ret += div_<(id = "toast", class_ = "toast show", hx_swap_oob = "true")>{
            info,
            small<>{reason},
        };

        if (can_spell) {
            ret += SpellPrompt(role, revealed);
        } else {
            ret += div_<(id = "modal", hx_swap_oob = "true")>{form<(hx_trigger = "load delay:2500ms", "hx-ws:send")>{
                input<(type = "hidden", name_ = "action", value = "reveal_next")>
            }};
        }

        return ret;
    }

    // {# Included by battle.html. Two cases:
    //    role == 'loser'  : you lost the clash and may reveal a face-down spell to turn it around
    //    role == 'winner' : opponent revealed a spell (`revealed`); you may counter it with one of yours
    //    Context: role, revealed (card or None), my_spells (your face-down spells in the field)
    //    The server enforces a response timeout (e.g. 15s) and treats silence as skip_spell. #}
    std::string SpellPrompt(std::string role, Card &reveal) {
        using namespace mup::html;
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
                    std::string dd = div_<(class_ = "deck-grid")>{reveal.Card_("mini")};
                    std::string pp = p<>{"Counter it with one of yours?"};
                    return hh + "\n" + pp;
                }
            ),

            div_<(class_ = "deck-grid")>{mup::each(
                spell_slots,
                [](Slot &s) {
                    return form<(hx_trigger = "click", "hx-ws:send")>{
                        input<(type = "hidden", name_ = "action", value = "use_spell")>,
                        [&]() {
                            auto in = input<(type = "hidden", name_ = "card_id")>;
                            in.attributes.push_back("value\"" + s.card.id + "\"");
                            return in;
                        }(),
                        s.card.Card_("mini"),
                    };
                }
            )},

            form<"hx-ws:send">(
                input<(type = "hidden", name_ = "action", value = "skip_spell")>,
                button<(type = "submit", class_ = "ghost")>{"Don't reveal"}
            ),
        });
    }

    // {# Sent after each clash resolves (and at game end).
    //    Loser -> graveyard, winner -> victory zone, used spells -> graveyard.
    //    animations.js animates the moves automatically by tracking card ids across the swap.
    //    Context: me, opp (same shape as board.html), log_line, game_over, winner ('me'|'opp'|'draw') #}
    std::string Result(std::string log_line) {
        using namespace mup::html;

        std::string ret;
        ret += div_<(id = "my-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{};
        ret += div_<(id = "opp-battle", R"(data-label="Battle")", class_ = "zone battle", hx_swap_oob = "true")>{};
        ret += div_<(id = "my-spells", R"(data-label="Spells")", class_ = "zone spell", hx_swap_oob = "true")>{};
        ret += div_<(id = "opp-spells", R"(data-label="Spells")", class_ = "zone spell", hx_swap_oob = "true")>{};

        ret += div_<(id = "my-victory", R"(data-label="Victory")", class_ = "zone victory", hx_swap_oob = "true")>{
            mup::each(victory, [&](Card &c) { return c.Card_("mini"); })
        };

        ret += div_<(id = "opp-victory", R"(data-label="Victory")", class_ = "zone victory", hx_swap_oob = "true")>{
            mup::each(opp->victory, [&](Card &c) { return c.Card_("mini"); })
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
            ret += div_<(id = "modal", class_ = "modal", hx_swap_oob = "true")>{div_<(id = "modal-box")>(
                h3<>{winner == "me" ? "Victory" : winner = "opp" ? "Defeat" : "Draw"},
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

    std::string Log(std::string log_line) {
        using namespace mup::html;
        return aside<(id = "log", hx_swap_oob = "beforeend")>{
            p<>{log_line},
        };
    }
};

int main() {
    boost::asio::io_context io;
    brb::Engine             engine{io};

    std::mutex                             mtx;
    std::unordered_map<std::string, State> states;

    Deck sawit, solo;
    sawit.name  = "Sawit Empire";
    solo.name   = "Solo Empire";
    sawit.id    = "sawit_empire";
    solo.id     = "solo_empire";
    sawit.css   = "sawit";
    solo.css    = "solo";
    sawit.blurb = "Plantation hordes and overgrown machines. Win by numbers.";
    solo.blurb  = "Lone operators and cold steel. Few allies, sharp tricks.";

    const auto logger = [](brb::Context &c) -> brb::awaitable<void> {
        const auto  ep  = c.stream->socket().remote_endpoint();
        const auto &req = c.req();
        fmt::println("[{}:{}] {} {}", ep.address().to_string(), ep.port(), req.method_string(), req.target());

        try {
            co_await c.next();
        } catch (boost::system::system_error &e) {
            fmt::println("[{}:{}] {}", ep.address().to_string(), ep.port(), e.code().message());
            c.res().result(brb::status::internal_server_error);
            co_return;
        } catch (std::exception &e) {
            fmt::println("[{}:{}] {}", ep.address().to_string(), ep.port(), e.what());
            c.res().result(brb::status::internal_server_error);
            co_return;
        }

        const auto &res = c.res();
        fmt::println("[{}:{}] {} {}", ep.address().to_string(), ep.port(), (int)res.result(), res.reason());
    };

    const auto static_files = brb::middlewares::static_files{
        .dir           = "dist",
        .spa           = false,
        .last_modified = false,
    };

    engine.use(logger, static_files);

    const auto get_id = [](brb::Context &c) {
        std::string_view cookie = c.req()[brb::field::cookie];
        std::string_view key    = "playground_id=";
        if (!cookie.starts_with(key)) {
            return std::string{};
        }

        cookie = cookie.substr(key.size());
        cookie = cookie.substr(0, cookie.find(';'));
        return std::string(cookie);
    };

    const auto create_id = [&](brb::Context &c) {
        std::string id = boost::uuids::to_string(boost::uuids::random_generator()());
        c.res().set(brb::field::set_cookie, fmt::format("playground_id={}; Max-Age=31536000", id));
        return id;
    };

    engine.route(brb::verb::get, "/", [&](brb::Context &c) -> brb::awaitable<void> {
        auto id = get_id(c);
        if (id.empty())
            id = create_id(c);

        {
            std::lock_guard<std::mutex> lock(mtx);

            auto &s = states[id];
            s.decks.push_back(sawit);
            s.decks.push_back(solo);
            s.deck_count = 22;

            c.response_string().body() = s.Index();
        }
        c.res().set(brb::field::content_type, "text/html");

        co_return;
    });

    engine.route(brb::verb::get, "/ws/game", [&](brb::Context &c) -> brb::awaitable<void> {
        auto &req = c.parser_empty().get();
        if (!brb::ws::is_upgrade(req)) {
            c.res().result(brb::status::bad_request);
            co_return;
        }

        auto id = get_id(c);
        auto ws = std::make_shared<brb::ws_stream>(std::move(*c.stream));
        {
            std::lock_guard<std::mutex> lock(mtx);

            if (states.count(id) == 0) {
                c.res().result(brb::status::unauthorized);
                c.response_string().body() = "failed to establish ws connection";
                co_return;
            }

            states.at(id).ws = ws;
        }

        co_await ws->async_accept(c.parser_empty().get());

        while (ws->is_open()) {
            brb::flat_buffer buf;
            co_await ws->async_read(buf);

            std::string action;
            std::string deck_id;
            std::string card_id;

            auto parser = std::tuple{
                cpx::field_ref(action)  = "action",
                cpx::field_ref(deck_id) = "deck_id,skipmissing",
                cpx::field_ref(card_id) = "card_id,skipmissing",
            };

            cpx::yy_json::parse(std::string_view(static_cast<char *>(buf.data().data()), buf.size()), parser);

            fmt::println("{}", parser);

            // TODO: process the action
        }
    });

    const auto run_opts = brb::Engine::run_opts{
        .host                 = "0.0.0.0",
        .port                 = 3000,
        .cancellation_signals = {SIGINT, SIGTERM},
    };
    boost::asio::co_spawn(io, engine.run(run_opts), boost::asio::detached);

    io.run();
}
