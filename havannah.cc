// Copyright (c) 2010-2012 Marcin Ciura, Piotr Wieczorek
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// Definitions of basic classes and helper functions for the game of Havannah.

#ifdef DUMP_RINGS
  #define DUMP(statement) statement
#else
  #define DUMP(statement)
#endif

#include "havannah.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <functional>
#include <map>
#include <utility>

namespace lajkonik {
namespace {

struct InitModule {
  InitModule() { Position::InitStaticFields(); }
} init_module;

}  // namespace

const int kNeighborOffsets[3 * 6] = {
  -1, -32, -31, +1, +32, +31,
  -33, -63, -30, +33, +63, +30,
  -2, -64, -62, +2, +64, +62,
};

const unsigned kReverseNeighborhoods[6] = { 8, 2, 1, 4, 16, 32 };

bool g_use_lg_coordinates = false;

bool LiesOnBoard(XCoord x, YCoord y) {
  return (x >= kGapLeft) && (x < kPastColumns) &&
  (y >= kGapAround) && (y < kPastRows) &&
  (x + y >= SIDE_LENGTH + kGapLeft + kGapAround - 1) &&
  (x + y < 3 * SIDE_LENGTH + kGapLeft + kGapAround - 2);
}

namespace {

bool ConvertCellToCoordinates(const std::string& cell, int* x, int* y) {
  *x = cell[0] - 'a';
  *y = cell[1] - '0';
  if (*y <= 0 || *y > 9)
    return false;
  if (cell[2] != '\0') {
    *y = 10 * *y + cell[2] - '0';
    if (cell[3] != '\0')
      return false;
  }
  return true;
}

}  // namespace

Cell FromString(const std::string& cell) {
  if (g_use_lg_coordinates)
    return FromLittleGolemString(cell);
  else
    return FromClassicalString(cell);
}

Cell FromClassicalString(const std::string& cell) {
  int xx;
  int yy;
  if (!ConvertCellToCoordinates(cell, &xx, &yy))
    return kZerothCell;
  const XCoord x = static_cast<XCoord>(kGapLeft + xx);
  const YCoord y = static_cast<YCoord>(kPastRows - yy);
  if (!LiesOnBoard(x, y))
    return kZerothCell;
  return XYToCell(x, y);
}

Cell FromLittleGolemString(const std::string& cell) {
  int xx;
  int yy;
  if (!ConvertCellToCoordinates(cell, &xx, &yy))
    return kZerothCell;
  YCoord y;
  if (xx < SIDE_LENGTH)
    y = static_cast<YCoord>(kPastRows - yy);
  else
    y = static_cast<YCoord>(kGapAround + 3 * SIDE_LENGTH - 2 - xx - yy);
  const XCoord x = static_cast<XCoord>(kGapLeft + xx);
  if (!LiesOnBoard(x, y))
    return kZerothCell;
  return XYToCell(x, y);
}

std::string ToString(XCoord x, YCoord y) {
  if (g_use_lg_coordinates)
    return ToLittleGolemString(x, y);
  else
    return ToClassicalString(x, y);
}

std::string ToClassicalString(XCoord x, YCoord y) {
  if (!LiesOnBoard(x, y))
    return "";
  const int xx = x - kGapLeft;
  const int yy = kPastRows - y;
  assert(yy > 0 && yy < 100);
  return StringPrintf("%c%d", 'a' + xx, yy);
}

std::string ToLittleGolemString(XCoord x, YCoord y) {
  if (!LiesOnBoard(x, y))
    return "";
  const int xx = x - kGapLeft;
  int yy;
  if (xx < SIDE_LENGTH)
    yy = kPastRows - y;
  else
    yy = kGapAround + 3 * SIDE_LENGTH - 2 - xx - y;
  assert(yy > 0 && yy < 100);
  return StringPrintf("%c%d", 'a' + xx, yy);
}

std::string ToString(Cell cell) {
  return ToString(CellToX(cell), CellToY(cell));
}

std::string ToClassicalString(Cell cell) {
  return ToClassicalString(CellToX(cell), CellToY(cell));
}

std::string ToLittleGolemString(Cell cell) {
  return ToLittleGolemString(CellToX(cell), CellToY(cell));
}

//-- PrintableBoard ---------------------------------------------------
std::string PrintableBoard::MakeString(Cell marked_cell) const {
  if (g_use_lg_coordinates)
    return MakeLittleGolemString(marked_cell);
  else
    return MakeClassicalString(marked_cell);
}

std::string PrintableBoard::MakeClassicalString(Cell marked_cell) const {
  std::string result;
  for (int yy = 0; yy < SIDE_LENGTH; ++yy) {
    for (int xx = 0; xx < SIDE_LENGTH - 1 - yy; ++xx) {
      result += ' ';
    }
    result += StringPrintf("%2d", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    Cell previous_cell = kNumCellsWithSentinels;
    for (int xx = SIDE_LENGTH - 1 - yy; xx < 2 * SIDE_LENGTH - 1; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell current_cell = XYToCell(x, y);
      result += (current_cell == marked_cell) ?
                    '[' : (previous_cell == marked_cell) ? ']': ' ';
      result += GetCharForCell(x, y);
      previous_cell = current_cell;
    }
    result += (previous_cell == marked_cell) ? "]\n" : "\n";
  }
  for (int yy = SIDE_LENGTH; yy < 2 * SIDE_LENGTH - 1; ++yy) {
    for (int xx = 0; xx <= yy - SIDE_LENGTH; ++xx) {
      result += ' ';
    }
    result += StringPrintf("%2d", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    Cell previous_cell = kNumCellsWithSentinels;
    for (int xx = 0; xx < 3 * SIDE_LENGTH - 2 - yy; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell current_cell = XYToCell(x, y);
      result += (current_cell == marked_cell) ?
                    '[' : (previous_cell == marked_cell) ? ']': ' ';
      result += GetCharForCell(x, y);
      previous_cell = current_cell;
    }
    result += StringPrintf((previous_cell == marked_cell) ? "]%c\n" : " %c\n",
                           3 * SIDE_LENGTH - 2 + 'a' - yy);
  }
  for (int xx = 0; xx < SIDE_LENGTH + 2; ++xx) {
    result += ' ';
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf(" %c", 'a' + xx);
  }
  result += '\n';
  return result;
}

std::string PrintableBoard::MakeLittleGolemString(Cell marked_cell) const {
  std::string result;
  for (int xx = 0; xx < SIDE_LENGTH + 2; ++xx) {
    result += ' ';
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf("%2d", 2 * SIDE_LENGTH - 1 - xx);
  }
  result += '\n';
  for (int yy = 0; yy < SIDE_LENGTH; ++yy) {
    for (int xx = 0; xx < SIDE_LENGTH - 1 - yy; ++xx) {
      result += ' ';
    }
    result += StringPrintf("%2d", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    Cell previous_cell = kNumCellsWithSentinels;
    for (int xx = SIDE_LENGTH - 1 - yy; xx < 2 * SIDE_LENGTH - 1; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell current_cell = XYToCell(x, y);
      result += (current_cell == marked_cell) ?
                    '[' : (previous_cell == marked_cell) ? ']': ' ';
      result += GetCharForCell(x, y);
      previous_cell = current_cell;
    }
    result += (previous_cell == marked_cell) ? ']' : ' ';
    if (yy != SIDE_LENGTH - 1) {
      result += StringPrintf("%d", SIDE_LENGTH - 1 - yy);
    }
    result += '\n';
  }
  for (int yy = SIDE_LENGTH; yy < 2 * SIDE_LENGTH - 1; ++yy) {
    for (int xx = 0; xx <= yy - SIDE_LENGTH; ++xx) {
      result += ' ';
    }
    result += StringPrintf("%2d", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    Cell previous_cell = kNumCellsWithSentinels;
    for (int xx = 0; xx < 3 * SIDE_LENGTH - 2 - yy; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell current_cell = XYToCell(x, y);
      result += (current_cell == marked_cell) ?
                    '[' : (previous_cell == marked_cell) ? ']': ' ';
      result += GetCharForCell(x, y);
      previous_cell = current_cell;
    }
    result += (previous_cell == marked_cell) ? ']' : ' ';
    result += StringPrintf("%c\n", 3 * SIDE_LENGTH - 2 + 'a' - yy);
  }
  for (int xx = 0; xx < SIDE_LENGTH + 2; ++xx) {
    result += ' ';
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf(" %c", 'a' + xx);
  }
  result += '\n';
  return result;
}

//-- BoardBitmask -----------------------------------------------------
void BoardBitmask::FillWithNeighborMask(
    const BoardBitmask& player_stones,
    const BoardBitmask& opponent_stones) {
  RowBitmask prev = 0;
  RowBitmask curr = player_stones.Row(kGapAround);
  RowBitmask next = player_stones.Row(NextY(kGapAround));
  for (YCoord y = kGapAround; y < kPastRows; y = NextY(y)) {
    RowBitmask current_mask =
        prev | next | ((prev | curr) >> 1) | ((curr | next) << 1);
    current_mask &= ~(curr | (opponent_stones).Row(y));
    current_mask &= Position::GetBoardBitmask().Row(y);
    rows_[y] = current_mask;
    prev = curr;
    curr = next;
    next = player_stones.Row(NextY(NextY(y)));
  }
}

unsigned BoardBitmask::Get6Neighbors(XCoord x, YCoord y) const {
  // For a board fragment
  //    ab
  //   cde
  //   fg
  // the six neighbors of d correspond to the bit pattern abcefg.
  unsigned neighborhood;
  neighborhood = (Row(PrevY(y)) >> x) & 3;
  const RowBitmask curr_line = Row(y);
  neighborhood = (neighborhood << 1) | ((curr_line >> (x + 1)) & 1);
  neighborhood = (neighborhood << 1) | ((curr_line >> (x - 1)) & 1);
  neighborhood = (neighborhood << 2) |
                 ((Row(NextY(y)) >> (x - 1)) & 3);
  return neighborhood;
}

Cell BoardBitmask::GetSampleStone() const {
  // Iterate through all rows and columns since this method
  // can also be called on Position::GetEdgeChain(i)
  // or Position::GetCornerChain(i).
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    RowBitmask yth_row = Row(y);
    if (yth_row != 0) {
      for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
        if (yth_row & (1 << x)) {
          return XYToCell(x, y);
        }
      }
    }
  }
  assert(false);
  return kZerothCell;
}

//-- Chain ------------------------------------------------------------
void Chain::AddStoneReversibly(XCoord x, YCoord y, Memento* memento) {
  assert(LiesOnBoard(x, y));
  unsigned mask =
      Position::GetMaskOfEdgesAndCorners(XYToCell(x, y)) | GetRingMask(x, y);
  if (mask != 0) {
    memento->Remember(&edges_corners_ring_);
    edges_corners_ring_ |= mask;
  }
  memento->Remember(&NthRow(y));
  assert(!stone_mask_.get(x, y));
  stone_mask_.set(x, y);
  memento->Remember(&num_stones_);
  ++num_stones_;
}

void Chain::AddStoneFast(XCoord x, YCoord y) {
  assert(LiesOnBoard(x, y));
  edges_corners_ring_ |=
      Position::GetMaskOfEdgesAndCorners(XYToCell(x, y)) | GetRingMask(x, y);
  assert(!stone_mask_.get(x, y));
  stone_mask_.set(x, y);
  ++num_stones_;
}

void Chain::AddStoneOutsideBoard(XCoord x, YCoord y) {
  assert(!LiesOnBoard(x, y));
  assert(!stone_mask_.get(x, y));
  stone_mask_.set(x, y);
  ++num_stones_;
}

void Chain::ComputeUnion(
    XCoord x, YCoord y, const Chain& other, Chain* result) const {
  assert(LiesOnBoard(x, y));
  assert(stone_mask_.get(x, y));
  assert(!other.stone_mask_.get(x, y));
  result->stone_mask_.FillWithOr(stone_mask_, other.stone_mask_);
  result->num_stones_ = num_stones() + other.num_stones();
  // Since this Chain contains the stone at (x, y), the ring bit of
  // edges_corners_ring_ reflects the ring status of this Chain.
  // But we need to update the ring bit of result->edges_corners_ring_
  // when X joins Chains A and B, closing the ring in Chain B like here:
  //   .A
  //   BX.
  //    .B
  result->edges_corners_ring_ =
      edges_corners_ring_ |
      other.edges_corners_ring_ |
      other.GetRingMask(x, y);
  result->newer_version_ = PlayerPosition::kNullChain;
}

void Chain::InitWithStone(XCoord x, YCoord y) {
  assert(LiesOnBoard(x, y));
  stone_mask_.ZeroBits();
  stone_mask_.set(x, y);
  num_stones_ = 1;
  edges_corners_ring_ = Position::GetMaskOfEdgesAndCorners(XYToCell(x, y));
  newer_version_ = PlayerPosition::kNullChain;
}

void Chain::InitWithStoneOutsideBoard(XCoord x, YCoord y) {
  assert(!LiesOnBoard(x, y));
  stone_mask_.ZeroBits();
  stone_mask_.set(x, y);
  num_stones_ = 1;
  edges_corners_ring_ = 0;
  newer_version_ = PlayerPosition::kNullChain;
}

WinningCondition Chain::IsVictory() {
  WinningCondition result = static_cast<WinningCondition>(
      kFork * (CountSetBits(edges_corners_ring_) >= 3) +
      kBridge * (CountSetBits(edges_corners_ring_ >> 6) >= 2) +
      ((edges_corners_ring_ >> 12) & 3));
  edges_corners_ring_ &= ~(3 << 12);
  return result;
}

void Chain::CopyFrom(const Chain& other) {
  stone_mask_.CopyFrom(other.stone_mask_);
  num_stones_ = other.num_stones_;
  edges_corners_ring_ = other.edges_corners_ring_;
  newer_version_ = other.newer_version_;
}

void Chain::SetNewerVersionReversibly(ChainNum nv, Memento* memento) {
  assert(PlayerPosition::ChainLiesOnBoard(nv));
  memento->Remember(&newer_version_);
  newer_version_ = nv;
}

unsigned Chain::ClosesAnyRing(XCoord x, YCoord y) const {
  assert(!stone_mask_.get(x, y));
  // For a board fragment
  //    ab
  //   cde
  //   fg
  // the neighborhood of d is the bit pattern baedcgf.
  int neighborhood;
  neighborhood = (NthRow(PrevY(y)) >> x) & 3;
  neighborhood = (neighborhood << 3) | ((NthRow(y) >> (x - 1)) & 5);
  neighborhood = (neighborhood << 2) |
                 ((NthRow(NextY(y)) >> (x - 1)) & 3);
  // Putting a stone in a cell closes a ring if among the six cells around
  // the stone we find at least two nonadjacent stones from the same chain
  // or else three adjacent stones from the same chain and three stones
  // behind them. The first case is encoded as 64; the second case as a
  // six-bit (1-63) mask indicating the need for more tests; failing to close
  // a ring is encoded as 0. If both cases are true (111010), 64 is enough.
  static const unsigned char kClosesRing[128] = {
     0,  0,  0,  0,  0,  0, 64,  4,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 64,  0,  8, 64, 64, 64, 12,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 64, 64, 64,  0,  2, 64,  6,  0,  0,  0,  0,  0,  0,  0,  0,
    64, 64, 64, 64, 64, 64, 64, 14,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 64, 64, 64, 64, 64, 64, 64,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 64, 16, 24, 64, 64, 64, 28,  0,  0,  0,  0,  0,  0,  0,  0,
     0, 64, 64, 64,  1,  3, 64,  7,  0,  0,  0,  0,  0,  0,  0,  0,
    32, 64, 48, 56, 33, 35, 49,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  };
  assert(neighborhood >= 0);
  assert(neighborhood < 128);
  unsigned char mask = kClosesRing[neighborhood];
  const unsigned kPositiveResult = (1 << 12);
  if (mask == 0)
    return 0;
  if (mask == 64)
    return kPositiveResult;
  const int xx = x - 2;
  const int prev2_line = NthRow(PrevY(PrevY(y))) >> xx;
  const int prev_line = NthRow(PrevY(y)) >> xx;
  const int this_line = NthRow(y) >> xx;
  const int next_line = NthRow(NextY(y)) >> xx;
  const int next2_line = NthRow(NextY(NextY(y))) >> xx;
  if ((mask & 1) && ((prev2_line & 12) == 12) && (prev_line & 2))
    return kPositiveResult;
  if ((mask & 2) && (prev_line & 2) && (this_line & next_line & 1))
    return kPositiveResult;
  if ((mask & 4) && (next_line & 1) && (next2_line & 3) == 3)
    return kPositiveResult;
  if ((mask & 8) && (next_line & 8) && ((next2_line & 6) == 6))
    return kPositiveResult;
  if ((mask & 16) && (prev_line & this_line & 16) && (next_line & 8))
    return kPositiveResult;
  if ((mask & 32) && (prev_line & 16) && ((prev2_line & 24) == 24))
    return kPositiveResult;
  return 0;
}

unsigned Chain::ClosesBenzeneRing(XCoord x, YCoord y) const {
  const unsigned kPositiveResult = (1 << 13);
  const int xx = x - 2;
  const int prev2_line = NthRow(PrevY(PrevY(y))) >> xx;
  const int prev_line = NthRow(PrevY(y)) >> xx;
  const int this_line = NthRow(y) >> xx;
  const int next_line = NthRow(NextY(y)) >> xx;
  const int next2_line = NthRow(NextY(NextY(y))) >> xx;
  if (this_line & 2) {
    if (((prev_line & 10) == 10) && ((prev2_line & 12) == 12))
      return kPositiveResult;
    if (((next_line & 5) == 5) && ((next2_line & 3) == 3))
      return kPositiveResult;
  }
  if (this_line & 8) {
    if (((prev_line & 20) == 20) && ((prev2_line & 24) == 24))
      return kPositiveResult;
    if (((next_line & 10) == 10) && ((next2_line & 6) == 6))
      return kPositiveResult;
  }
  if (((prev_line & 6) == 6) && (this_line & 1) && ((next_line & 3) == 3))
    return kPositiveResult;
  if (((prev_line & 24) == 24) && (this_line & 16) && ((next_line & 12) == 12))
    return kPositiveResult;
  return 0;
}

unsigned Chain::GetRingMask(XCoord x, YCoord y) const {
  unsigned result = ClosesAnyRing(x, y);
  if (result != 0)
    result |= ClosesBenzeneRing(x, y);
  return result;
}

//-- ChainSet ---------------------------------------------------------
ChainSet::ChainSet() { Reserve(1); }
ChainSet::~ChainSet() { ShrinkTo(PlayerPosition::kNumSpecialChains); }

void ChainSet::AddStoneToChainReversibly(
    XCoord x, YCoord y, ChainNum ch, Memento* memento) {
  assert(ch == NewestVersion(ch));
  chains_[ch]->AddStoneReversibly(x, y, memento);
}

void ChainSet::AddStoneToChainFast(XCoord x, YCoord y, ChainNum ch) {
  assert(ch == NewestVersion(ch));
  chains_[ch]->AddStoneFast(x, y);
}

ChainNum ChainSet::MergeChainsReversibly(
    XCoord x, YCoord y, ChainNum chain1, ChainNum chain2, Memento* memento) {
  assert(chain1 == NewestVersion(chain1));
  assert(chain2 == NewestVersion(chain2));
  if (chain1 == chain2)
    return chain2;
  Chain* chain1_p = chains_[chain1];
  Chain* chain2_p = chains_[chain2];
  int last_chain = chains_.size();
  assert(last_chain < kChainNumLimit);
  Chain* result = allocator_.MakeChain();
  chains_.push_back(result);
  chain1_p->ComputeUnion(x, y, *chain2_p, result);
  chain1_p->SetNewerVersionReversibly(last_chain, memento);
  chain2_p->SetNewerVersionReversibly(last_chain, memento);
  return last_chain;
}

ChainNum ChainSet::MergeChainsFast(
    XCoord x, YCoord y, ChainNum chain1, ChainNum chain2) {
  assert(chain1 == NewestVersion(chain1));
  assert(chain2 == NewestVersion(chain2));
  if (chain1 == chain2)
    return chain2;
  Chain* chain1_p = chains_[chain1];
  Chain* chain2_p = chains_[chain2];
  int last_chain = chains_.size();
  assert(last_chain < kChainNumLimit);
  Chain* result = allocator_.MakeChain();
  chains_.push_back(result);
  chain1_p->ComputeUnion(x, y, *chain2_p, result);
  chain1_p->SetNewerVersionFast(last_chain);
  chain2_p->SetNewerVersionFast(last_chain);
  return last_chain;
}

ChainNum ChainSet::MakeOneStoneChain(XCoord x, YCoord y) {
  int last_chain = chains_.size();
  assert(last_chain < kChainNumLimit);
  assert(PlayerPosition::ChainLiesOnBoard(last_chain));
  Chain* result = allocator_.MakeChain();
  chains_.push_back(result);
  result->InitWithStone(x, y);
  return last_chain;
}

ChainNum ChainSet::push_back(Chain* chain) {
  int chain_num = chains_.size();
  chains_.push_back(chain);
  return chain_num;
}

ChainNum ChainSet::PushBackChainOutsideBoard(const Chain* chain) {
  const int chain_num = push_back(const_cast<Chain*>(chain));
  assert(!PlayerPosition::ChainLiesOnBoard(chain_num));
  return chain_num;
}

ChainNum ChainSet::NewestVersion(ChainNum ch) const {
  assert(ch != PlayerPosition::kNullChain);
  while (chains_[ch]->newer_version() != PlayerPosition::kNullChain) {
    ch = chains_[ch]->newer_version();
  }
  return ch;
}

const Chain* ChainSet::NewestVersion(const Chain* ch) const {
  assert(ch != NULL);
  while (ch->newer_version() != PlayerPosition::kNullChain) {
    ch = chain(ch->newer_version());
  }
  return ch;
}

void ChainSet::Reserve(int n) {
  while (static_cast<int>(chains_.size()) < n) {
    chains_.push_back(NULL);
  }
}

void ChainSet::ShrinkTo(int n) {
  while (static_cast<int>(chains_.size()) > n) {
    allocator_.DeleteChain(chains_.back());
    chains_.pop_back();
  }
}

int ChainSet::CountChains() const {
  int count = 0;
  for (int i = 1, n = chains_.size(); i < n; ++i) {
    const Chain* ch = chain(i);
    if (ch != NULL && ch->newer_version() == PlayerPosition::kNullChain)
      ++count;
  }
  return count;
}

//-- PositionEvaluation -------------------------------------------------
int PositionEvaluation::GetEvaluation(const Position& position) {
  int minimum = BfsResult::kMaxDistance;
  int mobility = 0;
  for (MoveIndex move = kZerothMove; move < kNumMovesOnBoard;
       move = NextMove(move)) {
    if (!position.CellIsEmpty(Position::MoveIndexToCell(move)))
      continue;
    const int value = get(move);
    if (value < minimum) {
      minimum = value;
      mobility = 1;
    } else if (value == minimum) {
      ++mobility;
    }
  }
  return 100 * minimum - mobility;
}

void PositionEvaluation::SetAllMovesTo(int value) {
  for (MoveIndex move = kZerothMove; move <= kNumMovesOnBoard;
       move = NextMove(move)) {
    set(move, value);
  }
}

void PositionEvaluation::SetToCombination(
    const BfsResult& bfs_result_1,
    const BfsResult& bfs_result_2,
    const Chain* chain_1,
    const Chain* chain_2) {
  const int d1 = bfs_result_1.GetTwoDistance(chain_2);
  const int d2 = bfs_result_2.GetTwoDistance(chain_1);
  const int distance = std::max(d1, d2);
  if (distance == 0) {
    SetAllMovesTo(0);
  } else {
    for (MoveIndex move = kZerothMove; move < kNumMovesOnBoard;
         move = NextMove(move)) {
      const Cell cell = Position::MoveIndexToCell(move);
      const int f1 = bfs_result_1.get(cell);
      const int f2 = bfs_result_2.get(cell);
      set(move, std::min(f1 + f2, distance));
    }
    set(kNumMovesOnBoard, distance);
  }
}

void PositionEvaluation::CopyFrom(const PositionEvaluation& other) {
  for (MoveIndex move = kZerothMove; move <= kNumMovesOnBoard;
       move = NextMove(move)) {
    set(move, other.get(move));
  }
}

void PositionEvaluation::CopyFromBfsResult(const BfsResult& other) {
  for (MoveIndex move = kZerothMove; move <= kNumMovesOnBoard;
       move = NextMove(move)) {
    set(move, other.get(Position::MoveIndexToCell(move)));
  }
}

void PositionEvaluation::SetToSum(
    const PositionEvaluation& augend,
    const PositionEvaluation& addend) {
  for (MoveIndex move = kZerothMove; move <= kNumMovesOnBoard;
       move = NextMove(move)) {
    set(move, augend.get(move) + addend.get(move));
  }
}

void PositionEvaluation::SetToMinimum(
    const PositionEvaluation& first,
    const PositionEvaluation& second) {
  for (MoveIndex move = kZerothMove; move <= kNumMovesOnBoard;
       move = NextMove(move)) {
    set(move, std::min(first.get(move), second.get(move)));
  }
}

std::string PositionEvaluation::MakeString(const Position* position) const {
  if (g_use_lg_coordinates)
    return MakeLittleGolemString(position);
  else
    return MakeClassicalString(position);
}

std::string PositionEvaluation::MakeClassicalString(
    const Position* position) const {
  std::string result;
  for (int yy = 0; yy < SIDE_LENGTH; ++yy) {
    for (int xx = 0; xx < SIDE_LENGTH - 1 - yy; ++xx) {
      result += "  ";
    }
    result += StringPrintf("%2d ", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    for (int xx = SIDE_LENGTH - 1 - yy; xx < 2 * SIDE_LENGTH - 1; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell cell = XYToCell(x, y);
      if (position == NULL || position->CellIsEmpty(cell)) {
        result += StringPrintf(
            "%3d ", distance[Position::CellToMoveIndex(cell)]);
      } else {
        const char c = position->GetCell(cell)[".xo"];
        result += StringPrintf(" %c%c%c", c, c, c);
      }
    }
    result += "\n";
  }
  for (int yy = SIDE_LENGTH; yy < 2 * SIDE_LENGTH - 1; ++yy) {
    for (int xx = 0; xx <= yy - SIDE_LENGTH; ++xx) {
      result += "  ";
    }
    result += StringPrintf("%2d ", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    for (int xx = 0; xx < 3 * SIDE_LENGTH - 2 - yy; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell cell = XYToCell(x, y);
      if (position == NULL || position->CellIsEmpty(cell)) {
        result += StringPrintf(
            "%3d ", distance[Position::CellToMoveIndex(cell)]);
      } else {
        const char c = position->GetCell(cell)[".xo"];
        result += StringPrintf(" %c%c%c", c, c, c);
      }
    }
    result += StringPrintf("  %c\n", 3 * SIDE_LENGTH - 2 + 'a' - yy);
  }
  for (int xx = 0; xx < SIDE_LENGTH + 1; ++xx) {
    result += "  ";
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf("   %c", 'a' + xx);
  }
  result += StringPrintf("\n%d\n", get_baseline_distance());
  return result;
}

std::string PositionEvaluation::MakeLittleGolemString(
    const Position* position) const {
  std::string result;
  for (int xx = 0; xx < SIDE_LENGTH + 1; ++xx) {
    result += "  ";
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf("  %2d", 2 * SIDE_LENGTH - 1 - xx);
  }
  result += '\n';
  for (int yy = 0; yy < SIDE_LENGTH; ++yy) {
    for (int xx = 0; xx < SIDE_LENGTH - 1 - yy; ++xx) {
      result += "  ";
    }
    result += StringPrintf("%2d ", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    for (int xx = SIDE_LENGTH - 1 - yy; xx < 2 * SIDE_LENGTH - 1; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell cell = XYToCell(x, y);
      if (position == NULL || position->CellIsEmpty(cell)) {
        result += StringPrintf(
            "%3d ", distance[Position::CellToMoveIndex(cell)]);
      } else {
        const char c = position->GetCell(cell)[".xo"];
        result += StringPrintf(" %c%c%c", c, c, c);
      }
    }
    if (yy != SIDE_LENGTH - 1) {
      result += StringPrintf("  %d ", SIDE_LENGTH - 1 - yy);
    }
    result += '\n';
  }
  for (int yy = SIDE_LENGTH; yy < 2 * SIDE_LENGTH - 1; ++yy) {
    for (int xx = 0; xx <= yy - SIDE_LENGTH; ++xx) {
      result += "  ";
    }
    result += StringPrintf("%2d ", 2 * SIDE_LENGTH - 1 - yy);
    const YCoord y = static_cast<YCoord>(kGapAround + yy);
    for (int xx = 0; xx < 3 * SIDE_LENGTH - 2 - yy; ++xx) {
      const XCoord x = static_cast<XCoord>(kGapLeft + xx);
      const Cell cell = XYToCell(x, y);
      if (position == NULL || position->CellIsEmpty(cell)) {
        result += StringPrintf(
            "%3d ", distance[Position::CellToMoveIndex(cell)]);
      } else {
        const char c = position->GetCell(cell)[".xo"];
        result += StringPrintf(" %c%c%c", c, c, c);
      }
    }
    result += StringPrintf("  %c\n", 3 * SIDE_LENGTH - 2 + 'a' - yy);
  }
  for (int xx = 0; xx < SIDE_LENGTH + 2; ++xx) {
    result += "  ";
  }
  for (int xx = 0; xx < SIDE_LENGTH; ++xx) {
    result += StringPrintf(" %c  ", 'a' + xx);
  }
  result += StringPrintf("\n%d\n", get_baseline_distance());
  return result;
}

std::string PositionEvaluation::Get18Neighbors(
    Player player, Cell cell, const Position& position) const {
  std::string result;
  const MoveIndex move = Position::CellToMoveIndex(cell);
  const int base_evaluation = get(move);
  for (int i = 0; i < 18; ++i) {
    const Cell neighbor_cell = NthNeighbor(cell, i);
    if (position.GetCell(neighbor_cell) == player + 1) {
      result += 'x';
    } else if (position.GetCell(neighbor_cell) == Opponent(player) + 1) {
      result += 'o';
    } else if (position.GetCell(neighbor_cell) == 3) {
      result += '#';
    } else {
      const MoveIndex neighbor = Position::CellToMoveIndex(neighbor_cell);
      const int difference = std::min(get(neighbor) - base_evaluation, 9);
      result += ('2' + difference);
    }
    if (result.size() == 6 || result.size() == 13)
      result += '/';
  }
  return result;
}

//-- PlayerPosition ---------------------------------------------------
PlayerPosition::PlayerPosition() {
  memset(chains_for_cells_, 0, sizeof chains_for_cells_);
  stone_mask_.ZeroBits();
  two_bridge_mask_.ZeroCounters();
}

WinningCondition PlayerPosition::MakeMoveReversibly(
    Cell cell, Memento* memento) {
  const XCoord x = CellToX(cell);
  const YCoord y = CellToY(cell);
  assert(LiesOnBoard(x, y));
  assert(CellIsEmpty(cell));
  memento->RememberSize(&chain_set_);
  ChainNum previous_chain = kNullChain;
  for (int j = 0; j < 6; ++j) {
    ChainNum current_chain = chain_for_cell(NthNeighbor(cell, j));
    if (!ChainLiesOnBoard(current_chain))
      continue;
    if (previous_chain != kNullChain) {
      previous_chain = chain_set_.NewestVersion(previous_chain);
      current_chain = chain_set_.NewestVersion(current_chain);
      ring_db_.MergeChainEdgesReversibly(
          previous_chain, current_chain, chain_set_, memento);
      previous_chain = chain_set_.MergeChainsReversibly(
          x, y, previous_chain, current_chain, memento);
    } else {
      previous_chain = current_chain = chain_set_.NewestVersion(current_chain);
      chain_set_.AddStoneToChainReversibly(x, y, current_chain, memento);
    }
  }
  memento->Remember(&chains_for_cells_[cell]);
  if (previous_chain != kNullChain)
    chains_for_cells_[cell] = previous_chain;
  else
    chains_for_cells_[cell] = chain_set_.MakeOneStoneChain(x, y);
  modified_chain_ = chains_for_cells_[cell];
  memento->Remember(&stone_mask_.Row(y));
  stone_mask_.set(x, y);
  return chain_set_.IsVictory(chain_for_cell(cell));
}

WinningCondition PlayerPosition::MakeMoveFast(Cell cell) {
  const XCoord x = CellToX(cell);
  const YCoord y = CellToY(cell);
  assert(LiesOnBoard(x, y));
  assert(CellIsEmpty(cell));
  ChainNum previous_chain = kNullChain;
  for (int j = 0; j < 6; ++j) {
    ChainNum current_chain = chain_for_cell(NthNeighbor(cell, j));
    if (!ChainLiesOnBoard(current_chain))
      continue;
    if (previous_chain != kNullChain) {
      previous_chain = chain_set_.NewestVersion(previous_chain);
      current_chain = chain_set_.NewestVersion(current_chain);
      ring_db_.MergeChainEdgesFast(
          previous_chain, current_chain, chain_set_);
      previous_chain = chain_set_.MergeChainsFast(
          x, y, previous_chain, current_chain);
    } else {
      previous_chain = current_chain = chain_set_.NewestVersion(current_chain);
      chain_set_.AddStoneToChainFast(x, y, current_chain);
    }
  }
  if (previous_chain != kNullChain)
    chains_for_cells_[cell] = previous_chain;
  else
    chains_for_cells_[cell] = chain_set_.MakeOneStoneChain(x, y);
  modified_chain_ = chains_for_cells_[cell];
  stone_mask_.set(x, y);
  return chain_set_.IsVictory(chain_for_cell(cell));
}

void PlayerPosition::AddChainOutsideBoard(const Chain* chain) {
  ChainNum chain_num = chain_set_.PushBackChainOutsideBoard(chain);
  for (YCoord y = PrevY(kGapAround); y <= NextY(kLastRow); y = NextY(y)) {
    for (XCoord x = PrevX(kGapLeft); x <= NextX(kLastColumn); x = NextX(x)) {
      if (chain->stone_mask().get(x, y)) {
        chains_for_cells_[XYToCell(x, y)] = chain_num;
      }
    }
  }
}

void PlayerPosition::CreateTwoBridgesAfterOurMoveReversibly(
    Cell cell, const PlayerPosition& opponent, Memento* memento) {
// Player creates his own two-bridge.
#define SET_CELL(acell, a, b) \
  do { \
    const Cell bcell = OffsetCell(cell, (b)); \
    if (CellIsEmpty(bcell) && opponent.CellIsEmpty(bcell) && \
        StoneIsInCell(OffsetCell(cell, (a) + (b)))) { \
      memento->Remember(&two_bridge_mask_.get(acell)); \
      memento->Remember(&two_bridge_mask_.get(bcell)); \
      two_bridge_mask_.increment(acell); \
      two_bridge_mask_.increment(bcell); \
      DUMP(printf("Setting %s %s\n", \
                  ToString(acell).c_str(), ToString(bcell).c_str())); \
      ring_db_.AddTwoBridgeReversibly( \
          acell, bcell, \
          NewestChainForCell(cell), \
          NewestChainForCell(OffsetCell(cell, (a) + (b))), \
          memento); \
    } \
  } while (false)

  assert(StoneIsInCell(cell));
  Cell acell;
  acell = OffsetCell(cell, -31);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, -31, -32);
    SET_CELL(acell, -31, +1);
  }
  acell = OffsetCell(cell, +32);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, +32, +1);
    SET_CELL(acell, +32, +31);
  }
  acell = OffsetCell(cell, -1);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, -1, -32);
    SET_CELL(acell, -1, +31);
  }
#undef SET_CELL
}

