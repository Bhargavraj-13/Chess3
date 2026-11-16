#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>

#include "server/connection.hpp"

namespace chess::server {

// Waits at the server's port for new connections, and starts an HttpSession
// for each one. `handler` must outlive the listener and every connection.
class Listener : public std::enable_shared_from_this<Listener> {
public:
    // Opens the port straight away. Throws boost::system::system_error if it
    // can't, for example because another program is already using it.
    // Port 0 asks the operating system for any free port (used by the tests).
    Listener(boost::asio::io_context& io, const boost::asio::ip::tcp::endpoint& endpoint,
             MessageHandler& handler);

    // Starts accepting connections. They are handled once io.run() is running.
    void start();

    // The port actually in use, which matters when the endpoint asked for port 0.
    std::uint16_t port() const;

private:
    void accept_next();
    void on_accept(boost::system::error_code error, boost::asio::ip::tcp::socket socket);

    boost::asio::ip::tcp::acceptor acceptor_;
    MessageHandler& handler_;
};

}  // namespace chess::server
