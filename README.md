# Chess3

A C++ chess project built in stages. V0 is the classic game, built together by the team and playable over a network between two players. V1, V2, and V3 are variants, each built by one person on top of the shared core.

## Team

- [Bhargav Raj](https://github.com/Bhargavraj-13)
- [Anil Gehlot](https://github.com/anilgehlotn)
- [Supritha Rajashekar](https://github.com/Sup-ri-tha)

## V0: Classic Chess (networked)

**Description:** Standard chess rules, played over a network between two players. One player creates a game and receives a challenge code. The second player enters that code to join. There is no random matchmaking.

Covers board representation, FEN parsing, legal move generation (including castling, en passant, and promotion), checkmate and stalemate detection, and a networked session for each challenge code.

**Scope:**
- Challenge code generation: short, unique, and generated with a cryptographically secure random source so it can't be guessed
- Join flow: enter the code, connect, start the game
- Move sync between the two clients, with illegal moves rejected by the server
- Disconnect handling: a player who drops has 30 seconds to reconnect using a private player token, otherwise the game ends cleanly

**Owner(s):** [Bhargav Raj](https://github.com/Bhargavraj-13) [Anil Gehlot](https://github.com/anilgehlotn) [Supritha Rajashekar](https://github.com/Sup-ri-tha)


**Status:**

**Acceptance:** Perft counts match published values to the agreed depth. A full game can be played between two clients using a challenge code.

## V1: Decoy Chess

**Description:** Before the game starts, each player can secretly swap their king with another piece. The opponent doesn't know which piece is the real king. Both swapped pieces move according to the square they took, as if the swap never happened. When either the real king or the decoy king is checked, the swap is revealed to the opponent and both pieces move normally again.

**Owner:** Bhargav Raj

**Rule changes:**
- **Setup:** Before move 1, each player may secretly swap their king with a queen, rook, bishop, or knight, or keep the normal setup. Pawns can't be swapped.
- **Before the reveal:** Each swapped piece moves, attacks, gives check, and can deliver checkmate as the piece it appears to be. For example, after swapping the king with the a-file rook, the real king moves like a rook and the real rook moves like a king.
- **Safety:** Until the reveal, neither the real king nor the decoy king may move into attack or be left exposed to attack.
- **Reveal:** A player's swap is revealed to the opponent when their real king or decoy king is attacked. Play continues from the same position, with both pieces moving normally. Each player's swap is revealed separately.
- **After a reveal:** If the decoy king was attacked, it is now an ordinary piece and its owner is not in check. If the real king was attacked, normal check rules apply. Checkmate only applies to the real king.
- **Castling:** Allowed before the reveal if both pieces are in the right positions and haven't moved, using their apparent roles. Not allowed after the reveal.

**Interface hooks used:**

**Status:**

## V2: Chess Last Fight

**Description:** Once per game, a king in check can activate KingBoost to make two moves in a row while the opponent's turn is skipped.

**Owner:** [Anil Gehlot](https://github.com/anilgehlotn)

**Rule changes:**
- **KingBoost:** Each player can use it once per game, only while in check and only if their king has a legal move.
- **Sequence:** The king makes one legal move, the opponent's turn is skipped, and the king makes a second legal move. Only the king can move during a boost.
- **Checkmate:** Unchanged. A checkmated king has no legal first move, so it can't use KingBoost and the player loses.

**Interface hooks used:**

**Status:**

## V3: Chess Shapeshifter

**Description:** Each player gets one endgame power for the whole game: either recolor a bishop or transform a minor piece. Choosing which power to use, and when, is the strategic decision.

**Owner:** [Supritha Rajashekar](https://github.com/Sup-ri-tha)

**Rule changes:**
- **Endgame power:** Each player can use one of the two powers once, choosing which at the moment they use it. Using it takes their turn. Once it's spent, it's gone.
- **Recolor:** After activating shapeshift mode, a bishop moves like a king for one move, to an adjacent square of the opposite color. These are the squares directly above, below, left, and right of it. The square must be empty, so this move can't capture. It then continues as a normal bishop from that square.
- **Transform:** A knight can become a bishop, or a bishop can become a knight, on the same square.
- **Restrictions:** A power can't be used while the player is in check. Neither power can put the opponent's king in check or checkmate, including by opening a line for another piece. A power can't expose the player's own king to check. A power may cause stalemate, which is a draw as normal.
- **Endgame rule:** The power is only available in the endgame. The endgame starts when there are no queens on the board and each side has at most 8 points of non-pawn material (rook 5, bishop and knight 3). Once it starts, it stays on, even if a pawn later promotes to a queen. The 8-point limit may be adjusted after playtesting.

**Playtesting:** Test a wrong-bishop plus rook-pawn ending and an opposite-colored-bishops ending. The power should turn a draw into a win only when the attacker plays precisely, not easily.

**Interface hooks used:**

**Status:**

## Build

### Backend

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

### Frontend

The frontend is a React and TypeScript app in `frontend/`, built with Vite. It needs Node.js 24, the same version CI uses. If you use [nvm](https://github.com/nvm-sh/nvm), run `nvm install` in `frontend/` to get it.

```bash
cd frontend
npm ci        # install the exact versions in package-lock.json
npm run dev   # start the app at http://localhost:5173
```

The dev server reloads the page when you save a file. It forwards `/ws` and `/healthz` to the C++ server on port 8080, as Caddy does in production, so start `chess_server` with `PORT=8080` when you need it.

| Command | What it does |
|---|---|
| `npm run lint` | Checks the code with oxlint and the formatting with Prettier. Warnings fail it |
| `npm run typecheck` | Type-checks the app with TypeScript |
| `npm run format` | Fixes the formatting of every file |
| `npm run build` | Type-checks, then builds the site into `frontend/dist` |
| `npm run preview` | Serves `frontend/dist` at http://localhost:4173 |

CI runs `lint`, `typecheck` and `build` on every pull request. Run `npm run format` and `npm run lint` before you push.

Use `npm install <package>` to add a dependency, and commit the updated `package-lock.json` with it.

## CI/CD

Every pull request is built and tested. Every push to `main` is tested again, then deployed. See [docs/ci-cd.md](docs/ci-cd.md) for the project conventions the pipeline expects and how to turn on deployment.

## Rules for contributors

- V0 is the shared core. Variants add to the `Rules` interface and never edit core files directly.
- The rules engine knows nothing about the network. Keep game logic and transport separate.
- If a variant needs a core change, raise it with the team before merging.
- Each variant includes its own perft tests.
- Challenge codes are secrets. Never log them or commit them to the repo.
