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

// Unit tests for havannah.cc

#include <string.h>
#include <set>
#include <string>

#include "fct.h"
#include "havannah.h"

using lajkonik::uint64;

using lajkonik::Player;
using lajkonik::XCoord;
using lajkonik::YCoord;
using lajkonik::Cell;
using lajkonik::PositionEvaluation;
using lajkonik::RowBitmask;
using lajkonik::BoardBitmask;
using lajkonik::ChainNum;
using lajkonik::Chain;
using lajkonik::ChainSet;
using lajkonik::PlayerPosition;
using lajkonik::Position;
using lajkonik::Memento;

using lajkonik::CountSetBits;
using lajkonik::CountTrailingZeroes;
using lajkonik::FromClassicalString;
using lajkonik::FromLittleGolemString;
using lajkonik::NextY;

using lajkonik::kWhite;
using lajkonik::kBlack;
using lajkonik::kZeroX;
using lajkonik::kGapLeft;
using lajkonik::kMiddleColumn;
using lajkonik::kThirtyTwoX;
using lajkonik::kZeroY;
using lajkonik::kGapAround;
using lajkonik::kMiddleRow;
using lajkonik::kPastRows;
using lajkonik::kBoardHeight;
using lajkonik::kZerothCell;
using lajkonik::kBoardCenter;
using lajkonik::kNumCellsWithSentinels;

using lajkonik::kNeighborOffsets;
using lajkonik::kReverseNeighborhoods;
using lajkonik::g_use_lg_coordinates;

STATIC_ASSERT(SIDE_LENGTH_must_equal_10, SIDE_LENGTH == 10);

namespace {

const char kBoard[kBoardHeight][32 + 1] = {
/*       01234567890123456789012345678901 */
/* 0 */ "................................",
/* 1 */ "................................",
/* 2 */ "...........A00000000F...........",
/* 3 */ "..........1#########5...........",
/* 4 */ ".........1##########5...........",
/* 5 */ "........1###########5...........",
/* 6 */ ".......1############5...........",
/* 7 */ "......1#############5...........",
/* 8 */ ".....1##############5...........",
/* 9 */ "....1###############5...........",
/* 0 */ "...1################5...........",
/* 1 */ "..B#################E...........",
/* 2 */ "..2################4............",
/* 3 */ "..2###############4.............",
/* 4 */ "..2##############4..............",
/* 5 */ "..2#############4...............",
/* 6 */ "..2############4................",
/* 7 */ "..2###########4.................",
/* 8 */ "..2##########4..................",
/* 9 */ "..2#########4...................",
/* 0 */ "..C33333333D....................",
/* 1 */ "................................",
/* 2 */ "................................",
/*       01234567890123456789012345678901 */
};

// Slow implementation of CountSetBits().
int SlowCountSetBits(int n) {
  int bits_set = 0;
  while (n != 0) {
    bits_set += n & 1;
    n >>= 1;
  }
  return bits_set;
}

// Slow implementation of CountTrailingZeroes().
int SlowCountTrailingZeroes(unsigned mask) {
  mask &= -mask;
  int trailing_zeroes = 0;
  for (unsigned x = 1; (mask & x) == 0; x <<= 1) {
    ++trailing_zeroes;
  }
  return trailing_zeroes;
}

// Slow implementation of Chain::UpdateRing().
int SlowRing(int mask) {
  int nonadjacent_stones = 0;
  bool previous_cell_is_occupied = mask & 32;
  for (int bit = 1; bit < 64; bit <<= 1) {
    if ((mask & bit) && !previous_cell_is_occupied)
      nonadjacent_stones++;
    previous_cell_is_occupied = mask & bit;
  }
  return (nonadjacent_stones >= 2);
}

}  // namespace