void PlayerPosition::CreateTwoBridgesAfterOurMoveFast(
    Cell cell, const PlayerPosition& opponent) {
// Player creates his own two-bridge.
#define SET_CELL(acell, a, b) \
  do { \
    const Cell bcell = OffsetCell(cell, (b)); \
    if (CellIsEmpty(bcell) && opponent.CellIsEmpty(bcell) && \
        StoneIsInCell(OffsetCell(cell, (a) + (b)))) { \
      two_bridge_mask_.increment(acell); \
      two_bridge_mask_.increment(bcell); \
      ring_db_.AddTwoBridgeFast( \
          acell, bcell, \
          NewestChainForCell(cell), \
          NewestChainForCell(OffsetCell(cell, (a) + (b)))); \
    } \
  } while (false)

  assert(StoneIsInCell(cell));
  Cell acell;
  acell = OffsetCell(cell, -31);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, -31, -32);
    SET_CELL(acell, -31, +1);
  }
  acell = OffsetCell(cell, +32);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, +32, +1);
    SET_CELL(acell, +32, +31);
  }
  acell = OffsetCell(cell, -1);
  if (CellIsEmpty(acell) && opponent.CellIsEmpty(acell)) {
    SET_CELL(acell, -1, -32);
    SET_CELL(acell, -1, +31);
  }
