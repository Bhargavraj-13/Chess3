#include "server/config.hpp"

#include <charconv>

namespace chess::server {

std::optional<std::uint16_t> parse_port(std::string_view text) {
    unsigned int value = 0;
    const char* end = text.data() + text.size();
    auto [stopped_at, error] = std::from_chars(text.data(), end, value);

    // Every character must be part of the number, so "80x" is rejected.
    if (error != std::errc{} || stopped_at != end) {
        return std::nullopt;
    }
    if (value < 1 || value > 65535) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

}  // namespace chess::server