// Slow implementation of Position::Get18Neighbors() on an empty board.
uint64 SlowNeighbors(const Position& position, Player player, Cell cell) {
  const XCoord x = CellToX(cell);
  const YCoord y = CellToY(cell);

  const XCoord prev1_x = static_cast<XCoord>(x - 1);
  const XCoord prev2_x = static_cast<XCoord>(x - 2);
  const XCoord next1_x = NextX(x);
  const XCoord next2_x = NextX(next1_x);

  const YCoord prev1_y = PrevY(y);
  const YCoord prev2_y = PrevY(prev1_y);
  const YCoord next1_y = NextY(y);
  const YCoord next2_y = NextY(next1_y);

  uint64 result;
  result = (
      ((position.GetCell(XYToCell(prev2_x, next2_y)) == player + 1) << 0) |
      ((position.GetCell(XYToCell(prev1_x, next2_y)) == player + 1) << 1) |
      ((position.GetCell(XYToCell(x, next2_y)) == player + 1) << 2) |

      ((position.GetCell(XYToCell(prev2_x, next1_y)) == player + 1) << 3) |
      ((position.GetCell(XYToCell(prev1_x, next1_y)) == player + 1) << 4) |
      ((position.GetCell(XYToCell(x, next1_y)) == player + 1) << 5) |
      ((position.GetCell(XYToCell(next1_x, next1_y)) == player + 1) << 6) |

      ((position.GetCell(XYToCell(prev2_x, y)) == player + 1) << 7) |
      ((position.GetCell(XYToCell(prev1_x, y)) == player + 1) << 8) |
      ((position.GetCell(XYToCell(next1_x, y)) == player + 1) << 9) |
      ((position.GetCell(XYToCell(next2_x, y)) == player + 1) << 10) |

      ((position.GetCell(XYToCell(prev1_x, prev1_y)) == player + 1) << 11) |
      ((position.GetCell(XYToCell(x, prev1_y)) == player + 1) << 12) |
      ((position.GetCell(XYToCell(next1_x, prev1_y)) == player + 1) << 13) |
      ((position.GetCell(XYToCell(next2_x, prev1_y)) == player + 1) << 14) |

      ((position.GetCell(XYToCell(x, prev2_y)) == player + 1) << 15) |
      ((position.GetCell(XYToCell(next1_x, prev2_y)) == player + 1) << 16) |
      ((position.GetCell(XYToCell(next2_x, prev2_y)) == player + 1) << 17));

  result |= (1ULL << 18) * (
      ((position.GetCell(XYToCell(prev2_x, next2_y)) == 2 - player) << 0) |
      ((position.GetCell(XYToCell(prev1_x, next2_y)) == 2 - player) << 1) |
      ((position.GetCell(XYToCell(x, next2_y)) == 2 - player) << 2) |

      ((position.GetCell(XYToCell(prev2_x, next1_y)) == 2 - player) << 3) |
      ((position.GetCell(XYToCell(prev1_x, next1_y)) == 2 - player) << 4) |
      ((position.GetCell(XYToCell(x, next1_y)) == 2 - player) << 5) |
      ((position.GetCell(XYToCell(next1_x, next1_y)) == 2 - player) << 6) |

      ((position.GetCell(XYToCell(prev2_x, y)) == 2 - player) << 7) |
      ((position.GetCell(XYToCell(prev1_x, y)) == 2 - player) << 8) |
      ((position.GetCell(XYToCell(next1_x, y)) == 2 - player) << 9) |
      ((position.GetCell(XYToCell(next2_x, y)) == 2 - player) << 10) |

      ((position.GetCell(XYToCell(prev1_x, prev1_y)) == 2 - player) << 11) |
      ((position.GetCell(XYToCell(x, prev1_y)) == 2 - player) << 12) |
      ((position.GetCell(XYToCell(next1_x, prev1_y)) == 2 - player) << 13) |
      ((position.GetCell(XYToCell(next2_x, prev1_y)) == 2 - player) << 14) |

      ((position.GetCell(XYToCell(x, prev2_y)) == 2 - player) << 15) |
      ((position.GetCell(XYToCell(next1_x, prev2_y)) == 2 - player) << 16) |
      ((position.GetCell(XYToCell(next2_x, prev2_y)) == 2 - player) << 17));

  result |= ((1ULL << 18) + 1ULL) * (
      (!LiesOnBoard(prev2_x, next2_y) << 0) |
      (!LiesOnBoard(prev1_x, next2_y) << 1) |
      (!LiesOnBoard(x, next2_y) << 2) |

      (!LiesOnBoard(prev2_x, next1_y) << 3) |
      (!LiesOnBoard(prev1_x, next1_y) << 4) |
      (!LiesOnBoard(x, next1_y) << 5) |
      (!LiesOnBoard(next1_x, next1_y) << 6) |

      (!LiesOnBoard(prev2_x, y) << 7) |
      (!LiesOnBoard(prev1_x, y) << 8) |
      (!LiesOnBoard(next1_x, y) << 9) |
      (!LiesOnBoard(next2_x, y) << 10) |

      (!LiesOnBoard(prev1_x, prev1_y) << 11) |
      (!LiesOnBoard(x, prev1_y) << 12) |
      (!LiesOnBoard(next1_x, prev1_y) << 13) |
      (!LiesOnBoard(next2_x, prev1_y) << 14) |

      (!LiesOnBoard(x, prev2_y) << 15) |
      (!LiesOnBoard(next1_x, prev2_y) << 16) |
      (!LiesOnBoard(next2_x, prev2_y) << 17));
  return result;
}