#undef SET_CELL
}

void PlayerPosition::RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(
    Cell cell, Memento* memento) {
// Player attacks an opponent's two-bridge.
#define ZERO_CELL(a, b, c) \
  do { \
    ring_db_.RemoveHalfBridgeReversibly( \
        cell, \
        NewestChainForCell(OffsetCell(cell, (a))), \
        NewestChainForCell(OffsetCell(cell, (b))), \
        memento); \
    const Cell ccell = OffsetCell(cell, (c)); \
    if (two_bridge_mask_.get(ccell) != 0) { \
      memento->Remember(&two_bridge_mask_.get(ccell)); \
      two_bridge_mask_.decrement(ccell); \
      DUMP(printf("Clearing %s -> %d\n", \
                  ToString(ccell).c_str(), two_bridge_mask_.get(ccell))); \
      if (two_bridge_mask_.get(ccell) == 0) { \
        ring_db_.RemoveHalfBridgeReversibly( \
            ccell, \
            NewestChainForCell(OffsetCell(cell, (a))), \
            NewestChainForCell(OffsetCell(cell, (b))), \
            memento); \
      } \
    } \
  } while (false)

  assert(CellIsEmpty(cell));
  if (two_bridge_mask_.get(cell) != 0) {
    memento->Remember(&two_bridge_mask_.get(cell));
    two_bridge_mask_.zero(cell);

    if (StoneIsInCell(OffsetCell(cell, -31))) {
      if (StoneIsInCell(OffsetCell(cell, -1)))
        ZERO_CELL(-31, -1, -32);
      if (StoneIsInCell(OffsetCell(cell, +32)))
        ZERO_CELL(-31, +32, +1);
    }
    if (StoneIsInCell(OffsetCell(cell, +1))) {
      if (StoneIsInCell(OffsetCell(cell, -32)))
        ZERO_CELL(+1, -32, -31);
      if (StoneIsInCell(OffsetCell(cell, +31)))
        ZERO_CELL(+1, +31, +32);
    }
    if ((StoneIsInCell(OffsetCell(cell, -32))) &&
        (StoneIsInCell(OffsetCell(cell, +31))))
      ZERO_CELL(-32, +31, -1);
    if ((StoneIsInCell(OffsetCell(cell, -1))) &&
        (StoneIsInCell(OffsetCell(cell, +32))))
      ZERO_CELL(-1, +32, +31);
  }
#undef ZERO_CELL
}

