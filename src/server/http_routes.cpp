#include "server/http_routes.hpp"

#include <string>
#include <string_view>

namespace chess::server {

namespace {

// Builds a small plain-text response that matches the request's HTTP version
// and keep-alive setting.
Response text_response(const Request& request, http::status status, std::string body) {
    Response response{status, request.version()};
    response.set(http::field::content_type, "text/plain; charset=utf-8");
    response.keep_alive(request.keep_alive());
    response.body() = std::move(body);
    response.prepare_payload();  // fills in Content-Length from the body
    return response;
}

// "/healthz?probe=1" -> "/healthz"
std::string_view path_of(std::string_view target) { return target.substr(0, target.find('?')); }

}  // namespace

Response handle_request(const Request& request) {
    if (path_of(request.target()) == "/healthz") {
        if (request.method() != http::verb::get) {
            Response response =
                text_response(request, http::status::method_not_allowed, "method not allowed\n");
            response.set(http::field::allow, "GET");
            return response;
        }
        return text_response(request, http::status::ok, "ok\n");
    }
    return text_response(request, http::status::not_found, "not found\n");
}

}  // namespace chess::server
