#include "server/listener.hpp"

#include <boost/asio/error.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <iostream>
#include <utility>

#include "server/http_session.hpp"

namespace chess::server {

// This acceptor constructor opens the socket, binds it to the endpoint, and
// starts listening, all in one step.
Listener::Listener(boost::asio::io_context& io, const boost::asio::ip::tcp::endpoint& endpoint)
    : acceptor_(io, endpoint) {}

void Listener::start() { accept_next(); }

std::uint16_t Listener::port() const { return acceptor_.local_endpoint().port(); }

void Listener::accept_next() {
    // Returns immediately. Asio calls on_accept when a browser connects.
    acceptor_.async_accept(
        boost::beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
}

void Listener::on_accept(boost::system::error_code error, boost::asio::ip::tcp::socket socket) {
    if (error == boost::asio::error::operation_aborted) {
        return;  // the server is shutting down
    }
    if (error) {
        std::cerr << "accept failed: " << error.message() << '\n';
    } else {
        std::make_shared<HttpSession>(std::move(socket))->start();
    }
    accept_next();  // always go back to waiting for the next connection
}

}  // namespace chess::server