void PlayerPosition::RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveFast(
    Cell cell) {
// Player attacks an opponent's two-bridge.
#define ZERO_CELL(a, b, c) \
  do { \
    ring_db_.RemoveHalfBridgeFast( \
        cell, \
        NewestChainForCell(OffsetCell(cell, (a))), \
        NewestChainForCell(OffsetCell(cell, (b)))); \
    const Cell ccell = OffsetCell(cell, (c)); \
    if (two_bridge_mask_.get(ccell) != 0) { \
      two_bridge_mask_.decrement(ccell); \
      if (two_bridge_mask_.get(ccell) == 0) { \
        ring_db_.RemoveHalfBridgeFast( \
            ccell, \
            NewestChainForCell(OffsetCell(cell, (a))), \
            NewestChainForCell(OffsetCell(cell, (b)))); \
      } \
    } \
  } while (false)

  assert(CellIsEmpty(cell));
  if (two_bridge_mask_.get(cell) != 0) {
    two_bridge_mask_.zero(cell);

    if (StoneIsInCell(OffsetCell(cell, -31))) {
      if (StoneIsInCell(OffsetCell(cell, -1)))
        ZERO_CELL(-31, -1, -32);
      if (StoneIsInCell(OffsetCell(cell, +32)))
        ZERO_CELL(-31, +32, +1);
    }
    if (StoneIsInCell(OffsetCell(cell, +1))) {
      if (StoneIsInCell(OffsetCell(cell, -32)))
        ZERO_CELL(+1, -32, -31);
      if (StoneIsInCell(OffsetCell(cell, +31)))
        ZERO_CELL(+1, +31, +32);
    }
    if ((StoneIsInCell(OffsetCell(cell, -32))) &&
        (StoneIsInCell(OffsetCell(cell, +31))))
      ZERO_CELL(-32, +31, -1);
    if ((StoneIsInCell(OffsetCell(cell, -1))) &&
        (StoneIsInCell(OffsetCell(cell, +32))))
      ZERO_CELL(-1, +32, +31);
  }
#undef ZERO_CELL
}

unsigned PlayerPosition::Get18Neighbors(Cell cell) const {
  const XCoord x = CellToX(cell);
  const YCoord y = CellToY(cell);
  // For a board fragment
  //    abc
  //   defg
  //  hijkl
  //  mnop
  //  qrs
  // the 18 neighbors of j correspond to the bit pattern cbagfedlkihponmsrq.
  unsigned neighborhood;
  neighborhood = (stone_mask_.Row(PrevY(PrevY(y))) >> x) & 7;
  neighborhood = (neighborhood << 4) |
                 ((stone_mask_.Row(PrevY(y)) >> (x - 1)) & 15);
  const RowBitmask curr_line = stone_mask_.Row(y);
  neighborhood = (neighborhood << 2) | ((curr_line >> (x + 1)) & 3);
  neighborhood = (neighborhood << 2) | ((curr_line >> (x - 2)) & 3);
  neighborhood = (neighborhood << 4) |
                 ((stone_mask_.Row(NextY(y)) >> (x - 2)) & 15);
  neighborhood = (neighborhood << 3) |
                 ((stone_mask_.Row(NextY(NextY(y))) >> (x - 2)) & 7);
  return neighborhood;
}

bool PlayerPosition::MoveWouldCloseForkOrBridge(
    Cell cell, unsigned edges_corners, ChainNum injected_chain) const {
  assert(edges_corners == Position::GetMaskOfEdgesAndCorners(cell));
  if (injected_chain != kNullChain) {
    edges_corners |= chain_set_.edges_corners_ring(injected_chain);
  }
  for (int i = 0; i < 6; ++i) {
    ChainNum chain = chain_for_cell(NthNeighbor(cell, i));
    if (ChainLiesOnBoard(chain)) {
      chain = chain_set_.NewestVersion(chain);
      edges_corners |= chain_set_.edges_corners_ring(chain);
    }
  }
  return CountSetBits(edges_corners) >= 3 ||
         CountSetBits(edges_corners >> 6) >= 2;
}

bool PlayerPosition::MoveWouldCloseForkBridgeOrRing(
    Cell cell, unsigned edges_corners, ChainNum injected_chain) const {
  assert(edges_corners == Position::GetMaskOfEdgesAndCorners(cell));
  if (injected_chain != kNullChain) {
    injected_chain = chain_set_.NewestVersion(injected_chain);
    edges_corners |= chain_set_.edges_corners_ring(injected_chain);
  }
  const XCoord x = CellToX(cell);
  const YCoord y = CellToY(cell);
  for (int i = 0; i < 6; ++i) {
    ChainNum chain = chain_for_cell(NthNeighbor(cell, i));
    if (ChainLiesOnBoard(chain)) {
      chain = chain_set_.NewestVersion(chain);
      if (chain_set_.chain(chain)->ClosesAnyRing(x, y))
        return true;
      edges_corners |= chain_set_.edges_corners_ring(chain);
    }
  }
  return CountSetBits(edges_corners) >= 3 ||
         CountSetBits(edges_corners >> 6) >= 2;
}

namespace {

int AddChain(ChainNum chain, int num_neighbor_chains, ChainNum chain_set[4]) {
  assert(PlayerPosition::ChainLiesOnBoard(chain));
  for (int i = 0; i < num_neighbor_chains; ++i) {
    if (chain_set[i] == chain) {
      return num_neighbor_chains;
    }
  }
  chain_set[num_neighbor_chains] = chain;
  return num_neighbor_chains + 1;
}

}  // namespace

int PlayerPosition::GetSizeOfNeighborChains(
    Cell cell, int num_neighbors) const {
  assert(num_neighbors % 6 == 0);
  assert(num_neighbors >= 0);
  assert(num_neighbors <= 18);
  int num_neighbor_chains = 0;
  ChainNum chain_set[16];
  for (int i = 0; i < num_neighbors; ++i) {
    ChainNum chain = chain_for_cell(NthNeighbor(cell, i));
    if (ChainLiesOnBoard(chain)) {
      chain = chain_set_.NewestVersion(chain);
      num_neighbor_chains = AddChain(chain, num_neighbor_chains, chain_set);
    }
  }
  int size_of_neighbor_chains = 0;
  for (int i = 0; i < num_neighbor_chains; ++i) {
    assert(ChainLiesOnBoard(chain_set[i]));
    size_of_neighbor_chains += chain_set_.chain(chain_set[i])->num_stones();
  }
  return size_of_neighbor_chains;
}

