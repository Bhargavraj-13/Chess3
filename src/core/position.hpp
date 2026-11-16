#pragma once

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "core/types.hpp"

namespace chess::core {

inline constexpr std::string_view kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Thrown by Position::from_fen. what() says which part of the FEN is wrong.
//
// FEN never comes from players (the server never sends or receives it), only
// from our own code and tests, so an invalid FEN is a bug and throwing is fine.
class FenError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

struct CastlingRights {
    bool white_kingside = false;
    bool white_queenside = false;
    bool black_kingside = false;
    bool black_queenside = false;

    bool operator==(const CastlingRights&) const = default;
};

// Everything a FEN describes: the board, the side to move, castling rights, the
// en passant square and the two move counters.
//
// from_fen checks that the position makes sense. The setters don't, so move
// generation and variants can build any position they need.
class Position {
public:
    // An empty board, White to move, no castling rights, no en passant square,
    // halfmove clock 0 and fullmove number 1.
    Position() = default;

    // Throws FenError if `fen` is not a valid position.
    [[nodiscard]] static Position from_fen(std::string_view fen);
    [[nodiscard]] std::string to_fen() const;

    [[nodiscard]] std::optional<Piece> at(Square square) const { return board_[square]; }
    void set(Square square, std::optional<Piece> piece) { board_[square] = piece; }

    [[nodiscard]] Color side_to_move() const { return side_to_move_; }
    void set_side_to_move(Color color) { side_to_move_ = color; }

    [[nodiscard]] CastlingRights castling() const { return castling_; }
    void set_castling(CastlingRights rights) { castling_ = rights; }
    void clear_castling(Color color);

    // The square a pawn skipped over with a double push on the last move, as in
    // FEN. Set even when no en passant capture is actually possible.
    [[nodiscard]] std::optional<Square> en_passant() const { return en_passant_; }
    void set_en_passant(std::optional<Square> square) { en_passant_ = square; }
    void clear_en_passant() { en_passant_.reset(); }

    // Half-moves since the last capture or pawn move, for the 50- and 75-move rules.
    [[nodiscard]] int halfmove_clock() const { return halfmove_clock_; }
    void set_halfmove_clock(int value) { halfmove_clock_ = value; }

    // Starts at 1 and goes up after each Black move.
    [[nodiscard]] int fullmove_number() const { return fullmove_number_; }
    void set_fullmove_number(int value) { fullmove_number_ = value; }

    bool operator==(const Position&) const = default;

private:
    std::array<std::optional<Piece>, kNumSquares> board_{};
    Color side_to_move_ = Color::White;
    CastlingRights castling_{};
    std::optional<Square> en_passant_;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
};

}  // namespace chess::core
