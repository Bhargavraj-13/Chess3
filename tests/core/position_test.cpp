#include "core/position.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <string_view>

using Catch::Matchers::ContainsSubstring;
using chess::core::CastlingRights;
using chess::core::Color;
using chess::core::FenError;
using chess::core::kStartFen;
using chess::core::parse_square;
using chess::core::Piece;
using chess::core::PieceType;
using chess::core::Position;
using chess::core::Square;

namespace {

Square sq(std::string_view name) { return parse_square(name).value(); }

}  // namespace

TEST_CASE("the start position is parsed", "[core][fen]") {
    const Position position = Position::from_fen(kStartFen);

    CHECK(position.at(sq("e1")) == Piece{Color::White, PieceType::King});
    CHECK(position.at(sq("d8")) == Piece{Color::Black, PieceType::Queen});
    CHECK(position.at(sq("a1")) == Piece{Color::White, PieceType::Rook});
    CHECK(position.at(sq("g8")) == Piece{Color::Black, PieceType::Knight});
    CHECK(position.at(sq("c2")) == Piece{Color::White, PieceType::Pawn});
    CHECK(position.at(sq("f7")) == Piece{Color::Black, PieceType::Pawn});
    CHECK_FALSE(position.at(sq("e4")).has_value());

    CHECK(position.side_to_move() == Color::White);
    CHECK(position.castling() == CastlingRights{true, true, true, true});
    CHECK_FALSE(position.en_passant().has_value());
    CHECK(position.halfmove_clock() == 0);
    CHECK(position.fullmove_number() == 1);
}

TEST_CASE("every FEN field is parsed", "[core][fen]") {
    // After 1. e4 c5 2. Nf3 Nc6 3. Bb5 g6 4. O-O, with clocks changed by hand.
    const Position position =
        Position::from_fen("r1bqkbnr/pp1ppp1p/2n3p1/1Bp5/4P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 3 4");

    CHECK(position.at(sq("g1")) == Piece{Color::White, PieceType::King});
    CHECK(position.at(sq("f1")) == Piece{Color::White, PieceType::Rook});
    CHECK(position.side_to_move() == Color::Black);
    CHECK(position.castling() == CastlingRights{false, false, true, true});
    CHECK(position.halfmove_clock() == 3);
    CHECK(position.fullmove_number() == 4);
}

TEST_CASE("the en passant square is parsed", "[core][fen]") {
    SECTION("after a White double push") {
        const Position position =
            Position::from_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
        CHECK(position.en_passant() == sq("e3"));
    }
    SECTION("after a Black double push") {
        const Position position =
            Position::from_fen("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2");
        CHECK(position.en_passant() == sq("c6"));
    }
}

TEST_CASE("FEN round trips", "[core][fen]") {
    // The tricky positions are from the Chess Programming Wiki's perft page.
    const std::string fen =
        GENERATE(as<std::string>{}, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                 "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
                 "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
                 "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                 "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                 "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                 "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                 "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
                 "4k3/8/8/8/8/8/8/4K3 b - - 99 200");
    CAPTURE(fen);

    const Position position = Position::from_fen(fen);
    CHECK(position.to_fen() == fen);
    CHECK(Position::from_fen(position.to_fen()) == position);
}

TEST_CASE("positions can be built with the setters", "[core]") {
    Position position;
    CHECK(position.to_fen() == "8/8/8/8/8/8/8/8 w - - 0 1");

    position.set(sq("e1"), Piece{Color::White, PieceType::King});
    position.set(sq("e8"), Piece{Color::Black, PieceType::King});
    position.set(sq("a1"), Piece{Color::White, PieceType::Rook});
    position.set_castling(CastlingRights{false, true, false, false});
    position.set_side_to_move(Color::Black);
    position.set_halfmove_clock(5);
    position.set_fullmove_number(30);
    CHECK(position.to_fen() == "4k3/8/8/8/8/8/8/R3K3 b Q - 5 30");

    position.set(sq("a1"), std::nullopt);
    position.clear_castling(Color::White);
    CHECK(position.to_fen() == "4k3/8/8/8/8/8/8/4K3 b - - 5 30");
}

