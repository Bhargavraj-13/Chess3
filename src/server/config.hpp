#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace chess::server {

// Turns the text of the PORT environment variable into a port number.
// Returns nullopt for anything that is not a whole number from 1 to 65535,
// such as "", "abc", "80x", "0" or "70000".
std::optional<std::uint16_t> parse_port(std::string_view text);

}  // namespace chess::server