FCT_BGN()

FCT_QTEST_BGN(CountSetBits_gives_correct_results)
  for (int i = 0; i < 64; ++i) {
    fct_xchk(CountSetBits(i) == SlowCountSetBits(i),
             "CountSetBits(%d) returns %d while %d was expected",
             i, CountSetBits(i), SlowCountSetBits(i));
  }
FCT_QTEST_END();

FCT_QTEST_BGN(CountTrailingZeroes_gives_correct_results)
  for (int i = 0; i < 32; ++i) {
    const unsigned x = (1U << i);
    fct_xchk(CountTrailingZeroes(x) == SlowCountTrailingZeroes(x),
             "CountTrailingZeroes(%d) returns %d while %d was expected",
             x, CountTrailingZeroes(x), SlowCountTrailingZeroes(x));
  }
FCT_QTEST_END();

FCT_QTEST_BGN(LiesOnBoard_gives_correct_results)
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      fct_xchk(LiesOnBoard(x, y) == (kBoard[y][x] != '.'),
               "LiesOnBoard(%d, %d) returns %d while the board has '%c'",
               x, y, LiesOnBoard(x, y), kBoard[y][x]);
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(kReverseNeighborhoods_matches_kKeighborOffsets)
  Position position;
  position.InitToStartPosition();
  const Cell d4 = FromClassicalString("d4");
  position.MakeMoveFast(kWhite, d4);
  for (int i = 0; i < 6; ++i) {
    const Cell neighbor = NthNeighbor(d4, i);
    fct_xchk(kReverseNeighborhoods[i] ==
             position.Get6Neighbors(kWhite, neighbor),
             "kReverseNeighborhoods[%d] equals %d instead of %d",
             i, kReverseNeighborhoods[i],
             position.Get6Neighbors(kWhite, neighbor));
  }
FCT_QTEST_END();

FCT_QTEST_BGN(FromClassicalString_reverses_ToClassicalString)
  fct_chk_eq_int(
      FromClassicalString("a1"),
      32 * (kGapAround + 2 * SIDE_LENGTH - 2) + kGapLeft);
  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    if (!LiesOnBoard(CellToX(cell), CellToY(cell)))
      continue;
    std::string encoded = ToClassicalString(cell);
    fct_xchk(FromClassicalString(encoded) == cell,
             "FromClassicalString(%s) returns %d while %d was expected",
             encoded.c_str(), FromClassicalString(encoded), cell);
  }
FCT_QTEST_END();

FCT_QTEST_BGN(FromLittleGolemString_reverses_ToLittleGolemString)
  fct_chk_eq_int(
      FromLittleGolemString("a1"),
      32 * (kGapAround + 2 * SIDE_LENGTH - 2) + kGapLeft);
  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    if (!LiesOnBoard(CellToX(cell), CellToY(cell)))
      continue;
    std::string encoded = ToLittleGolemString(cell);
    fct_xchk(
        FromLittleGolemString(encoded) == cell,
        "FromLittleGolemString(%s) returns %d while %d was expected",
        encoded.c_str(), FromLittleGolemString(encoded), cell);
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Memento_undoes_assignments)
  static unsigned locations[2] = { 1, 2 };
  Memento memento;
  memento.Remember(&locations[0]);
  locations[0]++;
  memento.Remember(&locations[1]);
  locations[1]++;
  memento.Remember(&locations[0]);
  locations[0]++;
  fct_chk_eq_int(locations[0], 3);
  fct_chk_eq_int(locations[1], 3);
  memento.UndoAll();
  fct_chk_eq_int(locations[0], 1);
  fct_chk_eq_int(locations[1], 2);
FCT_QTEST_END();

FCT_QTEST_BGN(Memento_shrinks_ChainSets)
  ChainSet chain_sets[2];
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_sets[0].PushBackChainOutsideBoard(NULL);
    chain_sets[1].PushBackChainOutsideBoard(NULL);
  }
  Memento memento;
  memento.RememberSize(&chain_sets[0]);
  chain_sets[0].MakeOneStoneChain(
     static_cast<XCoord>(10), static_cast<YCoord>(10));
  memento.RememberSize(&chain_sets[1]);
  chain_sets[1].MakeOneStoneChain(
     static_cast<XCoord>(12), static_cast<YCoord>(12));
  memento.RememberSize(&chain_sets[0]);
  chain_sets[0].MakeOneStoneChain(
     static_cast<XCoord>(11), static_cast<YCoord>(11));
  fct_chk_eq_int(chain_sets[0].size(), PlayerPosition::kNumSpecialChains + 2);
  fct_chk_eq_int(chain_sets[1].size(), PlayerPosition::kNumSpecialChains + 1);
  memento.UndoAll();
  fct_chk_eq_int(chain_sets[0].size(), PlayerPosition::kNumSpecialChains);
  fct_chk_eq_int(chain_sets[1].size(), PlayerPosition::kNumSpecialChains);
