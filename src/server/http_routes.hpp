#pragma once

#include <boost/beast/http.hpp>

namespace chess::server {

namespace http = boost::beast::http;

using Request = http::request<http::string_body>;
using Response = http::response<http::string_body>;

// Decides the answer to one HTTP request. There is no networking in here,
// so it can be tested with plain function calls (tests/server/http_routes_test.cpp).
//
//   GET /healthz      200 "ok"   (used by the deploy check and Docker)
//   other /healthz    405 Method Not Allowed
//   anything else     404 Not Found
//
// The WebSocket endpoint /ws is not handled here. A WebSocket starts as an
// HTTP "upgrade" request, and HttpSession will hand those over before routing.
Response handle_request(const Request& request);

}  // namespace chess::server
