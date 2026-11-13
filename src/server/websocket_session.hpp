#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <memory>
#include <string>

#include "server/connection.hpp"
#include "server/http_routes.hpp"

namespace chess::server {

// Looks after one WebSocket connection, from the upgrade until it closes.
//
// Reading: one read is always waiting. Each message that arrives is checked
// (parse_client_message) and passed to the MessageHandler, then the next read starts.
//
// Writing: Beast allows only one write at a time on a WebSocket, but the game
// code may call send() several times in a row. So messages wait in outbox_,
// and each finished write starts the next one.
//
// Lifetime: owned by shared_ptrs, like HttpSession. The pending read always
// holds one, so the session lives until its connection ends.
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>, public Connection {
public:
    WebSocketSession(boost::beast::tcp_stream stream, MessageHandler& handler);

    // Answers the browser's upgrade request, which HttpSession has already read.
    void start(Request upgrade_request);

    void send(std::string text) override;
    void close() override;

private:
    void on_accept(boost::beast::error_code error);
    void read_next();
    void on_read(boost::beast::error_code error, std::size_t bytes_read);
    void write_next();
    void on_write(boost::beast::error_code error, std::size_t bytes_written);
    void start_close();
    void on_close(boost::beast::error_code error);

    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    MessageHandler& handler_;
    Request upgrade_request_;  // kept until the handshake has finished with it
    boost::beast::flat_buffer read_buffer_;
    std::deque<std::string> outbox_;  // outbox_.front() is being written now
    bool open_ = false;               // handshake done, and not yet closed
    bool closing_ = false;            // close() was called; no more sends
};

}  // namespace chess::server