FCT_QTEST_END();

FCT_QTEST_BGN(Memento_forgets_undone_changes)
  unsigned location = 1;
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento;
  memento.Remember(&location);
  memento.RememberSize(&chain_set);
  location++;
  chain_set.MakeOneStoneChain(static_cast<XCoord>(7), static_cast<YCoord>(7));
  fct_chk_eq_int(location, 2);
  fct_chk_eq_int(chain_set.size(), PlayerPosition::kNumSpecialChains + 1);
  memento.UndoAll();
  fct_chk_eq_int(location, 1);
  fct_chk_eq_int(chain_set.size(), PlayerPosition::kNumSpecialChains);
  location++;
  chain_set.MakeOneStoneChain(static_cast<XCoord>(7), static_cast<YCoord>(7));
  memento.UndoAll();
  fct_chk_eq_int(location, 2);
  fct_chk_eq_int(chain_set.size(), PlayerPosition::kNumSpecialChains + 1);
FCT_QTEST_END();

FCT_QTEST_BGN(ChainSet_sets_board_correctly)
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento;
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      if (!LiesOnBoard(x, y))
        continue;
      memento.RememberSize(&chain_set);
      int chain = chain_set.MakeOneStoneChain(x, y);
      fct_xchk(chain_set.stone_mask(chain).Row(y) == 1u << x,
               "stone_mask(%d, %d) returns 0x%x instead of 0x%x",
               x, y, chain_set.stone_mask(chain).Row(y), 1u << x);
      memento.UndoAll();
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(ChainSet_sets_edges_correctly)
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento;
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      if (!LiesOnBoard(x, y))
        continue;
      memento.RememberSize(&chain_set);
      int chain = chain_set.MakeOneStoneChain(x, y);
      if (kBoard[y][x] >= '0' && kBoard[y][x] <= '5') {
        fct_xchk(chain_set.edges(chain) == 1u << (kBoard[y][x] - '0'),
                 "edges(%d, %d) returns 0x%x instead of 0x%x",
                 x, y, chain_set.edges(chain),
                 1u << (kBoard[y][x] - '0'));
      } else {
        fct_xchk(chain_set.edges(chain) == 0,
                 "edges(%d, %d) returns 0x%x instead of 0",
                 x, y, chain_set.edges(chain));
      }
      memento.UndoAll();
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(ChainSet_sets_corners_correctly)
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento;
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      if (!LiesOnBoard(x, y))
        continue;
      memento.RememberSize(&chain_set);
      int chain = chain_set.MakeOneStoneChain(x, y);
      if (kBoard[y][x] >= 'A' && kBoard[y][x] <= 'F') {
        fct_xchk(chain_set.corners(chain) == 1u << (kBoard[y][x] - 'A'),
                 "corners(%d, %d) returns 0x%x instead of 0x%x",
                 x, y, chain_set.corners(chain),
                 1u << (kBoard[y][x] - 'A'));
      } else {
        fct_xchk(chain_set.corners(chain) == 0,
                 "corners(%d, %d) returns 0x%x instead of 0",
                 x, y, chain_set.corners(chain));
      }
      memento.UndoAll();
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(ChainSet_sets_thin_ring_correctly)
  static const int kDx[6] = { 0, -1, -1, 0, +1, +1 };
  static const int kDy[6] = { -1, 0, +1, +1, 0, -1 };
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento, reverter;
  const XCoord x = kMiddleColumn;
  const YCoord y = kMiddleRow;
  for (int mask = 0; mask < 64; ++mask) {
    memento.RememberSize(&chain_set);
    int index1 = chain_set.MakeOneStoneChain(
        static_cast<XCoord>(15), static_cast<YCoord>(15));
    int index2 = chain_set.MakeOneStoneChain(
        static_cast<XCoord>(15), static_cast<YCoord>(15));
    // Set up the neighborhood.
    for (int r = 0; r < 6; ++r) {
      if (mask & (1 << r))
        chain_set.AddStoneToChainReversibly(
            static_cast<XCoord>(x + kDx[r]),
            static_cast<YCoord>(y + kDy[r]), index1, &memento);
    }
    if (chain_set.ring(index1)) {
      memento.UndoAll();
      index1 = chain_set.MakeOneStoneChain(
          static_cast<XCoord>(15), static_cast<YCoord>(15));
      index2 = chain_set.MakeOneStoneChain(
          static_cast<XCoord>(15), static_cast<YCoord>(15));
      // Set up the neighborhood.
      for (int r = 3; r < 6; ++r) {
        if (mask & (1 << r))
          chain_set.AddStoneToChainReversibly(
              static_cast<XCoord>(x + kDx[r]),
              static_cast<YCoord>(y + kDy[r]), index1, &memento);
      }
      for (int r = 0; r < 3; ++r) {
        if (mask & (1 << r))
          chain_set.AddStoneToChainReversibly(
              static_cast<XCoord>(x + kDx[r]),
              static_cast<YCoord>(y + kDy[r]), index1, &memento);
      }
    }
    if (chain_set.ring(index1)) {
      fct_chk_eq_int(mask, 0x3f);
      continue;
    }
    fct_chk(!chain_set.ring(index1));
    // Check if the central stone closes a ring in chain 1.
    chain_set.AddStoneToChainReversibly(x, y, index1, &reverter);
    fct_xchk(chain_set.ring(index1) == SlowRing(mask),
             "ring(0x%x) returns %d instead of %d",
             mask, chain_set.ring(index1), SlowRing(mask));
    // Remove (x, y) from chain 1, put it into chain 2, merge them.
    reverter.UndoAll();
    chain_set.AddStoneToChainReversibly(x, y, index2, &memento);
    int index = chain_set.MergeChainsReversibly(
        x, y, index2, index1, &memento);
    fct_xchk(chain_set.ring(index) == SlowRing(mask),
             "ring(0x%x) returns %d instead of %d",
             mask, chain_set.ring(index), SlowRing(mask));
    memento.UndoAll();
  }
