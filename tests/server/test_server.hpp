#pragma once

// Helpers shared by the server tests: a real server running on a background
// thread, and a MessageHandler that never does anything.

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include <thread>

#include "server/connection.hpp"
#include "server/listener.hpp"

namespace chess::server::testing {

inline const boost::asio::ip::address kLocalhost = boost::asio::ip::make_address("127.0.0.1");

// Runs a real server on a free port, on a background thread, for the length of
// a test. The destructor always stops and joins it, even if a CHECK fails or
// something throws. `handler` must outlive the TestServer.
class TestServer {
public:
    explicit TestServer(MessageHandler& handler)
        : listener_(std::make_shared<Listener>(io_, boost::asio::ip::tcp::endpoint{kLocalhost, 0},
                                               handler)),
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
    boost::asio::io_context io_;
    std::shared_ptr<Listener> listener_;
    std::uint16_t port_;
    std::thread thread_;
};

class IgnoringHandler final : public MessageHandler {
public:
    void on_message(const std::shared_ptr<Connection>&, ClientMessage) override {}
    void on_disconnect(const std::shared_ptr<Connection>&) override {}
};

}  // namespace chess::server::testing
