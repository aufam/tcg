#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/uuid.hpp>
#include <mup/html.h>
#include "main.h"

import brb;
import brb.static_files;
import cpx;
import cpx.fmt;
import cpx.yy_json;
import fmt;

int main() {
    boost::asio::io_context                                 io;
    brb::Engine                                             engine{io};
    std::mutex                                              mtx;
    std::unordered_map<std::string, std::shared_ptr<State>> states;

    std::unordered_map<std::string, std::vector<std::shared_ptr<brb::ws_stream>>> ws_streams;

    const Deck sawit = Deck::Sawit(), solo = Deck::Solo();

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

    const auto remove_oob = [](std::string &&html) {
        constexpr std::string_view attr = R"(hx-swap-oob="true")";

        size_t pos = 0;
        while ((pos = html.find(attr, pos)) != std::string::npos)
            html.erase(pos, attr.size());

        return html;
    };

    engine.route(brb::verb::get, "/", [&](brb::Context &c) -> brb::awaitable<void> {
        auto id = get_id(c);
        if (id.empty())
            id = create_id(c);

        std::lock_guard<std::mutex> lock(mtx);

        if (states.count(id) == 0) {
            static int cnt = 0;

            State st;
            st.name    = "Player" + std::to_string(cnt++);
            st.decks   = {solo, sawit};
            states[id] = std::make_shared<State>(std::move(st));
        }

        c.response_string().body() = remove_oob(states[id]->Index());
        c.res().set(brb::field::content_type, "text/html");

        co_return;
    });

    engine.route(brb::verb::get, "/decks/solo", [&](brb::Context &c) -> brb::awaitable<void> {
        std::lock_guard<std::mutex> lock(mtx);
        c.response_string().body() = remove_oob(solo.Show());
        c.res().set(brb::field::content_type, "text/html");
        co_return;
    });

    engine.route(brb::verb::get, "/decks/sawit", [&](brb::Context &c) -> brb::awaitable<void> {
        std::lock_guard<std::mutex> lock(mtx);
        c.response_string().body() = remove_oob(sawit.Show());
        c.res().set(brb::field::content_type, "text/html");
        co_return;
    });

    engine.route(brb::verb::get, "/ws/game", [&](brb::Context &c) -> brb::awaitable<void> {
        if (!brb::ws::is_upgrade(c.parser_empty().get())) {
            c.res().result(brb::status::bad_request);
            co_return;
        }

        const auto id = get_id(c);
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (states.count(id) == 0) {
                // must answer before the tcp stream is moved into the websocket
                c.res().result(brb::status::unauthorized);
                c.res().set(brb::field::content_type, "text/plain");
                c.response_string().body() = "failed to establish ws connection";
                co_return;
            }
        }

        auto ws = std::make_shared<brb::ws_stream>(std::move(*c.stream));
        co_await ws->async_accept(c.parser_empty().get());

        {
            auto lock = std::unique_lock<std::mutex>(mtx);

            auto &self = states.at(id);
            if (states.at(id)->opp == nullptr) {
                for (auto [other_id, s] : states) {
                    auto &other = s;
                    if (id == other_id)
                        continue;

                    if (other->opp == nullptr) {
                        other->opp = self;
                        self->opp  = other;
                        break;
                    }
                }
            }
        }

        do {
            {
                auto  lock   = std::unique_lock<std::mutex>(mtx);
                auto &outbox = states.at(id)->outbox;

                auto outmsg = fmt::format("{}", fmt::join(outbox, "\n"));
                outbox.clear();

                lock.unlock();
                co_await ws->async_write(brb::buffer(outmsg));
            }

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

            auto  lock = std::lock_guard<std::mutex>(mtx);
            auto &s    = *states.at(id);

            fmt::println("{}: {}", s.name, parser);

            if (action == "choose_deck") {
                auto msg = s.ActionChooseDeck(deck_id);
                s.outbox.push_back(msg);
            }
        } while (ws->is_open());
    });

    const auto run_opts = brb::Engine::run_opts{
        .host                 = "0.0.0.0",
        .port                 = 3001,
        .cancellation_signals = {SIGINT, SIGTERM},
    };
    boost::asio::co_spawn(io, engine.run(run_opts), boost::asio::detached);

    io.run();
}