FCT_QTEST_END();

FCT_QTEST_BGN(ChainSet_sets_dumpling_ring_correctly)
  static const int kDx[6] = { 0, -1, -1, 0, +1, +1 };
  static const int kDy[6] = { -1, 0, +1, +1, 0, -1 };
  ChainSet chain_set;
  for (int i = 1; i < PlayerPosition::kNumSpecialChains; ++i) {
    chain_set.PushBackChainOutsideBoard(NULL);
  }
  Memento memento, reverter;
  const XCoord x = kMiddleColumn;
  const YCoord y = kMiddleRow;
  for (int r = 0; r < 6; ++r) {
    memento.RememberSize(&chain_set);
    int index = chain_set.MakeOneStoneChain(
        static_cast<XCoord>(x + kDx[r]),
        static_cast<YCoord>(y + kDy[r]));
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 5) % 6]),
        static_cast<YCoord>(y + kDy[(r + 5) % 6]),
        index, &memento);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 1) % 6]),
        static_cast<YCoord>(y + kDy[(r + 1) % 6]),
        index, &memento);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[r] + kDx[(r + 5) % 6]),
        static_cast<YCoord>(y + kDy[r] + kDy[(r + 5) % 6]),
        index, &memento);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[r] + kDx[r]),
        static_cast<YCoord>(y + kDy[r] + kDy[r]),
        index, &memento);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[r] + kDx[(r + 1) % 6]),
        static_cast<YCoord>(y + kDy[r] + kDy[(r + 1) % 6]),
        index, &memento);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(x, y, index, &reverter);
    fct_xchk(chain_set.ring(index), "ring(%d) is false", r);
    reverter.UndoAll();

    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 4) % 6]),
        static_cast<YCoord>(y + kDy[(r + 4) % 6]),
        index, &reverter);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(x, y, index, &reverter);
    fct_xchk(chain_set.ring(index), "ring(%d) is false", r);
    reverter.UndoAll();

    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 2) % 6]),
        static_cast<YCoord>(y + kDy[(r + 2) % 6]),
        index, &reverter);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(x, y, index, &reverter);
    fct_xchk(chain_set.ring(index), "ring(%d) is false", r);
    reverter.UndoAll();

    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 4) % 6]),
        static_cast<YCoord>(y + kDy[(r + 4) % 6]),
        index, &reverter);
    chain_set.AddStoneToChainReversibly(
        static_cast<XCoord>(x + kDx[(r + 2) % 6]),
        static_cast<YCoord>(y + kDy[(r + 2) % 6]),
        index, &reverter);
    fct_xchk(!chain_set.ring(index), "ring(%d) is true", r);
    chain_set.AddStoneToChainReversibly(x, y, index, &reverter);
    fct_xchk(chain_set.ring(index), "ring(%d) is false", r);
    reverter.UndoAll();
    memento.UndoAll();
  }
