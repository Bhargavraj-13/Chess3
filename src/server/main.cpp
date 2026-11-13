// chess_server: answers GET /healthz, and accepts WebSocket connections at /ws.
// Games are not implemented yet (#11), so every valid message gets a polite
// error for now. See docs/system-design.md for the whole plan.

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "server/config.hpp"
#include "server/listener.hpp"
#include "server/protocol.hpp"

namespace {

// Stands in for the game code until #11 replaces it.
class NotReadyHandler final : public chess::server::MessageHandler {
public:
    void on_message(const std::shared_ptr<chess::server::Connection>& from,
                    chess::server::ClientMessage message) override {
        from->send(chess::server::error_json({"internal", "games are not available yet",
                                              std::string(chess::server::type_name(message))}));
    }
    void on_disconnect(const std::shared_ptr<chess::server::Connection>&) override {}
};

}  // namespace

int main() {
    // The deploy setup sets PORT (deploy/compose.yaml). 8080 is the default for
    // local development, which is also what the frontend's dev proxy expects.
    const char* port_env = std::getenv("PORT");
    const auto port = chess::server::parse_port(port_env != nullptr ? port_env : "8080");
    if (!port) {
        std::cerr << "chess_server: PORT must be a number from 1 to 65535\n";
        return EXIT_FAILURE;
    }

    try {
        // Declared first, so it is destroyed last: after io, which owns the
        // pending operations that keep every connection alive.
        NotReadyHandler handler;

        // The event loop. Everything runs on this one thread, so no game data
        // is ever touched by two threads at once (docs/system-design.md, section 6).
        boost::asio::io_context io;

        const boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), *port};
        auto listener = std::make_shared<chess::server::Listener>(io, endpoint, handler);
        listener->start();

        // Ctrl+C sends SIGINT, and Docker sends SIGTERM when it stops the container.
        // Either one stops the loop, so the program exits cleanly.
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const boost::system::error_code&, int) { io.stop(); });

        std::cout << "chess_server listening on port " << listener->port() << std::endl;
        io.run();  // runs until io.stop()
        std::cout << "chess_server stopped" << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "chess_server: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
