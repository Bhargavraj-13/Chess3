#include "core/move.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

#include "core/movegen.hpp"
#include "core/position.hpp"
#include "core/types.hpp"

using chess::core::apply_move;
using chess::core::Color;
using chess::core::kStartFen;
using chess::core::Move;
using chess::core::MoveFlags;
using chess::core::parse_square;
using chess::core::Piece;
using chess::core::PieceType;
using chess::core::Position;
using chess::core::pseudo_legal_moves;
using chess::core::Square;
using chess::core::to_uci;

namespace {

Square sq(std::string_view name) { return parse_square(name).value(); }

// Looks the move up in the generator rather than building it by hand, so these
// tests use the same flags the rest of the engine will.
Move find(const Position& position, std::string_view uci) {
    for (const Move& move : pseudo_legal_moves(position)) {
        if (to_uci(move) == uci) {
            return move;
        }
    }
    FAIL("no move " << uci << " in " << position.to_fen());
    return Move{};
}

// Plays a move named in UCI and returns the FEN that follows.
std::string play(std::string_view fen, std::string_view uci) {
    const Position before = Position::from_fen(fen);
    return apply_move(before, find(before, uci)).to_fen();
}

}  // namespace

TEST_CASE("to_uci names the squares, and the promotion piece", "[core][move]") {
    CHECK(to_uci(Move{sq("e2"), sq("e4"), std::nullopt, MoveFlags{}}) == "e2e4");
    CHECK(to_uci(Move{sq("e7"), sq("e8"), PieceType::Queen, MoveFlags{}}) == "e7e8q");
    CHECK(to_uci(Move{sq("e7"), sq("e8"), PieceType::Knight, MoveFlags{}}) == "e7e8n");
    CHECK(to_uci(Move{sq("a7"), sq("b8"), PieceType::Rook, MoveFlags{}}) == "a7b8r");
    CHECK(to_uci(Move{sq("a7"), sq("b8"), PieceType::Bishop, MoveFlags{}}) == "a7b8b");
}

