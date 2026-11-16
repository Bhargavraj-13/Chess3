#include "server/http_routes.hpp"

#include <catch2/catch_test_macros.hpp>

using chess::server::handle_request;
using chess::server::Request;
namespace http = chess::server::http;

namespace {

Request make_request(http::verb method, const char* target) {
    return Request{method, target, 11};  // 11 means HTTP/1.1
}

}  // namespace

TEST_CASE("GET /healthz answers 200 ok", "[server]") {
    const auto response = handle_request(make_request(http::verb::get, "/healthz"));
    CHECK(response.result() == http::status::ok);
    CHECK(response.body() == "ok\n");
}

TEST_CASE("GET /healthz ignores a query string", "[server]") {
    const auto response = handle_request(make_request(http::verb::get, "/healthz?probe=1"));
    CHECK(response.result() == http::status::ok);
}

TEST_CASE("other methods on /healthz answer 405", "[server]") {
    const auto response = handle_request(make_request(http::verb::post, "/healthz"));
    CHECK(response.result() == http::status::method_not_allowed);
    CHECK(response[http::field::allow] == "GET");
}

TEST_CASE("unknown paths answer 404", "[server]") {
    CHECK(handle_request(make_request(http::verb::get, "/")).result() == http::status::not_found);
    CHECK(handle_request(make_request(http::verb::get, "/healthzz")).result() ==
          http::status::not_found);
}

TEST_CASE("responses keep the connection open only if the request asked to", "[server]") {
    auto request = make_request(http::verb::get, "/healthz");
    request.keep_alive(true);
    CHECK(handle_request(request).keep_alive());
    request.keep_alive(false);
    CHECK_FALSE(handle_request(request).keep_alive());
}
