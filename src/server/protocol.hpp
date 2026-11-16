#pragma once

// The messages a browser may send, as C++ types, and the function that turns
// raw WebSocket text into one of them. The full list, with meanings, is in
// docs/system-design.md, section 5.
//
// This file only checks the SHAPE of a message: is it JSON, is "type" known,
// does each field have the right JSON type. Whether a message makes sense right
// now (is it your turn, is the code real) is decided later, by the game code (#11).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace chess::server {

struct CreateGame {
    std::string variant;
};
struct JoinGame {
    std::string code;
};
struct Reconnect {
    std::string player_token;
};
struct ActionData {
    std::string kind;                  // "move", "claim_draw", ...
    std::vector<std::string> squares;  // each one "a1" to "h8"
    std::optional<std::string> piece;  // "pawn", "knight", ... "king"
};
struct Act {
    std::int64_t seq = 0;
    ActionData action;
};
struct Resign {};
struct OfferDraw {};
struct AcceptDraw {};
struct DeclineDraw {};

// Exactly one of the messages above. std::variant is a type-safe "one of these".
using ClientMessage =
    std::variant<CreateGame, JoinGame, Reconnect, Act, Resign, OfferDraw, AcceptDraw, DeclineDraw>;

// Why a message was rejected. Sent back as {"type": "error", ...}.
struct ProtocolError {
    std::string code;         // "bad_message" or "unknown_type"
    std::string message;      // for humans only
    std::string in_reply_to;  // the message's "type", or "" if there wasn't a usable one
};

// Either the message, or why it was rejected.
using ParseResult = std::variant<ClientMessage, ProtocolError>;

ParseResult parse_client_message(std::string_view text);

// The "type" string of a message, such as "create_game".
std::string_view type_name(const ClientMessage& message);

// {"type":"error","code":...,"message":...,"in_reply_to":...} as text, ready to send.
std::string error_json(const ProtocolError& error);

}  // namespace chess::server
