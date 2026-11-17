#include "core/move.hpp"

#include <optional>
#include <string>

#include "core/position.hpp"
#include "core/types.hpp"

namespace chess::core {
namespace {

char promotion_letter(PieceType type) {
    switch (type) {
        case PieceType::Knight:
            return 'n';
        case PieceType::Bishop:
            return 'b';
        case PieceType::Rook:
            return 'r';
        case PieceType::Queen:
            return 'q';
        case PieceType::Pawn:
        case PieceType::King:
            break;
    }
    return '?';
}

// A rook that leaves a corner, or is captured in one, ends that castling right.
// Looking only at the square covers both cases, and covers a king capturing the
// rook as well.
void clear_right_for_corner(Position& position, Square square) {
    CastlingRights rights = position.castling();
    if (square == make_square(0, 0)) {
        rights.white_queenside = false;
    } else if (square == make_square(7, 0)) {
        rights.white_kingside = false;
    } else if (square == make_square(0, 7)) {
        rights.black_queenside = false;
    } else if (square == make_square(7, 7)) {
        rights.black_kingside = false;
    } else {
        return;
    }
    position.set_castling(rights);
}

}  // namespace

std::string to_uci(const Move& move) {
    std::string text = square_name(move.from) + square_name(move.to);
    if (move.promotion) {
        text += promotion_letter(*move.promotion);
    }
    return text;
}

Position apply_move(const Position& position, const Move& move) {
    const Color mover = position.side_to_move();
    const Piece piece = position.at(move.from).value();

    Position after = position;

    // The halfmove clock counts towards the 50- and 75-move rules, so a pawn move
    // or a capture resets it. The fullmove number goes up after Black's move.
    const bool resets_clock = piece.type == PieceType::Pawn || move.flags.capture;
    after.set_halfmove_clock(resets_clock ? 0 : position.halfmove_clock() + 1);
    if (mover == Color::Black) {
        after.set_fullmove_number(position.fullmove_number() + 1);
    }

    after.set(move.from, std::nullopt);
    if (move.flags.en_passant) {
        // The captured pawn is beside the moving pawn, not on the square it lands
        // on, so clearing `to` would leave it on the board.
        const int back = mover == Color::White ? -1 : 1;
        after.set(make_square(file_of(move.to), rank_of(move.to) + back), std::nullopt);
    }
    after.set(move.to, Piece{mover, move.promotion.value_or(piece.type)});

    if (move.flags.castle) {
        // The king has just moved two squares. The rook jumps over it to the
        // square it passed, which is the only square between them.
        const int rank = rank_of(move.to);
        const bool kingside = file_of(move.to) == 6;
        const Square rook_from = make_square(kingside ? 7 : 0, rank);
        const Square rook_to = make_square(kingside ? 5 : 3, rank);
        after.set(rook_to, after.at(rook_from));
        after.set(rook_from, std::nullopt);
    }

    if (piece.type == PieceType::King) {
        after.clear_castling(mover);
    }
    clear_right_for_corner(after, move.from);
    clear_right_for_corner(after, move.to);

    // En passant is offered for one move only, so every other move clears it.
    if (move.flags.double_push) {
        const int skipped = (rank_of(move.from) + rank_of(move.to)) / 2;
        after.set_en_passant(make_square(file_of(move.from), skipped));
    } else {
        after.clear_en_passant();
    }

    after.set_side_to_move(opposite(mover));
    return after;
}

}  // namespace chess::core
