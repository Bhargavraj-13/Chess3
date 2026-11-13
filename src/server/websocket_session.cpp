#include "server/websocket_session.hpp"

#include <boost/asio/buffer.hpp>
#include <chrono>
#include <utility>
#include <variant>

#include "server/protocol.hpp"

namespace chess::server {

namespace beast = boost::beast;
namespace websocket = beast::websocket;

namespace {

// docs/system-design.md, section 5: bigger messages close the connection.
constexpr std::size_t kMaxMessageBytes = 4 * 1024;

}  // namespace

WebSocketSession::WebSocketSession(beast::tcp_stream stream, MessageHandler& handler)
    : ws_(std::move(stream)), handler_(handler) {}

void WebSocketSession::start(Request upgrade_request) {
    // HttpSession used the tcp_stream's own timeout. The WebSocket has its own
    // timeouts (below), so switch the old one off.
    beast::get_lowest_layer(ws_).expires_never();

    // If nothing arrives for 10 seconds, Beast sends a ping (at 5 seconds), and
    // closes the connection if even the ping gets no answer. Browsers answer
    // pings automatically. This is how a vanished player is noticed quickly
    // (docs/system-design.md, section 6).
    websocket::stream_base::timeout timeouts{};
    timeouts.handshake_timeout = std::chrono::seconds(30);
    timeouts.idle_timeout = std::chrono::seconds(10);
    timeouts.keep_alive_pings = true;
    ws_.set_option(timeouts);

    ws_.read_message_max(kMaxMessageBytes);
    ws_.text(true);  // our messages are JSON text, not binary

    upgrade_request_ = std::move(upgrade_request);
    ws_.async_accept(upgrade_request_,
                     beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
}

void WebSocketSession::on_accept(beast::error_code error) {
    if (error) {
        return;  // the handshake failed, so there was never a connection to report
    }
    open_ = true;
    read_next();
}

void WebSocketSession::read_next() {
    ws_.async_read(read_buffer_,
                   beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
}

void WebSocketSession::on_read(beast::error_code error, std::size_t /*bytes_read*/) {
    if (error) {
        // The browser closed, the connection broke, a timeout fired, a message
        // was too big, or we closed it ourselves. Either way, it's over.
        open_ = false;
        handler_.on_disconnect(shared_from_this());
        return;
    }

    const std::string text = beast::buffers_to_string(read_buffer_.data());
    read_buffer_.consume(read_buffer_.size());

    // Start the next read first: on_message may call close(), and the read
    // loop must still be running to see the close finish.
    read_next();

    ParseResult result = parse_client_message(text);
    if (auto* problem = std::get_if<ProtocolError>(&result)) {
        send(error_json(*problem));
        return;
    }
    handler_.on_message(shared_from_this(), std::get<ClientMessage>(std::move(result)));
}

void WebSocketSession::send(std::string text) {
    if (!open_ || closing_) {
        return;
    }
    outbox_.push_back(std::move(text));
    if (outbox_.size() == 1) {
        write_next();  // nothing was being written, so start now
    }
    // Otherwise a write is already running, and on_write will get to this one.
}

void WebSocketSession::write_next() {
    ws_.async_write(boost::asio::buffer(outbox_.front()),
                    beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
}

void WebSocketSession::on_write(beast::error_code error, std::size_t /*bytes_written*/) {
    if (error) {
        outbox_.clear();  // the connection is broken; on_read will report it
        return;
    }
    outbox_.pop_front();  // only now is the string no longer needed
    if (!outbox_.empty()) {
        write_next();
    } else if (closing_) {
        start_close();  // everything queued before close() has been sent
    }
}

void WebSocketSession::close() {
    if (!open_ || closing_) {
        return;
    }
    closing_ = true;
    // A close is also a write, so it must wait for queued messages to finish.
    if (outbox_.empty()) {
        start_close();
    }
}

void WebSocketSession::start_close() {
    ws_.async_close(websocket::close_code::normal,
                    beast::bind_front_handler(&WebSocketSession::on_close, shared_from_this()));
}

void WebSocketSession::on_close(beast::error_code /*error*/) {
    // Nothing to do. The pending read finishes with an error next, and on_read
    // reports the disconnect.
}

}  // namespace chess::server
