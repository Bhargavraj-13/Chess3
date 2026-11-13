#pragma once

// The boundary between the network code (#9) and the game code (#11).
//
// The network side owns the WebSocket connections and implements Connection.
// The game side implements MessageHandler. Neither needs to know how the other
// works: the game code never touches a socket, and the network code never
// knows what a chess game is.

#include <memory>
#include <string>

#include "server/protocol.hpp"

namespace chess::server {

// One open WebSocket, as the game code sees it.
class Connection {
public:
    virtual ~Connection() = default;

    // Queues one text message. Messages arrive in the order they were sent.
    // Does nothing once the connection is closing.
    virtual void send(std::string text) = 0;

    // Sends any queued messages, then closes the connection politely.
    virtual void close() = 0;
};

// What the server does with messages. Every call happens on the server's one
// thread, so implementations need no locks (docs/system-design.md, section 6).
class MessageHandler {
public:
    virtual ~MessageHandler() = default;

    // A well-formed message arrived. Badly formed ones are answered with an
    // error before they get here.
    virtual void on_message(const std::shared_ptr<Connection>& from, ClientMessage message) = 0;

    // The connection has ended, for any reason. Called exactly once per connection,
    // and never for a connection whose WebSocket handshake failed.
    // This is where #12 starts the 30-second reconnect timer.
    virtual void on_disconnect(const std::shared_ptr<Connection>& connection) = 0;
};

}  // namespace chess::server
