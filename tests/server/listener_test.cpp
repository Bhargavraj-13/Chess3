// Starts a real server on a free port and talks to it over a real connection.

#include "server/listener.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

const auto kLocalhost = asio::ip::make_address("127.0.0.1");

// Runs the server's event loop on a background thread for the length of a test.
// The destructor always stops and joins it, even if a CHECK fails or something throws.
class TestServer {
public:
    TestServer()
        : listener_(std::make_shared<chess::server::Listener>(io_, tcp::endpoint{kLocalhost, 0})),
          port_(listener_->port()) {
        listener_->start();
        thread_ = std::thread([this] { io_.run(); });
    }
    ~TestServer() {
        io_.stop();
        thread_.join();
    }
    TestServer(const TestServer&) = delete;
    TestServer& operator=(const TestServer&) = delete;

    std::uint16_t port() const { return port_; }

private:
    asio::io_context io_;
    std::shared_ptr<chess::server::Listener> listener_;
    std::uint16_t port_;
    std::thread thread_;
};

// The test is the "browser" here. Blocking calls are fine in a test, because
// it only has one connection to look after.
http::response<http::string_body> get(beast::tcp_stream& client, const std::string& target) {
    http::request<http::empty_body> request{http::verb::get, target, 11};
    request.set(http::field::host, "localhost");
    http::write(client, request);

    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(client, buffer, response);
    return response;
}

}  // namespace

TEST_CASE("a running server answers GET /healthz over a real connection", "[server]") {
    TestServer server;

    asio::io_context client_io;
    beast::tcp_stream client(client_io);
    client.connect(tcp::endpoint{kLocalhost, server.port()});

    const auto response = get(client, "/healthz");
    CHECK(response.result() == http::status::ok);
    CHECK(response.body() == "ok\n");
}

TEST_CASE("one connection can send several requests (keep-alive)", "[server]") {
    TestServer server;

    asio::io_context client_io;
    beast::tcp_stream client(client_io);
    client.connect(tcp::endpoint{kLocalhost, server.port()});

    CHECK(get(client, "/healthz").result() == http::status::ok);
    CHECK(get(client, "/nope").result() == http::status::not_found);
    CHECK(get(client, "/healthz").result() == http::status::ok);
}

TEST_CASE("two clients can be connected at the same time", "[server]") {
    TestServer server;

    asio::io_context client_io;
    beast::tcp_stream alice(client_io);
    beast::tcp_stream bob(client_io);
    alice.connect(tcp::endpoint{kLocalhost, server.port()});
    bob.connect(tcp::endpoint{kLocalhost, server.port()});

    // Bob is served even though Alice connected first and hasn't sent anything,
    // which a blocking one-connection-at-a-time server could not do.
    CHECK(get(bob, "/healthz").result() == http::status::ok);
    CHECK(get(alice, "/healthz").result() == http::status::ok);
}