void PlayerPosition::UpdateChainsToNewestVersionsReversibly(Memento* memento) {
  for (Cell cell = kZerothCell; cell < ARRAYSIZE(chains_for_cells_);
       cell = NextCell(cell)) {
    ChainNum chain = chain_for_cell(cell);
    if (ChainLiesOnBoard(chain)) {
      int newest_version = chain_set_.NewestVersion(chain);
      if (newest_version != chain) {
        memento->Remember(&chains_for_cells_[cell]);
        chains_for_cells_[cell] = newest_version;
      }
    }
  }
}

void PlayerPosition::CopyFrom(const PlayerPosition& other) {
  const int end = other.chain_set_.size();
  chain_set_.ShrinkTo(1);
  chain_set_.Reserve(end);
  for (int i = 1; i < end; ++i) {
    const Chain* p = other.chain_set_.chain(i);
    if (p != NULL) {
      if (p->newer_version() == kNullChain) {
        Chain* chain = chain_set_.allocator().MakeChain();
        chain->CopyFrom(*p);
        chain_set_.set_chain(i, chain);
      }
    }
  }
  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    const ChainNum chain = other.chain_for_cell(cell);
    if (chain == kNullChain) {
      chains_for_cells_[cell] = 0;
    } else {
      int newest_version = other.chain_set_.NewestVersion(chain);
      assert(chain_set_.chain(newest_version) != NULL);
      assert(chain_set_.chain(newest_version)->newer_version() == kNullChain);
      chains_for_cells_[cell] = newest_version;
    }
  }
  stone_mask_.CopyFrom(other.stone_mask());
  two_bridge_mask_.CopyFrom(other.two_bridge_mask());
  ring_db_.CopyFrom(other.ring_db_);
}

void PlayerPosition::GetCurrentChains(
    std::set<const Chain*>* current_chains) const {
  for (int i = kNumSpecialChains, size = chain_set_.size(); i < size; ++i) {
    if (chain_set_.chain(i) != NULL) {
      current_chains->insert(chain_set_.NewestVersion(chain_set_.chain(i)));
    }
  }
}

namespace {

typedef std::pair<int, Cell> DistanceCell;

class Queue {
 public:
  Queue() : back_(0), front_(0) {}

  bool empty() const { return (back_ == front_); }

  DistanceCell top() const { return queue_[back_]; }

  void pop() {
    assert(!empty());
    ++back_;
    if (back_ >= ARRAYSIZE(queue_))
      back_ = 0;
  }

  void push(const DistanceCell dc) {
    queue_[front_] = dc;
    ++front_;
    if (front_ >= ARRAYSIZE(queue_))
      front_ = 0;
    assert(front_ != back_);
  }

 private:
  int back_;
  int front_;
  DistanceCell queue_[kNumMovesOnBoard];

  Queue(const Queue&);
  void operator=(const Queue&);
};

ChainNum InitBfs(
    const PlayerPosition* pp,
    const BoardBitmask& start_stones,
    const BoardBitmask& opponent_stones,
    BfsResult* result,
    BfsResult* result_1,
    Queue* queue) {
  ChainNum start_chain = PlayerPosition::kNullChain;
  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    const XCoord x = CellToX(cell);
    const YCoord y = CellToY(cell);
    if (start_stones.get(x, y) && !opponent_stones.get(x, y)) {
      result->set(cell, 0);
      queue->push(DistanceCell(0, cell));
      start_chain = pp->chain_for_cell(cell);
    } else {
      result->set(cell, BfsResult::kMaxDistance);
      result_1->set(cell, BfsResult::kMaxDistance);
    }
  }
  if (start_chain != PlayerPosition::kNullChain) {
    start_chain = pp->NewestVersion(start_chain);
  }
  return start_chain;
}

void Bfs2(
    const PlayerPosition* pp,
    ChainNum start_chain,
    const BoardBitmask& opponent_stones,
    BfsResult* result,
    BfsResult* result_1,
    Queue* queue) {
  int distance_of_virtual_chains[PlayerPosition::kNumSpecialChains] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };
  while (!queue->empty()) {
    const DistanceCell nearest = queue->top();
    queue->pop();
    int distance = nearest.first;
    const Cell cell = nearest.second;
    if (distance > result->get(cell))
      continue;
    ChainNum chain = pp->chain_for_cell(cell);
    if (chain != PlayerPosition::kNullChain) {
      chain = pp->NewestVersion(chain);
      if (!PlayerPosition::ChainLiesOnBoard(chain) && chain != start_chain) {
        if (distance_of_virtual_chains[chain] == distance)
          continue;
        const BoardBitmask& stone_mask = pp->NthChain(chain)->stone_mask();
        for (Cell ccell = kZerothCell; ccell < kNumCellsWithSentinels;
             ccell = NextCell(ccell)) {
          const XCoord cx = CellToX(ccell);
          const YCoord cy = CellToY(ccell);
          if (stone_mask.get(cx, cy)) {
            result->set(ccell, distance);
          }
        }
        distance_of_virtual_chains[chain] = distance;
        continue;
      }
    } else {
      ++distance;
    }
    for (int i = 0; i < 6; ++i) {
      const Cell neighbor_cell = Position::GetNthNeighbor(cell, i);
      if (neighbor_cell == kZerothCell)
        break;
      const XCoord x = CellToX(neighbor_cell);
      const YCoord y = CellToY(neighbor_cell);
      if (opponent_stones.get(x, y))
        continue;
      if (chain != PlayerPosition::kNullChain) {
        if (result->get(neighbor_cell) > distance) {
          result->set(neighbor_cell, distance);
          queue->push(DistanceCell(distance, neighbor_cell));
        }
      } else if (result->get(neighbor_cell) > distance) {
        int distance_1 = result_1->get(neighbor_cell);
        if (distance_1 > distance) {
          result_1->set(neighbor_cell, distance);
          if (distance_1 < BfsResult::kMaxDistance) {
            result->set(neighbor_cell, distance_1);
            queue->push(DistanceCell(distance_1, neighbor_cell));
          }
        } else {
          result->set(neighbor_cell, distance);
          queue->push(DistanceCell(distance, neighbor_cell));
        }
      }
    }
  }
}

}  // namespace

void PlayerPosition::ComputeTwoDistance(
    const Chain* start_chain,
    const PlayerPosition& op,
    BfsResult* result) const {
  const BoardBitmask& start_stones = start_chain->stone_mask();
  const BoardBitmask& opponent_stones = op.stone_mask();
  BfsResult result_1;
  Queue queue;
  const ChainNum start_chain_num = InitBfs(
      this, start_stones, opponent_stones,
      result, &result_1, &queue);
  Bfs2(this, start_chain_num, opponent_stones,
      result, &result_1, &queue);
}

//-- Position ---------------------------------------------------------
uint64 Position::kEdgesCornersNeighbors[kNumCellsWithSentinels];
Cell Position::kConstMoveIndexToCell[kNumMovesOnBoard];
Cell Position::kMoveIndexToCell[kNumMovesOnBoard];
Cell Position::kNeighbors[kNumCellsWithSentinels][8];
MoveIndex Position::kConstCellToMoveIndex[kNumCellsWithSentinels];
MoveIndex Position::kCellToMoveIndex[kNumCellsWithSentinels];
Hash Position::kZobristHash[kNumMovesOnBoard][2];
BoardBitmask Position::kIsCellOnBoardBitmask;
BoardBitmask Position::kIsVirtualEdge;
BoardBitmask Position::kIsVirtualCorner;
Chain Position::kEdgeChains[6];
Chain Position::kCornerChains[6];
// The zero at position 63 is important for RingDB::VerifyCycle.
const unsigned char Position::kGroupCount[64] = {
  0, 1, 1, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 2, 1,
  1, 2, 2, 2, 1, 1, 2, 1, 2, 3, 2, 2, 2, 2, 2, 1,
  1, 2, 2, 2, 2, 2, 3, 2, 1, 2, 1, 1, 2, 2, 2, 1,
  1, 2, 2, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 0,
};
const unsigned char Position::kGroupCountWithPossibleBenzeneRings[64] = {
  0, 1, 1, 1, 1, 1, 2, 9, 1, 2, 1, 9, 2, 2, 2, 9,
  1, 2, 2, 2, 1, 9, 2, 9, 2, 3, 2, 9, 2, 9, 2, 9,
  1, 2, 2, 2, 2, 2, 3, 9, 1, 2, 9, 9, 2, 2, 9, 9,
  1, 2, 2, 2, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0,
};

