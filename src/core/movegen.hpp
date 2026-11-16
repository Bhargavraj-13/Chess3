#pragma once

#include <vector>

#include "core/move.hpp"
#include "core/position.hpp"

namespace chess::core {

// Every move the side to move could play if king safety did not matter.
//
// Castling is included whenever the rights, the king, the rook and the empty
// squares between them are all there. Whether the king would start, pass through
// or finish in check is #6's job, which is why these are only candidates.
//
// #6 builds legal_moves on top of this by dropping any move that leaves the
// mover's king attacked. Section 3 of docs/system-design.md.
[[nodiscard]] std::vector<Move> pseudo_legal_moves(const Position& position);

}  // namespace chess::core
