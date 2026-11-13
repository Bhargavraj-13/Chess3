#include "core/types.hpp"

namespace chess::core {

std::optional<Square> parse_square(std::string_view name) noexcept {
    if (name.size() != 2) {
        return std::nullopt;
    }
    const char file = name[0];
    const char rank = name[1];
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return std::nullopt;
    }
    return make_square(file - 'a', rank - '1');
}

std::string square_name(Square square) {
    return {static_cast<char>('a' + file_of(square)), static_cast<char>('1' + rank_of(square))};
}

}  // namespace chess::core
