#include "core/movegen.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "core/move.hpp"
#include "core/position.hpp"
#include "core/types.hpp"

using chess::core::kStartFen;
using chess::core::Move;
using chess::core::parse_square;
using chess::core::Position;
using chess::core::pseudo_legal_moves;
using chess::core::Square;
using chess::core::to_uci;

namespace {

Square sq(std::string_view name) { return parse_square(name).value(); }

// Every move in the position, in UCI, sorted so the expected lists below can be
// written in a readable order.
std::vector<std::string> moves_of(std::string_view fen) {
    std::vector<std::string> names;
    for (const Move& move : pseudo_legal_moves(Position::from_fen(fen))) {
        names.push_back(to_uci(move));
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Only the moves of the piece standing on `from`, which keeps a test about one
// piece from having to list the king's moves as well.
std::vector<std::string> moves_from(std::string_view fen, std::string_view from) {
    std::vector<std::string> names;
    for (const Move& move : pseudo_legal_moves(Position::from_fen(fen))) {
        if (move.from == sq(from)) {
            names.push_back(to_uci(move));
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

Move find(std::string_view fen, std::string_view uci) {
    for (const Move& move : pseudo_legal_moves(Position::from_fen(fen))) {
        if (to_uci(move) == uci) {
            return move;
        }
    }
    FAIL("no move " << uci << " in " << fen);
    return Move{};
}

bool has(std::string_view fen, std::string_view uci) {
    const std::vector<std::string> names = moves_of(fen);
    return std::find(names.begin(), names.end(), uci) != names.end();
}

}  // namespace

TEST_CASE("the start position has the 20 opening moves", "[core][movegen]") {
    const std::vector<std::string> names = moves_of(kStartFen);
    CHECK(names.size() == 20);
    CHECK(moves_from(kStartFen, "e2") == std::vector<std::string>{"e2e3", "e2e4"});
    CHECK(moves_from(kStartFen, "b1") == std::vector<std::string>{"b1a3", "b1c3"});
    CHECK(moves_from(kStartFen, "a1").empty());  // boxed in by its own pieces
    CHECK(moves_from(kStartFen, "e1").empty());
}

TEST_CASE("only the side to move gets moves", "[core][movegen]") {
    const std::string_view black_to_move =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1";
    CHECK(moves_of(black_to_move).size() == 20);
    CHECK(moves_from(black_to_move, "e7") == std::vector<std::string>{"e7e5", "e7e6"});

    const Position position = Position::from_fen(black_to_move);
    for (const Move& move : pseudo_legal_moves(position)) {
        CAPTURE(to_uci(move));
        CHECK(position.at(move.from)->color == chess::core::Color::Black);
    }
}

TEST_CASE("pawns push, jump and capture sideways", "[core][movegen]") {
    SECTION("a pawn on its starting square may move one or two squares") {
        CHECK(moves_from("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2") ==
              std::vector<std::string>{"e2e3", "e2e4"});
    }
    SECTION("a pawn that has already moved may only move one") {
        CHECK(moves_from("4k3/8/8/8/8/4P3/8/4K3 w - - 0 1", "e3") ==
              std::vector<std::string>{"e3e4"});
    }
    SECTION("a piece directly ahead blocks the pawn, and cannot be captured") {
        CHECK(moves_from("4k3/8/8/8/8/4p3/4P3/4K3 w - - 0 1", "e2").empty());
    }
    SECTION("a piece two squares ahead blocks only the jump") {
        CHECK(moves_from("4k3/8/8/8/4p3/8/4P3/4K3 w - - 0 1", "e2") ==
              std::vector<std::string>{"e2e3"});
    }
    SECTION("captures are diagonal, and only of enemy pieces") {
        CHECK(moves_from("4k3/8/8/8/8/3p1P2/4P3/4K3 w - - 0 1", "e2") ==
              std::vector<std::string>{"e2d3", "e2e3", "e2e4"});
    }
    SECTION("Black pawns move the other way") {
        CHECK(moves_from("4k3/4p3/8/8/8/8/8/4K3 b - - 0 1", "e7") ==
              std::vector<std::string>{"e7e5", "e7e6"});
    }
    SECTION("the double push is flagged, so apply_move can offer en passant") {
        CHECK(find("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2e4").flags.double_push);
        CHECK_FALSE(find("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2e3").flags.double_push);
    }
}

TEST_CASE("en passant is generated while the square is set", "[core][movegen]") {
    const std::string_view fen = "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2";
    CHECK(moves_from(fen, "e5") == std::vector<std::string>{"e5d6", "e5e6"});

    const Move capture = find(fen, "e5d6");
    CHECK(capture.flags.en_passant);
    CHECK(capture.flags.capture);  // it is a capture even though d6 is empty

    SECTION("and not once the square is cleared") {
        CHECK(moves_from("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 2", "e5") ==
              std::vector<std::string>{"e5e6"});
    }
}

TEST_CASE("a pawn reaching the far rank promotes four ways", "[core][movegen]") {
    CHECK(moves_from("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7") ==
          std::vector<std::string>{"a7a8b", "a7a8n", "a7a8q", "a7a8r"});

    SECTION("including when it promotes by capturing") {
        CHECK(moves_from("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1", "a7") ==
              std::vector<std::string>{"a7a8b", "a7a8n", "a7a8q", "a7a8r", "a7b8b", "a7b8n",
                                       "a7b8q", "a7b8r"});
    }
    SECTION("and for Black on the first rank") {
        CHECK(moves_from("4k3/8/8/8/8/8/7p/4K3 b - - 0 1", "h2") ==
              std::vector<std::string>{"h2h1b", "h2h1n", "h2h1q", "h2h1r"});
    }
}

TEST_CASE("knights jump, and are stopped only by their own pieces", "[core][movegen]") {
    SECTION("eight moves from the middle") {
        CHECK(moves_from("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1", "d4").size() == 8);
    }
    SECTION("two from a corner") {
        CHECK(moves_from("4k3/8/8/8/8/8/8/N3K3 w - - 0 1", "a1") ==
              std::vector<std::string>{"a1b3", "a1c2"});
    }
    SECTION("own pieces block, enemy pieces are captured") {
        CHECK(moves_from("4k3/8/8/8/8/8/2P5/N3K3 w - - 0 1", "a1") ==
              std::vector<std::string>{"a1b3"});
        CHECK(find("4k3/8/8/8/8/8/2p5/N3K3 w - - 0 1", "a1c2").flags.capture);
    }
}

TEST_CASE("sliding pieces run until something stops them", "[core][movegen]") {
    SECTION("a rook covers its rank and file") {
        CHECK(moves_from("4k3/8/8/8/8/8/8/R3K3 w - - 0 1", "a1").size() == 10);
    }
    SECTION("a bishop covers its diagonals") {
        CHECK(moves_from("4k3/8/8/8/8/8/8/B3K3 w - - 0 1", "a1") ==
              std::vector<std::string>{"a1b2", "a1c3", "a1d4", "a1e5", "a1f6", "a1g7", "a1h8"});
    }
    SECTION("a queen covers both") {
        CHECK(moves_from("4k3/8/8/8/3Q4/8/8/4K3 w - - 0 1", "d4").size() == 27);
    }
    SECTION("an own piece blocks the square before it") {
        CHECK(moves_from("4k3/8/8/8/8/2P5/8/B3K3 w - - 0 1", "a1") ==
              std::vector<std::string>{"a1b2"});
    }
    SECTION("an enemy piece is captured, and the ray stops there") {
        CHECK(moves_from("4k3/8/8/8/8/2p5/8/B3K3 w - - 0 1", "a1") ==
              std::vector<std::string>{"a1b2", "a1c3"});
        CHECK(find("4k3/8/8/8/8/2p5/8/B3K3 w - - 0 1", "a1c3").flags.capture);
    }
}

TEST_CASE("the king takes one step in any direction", "[core][movegen]") {
    CHECK(moves_from("4k3/8/8/8/3K4/8/8/8 w - - 0 1", "d4").size() == 8);
    CHECK(moves_from("4k3/8/8/8/8/8/8/K7 w - - 0 1", "a1") ==
          std::vector<std::string>{"a1a2", "a1b1", "a1b2"});
}

TEST_CASE("castling candidates need the rights, the pieces and the empty squares",
          "[core][movegen]") {
    const std::string_view both = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";

    SECTION("both sides are offered when everything is in place") {
        CHECK(has(both, "e1g1"));
        CHECK(has(both, "e1c1"));
        CHECK(find(both, "e1g1").flags.castle);
        CHECK(find(both, "e1c1").flags.castle);
    }
    SECTION("Black gets the same") {
        const std::string_view black = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1";
        CHECK(has(black, "e8g8"));
        CHECK(has(black, "e8c8"));
    }
    SECTION("a piece in the way stops that side only") {
        const std::string_view blocked = "r3k2r/8/8/8/8/8/8/R2QK2R w KQkq - 0 1";
        CHECK(has(blocked, "e1g1"));
        CHECK_FALSE(has(blocked, "e1c1"));
    }
    SECTION("the b-file square must be empty too, because the rook passes it") {
        const std::string_view blocked = "r3k2r/8/8/8/8/8/8/RN2K2R w KQkq - 0 1";
        CHECK_FALSE(has(blocked, "e1c1"));
    }
    SECTION("no right means no castling") {
        const std::string_view none = "r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1";
        CHECK_FALSE(has(none, "e1g1"));
        CHECK_FALSE(has(none, "e1c1"));
    }
    SECTION("a position built by hand without the rook is not offered castling") {
        Position position;
        position.set(sq("e1"),
                     chess::core::Piece{chess::core::Color::White, chess::core::PieceType::King});
        position.set(sq("e8"),
                     chess::core::Piece{chess::core::Color::Black, chess::core::PieceType::King});
        position.set_castling(chess::core::CastlingRights{true, true, true, true});

        for (const Move& move : pseudo_legal_moves(position)) {
            CAPTURE(to_uci(move));
            CHECK_FALSE(move.flags.castle);
        }
    }
}

TEST_CASE("pinned pieces still get moves, because king safety belongs to #6", "[core][movegen]") {
    // The White knight on e2 is pinned by the Black rook on e8, so none of its
    // moves are legal. pseudo_legal_moves must still offer them.
    const std::string_view pinned = "4rk2/8/8/8/8/8/4N3/4K3 w - - 0 1";
    CHECK(moves_from(pinned, "e2").size() == 6);
    CHECK(has(pinned, "e2d4"));
}
