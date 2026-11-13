// chess_server: answers GET /healthz for now. The WebSocket endpoint /ws is
// issue #9 step 2. See docs/system-design.md for the whole plan.

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>

#include "server/config.hpp"
#include "server/listener.hpp"

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
        // The event loop. Everything runs on this one thread, so no game data
        // is ever touched by two threads at once (docs/system-design.md, section 6).
        boost::asio::io_context io;

        const boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::tcp::v4(), *port};
        auto listener = std::make_shared<chess::server::Listener>(io, endpoint);
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
