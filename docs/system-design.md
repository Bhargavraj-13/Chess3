# System design

Status: **draft for team review** (issue #1). Once Anil and Supritha approve it, the `Rules` interface and the message list are frozen for V0. Any later change to them needs team agreement.

## 1. Goals and non-goals

We're building a basic two-player chess website. One player creates a game, the other joins with a challenge code, and they play classic chess (V0). Three variants (V1-V3) are added afterwards without editing the core.

Not in scope: accounts, a database, ratings, clocks, puzzles, premoves, spectators, and random matchmaking.

## 2. Architecture

```text
Browser (React + TypeScript)
   │  JSON over WebSocket (wss://<domain>/ws)
   ▼
Caddy  ── serves frontend/dist, proxies /ws and /healthz
   │
   ▼
chess_server (C++20, Boost.Beast + Asio)
   ├── server/    connections, protocol, game registry, timers
   │      │  calls only the Rules interface
   │      ▼
   ├── rules/     Rules interface, ClassicRules
   ├── variants/  DecoyRules, LastFightRules, ShapeshifterRules, registry
   │      │
   │      ▼
   └── core/      board, FEN, attacks, move generation, perft
```

**The server is authoritative.** The browser never decides whether something is legal. After every change, the server sends each player their own view of the game and the complete list of actions that player may take right now. The browser draws the board and lets the player pick from that list.

### Module boundaries

| Module | Contains | May depend on | Must not |
|---|---|---|---|
| `core/` | `Board`, `Square`, `Piece`, `Move`, FEN, attack tables, pseudo-legal and legal move generation, perft | the standard library | know about rules variants, JSON, or the network |
| `rules/` | the `Rules` interface, `ClassicRules`, `PlayerView`, `Action`, `Outcome` | `core/` | know about the network or JSON |
| `variants/` | one folder per variant, plus `registry.cpp` | `core/`, `rules/` | edit files in `core/` or `rules/` |
| `server/` | listener, WebSocket sessions, JSON protocol, `Game`, `GameRegistry`, secure random, timers | everything above, Boost | contain chess rules |
| `frontend/` | pages, board UI, WebSocket client | the protocol in section 5 | contain chess rules |

### Repository layout

```text
CMakeLists.txt            include(CTest), targets below
src/core/                 library chess_core
src/rules/                library chess_rules
src/variants/             library chess_variants (one folder per variant + registry.cpp)
src/server/               executable chess_server (install rule)
tests/core/               unit tests and perft (perft tests have the "perft" label)
tests/rules/              ClassicRules tests (draws, results)
tests/variants/<name>/    hand-checked positions and saved regression counts
tests/server/             protocol and session tests
frontend/                 Vite app, npm, builds to frontend/dist
```

### Libraries

- **Boost.Beast and Asio** for HTTP and WebSocket. If this is too slow to work with after one day, fall back to uWebSockets. Only `server/` changes in that case.
- **Boost.JSON** for messages. Use it header-only by including `<boost/json/src.hpp>` in exactly one `.cpp` file. CI and the server image only install `libboost-dev`, so no new dependency is needed.
- **Secure random:** `getrandom()` on Linux, wrapped in `server/secure_random.hpp`. Don't use `std::random_device` or `rand()` for codes or tokens.

## 3. Core engine

The core engine provides the building blocks, and `ClassicRules` combines them. Variants reuse the same blocks.

```cpp
enum class Color : uint8_t { White, Black };
enum class PieceType : uint8_t { Pawn, Knight, Bishop, Rook, Queen, King };
struct Piece { Color color; PieceType type; };   // `type` is how the piece MOVES
using Square = uint8_t;                          // 0 = a1 ... 63 = h8

struct Move {
  Square from, to;
  std::optional<PieceType> promotion;
  // Flags are filled in by the generator: capture, en passant, castle, double push.
};

class Position {                                 // board + side to move + castling + ep + clocks
 public:
  static Position from_fen(std::string_view);    // throws FenError, saying what is wrong
  std::string to_fen() const;
  // Builders that variants need:
  std::optional<Piece> at(Square) const;
  void set(Square, std::optional<Piece>);
  void set_side_to_move(Color);
  void clear_en_passant();
  void clear_castling(Color);
  // ...
};

bool is_attacked(const Position&, Square, Color by);         // uses the piece types on the board
std::vector<Move> pseudo_legal_moves(const Position&);
std::vector<Move> legal_moves(const Position&);              // king must not be attacked afterwards
Position apply_move(const Position&, const Move&);           // returns a new position
uint64_t perft(const Position&, int depth);
```

Two decisions here matter for the variants:

1. **The board stores how a piece moves, not what it really is.** In V1 the hidden real king sits on the board as, for example, a rook. The variant tracks its true identity separately. The core engine needs no "apparent type" field.
2. **King safety is a separate function.** `legal_moves` is "pseudo-legal moves that leave the king unattacked". A variant with a different safety rule, such as V1 protecting two royal pieces, writes its own filter with `pseudo_legal_moves` and `is_attacked`.

`apply_move` returns a new `Position` instead of using make and unmake. This is simpler and harder to get wrong. Perft to depth 5 is about 5 million positions, which is still fast enough. We'll only switch to make and unmake if perft becomes too slow for CI.

## 4. The Rules interface

One `Rules` object holds the whole state of one game, including any variant state. The server owns one per game and talks to it through this interface only.

```cpp
// rules/rules.hpp
struct Action {
  std::string kind;                         // "move", "claim_draw", "decoy_setup", "king_boost", "recolor", "transform"
  std::vector<Square> squares;              // meaning depends on kind, see table below
  std::optional<PieceType> piece;           // promotion piece, or transform target
  bool operator==(const Action&) const = default;
};

struct ViewPiece {
  Square square;
  Color color;
  PieceType type;                           // how it moves, as this viewer sees it
  std::optional<PieceType> true_type;       // V1 only: sent to the owner while the swap is secret
};

struct PlayerView {
  std::vector<ViewPiece> pieces;
  std::optional<Action> last_action;        // as this viewer may see it (hidden setup actions are omitted)
  std::vector<Color> in_check;              // only checks this viewer is allowed to know about
  boost::json::object extra;                // variant status, e.g. boost used, endgame on
};
// Note: `extra` uses boost::json so variants don't need to change the protocol code.
// It's the one place rules/ touches JSON, and it's plain data with no network code.

struct Outcome {
  std::optional<Color> winner;              // nullopt = draw
  std::string reason;                       // "checkmate", "stalemate", "fivefold_repetition", ...
};

class Rules {
 public:
  virtual ~Rules() = default;

  // Who may act now. One color in normal play, both during V1 setup, none when finished.
  virtual std::vector<Color> to_act() const = 0;

  // Every action `player` may take now. Empty if it isn't their turn.
  virtual std::vector<Action> legal_actions(Color player) const = 0;

  // Apply an action. Returns an error code instead of applying it if the action is illegal.
  virtual std::optional<std::string> apply(Color player, const Action&) = 0;

  // Everything `viewer` may see. This is the ONLY game data that leaves the server.
  virtual PlayerView view(Color viewer) const = 0;

  // Set once the rules end the game: checkmate, stalemate, automatic draw, accepted claim.
  virtual std::optional<Outcome> outcome() const = 0;
};

// variants/registry.cpp. Adding a variant means adding one line here.
std::unique_ptr<Rules> make_rules(std::string_view variant);   // "classic", "decoy", "last_fight", "shapeshifter"
```

`ClassicRules` implements this interface for V0. Variants subclass `ClassicRules` and override the parts they change. `ClassicRules` exposes protected helpers for this, such as `position()`, `history()`, `classic_moves()` and `check_classic_outcome()`.

### Action kinds

| Kind | `squares` | `piece` | Used by |
|---|---|---|---|
| `move` | `[from, to]` | promotion piece, if any | all |
| `claim_draw` | `[]` | none | all |
| `decoy_setup` | `[]` to keep the normal setup, or `[square]` to swap the king with the piece on that square | none | V1 |
| `king_boost` | `[from, middle, to]`, both king moves declared together | none | V2 |
| `recolor` | `[from, to]`, a bishop's one orthogonal step to an empty square | none | V3 |
| `transform` | `[square]` | the new type, `Knight` or `Bishop` | V3 |

`apply` checks that the action appears in `legal_actions(player)`. This is slower than a dedicated check, but it means legality is defined in only one place.

**Resigning and draw offers are not in `Rules`.** They work the same in every variant, so `server/Game` handles them and records the `Outcome` itself. Claiming a draw *is* in `Rules`, because whether a position repeated depends on the rules' state.

### Results and draws in ClassicRules (V0)

| Ends the game | How |
|---|---|
| Checkmate, stalemate | automatic |
| Fivefold repetition | automatic |
| 75-move rule (150 half-moves without a capture or pawn move) | automatic, unless that last move gave checkmate |
| Insufficient material: K v K, K+B v K, K+N v K, K+B v K+B with bishops on the same color | automatic |
| Threefold repetition | the player to move may send `claim_draw` |
| 50-move rule (100 half-moves) | the player to move may send `claim_draw` |
| Resignation | either player, at any time during play (handled by `Game`) |
| Draw by agreement | handled by `Game`, see section 6 |
| Abandonment | `Game`, when the reconnect window runs out (section 6) |

A **position key** for repetition is the FEN without the two move counters. It includes the en passant square only when an en passant capture is actually legal. Variants add their own state to this key (section 8).

Simplification: FIDE also lets a player claim a draw by announcing the move that would cause the repetition. We don't support that. The player makes the move, then claims on their next turn.

## 5. WebSocket protocol

- Endpoint: `/ws`, on the same port as `GET /healthz`, which returns `200 ok`. The port comes from the `PORT` environment variable.
- Each message is one JSON text frame: an object with a `"type"` field. Squares are strings like `"e4"`. Colors are `"white"` and `"black"`. Piece types are `"pawn"`, `"knight"`, `"bishop"`, `"rook"`, `"queen"` and `"king"`.
- The maximum message size is 4 KB. Larger messages close the connection.
- There's no protocol versioning, because the server and frontend are always deployed together.
- **Never send a FEN to the client.** In V1 it would reveal the swap. The client gets `pieces` only.

### Client to server

| Type | Fields | Allowed when |
|---|---|---|
| `create_game` | `variant` (`"classic"` in V0) | not in a game |
| `join_game` | `code` | not in a game |
| `reconnect` | `player_token` | not in a game |
| `act` | `seq`, `action` | your turn |
| `resign` | none | game in progress |
| `offer_draw` | none | game in progress, no offer pending, not offered since your last action |
| `accept_draw`, `decline_draw` | none | the opponent has a draw offer pending |

The `action` object has the same fields as the C++ struct: `{"kind": "move", "squares": ["e7", "e8"], "piece": "queen"}`. `seq` is the number from the latest `state` the client received. If it's out of date, the server rejects the action with `stale_state`. This stops a double click, or an action sent just as the opponent moved, from being applied to the wrong position.

### Server to client

| Type | Fields | Sent when |
|---|---|---|
| `game_created` | `code`, `player_token` | after `create_game` |
| `game_joined` | `player_token` | after a successful `join_game` |
| `state` | see below | after joining, reconnecting, and every change to the game |
| `game_over` | `winner` (or `null`), `reason` | the game ends. A final `state` is also sent |
| `error` | `code`, `message`, `in_reply_to` | a request fails |

Example `state`, sent to White in V0:

```json
{
  "type": "state",
  "seq": 12,
  "variant": "classic",
  "phase": "playing",
  "you": "white",
  "to_act": ["white"],
  "pieces": [{"square": "e1", "color": "white", "type": "king"}, "..."],
  "last_action": {"kind": "move", "squares": ["e7", "e5"]},
  "in_check": [],
  "legal_actions": [{"kind": "move", "squares": ["e2", "e4"]}, "..."],
  "draw_offer": null,
  "opponent_connected": true,
  "reconnect_deadline_ms": null,
  "extra": {}
}
```

- `phase` is `"waiting"` (no opponent yet), `"setup"` (V1), `"playing"`, or `"finished"`.
- `legal_actions` is empty when it isn't your turn.
- `reconnect_deadline_ms` is set while the opponent is disconnected. It's the number of milliseconds left in their 30-second window, which the client shows as a countdown.

### How the client uses `legal_actions` without knowing chess

- **Click a piece:** highlight the `to` square of every `move` action whose `from` is that square.
- **Click a target:** if several actions share the same `from` and `to`, they differ only by `piece`, so show a promotion picker.
- **Other kinds:** show a button for each other action kind, such as "Claim draw" or "KingBoost". Pressing it filters the clicks to that kind's actions. A boost is two clicks, chosen from the actions that start with the first square.

The frontend keeps one small table of button labels per kind. It has no move logic.

### Error codes

`bad_message`, `unknown_type`, `invalid_code`, `game_full`, `invalid_token`, `not_your_turn`, `illegal_action`, `stale_state`, `game_over`, `no_draw_offer`, `rate_limited`, `internal`.

The `message` text is for humans only. Errors go only to the player who caused them, and must never include the opponent's hidden state, a code or a token.

## 6. Games, codes, tokens and reconnecting

### Game lifecycle

```text
create_game ──► waiting ──join──► setup (V1 only) ──► playing ──► finished ──(60 s)──► deleted
                  │
                  └── no one joins in 10 minutes ──► deleted
```

- **Colors** are assigned randomly, using the secure random source, when the second player joins. The first `state` tells each player their color.
- **Challenge code:** 8 characters from the Crockford base32 alphabet (no I, L, O or U), which is 40 bits from `getrandom()`. It's used for joining only, and is removed from the registry as soon as the second player joins.
- **Player token:** 128 random bits, base64url-encoded, one per player. It's the only way to reconnect.
- **Guessing codes:** a connection that sends 5 wrong codes is closed. With 40 bits and that limit, guessing a code is not practical.
- **Never log codes or tokens**, and never put them in a URL query string. The frontend keeps the token in `sessionStorage`, so a page refresh reconnects automatically. A share link can put the code in the URL fragment (`/join#K7Q2M9XD`), because browsers never send the fragment to the server.

### Draw offers

- An offer stays pending until the opponent accepts, declines, or either player makes an action. Any of these clears it.
- A player can't offer again until they have made an action since their last offer. This prevents spam.

### Disconnecting and reconnecting

- Beast's WebSocket timeouts detect a dead connection. We set the idle timeout to 10 seconds with keep-alive pings, so a lost connection is noticed within about 10 seconds.
- When a player disconnects during `setup` or `playing`, their 30-second timer starts, and the opponent gets a `state` with `opponent_connected: false` and the deadline.
- `reconnect` with a valid token cancels the timer and sends both players a fresh `state`. If that token already has a live connection, the old one is closed, so the newest tab wins.
- **When the timer runs out:** the player who left loses (`reason: "abandoned"`). If both players are disconnected at that point, the game ends as a draw with no winner. In `waiting`, the creator's timer just deletes the game.

### Threading

For V0, the server runs one `io_context` on **one thread**. Every game is only ever touched from that thread, so `Game` and `GameRegistry` need no locks. A move takes microseconds to process, so one thread is plenty for this site.

If we need more threads later, each `Game` gets its own Asio strand and the registry gets a mutex. The TSan CI job is already set up for that change.

## 7. Server classes

```text
Listener        accepts TCP, routes GET /healthz and the /ws upgrade
WsSession       one per connection: read loop, write queue, timeouts, rate limit
Protocol        JSON <-> structs, validation, error replies (no game logic)
GameRegistry    code -> Game, token -> (Game, Color); creates and deletes games
Game            phase, two player slots, Rules, seq, draw offer, reconnect timers,
                builds a `state` per player from Rules::view + legal_actions
```

Beast allows only one write at a time on a WebSocket, so `WsSession` keeps a write queue.

## 8. Checking the interface against the variants

### Requirements from issue #1

| Need | Where the design provides it |
|---|---|
| Actions beyond normal moves (V2, V3) | `Action.kind`, with `king_boost`, `recolor` and `transform` |
| The rules decide who moves next (V2) | `Rules::to_act()` |
| Extra state per player (V1, V2, V3) | the variant's `Rules` subclass holds it; `PlayerView.extra` shows what each player may see |
| A separate view per player (V1) | `Rules::view(viewer)` is the only output. The server never sends a FEN |
| A setup phase (V1) | `to_act()` returns both colors, and `decoy_setup` actions are offered until both have chosen |
| Apparent type separate from true type (V1) | the board stores the apparent type; the variant stores the true identity; `ViewPiece.true_type` goes to the owner only |

### V1 Decoy Chess

- **Setup:** each player gets 9 `decoy_setup` actions: keep, or swap with d1, a1, h1, c1, f1, b1 or g1 (mirrored for Black). Players choose in any order, and the opponent only sees "opponent ready". The game starts with White to move once both have chosen.
- **State:** for each color, the square of the real king, the square of the decoy, and whether the swap is revealed. Both squares are updated when those pieces move.
- **Legal moves:** start from `pseudo_legal_moves`, which moves pieces by apparent type. Keep a move only if neither the real king nor the decoy is attacked afterwards. Castling uses the apparent king and rook.
- **Reveal:** after every action, check whether either player's real king or decoy is attacked. If so, set that player's revealed flag and put the true types back on the board. If it was the decoy that was attacked, its owner is not in check.
- **Results:** checkmate counts for the real king only. **This needs a decision from Bhargav:** the classic insufficient-material check counts by type, and before a reveal the visible types aren't the true ones. Proposal: no automatic insufficient-material draw while either swap is still hidden. Repetition still works, because the swap never changes after setup.
- **Leak risks, each with a test:**
  - `view(opponent)` must never contain `true_type`, the real king's square, or a hidden setup action.
  - `legal_actions(opponent)` must be the same whatever my swap is. This holds because the opponent's legal moves only depend on how my pieces move, which is public. A test runs every setup choice and checks the opponent's actions are equal.
  - `in_check` and error messages must not reveal anything before the reveal.

### V2 Chess Last Fight

- **Boost:** both moves are declared together in one `king_boost` action, so the opponent's skipped turn happens inside a single `apply`. `to_act()` then returns the opponent as usual. The interface would also allow a two-step boost if Anil prefers one.
- **Generating boosts:** only while in check and the boost is unused. For each legal king move, apply it, keep the same side to move, and clear en passant (the opponent's skipped turn uses up their en passant right). Then, for each legal king move from there with no castling, add `[from, middle, to]`. A first move with no legal second move gives no action. That's at most 64 actions, which is cheap.
- **State:** a "boost used" flag per color, shown to both players in `extra`. The flag is part of the position key, because a position where a boost is still available offers different moves.
- **Results:** checked once, after the whole action. The half-move clock goes up by 2, unless either move captured.
- The proposed details in #19 match this design. Anil still needs to confirm them.

### V3 Chess Shapeshifter

- **Endgame flag:** after every action, if there are no queens and each side has at most 8 points of non-pawn material, set the flag. It never resets. It's shown to both players in `extra`, along with which powers have been used.
- **Generating actions:** while not in check, in the endgame, with the power unused, the player gets:
  - `recolor` for each bishop's empty orthogonal neighbor
  - `transform` for each knight and bishop

  Drop any action that leaves your own king attacked or attacks the opponent's king, which also covers discovered checks.
- **Draw rules affected:**
  - **Insufficient material:** K+B v K+B with same-colored bishops can't end in checkmate in chess, but a recolor makes it opposite-colored, and then checkmate is possible. So `ShapeshifterRules` must not call that position a dead draw while either player can still recolor. K+N v K and K+B v K stay dead draws, because a lone minor piece still can't give checkmate.
  - **Clocks and repetition:** using a power counts as a move for the 50- and 75-move rules, since it isn't a capture or a pawn move. The "power used" flags and the endgame flag are part of the position key.

### Result of the check

All three variants fit the interface without changing `core/` or `rules/`, **provided `ClassicRules` makes these overridable** from the start:

- the position key
- insufficient material
- the outcome check
- move generation

## 9. Testing

- **Core:** perft from the start position to depth 5, plus the standard tricky positions (Kiwipete and positions 3-6 from the Chess Programming Wiki). Check the counts in #8 against the wiki before using them.
- **ClassicRules:** one test per row of the results table in section 4, including the en passant edge case in the position key.
- **Variants:** hand-checked positions, saved regression counts, and the V1 leak tests from section 8.
- **Server:** protocol tests that drive a `Game` directly, without sockets, covering every message, every error code, `stale_state`, draw offers, and the reconnect timer.

## 10. Pipeline conventions

This design keeps every convention in [ci-cd.md](ci-cd.md) unchanged:

- the `chess_server` target, with an install rule
- the `PORT` environment variable
- `GET /healthz`
- `/ws`
- the `perft` CTest label
- `frontend/`, using npm and building to `frontend/dist`

## 11. Decisions

| Decision | Status |
|---|---|
| Resigning and draw offers in V0 | decided |
| Colors assigned randomly when the opponent joins | decided |
| Automatic draws: fivefold, 75-move, insufficient material. Claimable: threefold, 50-move | decided |
| V1: no automatic insufficient-material draw while a swap is hidden | proposed, Bhargav to decide |
| V2: details in #19 | proposed, Anil to confirm |
| V3: insufficient-material exception while a recolor is available | proposed, Supritha to confirm |
| One server thread in V0 | proposed, team to review |
| Waiting games expire after 10 minutes, finished games are kept for 60 seconds | proposed, team to review |
