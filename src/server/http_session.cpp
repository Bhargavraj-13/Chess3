#include "server/http_session.hpp"

#include <chrono>
#include <utility>

namespace chess::server {

namespace beast = boost::beast;

namespace {

// A connection that sends nothing for this long is closed, so idle or broken
// connections can't pile up.
constexpr auto kIdleTimeout = std::chrono::seconds(30);

}  // namespace

HttpSession::HttpSession(boost::asio::ip::tcp::socket socket) : stream_(std::move(socket)) {}

void HttpSession::start() { read_request(); }

void HttpSession::read_request() {
    request_ = {};  // clear the previous request on a kept-alive connection
    stream_.expires_after(kIdleTimeout);

    // Returns immediately. Asio calls on_read once a full request has arrived,
    // the timeout fires, or the browser disconnects.
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code error, std::size_t /*bytes_read*/) {
    if (error == http::error::end_of_stream) {
        close();  // the browser closed the connection normally
        return;
    }
    if (error) {
        return;  // timeout or broken connection: let the session be destroyed
    }

    // Issue #9 step 2 goes here: if beast::websocket::is_upgrade(request_),
    // hand the connection to a WebSocket session instead of routing it.

    response_ = handle_request(request_);
    http::async_write(stream_, response_,
                      beast::bind_front_handler(&HttpSession::on_write, shared_from_this()));
}

void HttpSession::on_write(beast::error_code error, std::size_t /*bytes_written*/) {
    if (error) {
        return;
    }
    if (!response_.keep_alive()) {
        close();
        return;
    }
    read_request();  // wait for the next request on the same connection
}

void HttpSession::close() {
    // Tell the browser we're done sending. Errors are ignored, because there is
    // nothing useful to do if the connection is already gone.
    beast::error_code ignored;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ignored);
}

}  // namespace chess::server
