#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <memory>

#include "server/connection.hpp"
#include "server/http_routes.hpp"

namespace chess::server {

// Looks after one browser connection: reads an HTTP request, writes the answer,
// and repeats while the browser keeps the connection open. If the request asks
// to upgrade to a WebSocket at /ws, the connection is handed to a WebSocketSession.
//
// Every step is asynchronous: we ask Asio to start a read or write and give it
// a function to call when it finishes, and Asio's loop calls it later.
//
// Lifetime: the session is owned by a shared_ptr. Each pending read or write
// holds a copy of that shared_ptr (from shared_from_this()), so the session stays
// alive exactly as long as some operation is still waiting to call it back.
// When the last operation finishes without starting another, it is destroyed
// and the connection is closed.
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(boost::asio::ip::tcp::socket socket, MessageHandler& handler);

    void start();

private:
    void read_request();
    void on_read(boost::beast::error_code error, std::size_t bytes_read);
    void on_write(boost::beast::error_code error, std::size_t bytes_written);
    void close();

    MessageHandler& handler_;           // passed on to WebSocket sessions
    boost::beast::tcp_stream stream_;   // the socket, plus a timeout
    boost::beast::flat_buffer buffer_;  // bytes read but not yet parsed
    Request request_;
    Response response_;  // must live until the write has finished
};

}  // namespace chess::server
