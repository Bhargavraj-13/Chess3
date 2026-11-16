#include "server/protocol.hpp"

#include <boost/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <variant>

using namespace chess::server;

namespace {

// Parses text that must be valid, and returns the message as type T.
template <typename T>
T parse_as(const char* text) {
    ParseResult result = parse_client_message(text);
    REQUIRE(std::holds_alternative<ClientMessage>(result));
    ClientMessage& message = std::get<ClientMessage>(result);
    REQUIRE(std::holds_alternative<T>(message));
    return std::get<T>(message);
}

// Parses text that must be rejected, and returns why.
ProtocolError parse_error(const char* text) {
    ParseResult result = parse_client_message(text);
    REQUIRE(std::holds_alternative<ProtocolError>(result));
    return std::get<ProtocolError>(result);
}

}  // namespace

TEST_CASE("messages with fields are parsed", "[server]") {
    CHECK(parse_as<CreateGame>(R"({"type":"create_game","variant":"classic"})").variant ==
          "classic");
    CHECK(parse_as<JoinGame>(R"({"type":"join_game","code":"K7Q2M9XD"})").code == "K7Q2M9XD");
    CHECK(parse_as<Reconnect>(R"({"type":"reconnect","player_token":"abc"})").player_token ==
          "abc");
}

TEST_CASE("messages without fields are parsed", "[server]") {
    parse_as<Resign>(R"({"type":"resign"})");
    parse_as<OfferDraw>(R"({"type":"offer_draw"})");
    parse_as<AcceptDraw>(R"({"type":"accept_draw"})");
    parse_as<DeclineDraw>(R"({"type":"decline_draw"})");
}

TEST_CASE("act is parsed with its action", "[server]") {
    const Act act = parse_as<Act>(
        R"({"type":"act","seq":12,"action":{"kind":"move","squares":["e7","e8"],"piece":"queen"}})");
    CHECK(act.seq == 12);
    CHECK(act.action.kind == "move");
    CHECK(act.action.squares == std::vector<std::string>{"e7", "e8"});
    CHECK(act.action.piece == "queen");

    const Act claim = parse_as<Act>(R"({"type":"act","seq":0,"action":{"kind":"claim_draw"}})");
    CHECK(claim.action.squares.empty());
    CHECK_FALSE(claim.action.piece.has_value());
}

TEST_CASE("unknown extra fields are ignored", "[server]") {
    parse_as<Resign>(R"({"type":"resign","please":true})");
}

TEST_CASE("text that is not a JSON object is a bad_message", "[server]") {
    CHECK(parse_error("").code == "bad_message");
    CHECK(parse_error("hello").code == "bad_message");
    CHECK(parse_error("[1,2]").code == "bad_message");
    CHECK(parse_error(R"({"type":5})").code == "bad_message");
    CHECK(parse_error(R"({"no_type":"resign"})").in_reply_to.empty());
}

TEST_CASE("an unknown type is unknown_type", "[server]") {
    const ProtocolError error = parse_error(R"({"type":"fly_away"})");
    CHECK(error.code == "unknown_type");
    CHECK(error.in_reply_to == "fly_away");
}

TEST_CASE("missing or wrongly typed fields are a bad_message for that type", "[server]") {
    for (const char* text : {
             R"({"type":"create_game"})",
             R"({"type":"join_game","code":42})",
             R"({"type":"reconnect","player_token":null})",
             R"({"type":"act","action":{"kind":"move"}})",
             R"({"type":"act","seq":-1,"action":{"kind":"move"}})",
             R"({"type":"act","seq":1.5,"action":{"kind":"move"}})",
             R"({"type":"act","seq":"1","action":{"kind":"move"}})",
             R"({"type":"act","seq":1})",
             R"({"type":"act","seq":1,"action":"e2e4"})",
             R"({"type":"act","seq":1,"action":{"squares":["e2","e4"]}})",
             R"({"type":"act","seq":1,"action":{"kind":"move","squares":"e2"}})",
             R"({"type":"act","seq":1,"action":{"kind":"move","squares":["e9"]}})",
             R"({"type":"act","seq":1,"action":{"kind":"move","squares":["E2"]}})",
             R"({"type":"act","seq":1,"action":{"kind":"move","squares":[5]}})",
             R"({"type":"act","seq":1,"action":{"kind":"move","piece":"dragon"}})",
         }) {
        INFO(text);
        const ProtocolError error = parse_error(text);
        CHECK(error.code == "bad_message");
        CHECK_FALSE(error.in_reply_to.empty());
    }
}

TEST_CASE("type_name gives each message's type", "[server]") {
    CHECK(type_name(CreateGame{}) == "create_game");
    CHECK(type_name(Act{}) == "act");
    CHECK(type_name(DeclineDraw{}) == "decline_draw");
}

TEST_CASE("error_json builds the error message", "[server]") {
    const auto with_type = boost::json::parse(error_json({"bad_message", "oops", "act"}));
    CHECK(with_type.at("type") == "error");
    CHECK(with_type.at("code") == "bad_message");
    CHECK(with_type.at("message") == "oops");
    CHECK(with_type.at("in_reply_to") == "act");

    const auto without_type = boost::json::parse(error_json({"bad_message", "oops", ""}));
    CHECK(without_type.at("in_reply_to").is_null());
}
