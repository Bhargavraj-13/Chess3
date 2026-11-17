#include "core/movegen.hpp"

#include <array>
#include <optional>
#include <vector>

#include "core/move.hpp"
#include "core/position.hpp"
#include "core/types.hpp"

namespace chess::core {
namespace {

struct Step {
    int file;
    int rank;
};

constexpr std::array<Step, 8> kKnightSteps = {
    {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};

constexpr std::array<Step, 8> kKingSteps = {
    {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};

constexpr std::array<Step, 4> kDiagonals = {{{1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};

constexpr std::array<Step, 4> kStraights = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};

// The promotion choices, in the order a player usually wants to see them.
constexpr std::array<PieceType, 4> kPromotions = {PieceType::Queen, PieceType::Rook,
                                                  PieceType::Bishop, PieceType::Knight};

[[nodiscard]] constexpr bool on_board(int file, int rank) noexcept {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

MoveFlags capture_flag() {
    MoveFlags flags;
    flags.capture = true;
    return flags;
}

// A pawn reaching the far rank must promote, so one landing square becomes four
// moves. Everywhere else this adds the single move.
void add_pawn_move(Square from, Square to, int last_rank, MoveFlags flags,
                   std::vector<Move>& moves) {
    if (rank_of(to) != last_rank) {
        moves.push_back(Move{from, to, std::nullopt, flags});
        return;
    }
    for (const PieceType promotion : kPromotions) {
        moves.push_back(Move{from, to, promotion, flags});
    }
}

void add_pawn_moves(const Position& position, Square from, Color us, std::vector<Move>& moves) {
    const int forward = us == Color::White ? 1 : -1;
    const int start_rank = us == Color::White ? 1 : 6;
    const int last_rank = us == Color::White ? 7 : 0;
    const int file = file_of(from);
    const int rank = rank_of(from);
    const int ahead = rank + forward;
    if (!on_board(file, ahead)) {
        return;  // Only reachable on a board built by hand: from_fen rejects it.
    }

    // Pawns capture sideways only, so a push needs an empty square, and the
    // two-square opening move needs both squares empty.
    if (!position.at(make_square(file, ahead))) {
        add_pawn_move(from, make_square(file, ahead), last_rank, MoveFlags{}, moves);
        const int jump = rank + 2 * forward;
        if (rank == start_rank && !position.at(make_square(file, jump))) {
            MoveFlags flags;
            flags.double_push = true;
            moves.push_back(Move{from, make_square(file, jump), std::nullopt, flags});
        }
    }

    for (const int side : {-1, 1}) {
        const int target_file = file + side;
        if (!on_board(target_file, ahead)) {
            continue;
        }
        const Square to = make_square(target_file, ahead);
        const std::optional<Piece> target = position.at(to);
        if (target && target->color != us) {
            add_pawn_move(from, to, last_rank, capture_flag(), moves);
        } else if (!target && position.en_passant() == to) {
            // The pawn being taken stands beside this square, so the square the
            // capture lands on is empty. apply_move removes the right pawn.
            MoveFlags flags = capture_flag();
            flags.en_passant = true;
            moves.push_back(Move{from, to, std::nullopt, flags});
        }
    }
}

// Knights and kings: one hop per direction, blocked only by our own pieces.
template <std::size_t N>
void add_steps(const Position& position, Square from, Color us, const std::array<Step, N>& steps,
               std::vector<Move>& moves) {
    for (const Step step : steps) {
        const int file = file_of(from) + step.file;
        const int rank = rank_of(from) + step.rank;
        if (!on_board(file, rank)) {
            continue;
        }
        const Square to = make_square(file, rank);
        const std::optional<Piece> target = position.at(to);
        if (target && target->color == us) {
            continue;
        }
        moves.push_back(Move{from, to, std::nullopt, target ? capture_flag() : MoveFlags{}});
    }
}

// Bishops, rooks and queens: keep going until something stops us. An enemy piece
// can be captured, and the ray ends either way.
template <std::size_t N>
void add_slides(const Position& position, Square from, Color us,
                const std::array<Step, N>& directions, std::vector<Move>& moves) {
    for (const Step direction : directions) {
        int file = file_of(from);
        int rank = rank_of(from);
        while (true) {
            file += direction.file;
            rank += direction.rank;
            if (!on_board(file, rank)) {
                break;
            }
            const Square to = make_square(file, rank);
            const std::optional<Piece> target = position.at(to);
            if (!target) {
                moves.push_back(Move{from, to, std::nullopt, MoveFlags{}});
                continue;
            }
            if (target->color != us) {
                moves.push_back(Move{from, to, std::nullopt, capture_flag()});
            }
            break;
        }
    }
}

// Castling candidates. from_fen already checks that a right matches the king and
// rook squares, but the setters deliberately don't, so check the pieces here too
// rather than trusting the rights on a position built by hand.
void add_castling(const Position& position, Color us, std::vector<Move>& moves) {
    const int rank = us == Color::White ? 0 : 7;
    const Square king_square = make_square(4, rank);
    if (position.at(king_square) != Piece{us, PieceType::King}) {
        return;
    }

    const CastlingRights rights = position.castling();
    const bool kingside = us == Color::White ? rights.white_kingside : rights.black_kingside;
    const bool queenside = us == Color::White ? rights.white_queenside : rights.black_queenside;
    const auto empty = [&](int file) { return !position.at(make_square(file, rank)); };
    const auto rook_on = [&](int file) {
        return position.at(make_square(file, rank)) == Piece{us, PieceType::Rook};
    };

    MoveFlags flags;
    flags.castle = true;
    if (kingside && rook_on(7) && empty(5) && empty(6)) {
        moves.push_back(Move{king_square, make_square(6, rank), std::nullopt, flags});
    }
    // The b-file square must be empty for the rook to pass, even though the king
    // never stands on it.
    if (queenside && rook_on(0) && empty(1) && empty(2) && empty(3)) {
        moves.push_back(Move{king_square, make_square(2, rank), std::nullopt, flags});
    }
}

}  // namespace

std::vector<Move> pseudo_legal_moves(const Position& position) {
    std::vector<Move> moves;
    moves.reserve(64);  // A normal position has far fewer; this avoids regrowing.

    const Color us = position.side_to_move();
    for (int square = 0; square < kNumSquares; ++square) {
        const Square from = static_cast<Square>(square);
        const std::optional<Piece> piece = position.at(from);
        if (!piece || piece->color != us) {
            continue;
        }
        switch (piece->type) {
            case PieceType::Pawn:
                add_pawn_moves(position, from, us, moves);
                break;
            case PieceType::Knight:
                add_steps(position, from, us, kKnightSteps, moves);
                break;
            case PieceType::Bishop:
                add_slides(position, from, us, kDiagonals, moves);
                break;
            case PieceType::Rook:
                add_slides(position, from, us, kStraights, moves);
                break;
            case PieceType::Queen:
                add_slides(position, from, us, kDiagonals, moves);
                add_slides(position, from, us, kStraights, moves);
                break;
            case PieceType::King:
                add_steps(position, from, us, kKingSteps, moves);
                break;
        }
    }
    add_castling(position, us, moves);
    return moves;
}

}  // namespace chess::core
