#include "server/protocol.hpp"

#include <algorithm>
#include <array>
#include <boost/json.hpp>
#include <limits>

namespace chess::server {

namespace json = boost::json;

namespace {

constexpr std::array<std::string_view, 6> kPieceNames = {"pawn", "knight", "bishop",
                                                         "rook", "queen",  "king"};

// A failed check inside a parser. Thrown and caught only in this file, so a
// parser can stop at the first problem without checking every return value.
struct Invalid {
    std::string message;
};

const json::value& required(const json::object& object, std::string_view key) {
    const json::value* value = object.if_contains(key);
    if (value == nullptr) {
        throw Invalid{"missing field \"" + std::string(key) + "\""};
    }
    return *value;
}

std::string required_string(const json::object& object, std::string_view key) {
    const json::value& value = required(object, key);
    if (!value.is_string()) {
        throw Invalid{"field \"" + std::string(key) + "\" must be a string"};
    }
    return std::string(value.get_string());
}

bool is_square(std::string_view text) {
    return text.size() == 2 && text[0] >= 'a' && text[0] <= 'h' && text[1] >= '1' && text[1] <= '8';
}

ActionData parse_action(const json::value& value) {
    if (!value.is_object()) {
        throw Invalid{"field \"action\" must be an object"};
    }
    const json::object& object = value.get_object();

    ActionData action;
    action.kind = required_string(object, "kind");

    if (const json::value* squares = object.if_contains("squares")) {
        if (!squares->is_array()) {
            throw Invalid{"field \"squares\" must be an array"};
        }
        for (const json::value& square : squares->get_array()) {
            if (!square.is_string() || !is_square(square.get_string())) {
                throw Invalid{"each square must be a string from \"a1\" to \"h8\""};
            }
            action.squares.emplace_back(square.get_string());
        }
    }

    if (const json::value* piece = object.if_contains("piece"); piece && !piece->is_null()) {
        if (!piece->is_string() || std::find(kPieceNames.begin(), kPieceNames.end(),
                                             piece->get_string()) == kPieceNames.end()) {
            throw Invalid{"field \"piece\" must be a piece name such as \"queen\""};
        }
        action.piece = std::string(piece->get_string());
    }
    return action;
}

Act parse_act(const json::object& object) {
    const json::value& seq = required(object, "seq");
    // JSON has one number type. Accept whole, non-negative numbers only.
    if (seq.is_uint64() && seq.get_uint64() <= std::numeric_limits<std::int64_t>::max()) {
        return Act{static_cast<std::int64_t>(seq.get_uint64()),
                   parse_action(required(object, "action"))};
    }
    if (seq.is_int64() && seq.get_int64() >= 0) {
        return Act{seq.get_int64(), parse_action(required(object, "action"))};
    }
    throw Invalid{"field \"seq\" must be a whole number, 0 or more"};
}

// Returns nullopt if `type` is not a message type we know.
std::optional<ClientMessage> parse_by_type(std::string_view type, const json::object& object) {
    if (type == "create_game") return CreateGame{required_string(object, "variant")};
    if (type == "join_game") return JoinGame{required_string(object, "code")};
    if (type == "reconnect") return Reconnect{required_string(object, "player_token")};
    if (type == "act") return parse_act(object);
    if (type == "resign") return Resign{};
    if (type == "offer_draw") return OfferDraw{};
    if (type == "accept_draw") return AcceptDraw{};
    if (type == "decline_draw") return DeclineDraw{};
    return std::nullopt;
}

}  // namespace

ParseResult parse_client_message(std::string_view text) {
    boost::system::error_code error;
    const json::value value = json::parse(text, error);
    if (error) {
        return ProtocolError{"bad_message", "message is not valid JSON", ""};
    }
    if (!value.is_object()) {
        return ProtocolError{"bad_message", "message must be a JSON object", ""};
    }
    const json::object& object = value.get_object();

    const json::value* type = object.if_contains("type");
    if (type == nullptr || !type->is_string()) {
        return ProtocolError{"bad_message", "message needs a \"type\" string", ""};
    }
    const std::string_view type_text = type->get_string();

    try {
        std::optional<ClientMessage> message = parse_by_type(type_text, object);
        if (!message) {
            return ProtocolError{"unknown_type", "unknown message type", std::string(type_text)};
        }
        return std::move(*message);
    } catch (const Invalid& invalid) {
        return ProtocolError{"bad_message", invalid.message, std::string(type_text)};
    }
}

std::string_view type_name(const ClientMessage& message) {
    // The order matches the order of types in ClientMessage.
    constexpr std::array<std::string_view, std::variant_size_v<ClientMessage>> kNames = {
        "create_game", "join_game",  "reconnect",   "act",
        "resign",      "offer_draw", "accept_draw", "decline_draw"};
    return kNames[message.index()];
}

std::string error_json(const ProtocolError& error) {
    json::object object;
    object["type"] = "error";
    object["code"] = error.code;
    object["message"] = error.message;
    if (error.in_reply_to.empty()) {
        object["in_reply_to"] = nullptr;
    } else {
        object["in_reply_to"] = error.in_reply_to;
    }
    return json::serialize(object);
}

}  // namespace chess::server