void Position::InitStaticFields() {
  MoveIndex move = kZerothMove;
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      uint64 mask = 0ULL;
      if (LiesOnBoard(x, y)) {
        // Set up the mask of edges.
        if (x >= SIDE_LENGTH + kGapLeft && x < 2 * SIDE_LENGTH + kGapLeft - 2) {
          if (y == kGapAround)
            mask = (1u << 0) | ((65 * 0x30) << 16);
          if (x + y == 3 * SIDE_LENGTH + kGapLeft + kGapAround - 3)
            mask = (1u << 4) | ((65 * 0x0A) << 16);
        }
        if (x == kGapLeft && y >= SIDE_LENGTH + kGapAround &&
            y < 2 * SIDE_LENGTH + kGapAround - 2)
          mask = (1u << 2) | ((65 * 0x05) << 16);
        if (x > kGapLeft && x < SIDE_LENGTH + kGapLeft - 1) {
          if (y == 2 * SIDE_LENGTH + kGapAround - 2)
            mask = (1u << 3) | ((65 * 0x03) << 16);
          if (x + y == SIDE_LENGTH + kGapLeft + kGapAround - 1)
            mask = (1u << 1) | ((65 * 0x14) << 16);
        }
        if (x == 2 * SIDE_LENGTH + kGapLeft - 2 &&
            y > kGapAround && y < SIDE_LENGTH + kGapAround - 1)
          mask = (1u << 5) | ((65 * 0x28) << 16);

        // Set up the mask of corners.
        if (x == kGapLeft) {
          if (y == SIDE_LENGTH + kGapAround - 1)
            mask = (64u << 1) | ((65 * 0x15) << 16);
          if (y == 2 * SIDE_LENGTH + kGapAround - 2)
            mask = (64u << 2) | ((65 * 0x07) << 16);
        }
        if (x == SIDE_LENGTH + kGapLeft - 1) {
          if (y == kGapAround)
            mask = (64u << 0) | ((65 * 0x34) << 16);
          if (y == kGapAround + 2 * SIDE_LENGTH - 2)
            mask = (64u << 3) | ((65 * 0x0B) << 16);
        }
        if (x == 2 * SIDE_LENGTH + kGapLeft - 2) {
          if (y == kGapAround)
            mask = (64u << 5) | ((65 * 0x38) << 16);
          if (y == SIDE_LENGTH + kGapAround - 1)
            mask = (64u << 4) | ((65 * 0x2A) << 16);
        }
        const Cell cell = XYToCell(x, y);
        kConstMoveIndexToCell[move] = cell;
        kConstCellToMoveIndex[cell] = move;
        move = NextMove(move);
        kIsCellOnBoardBitmask.set(x, y);
      } else {
        const Cell cell = XYToCell(x, y);
        kConstCellToMoveIndex[cell] = kInvalidMove;
        kIsCellOnBoardBitmask.clear(x, y);
      }
      // Add the mask of 18-neighbors.
      static const int kOffset[18][2] = {
          { -2, +2 }, { -1, +2 }, { +0, +2 },
          { -2, +1 }, { -1, +1 }, { +0, +1 }, { +1, +1 },
          { -2, +0 }, { -1, +0 }, { +1, -0 }, { +2, -0 },
          { -1, -1 }, { -0, -1 }, { +1, -1 }, { +2, -1 },
          { -0, -2 }, { +1, -2 }, { +2, -2 },
      };
      uint64 mask18 = 0ULL;
      for (int i = 0; i < ARRAYSIZE(kOffset); ++i) {
        const XCoord nx = static_cast<XCoord>(x + kOffset[i][0]);
        const YCoord ny = static_cast<YCoord>(y + kOffset[i][1]);
        mask18 |= (!LiesOnBoard(nx, ny) << i);
      }
      kEdgesCornersNeighbors[XYToCell(x, y)] =
          (((1ULL << 18) + 1ULL) * (mask18 << 28)) | mask;
    }
  }
  assert(move == ARRAYSIZE(kMoveIndexToCell));

  static const int kInitializers[6][4] = {
      { kMiddleColumn,   kGapAround - 1, +1,  0 },
      { kGapLeft - 1,    kMiddleRow,     +1, -1 },
      { kGapLeft - 1,    kLastRow + 1,    0, -1 },
      { kMiddleColumn,   kLastRow + 1,   -1,  0 },
      { kLastColumn + 1, kMiddleRow,     -1, +1 },
      { kLastColumn + 1, kGapAround - 1,  0, +1 },
  };

  for (int i = 0; i < 6; ++i) {
    XCoord x = static_cast<XCoord>(kInitializers[i][0]);
    YCoord y = static_cast<YCoord>(kInitializers[i][1]);
    kCornerChains[i].InitWithStoneOutsideBoard(x, y);
    kIsVirtualCorner.set(x, y);
    x = static_cast<XCoord>(x + 2 * kInitializers[i][2]);
    y = static_cast<YCoord>(y + 2 * kInitializers[i][3]);
    kEdgeChains[i].InitWithStoneOutsideBoard(x, y);
    kIsVirtualEdge.set(x, y);
    for (int j = 3; j < SIDE_LENGTH - 1; ++j) {
      x = static_cast<XCoord>(x + kInitializers[i][2]);
      y = static_cast<YCoord>(y + kInitializers[i][3]);
      kEdgeChains[i].AddStoneOutsideBoard(x, y);
      kIsVirtualEdge.set(x, y);
    }
  }

  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    const XCoord x = CellToX(cell);
    const YCoord y = CellToY(cell);
    int p = 0;
    for (int i = 0; i < 6; ++i) {
      const Cell neighbor = NthNeighbor(cell, i);
      const XCoord nx = CellToX(neighbor);
      const YCoord ny = CellToY(neighbor);
      if ((kIsCellOnBoardBitmask.get(x, y) &&
           (kIsCellOnBoardBitmask.get(nx, ny) ||
            (IsEdgeCell(cell) && kIsVirtualEdge.get(nx, ny)) ||
            (IsCornerCell(cell) && kIsVirtualCorner.get(nx, ny)))) ||
          (kIsVirtualEdge.get(x, y) && IsEdgeCell(neighbor)) ||
          (kIsVirtualCorner.get(x, y) && IsCornerCell(neighbor))) {
        kNeighbors[cell][p] = neighbor;
        ++p;
      }
    }
    if (kIsCellOnBoardBitmask.get(x, y)) {
      if (IsEdgeCell(cell)) {
        assert(p == 5 || p == 6);
      } else if (IsCornerCell(cell)) {
        assert(p == 4);
      } else {
        assert(p == 6);
      }
    } else if (kIsVirtualEdge.get(x, y)) {
      assert(p == 1 || p == 2);
    } else if (kIsVirtualCorner.get(x, y)) {
      assert(p == 1);
    } else {
      assert(p == 0);
    }
  }

  srand48(time(NULL));
  for (MoveIndex mv = kZerothMove; mv < ARRAYSIZE(kZobristHash);
       mv = NextMove(mv)) {
    // Two consecutive XorShifts would not be linearly independent enough.
    // They cause hash collisions, manifesting in Lajkonik trying to move
    // into already occupied cells.
    kZobristHash[mv][kWhite] = (1ULL << 32) * mrand48() + mrand48();
    kZobristHash[mv][kBlack] = (1ULL << 32) * mrand48() + mrand48();
  }
}

Position::~Position() {
  for (int i = mementoes_.size() - 1; i >= 0; --i) {
    delete mementoes_[i];
  }
}

void Position::InitToStartPosition() {
  for (int i = 0; i < 6; ++i) {
    player_positions_[kWhite].AddChainOutsideBoard(GetEdgeChain(i));
    player_positions_[kWhite].AddChainOutsideBoard(GetCornerChain(i));
    player_positions_[kBlack].AddChainOutsideBoard(GetEdgeChain(i));
    player_positions_[kBlack].AddChainOutsideBoard(GetCornerChain(i));
  }
  num_available_moves_ = kNumMovesOnBoard;
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      Cell cell = XYToCell(x, y);
      cells_[cell] = LiesOnBoard(x, y) ? 0 : 3;
    }
  }
  assert(sizeof kMoveIndexToCell == sizeof kConstMoveIndexToCell);
  memcpy(kMoveIndexToCell, kConstMoveIndexToCell, sizeof kMoveIndexToCell);
  assert(sizeof kCellToMoveIndex == sizeof kConstCellToMoveIndex);
  memcpy(kCellToMoveIndex, kConstCellToMoveIndex, sizeof kCellToMoveIndex);
  is_initialized_ = true;
}

void Position::CopyFrom(const Position& other) {
  assert(other.is_initialized());
  player_positions_[kWhite].CopyFrom(other.player_positions_[kWhite]);
  player_positions_[kBlack].CopyFrom(other.player_positions_[kBlack]);
  memcpy(cells_, other.cells_, sizeof cells_);
  move_count_ = other.move_count_;
  num_available_moves_ = other.num_available_moves_;
  is_initialized_ = true;
}

void Position::SwapPlayers() {
  PlayerPosition tmp_player_position;
  tmp_player_position.CopyFrom(player_positions_[kWhite]);
  player_positions_[kWhite].CopyFrom(player_positions_[kBlack]);
  player_positions_[kBlack].CopyFrom(tmp_player_position);
  for (Cell cell = kZerothCell; cell < ARRAYSIZE(cells_);
       cell = NextCell(cell)) {
    if (!CellIsEmpty(cell) && (cells_[cell] & 3) != 3) {
      cells_[cell] = 3 - cells_[cell];
    }
  }
}

void Position::GetFreeCells(std::vector<Cell>* cells) const {
  cells->clear();
  for (MoveIndex move = kZerothMove; move < kNumMovesOnBoard;
       move = NextMove(move)) {
    const Cell cell = MoveIndexToCell(move);
    if (CellIsEmpty(cell))
      cells->push_back(cell);
  }
}

WinningCondition Position::MakeMoveReversibly(
    Player player, Cell cell, Memento* memento) {
  assert(is_initialized_);
  assert(CellIsEmpty(cell));
  memento->Remember(&cells_[cell]);
  cells_[cell] = player + 1;
  PlayerPosition& our = player_positions_[player];
  PlayerPosition& foe = player_positions_[Opponent(player)];
  our.RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(cell, memento);
  const WinningCondition result = our.MakeMoveReversibly(cell, memento);
  our.CreateTwoBridgesAfterOurMoveReversibly(cell, foe, memento);
  our.FindNewRingFramesReversibly(memento);
  foe.RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(cell, memento);
  memento->Remember(&move_count_);
  ++move_count_;
  return result;
}

WinningCondition Position::MakeMoveFast(Player player, Cell cell) {
  assert(is_initialized_);
  assert(CellIsEmpty(cell));
  cells_[cell] = player + 1;
  PlayerPosition& our = player_positions_[player];
  const WinningCondition result = our.MakeMoveFast(cell);
  return result;
}

WinningCondition Position::MakePermanentMove(Player player, Cell cell) {
  assert(is_initialized_);
  assert(CellIsEmpty(cell));
  Memento* memento = new Memento;
  memento->Remember(&cells_[cell]);
  cells_[cell] = player + 1;
  PlayerPosition& our = player_positions_[player];
  PlayerPosition& foe = player_positions_[Opponent(player)];
  our.RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(cell, memento);
  const WinningCondition result = our.MakeMoveReversibly(cell, memento);
  our.CreateTwoBridgesAfterOurMoveReversibly(cell, foe, memento);
  our.FindNewRingFramesReversibly(memento);
  our.UpdateChainsToNewestVersionsReversibly(memento);
  foe.RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(cell, memento);
  mementoes_.push_back(memento);
  past_moves_.resize(move_count_);
  past_moves_.push_back(std::make_pair(player, cell));
  num_available_moves_ = static_cast<MoveIndex>(num_available_moves_ - 1);
  const Cell swapped_cell = kMoveIndexToCell[num_available_moves_];
  const MoveIndex move = kCellToMoveIndex[cell];
  std::swap(kCellToMoveIndex[cell], kCellToMoveIndex[swapped_cell]);
  std::swap(kMoveIndexToCell[move], kMoveIndexToCell[num_available_moves_]);
  ++move_count_;
  return result;
}

bool Position::UndoPermanentMove() {
  assert(is_initialized_);
  if (mementoes_.empty())
    return false;
  mementoes_.back()->UndoAll();
  delete mementoes_.back();
  mementoes_.pop_back();
  assert(past_moves_.back().second == kMoveIndexToCell[num_available_moves_]);
  past_moves_.pop_back();
  num_available_moves_ = NextMove(num_available_moves_);
  --move_count_;
  return true;
}

Cell Position::MoveNPliesAgo(int plies) const {
  assert(is_initialized_);
  const int n = past_moves_.size() - plies - 1;
  return n >= 0 ? past_moves_.at(n).second : kBoardCenter;
}

bool Position::MoveIsWinning(Player player, Cell cell, int neighborhood,
                             ChainNum injected_chain) const {
  assert(is_initialized_);
  assert(LiesOnBoard(CellToX(cell), CellToY(cell)));
  const unsigned edges_corners = Position::GetMaskOfEdgesAndCorners(cell);
  const int neighbor_groups =
      CountNeighborGroupsWithPossibleBenzeneRings(neighborhood);
  const PlayerPosition& pp = player_position(player);
  if (neighbor_groups >= 2 &&
      pp.MoveWouldCloseForkBridgeOrRing(cell, edges_corners, injected_chain)) {
    return true;
  }
  if (neighbor_groups == 1 && edges_corners != 0 &&
      pp.MoveWouldCloseForkOrBridge(cell, edges_corners, injected_chain)) {
    return true;
  }
  return false;
}

bool Position::ParseString(const std::string& s) {
  InitToStartPosition();
  XCoord min_x = kMiddleColumn;
  XCoord max_x = kPastColumns;
  XCoord x = min_x;
  YCoord y = kGapAround;
  for (int i = 0, size = s.size(); i < size; ++i) {
    switch (s[i]) {
      case '.':
        x = NextX(x);
        break;
      case 'x':
        if (x < max_x) {
          MakeMoveFast(kWhite, XYToCell(x, y));
          x = NextX(x);
        }
        break;
      case 'o':
        if (x < max_x) {
          MakeMoveFast(kBlack, XYToCell(x, y));
          x = NextX(x);
        }
        break;
      case '\n':
        if (x == max_x) {
          if (y < kGapAround + SIDE_LENGTH - 1)
            min_x = static_cast<XCoord>(min_x - 1);
          else
            max_x = static_cast<XCoord>(max_x - 1);
          y = NextY(y);
          x = min_x;
        } else if (x != min_x) {
          InitToStartPosition();
          return false;
        }
        break;
      default:
        break;
    }
    if (y > kPastRows) {
      InitToStartPosition();
      return false;
    }
  }
  if (y < kPastRows) {
    InitToStartPosition();
    return false;
  }
  return true;
}

