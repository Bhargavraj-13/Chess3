#include "server/config.hpp"

#include <catch2/catch_test_macros.hpp>

using chess::server::parse_port;

TEST_CASE("parse_port accepts ports from 1 to 65535", "[server]") {
    CHECK(parse_port("1") == 1);
    CHECK(parse_port("8080") == 8080);
    CHECK(parse_port("65535") == 65535);
}

TEST_CASE("parse_port rejects anything else", "[server]") {
    CHECK_FALSE(parse_port(""));
    CHECK_FALSE(parse_port("0"));
    CHECK_FALSE(parse_port("65536"));
    CHECK_FALSE(parse_port("-1"));
    CHECK_FALSE(parse_port("80x"));
    CHECK_FALSE(parse_port(" 80"));
    CHECK_FALSE(parse_port("abc"));
    CHECK_FALSE(parse_port("99999999999999999999"));
}
