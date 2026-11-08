#include <mup/html.h>
#include <random>
#include <algorithm>
#include "main.h"

import fmt;

using namespace mup::html;

std::string Deck::Show() const {
    return div_<(id_ = "modal", class_ = "modal", hx_swap_oob = "true")>(div_<(class_ = "modal-box wide")>(
        header<>(
            h3<>{name, small<>{std::to_string(cards.size()) + " cards"}},
            button<(type = "button", class_ = "ghost", hx_get = "clean-modal", hx_target = "#modal")>{"Close"}
        ),

        div_<(class_ = "deck-grid")>{
            mup::each(cards, [](const Card &c) { return c.Render("mini"); }),
        }
    ));
}

std::vector<Card> Deck::shuffle() const {
    auto ret = cards;

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::shuffle(ret.begin(), ret.end(), rng);

    return ret;
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

Deck Deck::Solo() {
    return {
        .name  = "Solo Empire",
        .id    = "solo",
        .css   = "solo",
        .blurb = "Semua berawal dari Solo",
        .cards = {
                  //
            Card{
                .id          = "solo-01",
                .type        = "human",
                .name        = "Pria Solo",
                .art         = art_uri("human", "Pria Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "solo-02",
                .type        = "human",
                .name        = "Wanita Solo",
                .art         = art_uri("human", "Wanita Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "solo-03",
                .type        = "human",
                .name        = "Anak Solo",
                .art         = art_uri("human", "Anak Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "solo-04",
                .type        = "human",
                .name        = "Bapak Solo",
                .art         = art_uri("human", "Bapak Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "solo-05",
                .type        = "human",
                .name        = "Ibu Solo",
                .art         = art_uri("human", "Ibu Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "solo-06",
                .type        = "human",
                .name        = "Mertua Solo",
                .art         = art_uri("human", "Mertua Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "solo-11",
                .type        = "robot",
                .name        = "Meta Solo",
                .art         = art_uri("robot", "Meta Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "solo-12",
                .type        = "robot",
                .name        = "Midjourney Solo",
                .art         = art_uri("robot", "Midjourney Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "solo-13",
                .type        = "robot",
                .name        = "Grok Solo",
                .art         = art_uri("robot", "Grok Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "solo-14",
                .type        = "robot",
                .name        = "GPT Solo",
                .art         = art_uri("robot", "GPT Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "solo-15",
                .type        = "robot",
                .name        = "Perplexity Solo",
                .art         = art_uri("robot", "Perplexity Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "solo-16",
                .type        = "robot",
                .name        = "Claude Solo",
                .art         = art_uri("robot", "Claude Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "solo-21",
                .type        = "monster",
                .name        = "Zombi Solo",
                .art         = art_uri("monster", "Zombi Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "solo-22",
                .type        = "monster",
                .name        = "Kolor Solo",
                .art         = art_uri("monster", "Kolor Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "solo-23",
                .type        = "monster",
                .name        = "Kuyang Solo",
                .art         = art_uri("monster", "Kuyang Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "solo-24",
                .type        = "monster",
                .name        = "Pocong Solo",
                .art         = art_uri("monster", "Pocong Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "solo-25",
                .type        = "monster",
                .name        = "Kunti Solo",
                .art         = art_uri("monster", "Kunti Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "solo-26",
                .type        = "monster",
                .name        = "Genderuwo Solo",
                .art         = art_uri("monster", "Genderuwo Solo"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "spell-01",
                .type        = "spell",
                .name        = "Pasal Karet",
                .art         = art_uri("spell", "Pasal Karet"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-02",
                .type        = "spell",
                .name        = "MBG",
                .art         = art_uri("spell", "MBG"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-03",
                .type        = "spell",
                .name        = "Paman Sakti",
                .art         = art_uri("spell", "Paman Sakti"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-04",
                .type        = "spell",
                .name        = "Kopdes",
                .art         = art_uri("spell", "Kopdes"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //
        },
    };
}

Deck Deck::Sawit() {
    return {
        .name  = "Sawit Empire",
        .id    = "sawit",
        .css   = "sawit",
        .blurb = "Semua akan menjadi Sawit",
        .cards = {
                  //
            Card{
                .id          = "sawit-01",
                .type        = "human",
                .name        = "Pria Sawit",
                .art         = art_uri("human", "Pria Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "sawit-02",
                .type        = "human",
                .name        = "Wanita Sawit",
                .art         = art_uri("human", "Wanita Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "sawit-03",
                .type        = "human",
                .name        = "Anak Sawit",
                .art         = art_uri("human", "Anak Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "sawit-04",
                .type        = "human",
                .name        = "Bapak Sawit",
                .art         = art_uri("human", "Bapak Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "sawit-05",
                .type        = "human",
                .name        = "Ibu Sawit",
                .art         = art_uri("human", "Ibu Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "sawit-06",
                .type        = "human",
                .name        = "Mertua Sawit",
                .art         = art_uri("human", "Mertua Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "sawit-11",
                .type        = "robot",
                .name        = "Meta Sawit",
                .art         = art_uri("robot", "Meta Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "sawit-12",
                .type        = "robot",
                .name        = "Midjourney Sawit",
                .art         = art_uri("robot", "Midjourney Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "sawit-13",
                .type        = "robot",
                .name        = "Grok Sawit",
                .art         = art_uri("robot", "Grok Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "sawit-14",
                .type        = "robot",
                .name        = "GPT Sawit",
                .art         = art_uri("robot", "GPT Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "sawit-15",
                .type        = "robot",
                .name        = "Perplexity Sawit",
                .art         = art_uri("robot", "Perplexity Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "sawit-16",
                .type        = "robot",
                .name        = "Claude Sawit",
                .art         = art_uri("robot", "Claude Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "sawit-21",
                .type        = "monster",
                .name        = "Zombi Sawit",
                .art         = art_uri("monster", "Zombi Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 1
            }, //

            Card{
                .id          = "sawit-22",
                .type        = "monster",
                .name        = "Kolor Sawit",
                .art         = art_uri("monster", "Kolor Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 2
            }, //

            Card{
                .id          = "sawit-23",
                .type        = "monster",
                .name        = "Kuyang Sawit",
                .art         = art_uri("monster", "Kuyang Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 3
            }, //

            Card{
                .id          = "sawit-24",
                .type        = "monster",
                .name        = "Pocong Sawit",
                .art         = art_uri("monster", "Pocong Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 4
            }, //

            Card{
                .id          = "sawit-25",
                .type        = "monster",
                .name        = "Kunti Sawit",
                .art         = art_uri("monster", "Kunti Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 5
            }, //

            Card{
                .id          = "sawit-26",
                .type        = "monster",
                .name        = "Genderuwo Sawit",
                .art         = art_uri("monster", "Genderuwo Sawit"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 6
            }, //

            //
            Card{
                .id          = "spell-05",
                .type        = "spell",
                .name        = "19juta",
                .art         = art_uri("spell", "19juta"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-06",
                .type        = "spell",
                .name        = "Judol",
                .art         = art_uri("spell", "Judol"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-07",
                .type        = "spell",
                .name        = "Wamen",
                .art         = art_uri("spell", "Wamen"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //

            Card{
                .id          = "spell-08",
                .type        = "spell",
                .name        = "Buzzer",
                .art         = art_uri("spell", "Buzzer"),
                .effect      = "No effect",
                .description = "No description",
                .level       = 0
            }, //
        },
    };
}