TEST_CASE("the opening moves of a game give the expected positions", "[core][move]") {
    Position position = Position::from_fen(kStartFen);

    position = apply_move(position, find(position, "e2e4"));
    CHECK(position.to_fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    position = apply_move(position, find(position, "c7c5"));
    CHECK(position.to_fen() == "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");

    position = apply_move(position, find(position, "g1f3"));
    CHECK(position.to_fen() == "rnbqkbnr/pp1ppppp/8/2p5/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2");
}

TEST_CASE("apply_move leaves the position it was given alone", "[core][move]") {
    const Position before = Position::from_fen(kStartFen);
    const Position after = apply_move(before, find(before, "e2e4"));

    CHECK(before.to_fen() == kStartFen);
    CHECK(after.to_fen() != before.to_fen());
}

TEST_CASE("the clocks follow the 50-move rule", "[core][move]") {
    SECTION("a quiet move adds one to the halfmove clock") {
        CHECK(play("4k3/8/8/8/8/8/4B3/4K3 w - - 7 20", "e2d3") ==
              "4k3/8/8/8/8/3B4/8/4K3 b - - 8 20");
    }
    SECTION("a pawn move resets it") {
        CHECK(play("4k3/8/8/8/8/8/4P3/4K3 w - - 7 20", "e2e3") ==
              "4k3/8/8/8/8/4P3/8/4K3 b - - 0 20");
    }
    SECTION("a capture resets it") {
        CHECK(play("4k3/8/8/8/4b3/8/8/4K2B w - - 7 20", "h1e4") ==
              "4k3/8/8/8/4B3/8/8/4K3 b - - 0 20");
    }
    SECTION("the fullmove number goes up after Black's move, not White's") {
        CHECK(play("4k3/8/8/8/8/8/8/4KB2 w - - 7 20", "f1g2") ==
              "4k3/8/8/8/8/8/6B1/4K3 b - - 8 20");
        CHECK(play("4k3/8/8/8/8/8/6b1/4K3 b - - 7 20", "g2f1") ==
              "4k3/8/8/8/8/8/8/4Kb2 w - - 8 21");
    }
}

TEST_CASE("a double push offers en passant for one move only", "[core][move]") {
    CHECK(play("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2e4") == "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1");
    CHECK(play("4k3/4p3/8/8/8/8/8/4K3 b - - 0 1", "e7e5") == "4k3/8/8/4p3/8/8/8/4K3 w - e6 0 2");

    // Any other move clears the square the previous double push offered.
    CHECK(play("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2", "e1e2") == "4k3/8/8/3pP3/8/8/4K3/8 b - - 1 2");
}

TEST_CASE("an en passant capture removes the pawn beside the landing square", "[core][move]") {
    SECTION("White captures") {
        // The Black pawn is on d5, but the White pawn lands on d6.
        CHECK(play("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2", "e5d6") ==
              "4k3/8/3P4/8/8/8/8/4K3 b - - 0 2");
    }
    SECTION("Black captures") {
        CHECK(play("4k3/8/8/8/4pP2/8/8/4K3 b - f3 0 1", "e4f3") ==
              "4k3/8/8/8/8/5p2/8/4K3 w - - 0 2");
    }
}

TEST_CASE("a promotion puts the chosen piece on the board", "[core][move]") {
    CHECK(play("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7a8q") == "Q3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    CHECK(play("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7a8n") == "N3k3/8/8/8/8/8/8/4K3 b - - 0 1");
    CHECK(play("4k3/8/8/8/8/8/7p/4K3 b - - 0 1", "h2h1r") == "4k3/8/8/8/8/8/8/4K2r w - - 0 2");

    SECTION("capturing into a promotion works too") {
        CHECK(play("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7b8q") ==
              "1Q2k3/8/8/8/8/8/8/4K3 b - - 0 1");
    }
}

TEST_CASE("castling moves the rook as well as the king", "[core][move]") {
    const std::string_view both = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    const std::string_view black = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1";

    CHECK(play(both, "e1g1") == "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 1 1");
    CHECK(play(both, "e1c1") == "r3k2r/8/8/8/8/8/8/2KR3R b kq - 1 1");
    CHECK(play(black, "e8g8") == "r4rk1/8/8/8/8/8/8/R3K2R w KQ - 1 2");
    CHECK(play(black, "e8c8") == "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 1 2");
}

TEST_CASE("castling rights are given up when the king or a rook moves", "[core][move]") {
    const std::string_view both = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";

    SECTION("a king move gives up both of its own rights") {
        CHECK(play(both, "e1e2") == "r3k2r/8/8/8/8/8/4K3/R6R b kq - 1 1");
    }
    SECTION("a rook move gives up only that rook's right") {
        CHECK(play(both, "a1b1") == "r3k2r/8/8/8/8/8/8/1R2K2R b Kkq - 1 1");
        CHECK(play(both, "h1g1") == "r3k2r/8/8/8/8/8/8/R3K1R1 b Qkq - 1 1");
    }
    SECTION("capturing a rook in its corner ends the opponent's right as well") {
        // The h1 rook takes on h8, so White loses K and Black loses k.
        CHECK(play(both, "h1h8") == "r3k2R/8/8/8/8/8/8/R3K3 b Qq - 0 1");
    }
    SECTION("castling itself gives up both rights") {
        const Position after =
            apply_move(Position::from_fen(both), find(Position::from_fen(both), "e1g1"));
        CHECK_FALSE(after.castling().white_kingside);
        CHECK_FALSE(after.castling().white_queenside);
        CHECK(after.castling().black_kingside);
    }
}

TEST_CASE("every position apply_move produces can be written as FEN and read back",
          "[core][move]") {
    const Position start = Position::from_fen(kStartFen);
    for (const Move& move : pseudo_legal_moves(start)) {
        CAPTURE(to_uci(move));
        const Position after = apply_move(start, move);
        CHECK(Position::from_fen(after.to_fen()) == after);
    }
}
