// Real WebSocket clients talking to a real server.

#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>

#include "test_server.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;
using tcp = asio::ip::tcp;
using chess::server::ClientMessage;
using chess::server::Connection;
using chess::server::MessageHandler;
using chess::server::type_name;
using chess::server::testing::kLocalhost;
using chess::server::testing::TestServer;

namespace {

// Stands in for the game code. It answers every message with
// {"type":"ok","in_reply_to":<type>,"n":1}, and:
//   offer_draw    is answered three times (n = 1, 2, 3), to test the write queue
//   decline_draw  is answered, then the server closes the connection
//
// It runs on the server's thread while the test runs on another, so the one
// thing the test reads, `disconnects`, is atomic.
class ReplyingHandler final : public MessageHandler {
public:
    std::atomic<int> disconnects{0};

    void on_message(const std::shared_ptr<Connection>& from, ClientMessage message) override {
        const std::string type{type_name(message)};
        const int replies = type == "offer_draw" ? 3 : 1;
        for (int n = 1; n <= replies; ++n) {
            from->send(
                json::serialize(json::object{{"type", "ok"}, {"in_reply_to", type}, {"n", n}}));
        }
        if (type == "decline_draw") {
            from->close();
        }
    }
    void on_disconnect(const std::shared_ptr<Connection>&) override { ++disconnects; }
};

using Client = websocket::stream<beast::tcp_stream>;

void connect(Client& client, std::uint16_t port, const char* path = "/ws") {
    beast::get_lowest_layer(client).connect(tcp::endpoint{kLocalhost, port});
    client.handshake("localhost", path);  // throws if the server refuses the upgrade
}

void send(Client& client, const std::string& text) {
    client.text(true);
    client.write(asio::buffer(text));
}

json::object receive(Client& client) {
    beast::flat_buffer buffer;
    client.read(buffer);
    return json::parse(beast::buffers_to_string(buffer.data())).as_object();
}

// The handler runs on another thread, so give it a moment to catch up.
bool eventually(const std::atomic<int>& counter, int expected) {
    for (int i = 0; i < 200; ++i) {
        if (counter == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}  // namespace

TEST_CASE("a valid message reaches the handler, and its reply comes back", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    send(client, R"({"type": "resign"})");
    const json::object reply = receive(client);
    CHECK(reply.at("type") == "ok");
    CHECK(reply.at("in_reply_to") == "resign");
}

TEST_CASE("bad messages get an error, and the connection stays open", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    send(client, "this is not json");
    json::object reply = receive(client);
    CHECK(reply.at("type") == "error");
    CHECK(reply.at("code") == "bad_message");
    CHECK(reply.at("in_reply_to").is_null());

    send(client, R"({"type": "fly_away"})");
    reply = receive(client);
    CHECK(reply.at("code") == "unknown_type");
    CHECK(reply.at("in_reply_to") == "fly_away");

    send(client, R"({"type": "join_game"})");  // missing "code"
    reply = receive(client);
    CHECK(reply.at("code") == "bad_message");
    CHECK(reply.at("in_reply_to") == "join_game");

    send(client, R"({"type": "resign"})");
    CHECK(receive(client).at("type") == "ok");
}

TEST_CASE("several replies to one message arrive in order", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    send(client, R"({"type": "offer_draw"})");
    CHECK(receive(client).at("n") == 1);
    CHECK(receive(client).at("n") == 2);
    CHECK(receive(client).at("n") == 3);
}

TEST_CASE("two clients can talk to the server at the same time", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client alice(io);
    Client bob(io);
    connect(alice, server.port());
    connect(bob, server.port());

    send(bob, R"({"type": "resign"})");
    CHECK(receive(bob).at("in_reply_to") == "resign");
    send(alice, R"({"type": "accept_draw"})");
    CHECK(receive(alice).at("in_reply_to") == "accept_draw");
}

TEST_CASE("the server can close a connection after its last reply", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    send(client, R"({"type": "decline_draw"})");
    CHECK(receive(client).at("in_reply_to") == "decline_draw");  // sent before the close

    beast::flat_buffer buffer;
    beast::error_code error;
    client.read(buffer, error);
    CHECK(error == websocket::error::closed);
    CHECK(client.reason().code == websocket::close_code::normal);
    CHECK(eventually(handler.disconnects, 1));
}

TEST_CASE("the handler hears when a client disconnects", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    client.close(websocket::close_code::normal);
    CHECK(eventually(handler.disconnects, 1));
}

TEST_CASE("a message over 4 KB closes the connection", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);
    connect(client, server.port());

    send(client, std::string(5000, 'x'));
    beast::flat_buffer buffer;
    beast::error_code error;
    client.read(buffer, error);
    CHECK(error);
    CHECK(eventually(handler.disconnects, 1));
}

TEST_CASE("WebSocket upgrades are only accepted at /ws", "[server]") {
    ReplyingHandler handler;
    TestServer server(handler);
    asio::io_context io;
    Client client(io);

    CHECK_THROWS(connect(client, server.port(), "/somewhere-else"));
    CHECK(handler.disconnects == 0);  // never connected, so never disconnected
}