int Position::GetDistance(Cell cell1, Cell cell2) {
  XCoord x1 = CellToX(cell1);
  YCoord y1 = CellToY(cell1);
  int z1 = x1 + y1;
  XCoord x2 = CellToX(cell2);
  YCoord y2 = CellToY(cell2);
  int z2 = x2 + y2;
  return (abs(x1 - x2) + abs(y1 - y2) + abs(z1 - z2)) / 2;
}

//-- Memento ----------------------------------------------------------
void Memento::UndoAll() {
  for (std::vector<std::pair<unsigned*, unsigned> >::reverse_iterator rit =
       words_.rbegin(); rit != words_.rend(); ++rit) {
    *(rit->first) = rit->second;
  }
  words_.clear();
  for (std::vector<std::pair<ChainSet*, int> >::reverse_iterator rit =
       sizes_.rbegin(); rit != sizes_.rend(); ++rit) {
    rit->first->ShrinkTo(rit->second);
  }
  sizes_.clear();
}

//-- ChainAllocator ---------------------------------------------------
ChainAllocator::~ChainAllocator() {
  Node* next;
  for (Node* p = head_; p != NULL; p = next) {
    next = p->next;
    delete p;
  }
}

Chain* ChainAllocator::MakeChain() {
  if (head_ != NULL) {
    Node* first = head_;
    head_ = first->next;
    return reinterpret_cast<Chain*>(first);
  } else {
    return new Chain;
  }
}

void ChainAllocator::DeleteChain(Chain* chain)  {
  if (chain != NULL) {
    Node* first = reinterpret_cast<Node*>(chain);
    first->next = head_;
    head_ = first;
  }
}

//-- Arena ------------------------------------------------------------
Arena::Arena() : top_(0) {}

Arena::~Arena() {
  for (int i = 0, size = chunks_.size(); i < size; ++i) {
    delete[] chunks_[i];
  }
}

unsigned Arena::Allocate(int n) {
  assert(n <= kCellsInChunk);
  if ((top() + n) / kCellsInChunk >= chunks_.size()) {
    chunks_.push_back(new unsigned[kCellsInChunk]);
    top_ = (top() + n) / kCellsInChunk * kCellsInChunk;
  }
  int result = top();
  memset(&chunks_[result / kCellsInChunk][result % kCellsInChunk],
         0, n * sizeof(unsigned));
  top_ += n;
  return result;
}

// TODO(mciura): Avoid unnecessary deletes.
void Arena::CopyFrom(const Arena& other) {
  for (int i = 0, size = chunks_.size(); i < size; ++i) {
    delete[] chunks_[i];
  }
  chunks_.clear();
  for (int i = 0, size = other.chunks_.size(); i < size; ++i) {
    chunks_.push_back(new unsigned[kCellsInChunk]);
    memcpy(chunks_[i], other.chunks_[i], sizeof(unsigned[kCellsInChunk]));
  }
  top_ = other.top();
}

//-- RingDB -----------------------------------------------------------
RingDB::RingDB()
    : chain_graph_(arena_.Allocate(kChainNumLimit)),
      ring_frames_(arena_.Allocate(kMaxNumRingFrames)),
      ring_frames_top_(ring_frames_),
      ring_frames_through_cells_(arena_.Allocate(kNumMovesOnBoard)),
      changed_(false) {
  memset(blocked_bridges_, 0, sizeof blocked_bridges_);
}

void RingDB::AddTwoBridgeReversibly(
    Cell cell0, Cell cell1, ChainNum chain0, ChainNum chain1,
    Memento* memento) {
  if (!PlayerPosition::ChainLiesOnBoard(chain0) ||
      !PlayerPosition::ChainLiesOnBoard(chain1))
    return;
  memento->Remember(&arena_.top());
  if (static_cast<unsigned>(cell0) > static_cast<unsigned>(cell1))
    std::swap(cell0, cell1);
  memento->Remember(&arena_.get(chain_graph_ + chain0));
  AddOneWayTwoBridge(chain0, chain1, cell0, cell1);
  if (chain0 != chain1) {
    memento->Remember(&arena_.get(chain_graph_ + chain1));
    AddOneWayTwoBridge(chain1, chain0, cell0, cell1);
  }
  changed_ = true;
}

void RingDB::AddTwoBridgeFast(
    Cell cell0, Cell cell1, ChainNum chain0, ChainNum chain1) {
  if (!PlayerPosition::ChainLiesOnBoard(chain0) ||
      !PlayerPosition::ChainLiesOnBoard(chain1))
    return;
  if (static_cast<unsigned>(cell0) > static_cast<unsigned>(cell1))
    std::swap(cell0, cell1);
  AddOneWayTwoBridge(chain0, chain1, cell0, cell1);
  if (chain0 != chain1)
    AddOneWayTwoBridge(chain1, chain0, cell0, cell1);
  changed_ = true;
}

void RingDB::AddOneWayTwoBridge(
    ChainNum chain0, ChainNum chain1, Cell cell0, Cell cell1) {
  assert(cell0 < cell1);
  DUMP(printf(
      "Adding two-bridge %d-%d through %s, %s\n",
      chain0, chain1, ToString(cell0).c_str(), ToString(cell1).c_str()));
  unsigned p = arena_.Allocate(kCgSize);
  arena_.set(p, arena_.get(chain_graph_ + chain0));
  arena_.set(p + kCgChain, chain1);
  arena_.set(p + kCgCell0, cell0);
  arena_.set(p + kCgCell1, cell1);
  arena_.set(chain_graph_ + chain0, p);
}

void RingDB::AddRingFrameIndexToCell(
    Cell cell, int ring_frame_index, Memento* memento) {
  MoveIndex m = Position::CellToMoveIndex(cell);
  memento->Remember(&arena_.get(ring_frames_through_cells_ + m));
  unsigned p = arena_.Allocate(kRftcSize);
  arena_.set(p, arena_.get(ring_frames_through_cells_ + m));
  arena_.set(p + kRftcRingFrameIndex, ring_frame_index);
  arena_.set(ring_frames_through_cells_ + m, p);
}

void RingDB::RemoveHalfBridgeReversibly(
    Cell cell, ChainNum chain0, ChainNum chain1, Memento* memento) {
  if (!PlayerPosition::ChainLiesOnBoard(chain0) ||
      !PlayerPosition::ChainLiesOnBoard(chain1))
    return;
  RemoveOneWayTwoBridgesReversibly(chain0, chain1, cell, memento);
  if (chain0 != chain1)
    RemoveOneWayTwoBridgesReversibly(chain1, chain0, cell, memento);
  MoveIndex m = Position::CellToMoveIndex(cell);
  for (unsigned p = arena_.get(ring_frames_through_cells_ + m); p != 0;
       p = arena_.get(p)) {
    const unsigned n = arena_.get(p + kRftcRingFrameIndex);
    memento->Remember(&arena_.get(ring_frames_ + n));
    arena_.set(ring_frames_ + n, 0);
  }
  memento->Remember(&arena_.get(ring_frames_through_cells_ + m));
  arena_.set(ring_frames_through_cells_ + m, 0);
  changed_ = true;
}

void RingDB::RemoveHalfBridgeFast(
    Cell cell, ChainNum chain0, ChainNum chain1) {
  if (!PlayerPosition::ChainLiesOnBoard(chain0) ||
      !PlayerPosition::ChainLiesOnBoard(chain1))
    return;
  RemoveOneWayTwoBridgesFast(chain0, chain1, cell);
  if (chain0 != chain1)
    RemoveOneWayTwoBridgesFast(chain1, chain0, cell);
  MoveIndex m = Position::CellToMoveIndex(cell);
  for (unsigned p = arena_.get(ring_frames_through_cells_ + m); p != 0;
       p = arena_.get(p)) {
    const unsigned& n = arena_.get(p + kRftcRingFrameIndex);
    arena_.set(ring_frames_ + n, 0);
  }
  arena_.set(ring_frames_through_cells_ + m, 0);
  changed_ = true;
}

void RingDB::RemoveOneWayTwoBridgesReversibly(
    ChainNum chain0, ChainNum chain1, Cell cell, Memento* memento) {
  DUMP(printf("Removing two-bridge %d-%d through %s\n",
              chain0, chain1, ToString(cell).c_str()));
  unsigned prev = chain_graph_ + chain0;
  unsigned curr = arena_.get(prev);
  while (curr != 0) {
    if (arena_.get(curr + kCgChain) == chain1 &&
        (arena_.get(curr + kCgCell0) == static_cast<unsigned>(cell) ||
         arena_.get(curr + kCgCell1) == static_cast<unsigned>(cell))) {
      memento->Remember(&arena_.get(prev));
      arena_.set(prev, arena_.get(curr));
    }
    prev = curr;
    curr = arena_.get(prev);
  }
}

void RingDB::RemoveOneWayTwoBridgesFast(
    ChainNum chain0, ChainNum chain1, Cell cell) {
  unsigned prev = chain_graph_ + chain0;
  unsigned curr = arena_.get(prev);
  while (curr != 0) {
    if (arena_.get(curr + kCgChain) == chain1 &&
        (arena_.get(curr + kCgCell0) == static_cast<unsigned>(cell) ||
         arena_.get(curr + kCgCell1) == static_cast<unsigned>(cell))) {
      arena_.set(prev, arena_.get(curr));
    }
    prev = curr;
    curr = arena_.get(prev);
  }
}

void RingDB::MergeChainEdgesReversibly(
    ChainNum chain0, ChainNum chain1, const ChainSet& chain_set,
    Memento* memento) {
  assert(chain0 == chain_set.NewestVersion(chain0));
  assert(chain1 == chain_set.NewestVersion(chain1));
  if (chain0 == chain1)
    return;
  seen_two_bridges_.clear();
  ChainNum new_chain = chain_set.size();
  // List[new_chain] = List[chain0]
  const unsigned first = chain_graph_ + new_chain;
  unsigned prev = chain_graph_ + chain0;
  unsigned curr = arena_.get(prev);
  if (curr != 0) {
    memento->Remember(&arena_.get(first));
    arena_.set(first, curr);
    do {
      const unsigned ch = arena_.get(curr + kCgChain);
      const unsigned c0 = arena_.get(curr + kCgCell0);
      const unsigned c1 = arena_.get(curr + kCgCell1);
      const std::pair<unsigned, unsigned> two_bridge = std::make_pair(c0, c1);
      seen_two_bridges_.insert(two_bridge);
      if (ch == chain0 || ch == chain1) {
        memento->Remember(&arena_.get(curr + kCgChain));
        arena_.set(curr + kCgChain, new_chain);
      }
      // List[curr.chain].replace(chain0, new_chain)
      ReplaceChainInGraphReversibly(
          ch, chain0, new_chain, memento);
      prev = curr;
      curr = arena_.get(prev);
    } while (curr != 0);
  } else {
    prev = first;
  }
  // List[chain0] += List[chain1]
  memento->Remember(&arena_.get(prev));
  curr = arena_.get(chain_graph_ + chain1);
  arena_.set(prev, curr);
  while (curr != 0) {
    const unsigned ch = arena_.get(curr + kCgChain);
    const unsigned c0 = arena_.get(curr + kCgCell0);
    const unsigned c1 = arena_.get(curr + kCgCell1);
    const std::pair<unsigned, unsigned> two_bridge = std::make_pair(c0, c1);
    if (seen_two_bridges_.find(two_bridge) == seen_two_bridges_.end()) {
      if (ch == chain0 || ch == chain1) {
        memento->Remember(&arena_.get(curr + kCgChain));
        arena_.set(curr + kCgChain, new_chain);
      }
      // List[curr.chain].replace(chain1, new_chain)
      ReplaceChainInGraphReversibly(
          ch, chain1, new_chain, memento);
      prev = curr;
      curr = arena_.get(prev);
    } else {
      memento->Remember(&arena_.get(prev));
      curr = arena_.get(curr);
      arena_.set(prev, curr);
    }
  }
  changed_ = true;
}

