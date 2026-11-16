#include "core/position.hpp"

#include <charconv>
#include <string>
#include <system_error>
#include <vector>

namespace chess::core {
namespace {

[[noreturn]] void fail(const std::string& reason) { throw FenError("invalid FEN: " + reason); }

std::string quoted(std::string_view text) { return "\"" + std::string(text) + "\""; }

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        if (end == std::string_view::npos) {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
}

std::optional<Piece> piece_from_letter(char letter) {
    const Color color = (letter >= 'A' && letter <= 'Z') ? Color::White : Color::Black;
    const char lower = color == Color::White ? static_cast<char>(letter - 'A' + 'a') : letter;
    switch (lower) {
        case 'p':
            return Piece{color, PieceType::Pawn};
        case 'n':
            return Piece{color, PieceType::Knight};
        case 'b':
            return Piece{color, PieceType::Bishop};
        case 'r':
            return Piece{color, PieceType::Rook};
        case 'q':
            return Piece{color, PieceType::Queen};
        case 'k':
            return Piece{color, PieceType::King};
        default:
            return std::nullopt;
    }
}

char piece_letter(Piece piece) {
    char lower = 'p';
    switch (piece.type) {
        case PieceType::Pawn:
            lower = 'p';
            break;
        case PieceType::Knight:
            lower = 'n';
            break;
        case PieceType::Bishop:
            lower = 'b';
            break;
        case PieceType::Rook:
            lower = 'r';
            break;
        case PieceType::Queen:
            lower = 'q';
            break;
        case PieceType::King:
            lower = 'k';
            break;
    }
    return piece.color == Color::White ? static_cast<char>(lower - 'a' + 'A') : lower;
}

std::string color_name(Color color) { return color == Color::White ? "White" : "Black"; }

void parse_board(std::string_view field, Position& position) {
    const std::vector<std::string_view> ranks = split(field, '/');
    if (ranks.size() != 8) {
        fail("the board must have 8 ranks separated by '/', found " + std::to_string(ranks.size()));
    }
    // FEN lists rank 8 first.
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i;
        const std::string rank_label = "rank " + std::to_string(rank + 1);
        int file = 0;
        for (const char c : ranks[static_cast<std::size_t>(i)]) {
            if (c >= '1' && c <= '8') {
                file += c - '0';
            } else if (const std::optional<Piece> piece = piece_from_letter(c)) {
                if (file < 8) {
                    position.set(make_square(file, rank), piece);
                }
                ++file;
            } else {
                fail("unexpected character '" + std::string(1, c) + "' in " + rank_label);
            }
            if (file > 8) {
                fail(rank_label + " has more than 8 squares");
            }
        }
        if (file != 8) {
            fail(rank_label + " has " + std::to_string(file) + " squares, expected 8");
        }
    }
}

Color parse_side_to_move(std::string_view field) {
    if (field == "w") {
        return Color::White;
    }
    if (field == "b") {
        return Color::Black;
    }
    fail("the side to move must be 'w' or 'b', found " + quoted(field));
}

CastlingRights parse_castling(std::string_view field) {
    CastlingRights rights;
    if (field == "-") {
        return rights;
    }
    for (const char c : field) {
        bool* right = nullptr;
        switch (c) {
            case 'K':
                right = &rights.white_kingside;
                break;
            case 'Q':
                right = &rights.white_queenside;
                break;
            case 'k':
                right = &rights.black_kingside;
                break;
            case 'q':
                right = &rights.black_queenside;
                break;
            default:
                fail("castling rights must be '-' or letters from \"KQkq\", found " +
                     quoted(field));
        }
        if (*right) {
            fail("castling rights contain '" + std::string(1, c) + "' twice");
        }
        *right = true;
    }
    return rights;
}

std::optional<Square> parse_en_passant(std::string_view field) {
    if (field == "-") {
        return std::nullopt;
    }
    const std::optional<Square> square = parse_square(field);
    if (!square) {
        fail("the en passant square must be '-' or a square such as \"e3\", found " +
             quoted(field));
    }
    return square;
}

int parse_counter(std::string_view field, const std::string& name, int minimum) {
    int value = 0;
    const char* end = field.data() + field.size();
    const auto [ptr, error] = std::from_chars(field.data(), end, value);
    if (error == std::errc::result_out_of_range) {
        fail("the " + name + " is too large, found " + quoted(field));
    }
    if (error != std::errc{} || ptr != end) {
        fail("the " + name + " must be a whole number, found " + quoted(field));
    }
    if (value < minimum) {
        fail("the " + name + " must be at least " + std::to_string(minimum) + ", found " +
             quoted(field));
    }
    return value;
}

// Checks the rules a position must follow so move generation can rely on them.
void validate(const Position& position) {
    for (const Color color : {Color::White, Color::Black}) {
        int kings = 0;
        for (int s = 0; s < kNumSquares; ++s) {
            if (position.at(static_cast<Square>(s)) == Piece{color, PieceType::King}) {
                ++kings;
            }
        }
        if (kings != 1) {
            fail(color_name(color) + " must have exactly one king, found " + std::to_string(kings));
        }
    }

    for (int file = 0; file < 8; ++file) {
        for (const int rank : {0, 7}) {
            const Square square = make_square(file, rank);
            const std::optional<Piece> piece = position.at(square);
            if (piece && piece->type == PieceType::Pawn) {
                fail("there is a pawn on " + square_name(square) +
                     ", but pawns can't be on the first or last rank");
            }
        }
    }

    // Each castling right needs the king and that rook still on their starting squares.
    struct CastlingCheck {
        bool has_right;
        char letter;
        Color color;
        const char* king_square;
        const char* rook_square;
    };
    const CastlingRights rights = position.castling();
    const CastlingCheck checks[] = {
        {rights.white_kingside, 'K', Color::White, "e1", "h1"},
        {rights.white_queenside, 'Q', Color::White, "e1", "a1"},
        {rights.black_kingside, 'k', Color::Black, "e8", "h8"},
        {rights.black_queenside, 'q', Color::Black, "e8", "a8"},
    };
    for (const CastlingCheck& check : checks) {
        if (!check.has_right) {
            continue;
        }
        const bool king_there =
            position.at(*parse_square(check.king_square)) == Piece{check.color, PieceType::King};
        const bool rook_there =
            position.at(*parse_square(check.rook_square)) == Piece{check.color, PieceType::Rook};
        if (!king_there || !rook_there) {
            fail("castling right '" + std::string(1, check.letter) + "' needs the " +
                 color_name(check.color) + " king on " + check.king_square + " and a rook on " +
                 check.rook_square);
        }
    }

    // The en passant square is the one the opponent's pawn just skipped over, so
    // that pawn is one square further on, and the square it started from is empty.
    if (const std::optional<Square> ep = position.en_passant()) {
        const Color mover = position.side_to_move();
        const Color pusher = opposite(mover);
        const int expected_rank = mover == Color::White ? 5 : 2;
        const int forward = pusher == Color::White ? 1 : -1;
        const std::string name = square_name(*ep);
        if (rank_of(*ep) != expected_rank) {
            fail("the en passant square " + name + " is impossible with " + color_name(mover) +
                 " to move, it must be on rank " + std::to_string(expected_rank + 1));
        }
        const Square pawn_square = make_square(file_of(*ep), rank_of(*ep) + forward);
        const Square start_square = make_square(file_of(*ep), rank_of(*ep) - forward);
        if (position.at(pawn_square) != Piece{pusher, PieceType::Pawn}) {
            fail("the en passant square " + name + " needs a " + color_name(pusher) + " pawn on " +
                 square_name(pawn_square));
        }
        if (position.at(*ep) || position.at(start_square)) {
            fail("the en passant square " + name + " needs " + name + " and " +
                 square_name(start_square) + " to be empty");
        }
    }
}

}  // namespace