TEST_CASE("clear_castling only affects one color", "[core]") {
    Position position = Position::from_fen(kStartFen);
    position.clear_castling(Color::Black);
    CHECK(position.castling() == CastlingRights{true, true, false, false});

    position.set_en_passant(sq("e3"));
    position.clear_en_passant();
    CHECK_FALSE(position.en_passant().has_value());
}

TEST_CASE("invalid FEN is rejected with a clear error", "[core][fen]") {
    struct Case {
        std::string_view fen;
        std::string_view error;
    };
    const Case cases[] = {
        // Fields
        {"", "the FEN is empty"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0",
         "6 fields separated by spaces, found 5"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 extra",
         "6 fields separated by spaces, found 7"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w  KQkq - 0 1", "single spaces"},
        {" rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "single spaces"},
        // Board
        {"rnbqkbnr/pppppppp/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "8 ranks"},
        {"rnbqkbnr/ppppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         "rank 7 has more than 8 squares"},
        {"rnbqkbnr/pppppppp/8/8/44/8/PPPPPPPP/RNBQKBN w KQkq - 0 1",
         "rank 1 has 7 squares, expected 8"},
        {"rnbqkbnr/pppppppp/8/8/8//PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         "rank 3 has 0 squares, expected 8"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",
         "unexpected character 'X' in rank 1"},
        {"rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         "unexpected character '9' in rank 6"},
        // Side to move
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR W KQkq - 0 1",
         "side to move must be 'w' or 'b', found \"W\""},
        // Castling
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkx - 0 1",
         "castling rights must be '-' or letters from \"KQkq\""},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KKq - 0 1", "contain 'K' twice"},
        {"rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         "castling right 'k' needs the Black king on e8 and a rook on h8"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBK1BNR w Q - 0 1",
         "castling right 'Q' needs the White king on e1 and a rook on a1"},
        // En passant
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e9 0 1",
         "en passant square must be '-' or a square"},
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e3 0 1",
         "en passant square e3 is impossible with White to move, it must be on rank 6"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e6 0 1",
         "en passant square e6 needs a Black pawn on e5"},
        {"rnbqkbnr/pppppppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR w KQkq e6 0 1",
         "needs e6 and e7 to be empty"},
        // Counters
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - x 1",
         "halfmove clock must be a whole number, found \"x\""},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 1x 1",
         "halfmove clock must be a whole number"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - -1 1",
         "halfmove clock must be at least 0"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0",
         "fullmove number must be at least 1"},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 99999999999",
         "fullmove number is too large"},
        // Kings and pawns
        {"8/8/8/8/8/8/8/8 w - - 0 1", "White must have exactly one king, found 0"},
        {"4k3/8/8/8/8/8/8/3KK3 w - - 0 1", "White must have exactly one king, found 2"},
        {"8/8/8/8/8/8/8/4K3 w - - 0 1", "Black must have exactly one king, found 0"},
        {"P3k3/8/8/8/8/8/8/4K3 w - - 0 1", "there is a pawn on a8"},
        {"4k3/8/8/8/8/8/8/4K2p w - - 0 1", "there is a pawn on h1"},
    };

    for (const Case& c : cases) {
        CAPTURE(c.fen);
        CHECK_THROWS_AS(Position::from_fen(c.fen), FenError);
        CHECK_THROWS_WITH(Position::from_fen(c.fen), ContainsSubstring(std::string(c.error)));
    }
}

TEST_CASE("FenError messages start with \"invalid FEN\"", "[core][fen]") {
    CHECK_THROWS_WITH(Position::from_fen("not a fen"), ContainsSubstring("invalid FEN: "));
}
