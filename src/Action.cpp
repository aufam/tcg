#include <mup/html.h>
#include "main.h"

using namespace mup::html;


std::string State::ActionChooseDeck(std::string deck_id) {
    for (auto &deck : decks) {
        if (deck_id == deck.id) {
            cards = deck.shuffle();

            // draw five
            while (hand.size() < 5 && cards.size() > 0) {
                hand.push_back(cards.back());
                cards.pop_back();
            }

            if (!opp)
                return Log("Waiting for an opponent to pick a deck…");

            return Board();
        }
    }

    return Log("unknown deck id: " + deck_id);
}

std::string State::ActionPlaceCard(std::string card_id) {
    for (auto &card : hand) {
        if (card.id == card_id) {
            if (opp->battle.has_value())
                battle = Slot{card, true};
            break;
        }
    }

    std::erase_if(hand, [&](Card &c) { return c.id == card_id; });

    return Log("unknown card id: " + card_id);
}