Position Position::from_fen(std::string_view fen) {
    if (fen.empty()) {
        fail("the FEN is empty");
    }
    const std::vector<std::string_view> fields = split(fen, ' ');
    for (const std::string_view field : fields) {
        if (field.empty()) {
            fail("found an empty field, separate the fields with single spaces");
        }
    }
    if (fields.size() != 6) {
        fail("a FEN has 6 fields separated by spaces, found " + std::to_string(fields.size()));
    }

    Position position;
    parse_board(fields[0], position);
    position.side_to_move_ = parse_side_to_move(fields[1]);
    position.castling_ = parse_castling(fields[2]);
    position.en_passant_ = parse_en_passant(fields[3]);
    position.halfmove_clock_ = parse_counter(fields[4], "halfmove clock", 0);
    position.fullmove_number_ = parse_counter(fields[5], "fullmove number", 1);
    validate(position);
    return position;
}

std::string Position::to_fen() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const std::optional<Piece> piece = at(make_square(file, rank));
            if (!piece) {
                ++empty;
                continue;
            }
            if (empty > 0) {
                fen += std::to_string(empty);
                empty = 0;
            }
            fen += piece_letter(*piece);
        }
        if (empty > 0) {
            fen += std::to_string(empty);
        }
        if (rank > 0) {
            fen += '/';
        }
    }

    fen += side_to_move_ == Color::White ? " w " : " b ";

    std::string castling;
    if (castling_.white_kingside) castling += 'K';
    if (castling_.white_queenside) castling += 'Q';
    if (castling_.black_kingside) castling += 'k';
    if (castling_.black_queenside) castling += 'q';
    fen += castling.empty() ? "-" : castling;

    fen += ' ';
    fen += en_passant_ ? square_name(*en_passant_) : "-";
    fen += ' ' + std::to_string(halfmove_clock_) + ' ' + std::to_string(fullmove_number_);
    return fen;
}

void Position::clear_castling(Color color) {
    if (color == Color::White) {
        castling_.white_kingside = false;
        castling_.white_queenside = false;
    } else {
        castling_.black_kingside = false;
        castling_.black_queenside = false;
    }
}

}  // namespace chess::core