FCT_QTEST_END();

FCT_QTEST_BGN(PlayerPosition_correctly_updates_chains_to_newest_versions)
  PlayerPosition player_position;
  for (int i = 0; i < 6; ++i) {
    player_position.AddChainOutsideBoard(Position::GetEdgeChain(i));
    player_position.AddChainOutsideBoard(Position::GetCornerChain(i));
  }
  Memento memento;
  const Cell a1 = FromClassicalString("a1");
  const Cell a2 = FromClassicalString("a2");
  const Cell a3 = FromClassicalString("a3");
  const Cell a4 = FromClassicalString("a4");
  player_position.MakeMoveReversibly(a1, &memento);
  player_position.MakeMoveReversibly(a3, &memento);
  player_position.MakeMoveReversibly(a4, &memento);
  player_position.MakeMoveReversibly(a2, &memento);
  /*const ChainNo ch1 =*/ player_position.chain_for_cell(a1);
  const ChainNum ch2 = player_position.chain_for_cell(a3);
  const ChainNum ch3 = player_position.chain_for_cell(a2);
  fct_chk(player_position.chain_for_cell(a4) == ch2);
  player_position.UpdateChainsToNewestVersionsReversibly(&memento);
  fct_chk(player_position.chain_for_cell(a1) == ch3);
  fct_chk(player_position.chain_for_cell(a2) == ch3);
  fct_chk(player_position.chain_for_cell(a3) == ch3);
  fct_chk(player_position.chain_for_cell(a4) == ch3);
FCT_QTEST_END();

FCT_QTEST_BGN(PlayerPosition_MoveWouldCloseForkBridgeOrRing_sees_rings)
  PlayerPosition player_position;
  for (int i = 0; i < 6; ++i) {
    player_position.AddChainOutsideBoard(Position::GetEdgeChain(i));
    player_position.AddChainOutsideBoard(Position::GetCornerChain(i));
  }
  player_position.MakeMoveFast(FromClassicalString("d4"));
  player_position.MakeMoveFast(FromClassicalString("d5"));
  player_position.MakeMoveFast(FromClassicalString("f6"));
  player_position.MakeMoveFast(FromClassicalString("f5"));
  player_position.MakeMoveFast(FromClassicalString("e6"));
  const Cell cell = FromClassicalString("e4");
  fct_chk_eq_int(
      player_position.MoveWouldCloseForkBridgeOrRing(
          cell, Position::GetMaskOfEdgesAndCorners(cell), 0),
      true);
FCT_QTEST_END();

FCT_QTEST_BGN(PlayerPosition_MoveWouldCloseForkBridgeOrRing_sees_filled_rings)
  PlayerPosition(player_position);
  for (int i = 0; i < 6; ++i) {
    player_position.AddChainOutsideBoard(Position::GetEdgeChain(i));
    player_position.AddChainOutsideBoard(Position::GetCornerChain(i));
  }
  player_position.MakeMoveFast(FromClassicalString("d4"));
  player_position.MakeMoveFast(FromClassicalString("d5"));
  player_position.MakeMoveFast(FromClassicalString("f6"));
  player_position.MakeMoveFast(FromClassicalString("f5"));
  player_position.MakeMoveFast(FromClassicalString("e6"));
  player_position.MakeMoveFast(FromClassicalString("e5"));
  const Cell cell = FromClassicalString("e4");
  fct_chk_eq_int(
      player_position.MoveWouldCloseForkBridgeOrRing(
          cell, Position::GetMaskOfEdgesAndCorners(cell), 0),
      true);
FCT_QTEST_END();

