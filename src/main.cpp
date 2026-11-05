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

    // ── game-logic bookkeeping (managed by the ws handler below) ──────────────
    std::vector<Card>                     draw_pile;           // face-down deck, back() = next draw
    std::string                           deck_choice;         // chosen starter deck id
    std::string                           session;             // cookie id of this player (used by watchdog timers)
    std::vector<std::pair<Card, bool>>    clash;               // spells revealed this clash, in order; bool = mine
    std::vector<std::string>              used_spells;         // spell ids revealed this clash
    bool                                  acked  = false;      // player saw current battle/result screen
    int                                   prompt = 0;          // 0 none | 1 'loser' role | 2 'winner' role
    std::chrono::steady_clock::time_point deadline{};          // 15s response window starts here
    bool                                  want_replay = false; // pressed "play again" at game over
    std::vector<std::string>              outbox;              // pending htmx frames; one ws writer only
    int                                   stage = 0;           // 0 idle | 1 placement | 2 reveal | 3 round end

    std::string Index() {
        using namespace mup::html;
        return html<(lang = "en")>(
            head<>(
                meta<(charset = "utf-8")>,
                meta<(name_ = "viewport", content_ = "width=device-width, initial-scale=1")>,
                meta<(name_ = "htmx-config", content_ = "ws.pauseOnBackground:false")>,

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

                    div_<(class_ = "zone spell", id = "opp-spells", R"(data-label="Spells")")>{
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

                    div_<(class_ = "zone spell", id = "my-spells", R"(data-label="Spells")")>{
                        mup::each(me.spell_slots, [&](Slot &s) { return s.card.Card_("mini", "", s.face_down); })
                    },

                    div_<(class_ = "zone victory", id = "my-victory", R"(data-label="Victory")")>{
                        mup::each(me.victory, [&](Card &c) { return c.Card_("mini", "", false); })
                    }
                ),

                div_<(class_ = "pile deck-pile")>{
                    div_<(class_ = "stack")>{},
                    b<>{me.deck_count},
                }
            ),

            div_<(class_ = "divider")>{
                span<>{me.name + "— " + std::to_string(me.victory.size()) + " won"},
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
                if (phase == "main" && !placed_ids.count(c.id)) {
                    return form<(hx_trigger = "click", "hx-ws:send")>(
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

                        c.Card_("")
                    );
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
        auto info    = outcome == "win" ? "You win this clash" : outcome == "lose" ? "You lose this clash" : "Clash cancelled";

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
                            in.attributes.push_back("value=\"" + s.card.id + "\"");
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

    std::string Log(std::string log_line) {
        using namespace mup::html;
        return aside<(id = "log", hx_swap_oob = "beforeend")>{
            p<>{log_line},
        };
    }
};

// ─────────────────────────── cards & duel rules ─────────────────────────────
// A starter deck = 18 creatures (6 human / 6 robot / 6 monster) + 4 spells.
// Card types: human beats robot, robot beats monster, monster beats human.
// Ties (same type) go to the highest level — except level 1 beats level 9.
// Effect strings double as machine-readable tags consumed by evaluate():
//   creature "Artillery +N" : flat +N level in every clash
//   creature "Shell Game"   : +1 level per face-down spell you control
//   spell    "Trample +N"   : your creature gains +N level this clash
//   spell    "Weaken -N"    : enemy creature loses N level this clash
//   spell    "Mirror"       : swap effective levels this clash
//   spell    "Attune"       : your creature becomes the type that beats the
//                            enemy's type for this clash
static std::mt19937     rng{std::random_device{}()};
static std::atomic<int> match_seq{0};

static std::string beats(const std::string &type) {
    if (type == "human")
        return "robot";
    if (type == "robot")
        return "monster";
    return "human";
}

// "…Trample +2…" → +2 ; "…Weaken -3…" → -3 ; missing / no number → 0
static int effect_number(const std::string &effect, std::string_view key) {
    auto i = effect.find(key);
    if (i == std::string::npos)
        return 0;

    size_t j = i + key.size();
    while (j < effect.size() && (effect[j] == ' ' || effect[j] == ':'))
        j++;

    int sign = 1;
    if (j < effect.size() && (effect[j] == '-' || effect[j] == '+')) {
        sign = effect[j] == '-' ? -1 : 1;
        j++;
    }

    int  value = 0;
    bool any   = false;
    while (j < effect.size() && isdigit((unsigned char)effect[j])) {
        value = value * 10 + (effect[j] - '0');
        j++;
        any = true;
    }
    return any ? sign * value : 0;
}

// 22 cards, ids get their per-match unique prefix later in start_match().
static std::string art_uri(const std::string &type, const std::string &name) {
    std::string_view  fill  = type == "human"   ? "%233a9a4f"  // #3a9a4f
                              : type == "robot" ? "%232f6fdb"  // #2f6fdb
                              : type == "spell" ? "%23e3b520"  // #e3b520
                                                : "%23c8372d"; // #c8372d
    const std::string glyph = name.empty() ? "?" : std::string(1, name.front());

    auto enc = [](std::string s) {
        std::string out;
        for (char ch : s) {
            switch (ch) {
            case '<': out += "%3C"; break;
            case '>': out += "%3E"; break;
            case '"': out += "%22"; break;
            case '#': out += "%23"; break;
            case '%': out += "%25"; break;
            case ' ': out += "%20"; break;
            default: out += ch; break;
            }
        }
        return out;
    };

    const std::string svg = fmt::format(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 150 210'>"
        "<rect width='150' height='210' fill='{}'/>"
        "<rect x='26' y='26' width='98' height='158' fill='none' stroke='%23141210' stroke-width='4'/>"
        "<text x='75' y='132' font-size='76' font-family='Impact,sans-serif' fill='%23141210' text-anchor='middle'>{}</text>"
        "</svg>",
        fill,
        glyph
    );
    return "data:image/svg+xml," + enc(svg);
}

static std::vector<Card> make_starter_deck() {
    static const std::string_view prefixes[]      = {"Rust", "Iron",  "Bone",  "Ash",   "Grim", "Night", "Chain", "Soot",
                                                     "Husk", "Blood", "Ember", "Thorn", "Oil",  "Rat",   "Glass", "Static",
                                                     "Orb",  "Mold",  "Salt",  "Slag",  "Wire", "Grit",  "Venom", "Lantern"};
    static const std::string_view monster_tails[] = {"Devourer", "Behemoth", "Wretch", "Colossus", "Howler", "Chimera"};
    static const std::string_view human_tails[]   = {"Marauder", "Sniper", "Duelist", "Preacher", "Bounty", "Deserter"};
    static const std::string_view robot_tails[]   = {"Hound", "Sentinel", "Drone", "Pilgrim", "Treader", "Automaton"};
    static const std::string_view descs[]         = {
        "Dark Era reprint.",
        "No one remembers who built it.",
        "Signed and sealed in soot.",
        "Salvaged from a quieter age.",
        "It answers only to the sun.",
        "Everybody owes somebody something."
    };
    static const std::string_view creature_effects[] = {"", "", "Artillery +1", "Artillery +2", "Shell Game"};

    std::uniform_int_distribution<size_t> pick(0, 1000);
    const auto                            name = [&](std::string_view tail, int i) {
        std::string n = std::string(prefixes[(pick(rng) + i) % std::size(prefixes)]);
        n += " ";
        n += tail;
        return n;
    };

    std::vector<Card> cards;

    const auto creature = [&](const char *type, std::string_view tail) {
        Card c;
        c.id          = "";
        c.type        = type;
        c.level       = 1 + (int)(pick(rng) % 9); // 1..9
        c.name        = name(tail, (int)cards.size());
        c.effect      = creature_effects[pick(rng) % std::size(creature_effects)];
        c.description = std::string(descs[pick(rng) % std::size(descs)]);
        c.art         = art_uri(c.type, c.name);
        cards.push_back(std::move(c));
    };

    // 6 of each type — two guaranteed ramp cards + four random pulls per type
    for (int round = 0; round < 6; ++round) {
        creature("monster", monster_tails[(pick(rng) + round) % std::size(monster_tails)]);
        creature("human", human_tails[(pick(rng) + round) % std::size(human_tails)]);
        creature("robot", robot_tails[(pick(rng) + round) % std::size(robot_tails)]);
    }

    // guarantee the "1 beats 9" rule has material in every deck
    cards[0].level = 9;
    cards[6].level = 1;

    // 4 spells drawn from 5 templates
    static const std::array<std::pair<const char *, const char *>, 5> spell_pool{
        std::pair{"Trample +2", "Marches over whatever is in the way."            },
        std::pair{"Trample +3", "Boots first, questions later."                   },
        std::pair{"Weaken -2",  "The enemy's engine answers a little slower."     },
        std::pair{"Mirror",     "Your level becomes their level, for one clash."  },
        std::pair{"Attune",     "Tunes your creature to whatever beats the enemy."},
    };

    std::array<int, 5> indices{0, 1, 2, 3, 4};
    std::shuffle(indices.begin(), indices.end(), rng);
    for (int k = 0; k < 4; ++k) {
        const std::string eff = spell_pool[(size_t)indices[k]].first;
        Card              c;
        c.id          = "";
        c.type        = "spell";
        c.level       = 0;
        c.name        = eff.substr(0, eff.find(' '));
        c.effect      = eff;
        c.description = spell_pool[(size_t)indices[k]].second;
        c.art         = art_uri(c.type, c.name);
        cards.push_back(std::move(c));
    }

    return cards;
}

static Card &dummy_card() {
    static Card dummy;
    dummy.type = "spell";
    return dummy;
}

// Effective (level, type) of one side's battle creature, after its own
// keywords and every spell revealed so far (reveal order = clash order).
struct Side {
    int         level;
    std::string type;
};

static Side effective_side(const Slot &s, size_t face_down_spells) {
    Side side{s.card.level, s.card.type};

    if (s.card.effect.find("Artillery") != std::string::npos) {
        side.level += effect_number(s.card.effect, "Artillery");
    }
    if (s.card.effect.find("Shell Game") != std::string::npos) {
        side.level += (int)face_down_spells;
    }
    return side;
}

// Full verdict from a's perspective. Returns {+1 a wins | 0 draw | -1 b wins, reason}.
static std::pair<int, std::string> evaluate(const State &a, const State &b) {
    const Slot *mine   = a.battle ? &*a.battle : nullptr;
    const Slot *theirs = b.battle ? &*b.battle : nullptr;

    if (!mine && !theirs)
        return {0, "Both fields empty — clash cancelled"};
    if (!mine)
        return {-1, "No creature to answer with"};
    if (!theirs)
        return {+1, "Opponent has no creature"};

    Side me   = effective_side(*mine, a.spell_slots.size());
    Side them = effective_side(*theirs, b.spell_slots.size());

    // spells apply in reveal order regardless of who played them
    for (const auto &event : a.clash) {
        const auto &spell      = event.first;
        const bool  mine_spell = event.second;

        if (spell.effect.find("Trample") != std::string::npos) {
            if (mine_spell)
                me.level += effect_number(spell.effect, "Trample");
            else
                them.level += effect_number(spell.effect, "Trample");
        } else if (spell.effect.find("Weaken") != std::string::npos) {
            const int n = effect_number(spell.effect, "Weaken"); // negative value
            if (mine_spell)
                them.level += n;
            else
                me.level += n;
        } else if (spell.effect.find("Mirror") != std::string::npos) {
            std::swap(me.level, them.level);
        } else if (spell.effect.find("Attune") != std::string::npos) {
            if (mine_spell)
                me.type = beats(them.type);
            else
                them.type = beats(me.type);
        }
    }

    if (me.level == 1 && them.level == 9)
        return {+1, "Level 1 topples Level 9"};
    if (them.level == 1 && me.level == 9)
        return {-1, "Level 1 topples Level 9"};

    if (me.type != them.type) {
        if (beats(me.type) == them.type)
            return {+1, me.type + " beats " + them.type};
        return {-1, them.type + " beats " + me.type};
    }

    if (me.level != them.level) {
        const int w = std::max(me.level, them.level);
        const int l = std::min(me.level, them.level);
        return {me.level > them.level ? +1 : -1, "Level " + std::to_string(w) + " beats Level " + std::to_string(l)};
    }

    return {0, "Same level — clash cancelled"};
}

struct Lobby {
    std::mutex                             &mtx;
    std::unordered_map<std::string, State> &states;
    std::unordered_map<std::string, Deck>  &decks;
};

static void re_veil(Lobby &L, boost::asio::io_context &io, State &a, State &b); // forward declaration

// ─────────────────────────── turn engine ────────────────────────────────────
constexpr int  WIN_TARGET      = 6; // first to 6 victory cards wins
constexpr auto RESPONSE_WINDOW = std::chrono::seconds(15);

static void strand(State &s) {
    // A hand with no playable creature auto-presses Ready: the round is lost.
    if (s.phase != "main" || s.battle)
        return;
    const bool has_creature =
        std::any_of(s.hand.begin(), s.hand.end(), [&](const Card &c) { return c.type != "spell" && !s.placed_ids.count(c.id); });
    if (!has_creature)
        s.phase = "battle";
}

static void refresh_screen(State &s) {
    s.outbox.push_back(s.PhaseBar());
    s.outbox.push_back(s.Board());
    s.outbox.push_back(s.Hand(int(s.spell_slots.size()), s.battle.has_value()));
}

static void start_match(Lobby &L, State &a, State &b) {
    const auto &sawit = L.decks.at("sawit_empire");
    const auto &solo  = L.decks.at("solo_empire");

    const int    tag = match_seq.fetch_add(1);
    std::mt19937 local_rng{std::random_device{}()};

    const auto deal = [&](State &p, const char side_tag) {
        auto &source = p.deck_choice == "solo_empire" ? solo : sawit;
        p.name       = fmt::format("Duelist {}", (char)std::toupper(side_tag));
        auto pile    = source.cards;
        int  i       = 0;
        for (auto &c : pile)
            c.id = fmt::format("{}{}-{:02d}", tag, side_tag, i++);
        std::shuffle(pile.begin(), pile.end(), local_rng);

        p.draw_pile = std::move(pile);
        p.hand.clear();
        p.victory.clear();
        p.graveyard.clear();
        p.spell_slots.clear();
        p.placed_ids.clear();
        p.battle.reset();
        p.clash.clear();
        p.used_spells.clear();
        p.acked       = false;
        p.prompt      = 0;
        p.want_replay = false;
        p.game_over   = false;
        p.winner.clear();
        p.stage = 1;

        // entering the table draws the opening hand automatically
        while (p.hand.size() < 5 && !p.draw_pile.empty()) {
            p.hand.push_back(p.draw_pile.back());
            p.draw_pile.pop_back();
        }
        p.deck_count = p.draw_pile.size();
        p.phase      = "main";
    };

    deal(a, 'a');
    deal(b, 'b');
    refresh_screen(a);
    refresh_screen(b);
}

// Renders the current clash for both sides and arms a 15s response window for
// whoever can turn it around (the losing side). Silence = "don't reveal".
static void show_clash(Lobby &L, boost::asio::io_context &io, State &a, State &b, int code, const std::string &reason) {
    auto show = [&](State &s, State &foe) {
        s.acked = false;

        const std::string outcome = code == 0 ? "draw" : (code > 0) == (&s == &a) ? "win" : "lose";
        const bool        losing  = outcome == "lose";

        const bool enemy_revealed = std::any_of(s.clash.begin(), s.clash.end(), [](const auto &e) { return !e.second; });
        s.prompt                  = losing && !s.spell_slots.empty() ? (enemy_revealed ? 2 : 1) : 0;

        Card *revealed = nullptr;
        for (auto e = s.clash.rbegin(); e != s.clash.rend(); ++e)
            if (!e->second) {
                revealed = &e->first;
                break;
            }
        if (!revealed)
            revealed = &dummy_card();

        s.outbox.push_back(s.PhaseBar());
        s.outbox.push_back(s.Battle(
            s.battle->card, foe.battle->card, outcome, reason, s.prompt != 0, s.prompt == 2 ? "winner" : "loser", *revealed
        ));

        if (s.prompt) {
            const auto deadline = std::chrono::steady_clock::now() + RESPONSE_WINDOW;
            s.deadline          = deadline;

            // arm the silence timer for this player
            boost::asio::co_spawn(
                io,
                [&L, &io, id = s.session, deadline]() -> boost::asio::awaitable<void> {
                    boost::asio::steady_timer timer(io);
                    timer.expires_after(RESPONSE_WINDOW + std::chrono::seconds(1));
                    co_await timer.async_wait(boost::asio::use_awaitable);

                    std::lock_guard lock(L.mtx);
                    auto           &t = L.states.at(id);
                    if (t.opp && t.prompt > 0 && t.deadline == deadline) {
                        t.prompt = -1; // window expired → counts as "don't reveal"
                        re_veil(L, io, t, *t.opp);
                    }
                },
                boost::asio::detached
            );
        }
    };

    show(a, b);
    show(b, a);
}

// Opening render of a fresh clash (both sides pressed Ready)
static void begin_clash(Lobby &L, boost::asio::io_context &io, State &a, State &b) {
    a.clash.clear();
    b.clash.clear();
    a.used_spells.clear();
    b.used_spells.clear();

    const auto verdict = evaluate(a, b);
    show_clash(L, io, a, b, verdict.first, verdict.second);
}

// A spell was revealed (or window expired): re-evaluate and re-show. The newly
// losing side may now counter, and the chain continues until spells run dry,
// a skip, or a cancel-worthy (equal) evaluation.
static void re_veil(Lobby &L, boost::asio::io_context &io, State &a, State &b) {
    const auto verdict = evaluate(a, b);
    show_clash(L, io, a, b, verdict.first, verdict.second);
}

// Both players confirmed the final screen: move cards, log it,
// check win conditions (6 victories or both libraries empty), render result.
static void settle(State &a, State &b) {
    const auto verdict = evaluate(a, b);
    const int  code    = verdict.first;

    const auto take = [&](State &winner, State &loser) {
        winner.victory.push_back(winner.battle->card);
        loser.graveyard.push_back(loser.battle->card);
    };
    if (code > 0)
        take(a, b);
    else if (code < 0)
        take(b, a);
    else {
        a.graveyard.push_back(a.battle->card);
        b.graveyard.push_back(b.battle->card);
    }

    // revealed spells are spent to the graveyard; unplayed face-down spells
    // never left the hand, so they simply stay available
    const auto spend = [&](State &s) {
        for (const auto &event : s.clash)
            if (event.second)
                s.graveyard.push_back(event.first);
    };
    spend(a);
    spend(b);

    std::string log_line;
    if (code == 0)
        log_line = fmt::format("{} and {} cancel the clash", a.name, b.name);
    else {
        const State &w = code > 0 ? a : b;
        const State &l = code > 0 ? b : a;
        log_line       = fmt::format(
            "{}'s {} ({}) beats {}'s {} ({}): {}",
            w.name,
            w.battle->card.name,
            w.battle->card.level,
            l.name,
            l.battle->card.name,
            l.battle->card.level,
            verdict.second
        );
    }

    const auto tidy = [&](State &s) {
        for (const auto &spell_id : s.used_spells)
            std::erase_if(s.hand, [&](const Card &c) { return c.id == spell_id; });
        if (s.battle)
            std::erase_if(s.hand, [&](const Card &c) { return c.id == s.battle->card.id; });
        s.used_spells.clear();
        s.clash.clear();
        s.battle.reset();
        s.spell_slots.clear();
        s.placed_ids.clear();
        s.prompt = 0;
        s.acked  = false;
        s.phase  = "wait";
    };

    const bool over = (int)a.victory.size() >= WIN_TARGET || (int)b.victory.size() >= WIN_TARGET ||
                      (a.draw_pile.empty() && b.draw_pile.empty());
    a.game_over = b.game_over = over;

    if (over) {
        if (a.victory.size() > b.victory.size()) {
            a.winner = "me";
            b.winner = "opp";
        } else if (a.victory.size() < b.victory.size()) {
            a.winner = "opp";
            b.winner = "me";
        } else {
            a.winner = "draw";
            b.winner = "draw";
        }
    }

    tidy(a);
    tidy(b);

    a.stage = b.stage = 3;
    a.outbox.push_back(a.Result(log_line));
    a.outbox.push_back(a.PhaseBar());
    b.outbox.push_back(b.Result(log_line));
    b.outbox.push_back(b.PhaseBar());
}

// Round over and confirmed: back to the draw phase for both sides.
static void next_round(State &a, State &b) {
    a.stage = b.stage = 1;
    a.phase = b.phase = "draw";
    a.acked = b.acked = false;
    refresh_screen(a);
    refresh_screen(b);
}

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
    sawit.cards = make_starter_deck();
    solo.cards  = make_starter_deck();

    // Shared game machine context; outlives connections, so detached watchdog
    // timers (spell-prompt windows) can always reach it safely.
    std::unordered_map<std::string, Deck> lobby_decks;
    lobby_decks.emplace("sawit_empire", sawit);
    lobby_decks.emplace("solo_empire", solo);
    Lobby lobby{mtx, states, lobby_decks};

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
        auto             pos    = cookie.find(key); // Find the key anywhere
        if (pos == std::string_view::npos) {
            return std::string{};
        }

        cookie = cookie.substr(pos + key.size()); // Start after the key
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
            if (s.decks.empty()) {
                s.decks.push_back(sawit);
                s.decks.push_back(solo);
            }
            s.deck_count = 22;

            c.response_string().body() = s.Index();
        }
        c.res().set(brb::field::content_type, "text/html");

        co_return;
    });

    engine.route(brb::verb::get, "/decks/:deck_id", [&](brb::Context &c) -> brb::awaitable<void> {
        const auto deck_id = std::string(c.get(":deck_id"));
        auto      &deck    = deck_id == "solo_empire" ? solo : sawit;

        c.response_string().body() = deck.GetDeck();
        c.res().set(brb::field::content_type, "text/html");
        co_return;
    });

    engine.route(brb::verb::get, "/ws/game", [&](brb::Context &c) -> brb::awaitable<void> {
        if (!brb::ws::is_upgrade(c.parser_empty().get())) {
            c.res().result(brb::status::bad_request);
            co_return;
        }

        auto id = get_id(c);
        if (states.count(id) == 0) {
            // must answer before the tcp stream is moved into the websocket
            c.res().result(brb::status::unauthorized);
            c.res().set(brb::field::content_type, "text/plain");
            c.response_string().body() = "failed to establish ws connection";
            co_return;
        }

        auto ws = std::make_shared<brb::ws_stream>(std::move(*c.stream));
        {
            std::lock_guard<std::mutex> lock(mtx);
            states.at(id).ws      = ws;
            states.at(id).session = id;
        }

        co_await ws->async_accept(c.parser_empty().get());
        co_await ws->async_write(brb::buffer(states.at(id).Log("Hello")));

        // Mid-match resume: a reload (or reconnect) replays the current screen
        // so the player doesn't land on an empty board.
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto                       &s = states.at(id);
            if (s.opp)
                refresh_screen(s);
        }

        // One writer per socket: this coroutine drains the state's outbox, so
        // game logic anywhere else only ever appends frames (never interleaves).
        boost::asio::co_spawn(
            io,
            [&, id, ws]() -> brb::awaitable<void> {
                boost::asio::steady_timer idle(io);
                for (;;) {
                    std::string frame;
                    bool        stop = false;
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        auto                       &s = states.at(id);
                        if (!s.outbox.empty()) {
                            frame = std::move(s.outbox.front());
                            s.outbox.erase(s.outbox.begin());
                        } else if (s.ws != ws) {
                            stop = true; // socket replaced or detached
                        }
                    }
                    if (stop)
                        co_return;
                    if (frame.empty()) {
                        idle.expires_after(std::chrono::milliseconds(25));
                        co_await idle.async_wait(boost::asio::use_awaitable);
                        continue;
                    }
                    try {
                        co_await ws->async_write(boost::asio::buffer(frame));
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(mtx);
                        auto                       &s = states.at(id);
                        if (s.ws == ws)
                            s.ws = nullptr; // socket died
                        s.outbox.clear();
                        co_return;
                    }
                }
            },
            boost::asio::detached
        );

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

            // ─────────────────────── game logic ─────────────────────────────────
            // Players pair up by deck choice; the turn machine then walks the
            // pair through draw → main → battle → reveal (spell chain) → result.
            // The machine only appends frames into state outboxes; the flusher
            // coroutine of each socket does the actual delivering.

            const auto pair_up = [&](State &me) {
                if (me.opp || me.deck_choice.empty() || !me.ws)
                    return;
                for (auto &[pid, t] : states) {
                    if (&t == &me || !t.ws || t.opp || t.deck_choice.empty())
                        continue;
                    // both map entries live forever — the gate keeps the
                    // shared_ptr from ever "owning" a map value
                    static const std::shared_ptr<void> gate = std::make_shared<char>();
                    me.opp                                  = std::shared_ptr<State>(gate, &t);
                    t.opp                                   = std::shared_ptr<State>(gate, &me);
                    start_match(lobby, me, t);
                    return;
                }
                me.outbox.push_back(me.Log("Waiting for an opponent to pick a deck…"));
            };

            const auto advance = [&](State &a) {
                if (!a.opp)
                    return;
                State &b = *a.opp;

                if (a.stage == 1) {
                    // placement: fire the clash once both sides pressed Ready
                    strand(a);
                    strand(b);
                    if (a.phase == "battle" && b.phase == "battle") {
                        a.stage = b.stage = 2;
                        begin_clash(lobby, io, a, b);
                    }
                } else if (a.stage == 2) {
                    // reveal + spell chain: expired response windows count as a skip
                    const auto expire = [](State &s) {
                        if (s.prompt > 0 && std::chrono::steady_clock::now() >= s.deadline)
                            s.prompt = -1;
                    };
                    expire(a);
                    expire(b);

                    if (a.prompt < 0 || b.prompt < 0)
                        re_veil(lobby, io, a, b);
                    else if (a.acked && b.acked && !a.prompt && !b.prompt)
                        settle(a, b);
                } else if (a.stage == 3) {
                    // result confirmed by both: deal out the next round
                    if (a.acked && b.acked && !a.game_over)
                        next_round(a, b);
                }
            };

            {
                std::lock_guard<std::mutex> lock(mtx);
                if (auto it = states.find(id); it != states.end()) {
                    auto &s = it->second;

                    if (action == "choose_deck") {
                        if (!s.opp && (deck_id == "sawit_empire" || deck_id == "solo_empire")) {
                            s.deck_choice = deck_id;
                            pair_up(s);
                        }
                    } else if (s.opp) {
                        if (action == "draw") {
                            if (s.phase == "draw" && !s.game_over) {
                                while (s.hand.size() < 5 && !s.draw_pile.empty()) {
                                    s.hand.push_back(s.draw_pile.back());
                                    s.draw_pile.pop_back();
                                }
                                s.deck_count = s.draw_pile.size();
                                s.phase      = "main";
                                s.outbox.push_back(s.PhaseBar());
                                s.outbox.push_back(s.Hand(int(s.spell_slots.size()), s.battle.has_value()));
                                s.outbox.push_back(s.opp->Board()); // their view of my deck count
                            }
                        } else if (action == "place_card") {
                            if (s.phase == "main" && !s.battle && !s.placed_ids.count(card_id)) {
                                auto card = std::find_if(s.hand.begin(), s.hand.end(), [&](const Card &c) {
                                    return c.id == card_id && c.type != "spell";
                                });
                                if (card != s.hand.end()) {
                                    s.battle = Slot{*card, true}; // face down until the reveal
                                    s.placed_ids.insert(card_id);
                                    s.outbox.push_back(s.Board());
                                    s.outbox.push_back(s.Hand(int(s.spell_slots.size()), true));
                                    s.outbox.push_back(s.opp->Board()); // they only see the card back
                                }
                            }
                        } else if (action == "place_spell") {
                            if (s.phase == "main" && s.spell_slots.size() < 2 && !s.placed_ids.count(card_id)) {
                                auto card = std::find_if(s.hand.begin(), s.hand.end(), [&](const Card &c) {
                                    return c.id == card_id && c.type == "spell";
                                });
                                if (card != s.hand.end()) {
                                    s.spell_slots.push_back(Slot{*card, true});
                                    s.placed_ids.insert(card_id);
                                    s.outbox.push_back(s.Board());
                                    s.outbox.push_back(s.Hand(int(s.spell_slots.size()), s.battle.has_value()));
                                    s.outbox.push_back(s.opp->Board());
                                }
                            }
                        } else if (action == "ready") {
                            if (s.phase == "main" && s.battle) {
                                s.phase = "battle";
                                s.outbox.push_back(s.PhaseBar());
                            }
                        } else if (action == "reveal_next") {
                            if (s.stage == 2 || s.stage == 3)
                                s.acked = true;
                        } else if (action == "use_spell") {
                            if (s.prompt > 0 && std::chrono::steady_clock::now() < s.deadline) {
                                auto slot = std::find_if(s.spell_slots.begin(), s.spell_slots.end(), [&](const Slot &spell) {
                                    return spell.face_down && spell.card.id == card_id;
                                });
                                if (slot != s.spell_slots.end()) {
                                    Card played = slot->card;
                                    s.spell_slots.erase(slot);
                                    s.clash.push_back({played, true});
                                    s.used_spells.push_back(played.id);
                                    s.prompt = -1; // re-evaluate and re-show the clash
                                }
                            }
                        } else if (action == "skip_spell") {
                            if (s.prompt > 0)
                                s.prompt = -1;
                        } else if (action == "play_again") {
                            if (s.opp && s.game_over) {
                                if (s.opp->want_replay && s.opp->ws) {
                                    start_match(lobby, s, *s.opp);
                                } else if (!s.opp->ws) {
                                    // opponent is gone: unlink, back to the lobby
                                    s.opp->opp         = nullptr;
                                    s.opp->stage       = 0;
                                    s.opp->want_replay = false;
                                    s.opp->game_over   = false;
                                    s.opp              = nullptr;
                                    s.stage            = 0;
                                    s.want_replay      = false;
                                    s.game_over        = false;
                                    s.outbox.push_back(s.DeckSelect());
                                } else {
                                    s.want_replay = true;
                                    s.outbox.push_back(s.Log("Waiting for the other duelist to press Play again…"));
                                }
                            }
                        }

                        advance(s); // drive the turn machine after every change
                    }
                }
            }
        }

        // socket closed: if a duel was live, the survivor wins by forfeit
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto                       &s = states.at(id);
            if (s.ws == ws)
                s.ws = nullptr;
            s.outbox.clear();

            if (s.opp && !s.game_over) {
                auto &o = *s.opp;
                if (s.battle) {
                    s.graveyard.push_back(s.battle->card);
                    s.battle.reset();
                }
                if (o.battle) {
                    o.victory.push_back(o.battle->card);
                    o.battle.reset();
                }
                s.spell_slots.clear();
                o.spell_slots.clear();
                o.prompt      = 0;
                o.want_replay = false;
                o.game_over   = true;
                o.winner      = "me";
                o.phase       = "wait";
                o.stage       = 3;
                o.outbox.push_back(o.Result(fmt::format("{} left the table", s.name)));
                o.outbox.push_back(o.PhaseBar());
            }
        }
    });

    const auto run_opts = brb::Engine::run_opts{
        .host                 = "0.0.0.0",
        .port                 = 3001,
        .cancellation_signals = {SIGINT, SIGTERM},
    };
    boost::asio::co_spawn(io, engine.run(run_opts), boost::asio::detached);

    io.run();
}
