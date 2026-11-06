#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

struct Card {
    std::string id, type, name, art, effect, description;
    int         level;

    // {#
    //   Reusable card partial.
    //   Context: card {id, type, name, level, art, effect, description}
    //            face_down (bool), size ('' | 'mini'), fx (optional: 'reveal' | 'won' | 'lost')
    //   type is one of: robot (blue) | human (green) | monster (red) | spell (yellow)
    //   `art` is an image URL, e.g. /static/art/solo_empire/iron-hound.png
    //   NEVER include real card data when face_down is true for the opponent.
    // #}
    std::string Render(std::string size = "", std::string fx = "", bool face_down = false) const;

    std::pair<int, std::string> matchup(const Card &other) const;
};


struct Deck {
    std::string       name, id, css, blurb;
    std::vector<Card> cards;

    // {# GET /decks/<deck_id> (plain HTTP, htmx hx-get). Swapped into #modal. #}
    // {# Context: deck.name, deck.cards (22 cards, sorted by type) #}
    std::string Show() const;

    std::vector<Card> shuffle() const;

    static Deck Sawit();
    static Deck Solo();
};


struct Slot {
    Card card;
    bool face_down;
};


struct State {
    std::string            name;
    std::vector<Deck>      decks;
    std::shared_ptr<State> opp;

    std::vector<Card> cards;
    std::vector<Card> hand;
    std::vector<Card> victory;
    std::vector<Card> graveyard;

    std::optional<Slot> battle;

    std::vector<std::string> outbox;

    bool        game_over = false;
    std::string winner;


    std::string Index() const;

    // {# Sent on every phase change. phase: draw | main | battle | wait #}
    std::string PhaseBar() const;

    // {# Sent over WS when a player (re)enters the lobby. Replaces #screen. #}
    std::string DeckSelect() const;

    // {# Full board. Sent once the game starts. Replaces #screen.
    //    Context: me / opp each have: name, deck_count, graveyard (list), victory (list),
    //    spell_slots (list of {card, face_down}), battle ({card, face_down} or None).
    //    Opponent face-down cards are placeholders only. #}
    std::string Board() const;

    // {# Sent on draw phase and after every placement. Replaces #hand. #}
    // {# Context: hand (cards), phase ('draw'|'main'|'battle'|'wait'), placed_ids, spells_placed (0..2), monster_placed (bool) #}
    std::string Hand() const;


    // {# Sent when a battle pair is revealed. Then, after the player's decision (or a timer),
    //    the server sends result.html.
    //    Context: mine, theirs (cards, face up), outcome ('win'|'lose'|'draw'),
    //    reason_text (e.g. "Robot beats Monster", "9 loses to 1"),
    //    can_spell (bool: I may reveal a spell now), role ('loser'|'winner'), revealed, my_spells #}
    std::string Battle(
        const Card &mine,
        const Card &theirs,
        std::string outcome,
        std::string reason,
        bool        can_spell,
        std::string role,
        const Card &revealed
    ) const;


    // {# Included by battle.html. Two cases:
    //    role == 'loser'  : you lost the clash and may reveal a face-down spell to turn it around
    //    role == 'winner' : opponent revealed a spell (`revealed`); you may counter it with one of yours
    //    Context: role, revealed (card or None), my_spells (your face-down spells in the field)
    //    The server enforces a response timeout (e.g. 15s) and treats silence as skip_spell. #}
    std::string SpellPrompt(std::string role, const Card &reveal) const;

    // {# Sent after each clash resolves (and at game end).
    //    Loser -> graveyard, winner -> victory zone, used spells -> graveyard.
    //    animations.js animates the moves automatically by tracking card ids across the swap.
    //    Context: me, opp (same shape as board.html), log_line, game_over, winner ('me'|'opp'|'draw') #}
    std::string Result(std::string log_line) const;

    std::string Log(std::string log_line) const;


    std::string ActionChooseDeck(std::string deck_id);
    std::string ActionPlaceCard(std::string card_id);
};

std::string CleanModal();

// Full verdict from a's perspective. Returns {+1 a wins | 0 draw | -1 b wins, reason}.
std::pair<int, std::string> evaluate(const State &a, const State &b);