void RingDB::MergeChainEdgesFast(
    ChainNum chain0, ChainNum chain1, const ChainSet& chain_set) {
  assert(chain0 == chain_set.NewestVersion(chain0));
  assert(chain1 == chain_set.NewestVersion(chain1));
  if (chain0 == chain1)
    return;
  seen_two_bridges_.clear();
  ChainNum new_chain = chain_set.size();
  // List[new_chain] = List[chain0]
  const unsigned first = chain_graph_ + new_chain;
  unsigned prev = chain_graph_ + chain0;
  unsigned curr = arena_.get(prev);
  if (curr != 0) {
    arena_.set(first, curr);
    do {
      const unsigned ch = arena_.get(curr + kCgChain);
      const unsigned c0 = arena_.get(curr + kCgCell0);
      const unsigned c1 = arena_.get(curr + kCgCell1);
      const std::pair<unsigned, unsigned> two_bridge = std::make_pair(c0, c1);
      seen_two_bridges_.insert(two_bridge);
      if (ch == chain0 || ch == chain1) {
        arena_.set(curr + kCgChain, new_chain);
      }
      // List[curr.chain].replace(chain0, new_chain)
      ReplaceChainInGraphFast(ch, chain0, new_chain);
      prev = curr;
      curr = arena_.get(prev);
    } while (curr != 0);
  } else {
    prev = first;
  }
  // List[chain0] += List[chain1]
  curr = arena_.get(chain_graph_ + chain1);
  arena_.set(prev, curr);
  while (curr != 0) {
    const unsigned ch = arena_.get(curr + kCgChain);
    const unsigned c0 = arena_.get(curr + kCgCell0);
    const unsigned c1 = arena_.get(curr + kCgCell1);
    const std::pair<unsigned, unsigned> two_bridge = std::make_pair(c0, c1);
    if (seen_two_bridges_.find(two_bridge) == seen_two_bridges_.end()) {
      if (ch == chain0 || ch == chain1) {
        arena_.set(curr + kCgChain, new_chain);
      }
      // List[curr.chain].replace(chain1, new_chain)
      ReplaceChainInGraphFast(ch, chain1, new_chain);
      prev = curr;
      curr = arena_.get(prev);
    } else {
      curr = arena_.get(curr);
      arena_.set(prev, curr);
    }
  }
  changed_ = true;
}

void RingDB::ReplaceChainInGraphReversibly(
    ChainNum chain, ChainNum old_chain, ChainNum new_chain, Memento* memento) {
  for (unsigned p = arena_.get(chain_graph_ + chain); p != 0;
       p = arena_.get(p)) {
    if (arena_.get(p + kCgChain) == old_chain) {
      memento->Remember(&arena_.get(p + kCgChain));
      arena_.set(p + kCgChain, new_chain);
    }
  }
}

void RingDB::ReplaceChainInGraphFast(
    ChainNum chain, ChainNum old_chain, ChainNum new_chain) {
  for (unsigned p = arena_.get(chain_graph_ + chain); p != 0;
       p = arena_.get(p)) {
    if (arena_.get(p + kCgChain) == old_chain) {
      arena_.set(p + kCgChain, new_chain);
    }
  }
}

void RingDB::FindNewCyclesReversibly(
    ChainNum modified_chain, const ChainSet& chain_set, Memento* memento) {
  if (changed_) {
    assert(path_.empty());
    assert(bridges_.empty());
    for (std::map<ChainNum, std::set<ChainNum> >::iterator it =
         b_sets_.begin(); it != b_sets_.end(); ++it) {
      it->second.clear();
    }
    memset(blocked_, 0, sizeof blocked_);
    FindCycles(modified_chain, modified_chain, chain_set, memento);
    changed_ = false;
  }
}

void RingDB::FindNewCyclesFast(
    ChainNum modified_chain, const ChainSet& chain_set) {
  Memento memento;
  FindNewCyclesReversibly(modified_chain, chain_set, &memento);
}

bool RingDB::FindCycles(
    ChainNum this_node, ChainNum start_node, const ChainSet& chain_set,
    Memento* memento) {
  bool closed = false;
  path_.push_back(this_node);
  blocked_[this_node] = true;
  for (unsigned p = arena_.get(chain_graph_ + this_node);
       p != 0; p = arena_.get(p)) {
    const ChainNum next_node = arena_.get(p + kCgChain);
    Cell c0 = static_cast<Cell>(arena_.get(p + kCgCell0));
    Cell c1 = static_cast<Cell>(arena_.get(p + kCgCell1));
    const MoveIndex m0 = Position::CellToMoveIndex(c0);
    const MoveIndex m1 = Position::CellToMoveIndex(c1);
    if (blocked_bridges_[m0] || blocked_bridges_[m1])
      continue;
    blocked_bridges_[m0] = blocked_bridges_[m1] = true;
    bridges_.push_back(std::make_pair(c0, c1));
    if (next_node == start_node) {
      VerifyCycle(chain_set, memento);
      closed = true;
    } else if (!blocked_[next_node]) {
      closed |= FindCycles(next_node, start_node, chain_set, memento);
    }
    bridges_.pop_back();
    blocked_bridges_[m0] = blocked_bridges_[m1] = false;
  }
  if (closed) {
    Unblock(this_node);
  } else {
    for (unsigned p = arena_.get(chain_graph_ + this_node);
         p != 0; p = arena_.get(p)) {
      const ChainNum next_node = arena_.get(p + kCgChain);
      b_sets_[next_node].insert(this_node);
    }
  }
  path_.pop_back();
  return closed;
}

void RingDB::Unblock(ChainNum this_node) {
  if (blocked_[this_node]) {
    blocked_[this_node] = false;
    std::set<ChainNum>& this_b_set = b_sets_[this_node];
    for (std::set<ChainNum>::iterator it = this_b_set.begin();
         it != this_b_set.end(); /**/) {
      Unblock(*it);
      std::set<ChainNum>::iterator bit = it;
      ++it;
      // By clause 23.1.2 of the C++ standard, the erase method shall
      // invalidate only iterators and references to the erased elements.
      this_b_set.erase(bit);
    }
  }
}

void RingDB::VerifyCycle(const ChainSet& chain_set, Memento* memento) {
  const int size = path_.size();
  assert(size != 0);
  assert(size == static_cast<int>(bridges_.size()));
  // Since Johnson's algorithm works on directed graphs, for our graphs
  // it yields each cycle twice, with opposite directions. The code below
  // rejects the instances with noncanonical ordering.
  if (size > 2) {
    if (path_[1] > path_.back())
      return;
  } else if (size == 2) {
    assert(bridges_[0].first < bridges_[0].second);
    assert(bridges_[1].first < bridges_[1].second);
    if (bridges_[0].first > bridges_[1].first)
      return;
  }
  stones_.CopyFrom(chain_set.chain(path_[0])->stone_mask());
  Cell cell;
  cell = bridges_[0].first;
  stones_.set(CellToX(cell), CellToY(cell));
  cell = bridges_[0].second;
  stones_.set(CellToX(cell), CellToY(cell));
  for (int i = 1; i < size; ++i) {
    stones_.FillWithOr(stones_, chain_set.chain(path_[i])->stone_mask());
    cell = bridges_[i].first;
    stones_.set(CellToX(cell), CellToY(cell));
    cell = bridges_[i].second;
    stones_.set(CellToX(cell), CellToY(cell));
  }
  static const int kOffsets[6][2] = {
      { 0, -1 }, { +1, -1 }, { -1, 0 }, { +1, 0 }, { -1, +1 }, { 0, +1 },
  };
  bool changed;
  do {
    changed = false;
    for (YCoord y = kGapAround; y < kPastRows; y = NextY(y)) {
      RowBitmask tmp_mask = stones_.Row(y);
      if (tmp_mask != 0) {
        XCoord xx = static_cast<XCoord>(CountTrailingZeroes(tmp_mask));
        tmp_mask >>= xx;
        YCoord yy = y;
        assert(stones_.get(xx, yy));
        const unsigned neighborhood = stones_.Get6Neighbors(xx, yy);
        if (Position::CountNeighborGroups(neighborhood) == 1) {
          stones_.clear(xx, yy);
          const unsigned neighbor = CountTrailingZeroes(neighborhood);
          xx = static_cast<XCoord>(xx + kOffsets[neighbor][0]);
          yy = static_cast<YCoord>(yy + kOffsets[neighbor][1]);
          changed = true;
        } else if (neighborhood == 0) {
          stones_.clear(xx, yy);
          changed = true;
        }
      }
    }
  } while (changed);
  RowBitmask all = 0;
  for (YCoord y = kGapAround; y < kPastRows; y = NextY(y)) {
    all |= stones_.Row(y);
  }
  if (all == 0)
    return;
  const int ring_frame_index = ring_frames_top_ - ring_frames_;
  assert(ring_frame_index < kMaxNumRingFrames);
  memento->Remember(&ring_frames_top_);
  memento->Remember(&arena_.top());
  unsigned p = arena_.Allocate(2 * size + 1);
  arena_.set(ring_frames_top_, p);
  ++ring_frames_top_;
  arena_.set(p, size);
  for (int i = 0; i < size; ++i) {
    ++p;
    arena_.set(p, bridges_[i].first);
    AddRingFrameIndexToCell(bridges_[i].first, ring_frame_index, memento);
    ++p;
    arena_.set(p, bridges_[i].second);
    AddRingFrameIndexToCell(bridges_[i].second, ring_frame_index, memento);
  }
}

void RingDB::CopyFrom(const RingDB& other) {
  arena_.CopyFrom(other.arena_);
  chain_graph_ = other.chain_graph_;
  ring_frames_ = other.ring_frames_;
  ring_frames_top_ = other.ring_frames_top_;
  ring_frames_through_cells_ = other.ring_frames_through_cells_;
  changed_ = other.changed_;
}

std::string RingDB::MakeString(const ChainSet& chain_set) const {
  std::string result;
  std::set<ChainNum> newest_versions;
  for (int i = 1, size = chain_set.size(); i < size; ++i) {
    newest_versions.insert(chain_set.NewestVersion(i));
  }
  for (std::set<ChainNum>::const_iterator it = newest_versions.begin();
       it != newest_versions.end(); ++it) {
    unsigned p = arena_.get(chain_graph_ + *it);
    if (p == 0)
      continue;
    result.append(StringPrintf("%d:", *it));
    while (p != 0) {
      result.append(StringPrintf(" %d(%s,%s)",
          arena_.get(p + kCgChain),
          ToString(static_cast<Cell>(arena_.get(p + kCgCell0))).c_str(),
          ToString(static_cast<Cell>(arena_.get(p + kCgCell1))).c_str()));
      p = arena_.get(p);
    }
    result.append("; ");
  }
  result.append("\n");
  return result;
}

}  // namespace lajkonik