FCT_QTEST_BGN(Position_kNeighbors_are_initialized_correctly)
  for (Cell cell = kZerothCell; cell < kNumCellsWithSentinels;
       cell = NextCell(cell)) {
    for (int i = 0; i < 6; ++i) {
      const Cell neighbor = Position::GetNthNeighbor(cell, i);
      if (neighbor == 0)
        break;
      bool found = false;
      for (int j = 0; j < 6; ++j) {
        if (cell == Position::GetNthNeighbor(neighbor, j)) {
          found = true;
          break;
        }
      }
      fct_xchk(found, "Expected %s-%s-%s, not found the right part",
               ToString(cell).c_str(), ToString(neighbor).c_str(),
               ToString(cell).c_str());
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Position_moves_are_remembered_correctly)
  Position position;
  position.InitToStartPosition();
  const Cell a1 = FromClassicalString("a1");
  const Cell a2 = FromClassicalString("a2");
  const Cell a3 = FromClassicalString("a3");

  fct_chk_eq_int(position.MoveNPliesAgo(0), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(1), kBoardCenter);
  fct_chk(position.CellIsEmpty(a1));
  position.MakePermanentMove(kWhite, a1);
  fct_chk(!position.CellIsEmpty(a1));

  fct_chk_eq_int(position.MoveNPliesAgo(0), a1);
  fct_chk_eq_int(position.MoveNPliesAgo(1), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(2), kBoardCenter);
  fct_chk(position.CellIsEmpty(a2));
  position.MakePermanentMove(kBlack, a2);
  fct_chk(!position.CellIsEmpty(a2));

  fct_chk_eq_int(position.MoveNPliesAgo(0), a2);
  fct_chk_eq_int(position.MoveNPliesAgo(1), a1);
  fct_chk_eq_int(position.MoveNPliesAgo(2), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(3), kBoardCenter);
  fct_chk(position.CellIsEmpty(a3));
  position.MakePermanentMove(kWhite, a3);
  fct_chk(!position.CellIsEmpty(a3));

  fct_chk_eq_int(position.MoveNPliesAgo(0), a3);
  fct_chk_eq_int(position.MoveNPliesAgo(1), a2);
  fct_chk_eq_int(position.MoveNPliesAgo(2), a1);
  fct_chk_eq_int(position.MoveNPliesAgo(3), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(4), kBoardCenter);
  fct_chk(!position.CellIsEmpty(a3));
  fct_chk(position.UndoPermanentMove());
  fct_chk(position.CellIsEmpty(a3));

  fct_chk_eq_int(position.MoveNPliesAgo(0), a2);
  fct_chk_eq_int(position.MoveNPliesAgo(1), a1);
  fct_chk_eq_int(position.MoveNPliesAgo(2), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(3), kBoardCenter);
  fct_chk(!position.CellIsEmpty(a2));
  fct_chk(position.UndoPermanentMove());
  fct_chk(position.CellIsEmpty(a2));

  fct_chk_eq_int(position.MoveNPliesAgo(0), a1);
  fct_chk_eq_int(position.MoveNPliesAgo(1), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(2), kBoardCenter);
  fct_chk(!position.CellIsEmpty(a1));
  fct_chk(position.UndoPermanentMove());
  fct_chk(position.CellIsEmpty(a1));

  fct_chk_eq_int(position.MoveNPliesAgo(0), kBoardCenter);
  fct_chk_eq_int(position.MoveNPliesAgo(1), kBoardCenter);
  fct_chk(!position.UndoPermanentMove());
FCT_QTEST_END();

#if 0
FCT_QTEST_BGN(Position_CellHasNeighbors_gives_correct_results)
  Position position;
  position.InitToStartPosition();
  static const char* neighbors[19] = {
    "j12", "k12", "l12",
    "i11", "j11", "k11", "l11",
    "h10", "i10", "j10", "k10", "l10",
    "h9", "i9", "j9", "k9",
    "h8", "i8", "j8",
  };
  static const char* nonneighbors[18] = {
    "j13", "k13", "l13", "m13",
    "i12", "m12",
    "h11", "m11",
    "g10", "m10",
    "g9", "l9",
    "g8", "k8",
    "g7", "h7", "i7", "j7",
  };
  for (int i = 0; i < ARRAYSIZE(neighbors); ++i) {
    fct_xchk(!position.CellHasNeighbors(FromClassicalString(neighbors[i])),
             "Expected false at cell %s; got true", neighbors[i]);
  }
  for (int i = 0; i < ARRAYSIZE(nonneighbors); ++i) {
    fct_xchk(!position.CellHasNeighbors(FromClassicalString(nonneighbors[i])),
             "Expected false at cell %s; got true", nonneighbors[i]);
  }
  Memento memento;
  position.MakeMoveReversibly(kWhite, FromClassicalString("j10"), &memento);
  for (int i = 0; i < ARRAYSIZE(neighbors); ++i) {
    fct_xchk(position.CellHasNeighbors(FromClassicalString(neighbors[i])),
             "Expected true at cell %s; got false", neighbors[i]);
  }
  for (int i = 0; i < ARRAYSIZE(nonneighbors); ++i) {
    fct_xchk(!position.CellHasNeighbors(FromClassicalString(nonneighbors[i])),
             "Expected false at cell %s; got true", nonneighbors[i]);
  }
  memento.UndoAll();
  for (int i = 0; i < ARRAYSIZE(neighbors); ++i) {
    fct_xchk(!position.CellHasNeighbors(FromClassicalString(neighbors[i])),
             "Expected false at cell %s; got true", neighbors[i]);
  }
  for (int i = 0; i < ARRAYSIZE(nonneighbors); ++i) {
    fct_xchk(!position.CellHasNeighbors(FromClassicalString(nonneighbors[i])),
             "Expected false at cell %s; got true", nonneighbors[i]);
  }
FCT_QTEST_END();
#endif  // 0

FCT_QTEST_BGN(Position_Get6Neighbors_gives_correct_results)
  Position position;
  position.InitToStartPosition();
  const Cell d4 = FromClassicalString("d4");
  static const int kMyOffsets[] = { +31, +32, -1, +1, -32, -31 };
  for (unsigned mask = 0; mask < 64; ++mask) {
    Memento memento;
    for (int i = 0; i < 6; ++i) {
      if (mask & (1 << i)) {
        position.MakeMoveReversibly(
            kWhite, OffsetCell(d4, kMyOffsets[i]), &memento);
      }
    }
    fct_xchk(position.Get6Neighbors(kWhite, d4) == mask,
             "GetNeighborhood returns %d instead of %d",
             position.Get6Neighbors(kWhite, d4), mask);
    memento.UndoAll();
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Position_GetDistance_gives_correct_results)
  static const struct {
    const char* cell;
    int distance;
  } test_data[19] = {
    { "d4", 0 },
    { "d3", 1 }, { "c3", 1 }, { "c4", 1 },
    { "d5", 1 }, { "e5", 1 }, { "e4", 1 },
    { "d2", 2 }, { "c2", 2 }, { "b2", 2 },
    { "b3", 2 }, { "b4", 2 }, { "c5", 2 },
    { "d6", 2 }, { "e6", 2 }, { "f6", 2 },
    { "f5", 2 }, { "f4", 2 }, { "e3", 2 },
  };
  const Cell d4 = FromClassicalString("d4");
  for (int i = 0; i < ARRAYSIZE(test_data); ++i) {
    const Cell cell = FromClassicalString(test_data[i].cell);
    const int distance = test_data[i].distance;
    fct_xchk(Position::GetDistance(d4, cell) == distance,
             "Position::GetDistance(d4, %s) returns %d instead of %d",
             test_data[i].cell, Position::GetDistance(d4, cell), distance);
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Position_GetBoardBitmask_gives_correct_results)
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      fct_xchk(
          ((Position::GetBoardBitmask().Row(y) >> x) & 1) == LiesOnBoard(x, y),
          "(Position::GetBoardBitmask().Row(%d) >> %d) & 1 returns %d "
          "instead of %d", y, x, (Position::GetBoardBitmask().Row(y) >> x) & 1,
          LiesOnBoard(x, y));
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Position_Get18Neighbors_returns_correct_results)
  Position position;
  position.InitToStartPosition();
  position.MakeMoveFast(kWhite, FromClassicalString("d4"));
  position.MakeMoveFast(kBlack, FromClassicalString("e4"));
  for (YCoord y = kZeroY; y < kBoardHeight; y = NextY(y)) {
    for (XCoord x = kZeroX; x < kThirtyTwoX; x = NextX(x)) {
      if (!LiesOnBoard(x, y))
        continue;
      const Cell cell = XYToCell(x, y);
      fct_xchk(position.Get18Neighbors(kWhite, cell) ==
               SlowNeighbors(position, kWhite, cell),
               "Get18Neighbors(%d, %d) returns %llx while %llx was expected",
               x, y, position.Get18Neighbors(kWhite, cell),
               SlowNeighbors(position, kWhite, cell));
    }
  }
FCT_QTEST_END();

FCT_QTEST_BGN(Position_ParseString_gives_correct_results)
  Position position;
  position.InitToStartPosition();
  position.MakeMoveFast(kWhite, FromClassicalString("d7"));
  position.MakeMoveFast(kBlack, FromClassicalString("j3"));
  bool remember_coordinate_system = g_use_lg_coordinates;

  g_use_lg_coordinates = false;
  std::string s1 = position.MakeString();
  Position position1;
  const bool result1 = position1.ParseString(s1);
  fct_chk_eq_int(result1, true);
  fct_xchk(position1.MakeString() == s1, "%s%s",
           position1.MakeString().c_str(), s1.c_str());

  g_use_lg_coordinates = true;
  std::string s2 = position.MakeString();
  Position position2;
  const bool result2 = position2.ParseString(s2);
  fct_chk_eq_int(result2, true);
  fct_xchk(position2.MakeString() == s2, "%s%s",
           position2.MakeString().c_str(), s2.c_str());

  g_use_lg_coordinates = remember_coordinate_system;
FCT_QTEST_END();

FCT_END();
