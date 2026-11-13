#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace chess::core {

enum class Color : std::uint8_t { White, Black };

// How a piece moves, which is not always what it really is. In V1 a hidden king
// sits on the board as another type (section 3 of docs/system-design.md).
enum class PieceType : std::uint8_t { Pawn, Knight, Bishop, Rook, Queen, King };

struct Piece {
    Color color;
    PieceType type;

    bool operator==(const Piece&) const = default;
};

[[nodiscard]] constexpr Color opposite(Color color) noexcept {
    return color == Color::White ? Color::Black : Color::White;
}

// 0 = a1, 1 = b1, ..., 7 = h1, 8 = a2, ..., 63 = h8.
using Square = std::uint8_t;

inline constexpr int kNumSquares = 64;

// `file` 0-7 is a-h and `rank` 0-7 is ranks 1-8. Both must be in range.
[[nodiscard]] constexpr Square make_square(int file, int rank) noexcept {
    return static_cast<Square>(rank * 8 + file);
}

[[nodiscard]] constexpr int file_of(Square square) noexcept { return square % 8; }
[[nodiscard]] constexpr int rank_of(Square square) noexcept { return square / 8; }

// "e4" -> 28. Returns nullopt for anything that isn't a lowercase square name.
[[nodiscard]] std::optional<Square> parse_square(std::string_view name) noexcept;

// 28 -> "e4". `square` must be below 64.
[[nodiscard]] std::string square_name(Square square);

}  // namespace chess::core
