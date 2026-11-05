# chess_variants

A C++ chess project built in stages. V0 is the classic game, built together by the team and playable over a network between two players. V1, V2, and V3 are variants, each built by one person on top of the shared core.

## V0: Classic Chess (networked)

**Description:** Standard chess rules, played over a network between two players. One player creates a game and receives a challenge code. The second player enters that code to join. There is no random matchmaking.

Covers board representation, FEN parsing, legal move generation (including castling, en passant, and promotion), checkmate and stalemate detection, and a networked session for each challenge code.

**Scope:**
- Challenge code generation: short, unique, and generated with a cryptographically secure random source so it can't be guessed
- Join flow: enter the code, connect, start the game
- Move sync between the two clients, with illegal moves rejected by the server
- Disconnect handling: the game ends cleanly if a player drops

**Owner(s):** 

**Status:**

**Acceptance:** Perft counts match published values to the agreed depth. A full game can be played between two clients using a challenge code.

## V1: [Variant name]

**Description:**

**Owner:**

**Rule changes:**

**Interface hooks used:**

**Status:**

## V2: [Variant name]

**Description:**

**Owner:**

**Rule changes:**

**Interface hooks used:**

**Status:**

## V3: [Variant name]

**Description:**

**Owner:**

**Rule changes:**

**Interface hooks used:**

**Status:**

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Rules for contributors

- V0 is the shared core. Variants add to the `Rules` interface and never edit core files directly.
- The rules engine knows nothing about the network. Keep game logic and transport separate.
- If a variant needs a core change, raise it with the team before merging.
- Each variant includes its own perft tests.
- Challenge codes are secrets. Never log them or commit them to the repo.
