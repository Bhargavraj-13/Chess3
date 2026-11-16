#include "core/types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using chess::core::Color;
using chess::core::file_of;
using chess::core::make_square;
using chess::core::opposite;
using chess::core::parse_square;
using chess::core::rank_of;
using chess::core::Square;
using chess::core::square_name;

TEST_CASE("squares are numbered from a1 to h8", "[core]") {
    CHECK(make_square(0, 0) == 0);   // a1
    CHECK(make_square(7, 0) == 7);   // h1
    CHECK(make_square(0, 1) == 8);   // a2
    CHECK(make_square(4, 3) == 28);  // e4
    CHECK(make_square(7, 7) == 63);  // h8
    CHECK(file_of(28) == 4);
    CHECK(rank_of(28) == 3);
}

TEST_CASE("square names convert both ways", "[core]") {
    CHECK(parse_square("a1") == Square{0});
    CHECK(parse_square("e4") == Square{28});
    CHECK(parse_square("h8") == Square{63});
    CHECK(square_name(28) == "e4");

    for (int s = 0; s < chess::core::kNumSquares; ++s) {
        const auto square = static_cast<Square>(s);
        CHECK(parse_square(square_name(square)) == square);
    }
}

TEST_CASE("invalid square names are rejected", "[core]") {
    const auto name = GENERATE("", "e", "e44", "i1", "a0", "a9", "E4", "4e", " e4");
    CAPTURE(name);
    CHECK_FALSE(parse_square(name).has_value());
}

TEST_CASE("opposite swaps the colors", "[core]") {
    CHECK(opposite(Color::White) == Color::Black);
    CHECK(opposite(Color::Black) == Color::White);
}
