#pragma once

#include <optional>
#include <string>

#include "core/position.hpp"
#include "core/types.hpp"

namespace chess::core {

// What a move does besides moving the piece. The generator fills these in, so
// apply_move and the rules layer never have to work them out from the board
// again. Section 3 of docs/system-design.md.
struct MoveFlags {
    bool capture = false;
    bool en_passant = false;   // a pawn capture onto the empty en passant square
    bool castle = false;       // the king's two-square move, with the rook following
    bool double_push = false;  // a pawn's two-square opening move

    bool operator==(const MoveFlags&) const = default;
};

struct Move {
    Square from = 0;
    Square to = 0;
    std::optional<PieceType> promotion;  // Queen, Rook, Bishop or Knight
    MoveFlags flags;

    bool operator==(const Move&) const = default;
};

// "e2e4", or "e7e8q" for a promotion. For tests and error messages only: the
// protocol in section 5 sends squares, never strings.
[[nodiscard]] std::string to_uci(const Move& move);

// Plays `move` and returns the position that follows. The original is unchanged.
//
// `move` must be one that pseudo_legal_moves returned for `position`, because
// apply_move trusts the flags instead of working them out again. It throws
// std::bad_optional_access if `from` is empty, rather than corrupting the board.
//
// Updates everything the next position needs: the captured piece (including the
// en passant one, which is not on the square the pawn lands on), the rook that
// travels with a castling king, castling rights, the en passant square, and both
// clocks.
[[nodiscard]] Position apply_move(const Position& position, const Move& move);

}  // namespace chess::core
