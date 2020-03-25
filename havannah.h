#ifndef HAVANNAH_H_
#define HAVANNAH_H_

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

// Declarations of basic functions, global variables, and classes
// for the game of Havannah. The file is divided into four parts:
//
// I. Simple types
// II. Basic functions and global variables
// III. Essential classes
// IV. Auxiliaries

#include <stddef.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base.h"

namespace lajkonik {

STATIC_ASSERT(SIDE_LENGTH_must_be_greater_or_equal_to_3, SIDE_LENGTH >= 3);

// I. SIMPLE TYPES

// These types are plain enums, introduced for clarity and type safety,
// or even simpler typedefs for clarity only.

// Symbolic names for the two players.
enum Player {
  kWhite = 0,
  kBlack = 1
};

// Returns the other Player.
inline Player Opponent(Player player) {
  return static_cast<Player>(player ^ 1);
}

// Symbolic names for the kinds of wins. Values of type WinningCondition
// are bit masks, equal either to kNoWinningCondition or to a bitwise OR
// of possible values: kRing, kBenzeneRing, kBridge, or kFork.
// kBenzeneRing | kRing is contained in a WinningCondition for rings
// made up of six stones, possibly with a seventh stone inside.
// The rules of Havannah do not require us to detect if a kRing is also
// a kBenzeneRing; this is done for statistical purposes only.
enum WinningCondition {
  kNoWinningCondition = 0,
  kRing = 1,
  kBenzeneRing = 2,
  kBridge = 4,
  kFork = 8
};

// These two types index two-dimensional arrays with kThirtyTwoX columns
// and kBoardHeight rows. The arrays reflect the hexagonal board as below.
// The sentinels on their sides simplify the getting of the neighbors
// of a given cell.
// Here is the layout of the board in memory
// for SIDE_LENGTH = 4, kGapLeft = 2, and kGapAround = 1:
//  01234567890123456789012345678901
// 0................................
// 1.....####.......................
// 2....#####.......................
// 3...######.......................
// 4..#######.......................
// 5..######........................
// 6..#####.........................
// 7..####..........................
// 8................................
enum XCoord {
  kZeroX = 0,
  kGapLeft = 2,
  kMiddleColumn = kGapLeft + SIDE_LENGTH - 1,
  kLastColumn = kGapLeft + SIDE_LENGTH - 1 + SIDE_LENGTH - 1,
  kPastColumns = kLastColumn + 1,
  kThirtyTwoX = 32
};

enum YCoord {
  kZeroY = 0,
  kGapAround = 2,
  kMiddleRow = kGapAround + SIDE_LENGTH - 1,
  kLastRow = kGapAround + SIDE_LENGTH - 2 + SIDE_LENGTH,
  kPastRows = kLastRow + 1,
  kBoardHeight = kPastRows + kGapAround
};

// Used in loops.
inline XCoord NextX(XCoord x) { return static_cast<XCoord>(x + 1); }
inline XCoord PrevX(XCoord x) { return static_cast<XCoord>(x - 1); }
inline YCoord NextY(YCoord y) { return static_cast<YCoord>(y + 1); }
inline YCoord PrevY(YCoord y) { return static_cast<YCoord>(y - 1); }

// Indexes one-dimensional versions of the above-mentioned arrays.
enum Cell {
  kZerothCell = 0,
  kBoardCenter = 32 * kMiddleRow + kMiddleColumn,
  kNumCellsWithSentinels = 32 * kBoardHeight
};

// Converts between XCoord, YCoord, and Cell.
inline Cell XYToCell(XCoord x, YCoord y) {
  return static_cast<Cell>(32 * y + x);
}
inline XCoord CellToX(Cell cell) { return static_cast<XCoord>(cell % 32); }
inline YCoord CellToY(Cell cell) { return static_cast<YCoord>(cell / 32); }

// Returns a cell given its reference point and relative offset.
inline Cell OffsetCell(Cell cell, int offset) {
  return static_cast<Cell>(cell + offset);
}

// Used in loops.
inline Cell NextCell(Cell cell) { return OffsetCell(cell, 1); }

// Indexes dense arrays of cells. The mutually converse
// Position::MoveIndexToCell() and Position::CellToMoveIndex() methods
// convert between MoveIndex and Cell.
enum MoveIndex {
  kInvalidMove = -1,
  kZerothMove = 0,
  kNumMovesOnBoard = 3 * SIDE_LENGTH * (SIDE_LENGTH - 1) + 1
};

// Used in loops.
inline MoveIndex NextMove(MoveIndex move) {
  return static_cast<MoveIndex>(move + 1);
}

// An unsigned type representing the index of a chain in a ChainSet.
// The smaller the type is, the better an array with kNumCellsWithSentinels
// ChainNum elements fits into the cache.
typedef unsigned char ChainNum;
enum { kChainNumLimit = (1 << (8 * sizeof(ChainNum))) };

// An unsigned type for marking cells in one row of the board.
typedef unsigned RowBitmask;

// Used for Zobrist hashing, in which the Hash of a position
// is a bitwise XOR of Hashes of all its moves.
typedef uint64 Hash;

STATIC_ASSERT(row_must_have_32_cells, sizeof(RowBitmask) == 4);

// The width of the board, including gaps around it.
const int kBoardWidth =
    kGapLeft + SIDE_LENGTH - 1 + SIDE_LENGTH + kGapAround;

STATIC_ASSERT(board_must_fit_in_RowBitmask,
              kBoardWidth <= 8 * static_cast<int>(sizeof(RowBitmask)));

// II. BASIC FUNCTIONS AND GLOBAL VARIABLES

// Do we use the Little Golem or the HavannahGUI coordinate system?
// Influences all conversions to and from strings.
extern bool g_use_lg_coordinates;

// The offsets of neighbors relative to the index of a cell.
// Elements 0-5 contain offsets of nearest neighbors;
// elements 6-11 contain offsets of two-bridge neighbors;
// elements 12-17 contain offsets of further neighbors.
extern const int kNeighborOffsets[3 * 6];

// TODO(mciura)
extern const unsigned kReverseNeighborhoods[6];

// Returns the nth neighbor of a cell.
inline Cell NthNeighbor(Cell cell, int n) {
  return OffsetCell(cell, kNeighborOffsets[n]);
}

// Returns true if the cell with coordinates (x, y) lies on the board.
bool LiesOnBoard(XCoord x, YCoord y);

// Returns the index of the cell encoded in the string, e.g. "a1", "s19",
// or kZerothCell when the cell lies outside the board.
// Supports two most common notations.
Cell FromString(const std::string& cell);
Cell FromClassicalString(const std::string& cell);
Cell FromLittleGolemString(const std::string& cell);

// Returns the string denoting the cell at coordinates (x, y),
// or "" when the cell lies outside the board.
// Supports two most common notations.
std::string ToString(XCoord x, YCoord y);
std::string ToClassicalString(XCoord x, YCoord y);
std::string ToLittleGolemString(XCoord x, YCoord y);
std::string ToString(Cell cell);
std::string ToClassicalString(Cell cell);
std::string ToLittleGolemString(Cell cell);

// III. ESSENTIAL CLASSES

// These classes build on top of each other, culminating in class Position.

// Abstract base class for classes that represent boards
// and are convertible to strings.
class PrintableBoard {
 public:
  PrintableBoard() {}
  virtual ~PrintableBoard() {}
  // Returns a string representation of the board,
  // putting the character for marked_cell inside brackets.
  std::string MakeString(Cell marked_cell = kZerothCell) const;
  std::string MakeClassicalString(Cell marked_cell = kZerothCell) const;
  std::string MakeLittleGolemString(Cell marked_cell = kZerothCell) const;

 private:
  // Returns the character to be displayed in cell with coordinates (x, y).
  virtual char GetCharForCell(XCoord x, YCoord y) const = 0;
};

// A bit mask of stones on the board.
class BoardBitmask : public PrintableBoard {
 public:
  BoardBitmask() {}
  ~BoardBitmask() {}

  // Zeroes all cells in this BoardBitmask.
  void ZeroBits() { memset(rows_, 0, sizeof rows_); }
  // Copies other to this BoardBitmask.
  void CopyFrom(const BoardBitmask& other) {
    memcpy(rows_, other.rows_, sizeof rows_);
  }
  // ORs first with second into this BoardBitmask.
  // Both arguments can be equal to this, if needed.
  void FillWithOr(const BoardBitmask& first, const BoardBitmask& second) {
    for (int i = 0; i < ARRAYSIZE(rows_); ++i) {
      rows_[i] = first.rows_[i] | second.rows_[i];
    }
  }
  // TODO(mciura)
  void FillWithNeighborMask(
      const BoardBitmask& player_stones,
      const BoardBitmask& opponent_stones);
  // Getters for rows_[y].
  const RowBitmask& Row(YCoord y) const { return rows_[y]; }
  RowBitmask& Row(YCoord y) { return rows_[y]; }

  // General-purpose getter and setters.
  bool get(XCoord x, YCoord y) const { return (Row(y) & (1 << x)); }
  void set(XCoord x, YCoord y) { Row(y) |= (1 << x); }
  void clear(XCoord x, YCoord y) { Row(y) &= ~(1 << x); }

  // Returns the 6-bit immediate neighborhood of the given cell
  // composed of this BoardBitmasks's stones.
  unsigned Get6Neighbors(XCoord x, YCoord y) const;

  // Returns a sample cell where this BoardBitmask has a stone.
  Cell GetSampleStone() const;

 private:
  virtual char GetCharForCell(XCoord x, YCoord y) const {
    return Row(y) & (1 << x) ? 'x' : '.';
  }
  // TODO(mciura)
  RowBitmask rows_[kBoardHeight];

  BoardBitmask(const BoardBitmask&);
  void operator=(const BoardBitmask&);
};

// A counter for each cell on the board.
class BoardCounter : public PrintableBoard {
 public:
  BoardCounter() {}
  ~BoardCounter() {}

  // Zeroes the counters for all cells.
  void ZeroCounters() { memset(board_, 0, sizeof board_); }
  // Copies other to this BoardCounter.
  void CopyFrom(const BoardCounter& other) {
    memcpy(board_, other.board_, sizeof board_);
  }

  // General-purpose getter and setters.
  unsigned char& get(Cell cell) { return board_[cell]; }
  void zero(Cell cell) { board_[cell] = 0; }
  void increment(Cell cell) {
    ++board_[cell];
    assert(board_[cell] != 0);
  }
  void decrement(Cell cell) {
    assert(board_[cell] != 0);
    --board_[cell];
  }

 private:
  friend class PlayerPosition;

  virtual char GetCharForCell(XCoord x, YCoord y) const {
    const char c = board_[XYToCell(x, y)];
    return (c == 0) ? '.' : '0' + c;
  }

  unsigned char board_[kNumCellsWithSentinels];

  BoardCounter(const BoardCounter&);
  void operator=(const BoardCounter&);
};

// See part IV below.
class Memento;

// A group of adjacent stones of one color.
class Chain : public PrintableBoard {
 public:
  Chain() {}
  ~Chain() {}

  // Adds a stone in the cell at coordinates (x, y) to this Chain.
  void AddStoneReversibly(XCoord x, YCoord y, Memento* memento);
  void AddStoneFast(XCoord x, YCoord y);
  void AddStoneOutsideBoard(XCoord x, YCoord y);
  // Initializes the fields of result to the cellwise union of this Chain with
  // the other Chain. Assumes that the stone in the cell at coordinates (x, y)
  // has been added to this Chain and is not present in the other Chain.
  void ComputeUnion(
      XCoord x, YCoord y, const Chain& other, Chain* result) const;
  // Initializes the fields of this Chain to store a single stone in the cell
  // at coordinates (x, y).
  void InitWithStone(XCoord x, YCoord y);
  void InitWithStoneOutsideBoard(XCoord x, YCoord y);
  // Returns a nonzero value if this chain forms a winning configuration.
  WinningCondition IsVictory();
  // Clones the other Chain to this Chain.
  void CopyFrom(const Chain& other);
  // Returns (1 << 12) if putting a stone in the cell at coordinates (x, y)
  // closes any ring, 0 otherwise.
  unsigned ClosesAnyRing(XCoord x, YCoord y) const;
  // Getter for num_stones_.
  int num_stones() const { return num_stones_; }
  // Getter for edges_corners_ring_.
  unsigned edges_corners_ring() const { return edges_corners_ring_; }
  // Getters for stone_mask_ and stone_mask_.Row();
  const BoardBitmask& stone_mask() const { return stone_mask_; }
  const RowBitmask& NthRow(YCoord y) const { return stone_mask_.Row(y); }
  RowBitmask& NthRow(YCoord y) { return stone_mask_.Row(y); }
  // Getters used for testing.
  unsigned edges() const { return edges_corners_ring_ & 63; }
  unsigned corners() const { return (edges_corners_ring_ >> 6) & 63; }
  bool ring() const { return (edges_corners_ring_ >> 12) & 1; }
  // Setters and getter for newer_version_.
  void SetNewerVersionReversibly(ChainNum nv, Memento* memento);
  void SetNewerVersionFast(ChainNum nv) {
    newer_version_ = nv;
  }
  int newer_version() const { return newer_version_; }

 private:
  // Returns (1 << 13) if putting a stone in the cell at coordinates (x, y)
  // closes a benzene ring, 0 otherwise.
  unsigned ClosesBenzeneRing(XCoord x, YCoord y) const;
  // Returns the ring bits for a stone put in the cell at coordinates (x, y).
  // They are (1 << 12) for any ring and (1 << 13) for a benzene ring.
  unsigned GetRingMask(XCoord x, YCoord y) const;
  // Returns 'x' if this Chain contains a stone in the cell at coordinates
  // (x, y) or '.' if it does not.
  virtual char GetCharForCell(XCoord x, YCoord y) const {
    return ((stone_mask().Row(y) >> x) & 1)[".x"];
  }

  // The xth bit of stone_mask_.Row(y) is set if this Chain
  // contains a stone in the cell at coordinates (x, y).
  BoardBitmask stone_mask_;
  // How many stones is this Chain composed of?
  unsigned num_stones_;
  // The nth bit (0 <= n <= 5) is set if this Chain contains a stone on
  // the nth edge. The (n + 6)-th bit (0 <= n <= 5) is set if this Chain
  // contains a stone in the nth corner. The 12th bit is set if this Chain
  // contains a closed ring. The 13th bit is set only if the 12th bit is:
  // it tells if this Chain contains a benzene ring. The 16th bit and above
  // are garbage.
  unsigned edges_corners_ring_;
  // The index of the Chain that supersedes this Chain or zero if none does.
  // Such a linked list of chains where only one head is updated on a union
  // operation seems faster than a full-blown disjoint set with reversible
  // unions from
  // S. Conchon, J.-C. Filliâtre: A Persistent Union-Find Structure,
  // http://www.lri.fr/~filliatr/publis/puf-wml07.ps
  ChainNum newer_version_;

  Chain(const Chain&);
  void operator=(const Chain&);
};

// A freelist allocator for Chains. It decreases the memory management
// contention in multithreaded versions of Lajkonik. Each Chain has its
// own ChainAllocator.
class ChainAllocator {
 public:
  ChainAllocator() : head_(NULL) {}
  ~ChainAllocator();

  // TODO(mciura)
  Chain* MakeChain();
  // TODO(mciura)
  void DeleteChain(Chain* chain);

 private:
  // An element of the freelist.
  struct Node {
    Node* next;
  };
  // The head of the freelist.
  Node* head_;

  ChainAllocator(const ChainAllocator&);
  void operator=(const ChainAllocator&);
};

// All chains of stones of one player.
class ChainSet {
 public:
  ChainSet();
  ~ChainSet();

  // Adds a stone to the cell at coordinates (x, y) in chains_[chain].
  void AddStoneToChainReversibly(
      XCoord x, YCoord y, ChainNum chain, Memento* memento);
  void AddStoneToChainFast(XCoord x, YCoord y, ChainNum chain);
  // Merges chain1 with chain2, provided that a stone at coordinates (x, y)
  // has been added to chain1. Returns the index of the resulting Chain.
  ChainNum MergeChainsReversibly(
      XCoord x, YCoord y, ChainNum chain1, ChainNum chain2, Memento* memento);
  ChainNum MergeChainsFast(
      XCoord x, YCoord y, ChainNum chain1, ChainNum chain2);
  // Appends to chains_[] a new Chain consisting of one stone
  // in the cell at coordinates (x, y). Returns the index of the new Chain.
  ChainNum MakeOneStoneChain(XCoord x, YCoord y);
  // TODO(mciura)
  ChainNum push_back(Chain* chain);
  ChainNum PushBackChainOutsideBoard(const Chain* chain);
  // Returns the index of the Chain that supersedes chains_[ch].
  ChainNum NewestVersion(ChainNum ch) const;
  //
  int CountChains() const;
  const Chain* NewestVersion(const Chain* ch) const;
  // Returns the edges_corners_ring mask of the Chain
  // that supersedes chains_[ch].
  unsigned edges_corners_ring(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->edges_corners_ring();
  }
  // Returns a nonzero value if chains_[NewestVersion(ch)]
  // forms a winning configuration.
  WinningCondition IsVictory(ChainNum ch) {
    return chains_[NewestVersion(ch)]->IsVictory();
  }
  // Getters used for testing.
  const BoardBitmask& stone_mask(ChainNum ch) const {
    return chains_[ch]->stone_mask();
  }
  const BoardBitmask& newest_stone_mask(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->stone_mask();
  }
  unsigned edges(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->edges();
  }
  unsigned corners(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->corners();
  }
  bool ring(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->ring();
  }
  // Returns the underlying ChainAllocator;
  ChainAllocator& allocator() { return allocator_; }
  // Returns the number of elements in chains_[].
  int size() const { return chains_.size(); }
  // Reserves space for n Chains.
  void Reserve(int n);
  // Removes the tail of chains_[], leaving at most n of them.
  void ShrinkTo(int n);
  // Setter and getter for chains_[];
  void set_chain(int n, Chain* ch) { chains_[n] = ch; }
  const Chain* chain(int n) const { return chains_[n]; }
  // Returns a string representation of NewestVersion(ch).
  std::string MakeString(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->MakeString();
  }
  std::string MakeClassicalString(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->MakeClassicalString();
  }
  std::string MakeLittleGolemString(ChainNum ch) const {
    return chains_[NewestVersion(ch)]->MakeLittleGolemString();
  }

 private:
  // The ChainAllocator for this ChainSet;
  ChainAllocator allocator_;
  // The Chains of this ChainSet. The element at index zero is NULL.
  std::vector<Chain*> chains_;

  ChainSet(const ChainSet&);
  void operator=(const ChainSet&);
};

// Arena allocator for RingDB.
class Arena {
 public:
  Arena();
  ~Arena();

  // Allocates n cells and zeroes them.
  // Returns the index of the first allocated cell.
  unsigned Allocate(int n);

  // Getter for top_.
  const unsigned& top() const { return top_; }

  // Gets the contents of the nth cell.
  const unsigned& get(unsigned n) const {
    assert(n < top());
    return chunks_[n / kCellsInChunk][n % kCellsInChunk];
  }

  // Sets the contents of the nth cell to value.
  void set(unsigned n, unsigned value) {
    assert(n < top());
    chunks_[n / kCellsInChunk][n % kCellsInChunk] = value;
  }

  // Clones the other Arena to this one.
  void CopyFrom(const Arena& other);

 private:
  // How many unsigned cells are there in one chunk of the Arena?
  static const int kCellsInChunk = (1 << 12);

  // The underlying chunks of memory.
  std::vector<unsigned*> chunks_;
  // The first unallocated cell.
  unsigned top_;

  Arena(const Arena&);
  void operator=(const Arena&);
};

// Database of ring frames of one Player.
class RingDB {
 public:
  RingDB();
  ~RingDB() {}

  // Adds a two-bridge on cell0 and cell1, joining chain0 with chain1.
  // Time complexity: O(1).
  void AddTwoBridgeReversibly(
      Cell cell0, Cell cell1, ChainNum chain0, ChainNum chain1,
      Memento* memento);
  void AddTwoBridgeFast(
      Cell cell0, Cell cell1, ChainNum chain0, ChainNum chain1);

  // Removes a bridge between chain0 and chain1 on cell.
  // Time complexity: O(N).
  void RemoveHalfBridgeReversibly(
    Cell cell, ChainNum chain0, ChainNum chain1, Memento* memento);
  void RemoveHalfBridgeFast(
    Cell cell, ChainNum chain0, ChainNum chain1);

  // Merges the edges of chain0 and chain1 into new_chain.
  // Time complexity: O(N**2).
  void MergeChainEdgesReversibly(
      ChainNum chain0, ChainNum chain1, const ChainSet& chain_set,
      Memento* memento);
  void MergeChainEdgesFast(
      ChainNum chain0, ChainNum chain1, const ChainSet& chain_set);

  // Finds new cycles in the graph.
  // Time complexity: O((v + e)*(c + 1)) for v vertices, e edges, c cycles.
  void FindNewCyclesReversibly(
      ChainNum modified_chain, const ChainSet& chain_set, Memento* memento);
  void FindNewCyclesFast(ChainNum modified_chain, const ChainSet& chain_set);

  // Accessors for the current ring frames.
  int ring_frame_count() const { return ring_frames_top_ - ring_frames_; }
  const unsigned* ring_frame(int n) const {
    const unsigned p = arena_.get(ring_frames_ + n);
    return (p == 0) ? NULL : &arena_.get(p);
  }

  // Clones the other RingDB to this one.
  void CopyFrom(const RingDB& other);
  // Returns a string representation of the adjacency graph.
  std::string MakeString(const ChainSet& chain_set) const;

 private:
  // Note: offset 0 is the "next" pointer in both lists.
  // Offsets inside records in chain_graph_.
  enum { kCgChain = 1, kCgCell0, kCgCell1, kCgSize };
  // Offset inside records in ring_frames_through_cells_.
  enum { kRftcRingFrameIndex = 1, kRftcSize };
  // Upper bound for the number of ring frames during the game.
  static const int kMaxNumRingFrames = (1 << 8);

  // Adds to chain_graph_[chain0]
  // a two-bridge to chain1 on m0 and m1.
  void AddOneWayTwoBridge(
      ChainNum chain0, ChainNum chain1, Cell cell0, Cell cell1);

  // Removes from chain_graph_[chain0]
  // all two-bridge to chain1 containing m.
  void RemoveOneWayTwoBridgesReversibly(
      ChainNum chain0, ChainNum chain1, Cell cell, Memento* memento);
  void RemoveOneWayTwoBridgesFast(
      ChainNum chain0, ChainNum chain1, Cell cell);

  // Replaces old_chain with new_chain in chain_graph_[chain].
  void ReplaceChainInGraphReversibly(
      ChainNum chain, ChainNum old_chain, ChainNum new_chain,
      Memento* memento);
  void ReplaceChainInGraphFast(
      ChainNum chain, ChainNum old_chain, ChainNum new_chain);

  // Donald B. Johnson, Finding All the Elementary Circuits of a Directed
  // Graph, SIAM J. Comput. vol. 4, no. 1, March 1975, pp. 77-84.
  // Time complexity: O((v + e)(c + 1)) for v vertices, e edges, c cycles.
  // Appears to work incrementally. Appears to work for multigraphs.
  bool FindCycles(
      ChainNum this_node, ChainNum start_node, const ChainSet& chain_set,
      Memento* memento);
  void Unblock(ChainNum this_node);
  void VerifyCycle(const ChainSet& chain_set, Memento* memento);
  void AddRingFrameIndexToCell(
      Cell cell, int ring_frame_index, Memento* memento);

  // The underlying Arena.
  Arena arena_;
  // For each Chain, a list of (next, Chain, MoveIndex, MoveIndex).
  unsigned chain_graph_;
  // For each index, a (cycle_length, CellA0, CellB0, CellA1, CellB1, ...
  // CellAN, CellBN) record. Cycle length is equivalent to number of
  // remaining moves to complete the ring frame. 2 * cycle_length Cells
  // follow. They comprise the two-bridges of the ring frame.
  unsigned ring_frames_;
  // The first unallocated ring frame.
  unsigned ring_frames_top_;
  // For each MoveIndex, a list of (next, ring_frame_index).
  unsigned ring_frames_through_cells_;
  // Do we have to find cycles passing through the largest ChainNum?
  bool changed_;

  // Used in MergeChainEdges...().
  std::set<std::pair<unsigned, unsigned> > seen_two_bridges_;

  // The following data structures are for Johnson's algoprithm.
  // Stack of vertices in the current path.
  std::vector<ChainNum> path_;
  // Stack of bridges between vertices of the current path.
  std::vector<std::pair<Cell, Cell> > bridges_;
  // Is a vertex blocked from search?
  bool blocked_[kChainNumLimit];
  // Graph portions that yield no elementary circuit.
  std::map<ChainNum, std::set<ChainNum> > b_sets_;
  // Has this cell already been used in the path?
  bool blocked_bridges_[kNumMovesOnBoard];

  // Used in VerifyCycle().
  BoardBitmask stones_;

  RingDB(const RingDB&);
  void operator=(const RingDB&);
};

// Represents shortest distances from a source.
class BfsResult {
 public:
  BfsResult() {}
  ~BfsResult() {}

  void set(Cell cell, int value) {
    assert(cell >= kZerothCell);
    assert(cell < kNumCellsWithSentinels);
    distance[cell] = value;
  }
  int get(Cell cell) const {
    assert(cell >= kZerothCell);
    assert(cell < kNumCellsWithSentinels);
    return distance[cell];
  }

  int GetTwoDistance(const Chain* chain) const {
    return get(chain->stone_mask().GetSampleStone());
  }

  static const int kMaxDistance = 99;

 private:
  unsigned char distance[kNumCellsWithSentinels];

  BfsResult(const BfsResult&);
  void operator=(const BfsResult&);
};

class Position;

// Represents shortest distances to victory.
class PositionEvaluation {
 public:
  PositionEvaluation() {}
  ~PositionEvaluation() {}

  int GetEvaluation(const Position& position);

  void set(MoveIndex move, int value) {
    assert(move >= kZerothMove);
    assert(move <= kNumMovesOnBoard);
    distance[move] = value;
  }
  int get(MoveIndex move) const {
    assert(move >= kZerothMove);
    assert(move <= kNumMovesOnBoard);
    return distance[move];
  }
  int get_baseline_distance() const { return get(kNumMovesOnBoard); }

  void CopyFrom(const PositionEvaluation& other);
  void CopyFromBfsResult(const BfsResult& other);

  void SetAllMovesTo(int value);
  void SetToCombination(
      const BfsResult& bfs_result_1,
      const BfsResult& bfs_result_2,
      const Chain* chain_1,
      const Chain* chain_2);
  void SetToSum(
      const PositionEvaluation& augend,
      const PositionEvaluation& addend);
  void SetToMinimum(
      const PositionEvaluation& first,
      const PositionEvaluation& second);

  std::string MakeString(const Position* position) const;
  std::string Get18Neighbors(
      Player player, Cell cell, const Position& position) const;

 private:
  std::string MakeClassicalString(const Position* position) const;
  std::string MakeLittleGolemString(const Position* position) const;

  signed int distance[kNumMovesOnBoard + 1];

  PositionEvaluation(const PositionEvaluation&);
  void operator=(const PositionEvaluation&);
};

// The stones of one player.
class PlayerPosition : public PrintableBoard {
 public:
  PlayerPosition();
  ~PlayerPosition() {}

  // Puts a stone in the cell and updates internal data structures.
  // Returns a nonzero value if the position of the player forms a winning
  // configuration.
  WinningCondition MakeMoveReversibly(Cell cell, Memento* memento);
  WinningCondition MakeMoveFast(Cell cell);
  // Adds a Chain outside the board.
  void AddChainOutsideBoard(const Chain* chain);

  // Creates two-bridges and merges chain edges
  // if a two-bridge has been connected.
  void CreateTwoBridgesAfterOurMoveReversibly(
      Cell cell, const PlayerPosition& opponent, Memento* memento);
  void CreateTwoBridgesAfterOurMoveFast(
      Cell cell, const PlayerPosition& opponent);
  // Removes two-bridges.
  void RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveReversibly(
      Cell cell, Memento* memento);
  void RemoveTwoBridgesBeforeOurMoveOrAfterFoeMoveFast(Cell cell);

  // Called after updating two-bridges and chain edges.
  void FindNewRingFramesReversibly(Memento* memento) {
    ring_db_.FindNewCyclesReversibly(modified_chain_, chain_set_, memento);
  }
  void FindNewRingFramesFast() {
    ring_db_.FindNewCyclesFast(modified_chain_, chain_set_);
  }
  // Returns the 6-bit immediate neighborhood of the given cell
  // composed of this player's stones.
  unsigned Get6Neighbors(Cell cell) const {
    return stone_mask_.Get6Neighbors(CellToX(cell), CellToY(cell));
  }
  // Returns the 18-bit neighborhood of the given cell
  // composed of this player's stones.
  unsigned Get18Neighbors(Cell cell) const;
  // Returns true if move into cell would close a fork or a bridge,
  // where edges_corners is the static mask of edges and corners
  // the cell belongs to and injected_chain -- if nonzero -- is a chain
  // that is not present in the position, yet should be counted among
  // chains that neighbor cell.
  bool MoveWouldCloseForkOrBridge(
      Cell cell, unsigned edges_corners, ChainNum injected_chain) const;
  // Returns true if move into cell would close a fork, a bridge, or a ring,
  // where edges_corners is the static mask of edges and corners
  // the cell belongs to, and injected_chain -- if nonzero -- is a chain
  // that is not present in the position, yet should be counted among
  // chains that neighbor cell.
  // Does not recognize 7-rings (benzene rings with the interior cell
  // occupied by our stone.
  bool MoveWouldCloseForkBridgeOrRing(
      Cell cell, unsigned edges_corners, ChainNum injected_chain) const;
  // TODO(mciura)
  int GetSizeOfNeighborChains(Cell cell, int num_neighbors) const;
  // Replaces Chains in chain_set_ with their newest versions.
  // This accelerates future calls to MakeMove...().
  // Should be called if the move just made is permanent.
  void UpdateChainsToNewestVersionsReversibly(Memento* memento);
  // Clones the other PlayerPosition to this PlayerPosition.
  void CopyFrom(const PlayerPosition& other);
  // TODO(mciura): Refactor the methods below.
  ChainNum chain_for_cell(Cell cell) const { return chains_for_cells_[cell]; }
  const Chain* NthChain(ChainNum n) const { return chain_set_.chain(n); }
  ChainNum NewestVersion(ChainNum n) const {
    return chain_set_.NewestVersion(n);
  }
  // TODO(mciura)
  ChainNum NewestChainForCell(Cell cell) const {
    return NewestVersion(chain_for_cell(cell));
  }
  // TODO(mciura)
  unsigned EdgesCornersRingForCell(Cell cell) const {
    return chain_set_.edges_corners_ring(chains_for_cells_[cell]);
  }
  // TODO(mciura)
  const BoardBitmask& ChainMaskForCell(Cell cell) const {
    return chain_set_.newest_stone_mask(chains_for_cells_[cell]);
  }
  const BoardBitmask& ChainMaskForChain(ChainNum chain) const {
    return chain_set_.stone_mask(chain);
  }
  void GetCurrentChains(std::set<const Chain*>* current_chains) const;
  int CountChains() const { return chain_set_.CountChains(); }

  // Getter for stone_mask_.
  const BoardBitmask& stone_mask() const { return stone_mask_; }
  // Getter for two_bridge_mask_.
  const BoardCounter& two_bridge_mask() const { return two_bridge_mask_; }
  // Returns true if the cell lies on the board and is empty.
  bool CellIsEmpty(Cell cell) const {
    return (chains_for_cells_[cell] == kNullChain);
  }
  // Returns true if the cell lies on the board and there is a stone in it.
  bool StoneIsInCell(Cell cell) const {
    return ChainLiesOnBoard(chains_for_cells_[cell]);
  }

  // Fills the result with two-distances from the start Chain.
  void ComputeTwoDistance(
      const Chain* start_chain,
      const PlayerPosition& op,
      BfsResult* result) const;

  // Accessors for the current ring frames.
  int ring_frame_count() const { return ring_db_.ring_frame_count(); }
  const unsigned* ring_frame(int n) const { return ring_db_.ring_frame(n); }

  // The index of the NULL Chain inside chain_set_.
  static const ChainNum kNullChain = 0;
  // The number of edge or corner Chain inside chain_set_.
  static const ChainNum kNumSpecialChains = 13;

  // Returns true if the argument is not an edge or corner chain.
  static bool ChainLiesOnBoard(ChainNum chain) {
    return chain >= kNumSpecialChains;
  }

 private:
  // Returns 'x' if the cell at coordinates (x, y) is occupied
  // or '.' otherwise.
  virtual char GetCharForCell(XCoord x, YCoord y) const {
    if (StoneIsInCell(XYToCell(x, y)))
      return 'x';
    else
      return two_bridge_mask().GetCharForCell(x, y);
  }

  // The Chains of this player.
  ChainSet chain_set_;
  // The chains_for_cells_[cell] element is the index of the Chain from
  // chain_set_ that the stone in the cell belongs to. An empty cell has
  // its chains_for_cells_[cell] element set to zero.
  // Memento requires the elements of chains_for_cells_ to be unsigned.
  ChainNum chains_for_cells_[kNumCellsWithSentinels];
  // The Chain modified in the last move.
  ChainNum modified_chain_;
  // The bit mask of this player's stones.
  BoardBitmask stone_mask_;
  // The counters of this player's two-bridges.
  BoardCounter two_bridge_mask_;
  // Database of ring frames.
  RingDB ring_db_;

  PlayerPosition(const PlayerPosition&);
  void operator=(const PlayerPosition&);
};

// The state of the game.
class Position : public PrintableBoard {
 public:
  // Initializes kEdgesCornersNeighbors, kMovesToOrderedCells, kEdgeChains,
  // kCornerChains, kNeighbors, kZobristHash, kIsCellOnBoardBitmask,
  // kIsVirtualEdge, and kIsVirtualCorner.
  static void InitStaticFields();

  Position()
      : move_count_(0),
        is_initialized_(false) {}
  ~Position();
  // Getter for is_initialized_.
  bool is_initialized() const { return is_initialized_; }
  // Initializes this to a start position.
  void InitToStartPosition();
  // Clones the other Position's player_positions_ and cells_ into
  // this Position. Does not clone mementoes_.
  void CopyFrom(const Position& other);
  // Fills cells with the indices of empty cells.
  void GetFreeCells(std::vector<Cell>* cells) const;
  // Makes a move for player. Returns a nonzero value if the game
  // forms a winning configuration for the player.
  WinningCondition MakeMoveReversibly(
      Player player, Cell cell, Memento* memento);
  WinningCondition MakeMoveFast(Player player, Cell cell);
  WinningCondition MakePermanentMove(Player player, Cell cell);
  WinningCondition MakeMoveReversibly(
      Player player, MoveIndex move, Memento* memento) {
    const Cell cell = MoveIndexToCell(move);
    return MakeMoveReversibly(player, cell, memento);
  }
  // Unodes the last move remembered by MakePermanentMove().
  // Returns false if this is the start position.
  // TODO(mciura): RedoPermanentMove().
  bool UndoPermanentMove();
  // Swaps player_positions_. Updates cells_.
  void SwapPlayers();
  // Returns one of previously made moves. For the last move, call with zero.
  // Returns kBoardCenter if the requested move precedes the start position.
  Cell MoveNPliesAgo(int plies) const;
  // Getter for player_positions_.
  const PlayerPosition& player_position(Player player) const {
    return player_positions_[player];
  }
  // Getter for cells_[].
  unsigned char GetCell(Cell cell) const { return cells_[cell]; }
  // Getter for num_available_moves_.
  MoveIndex NumAvailableMoves() const { return num_available_moves_; }
  // Returns the move counter.
  int MoveCount() const { return move_count_; }
  // Returns true if the cell lies on the board and is empty.
  bool CellIsEmpty(Cell cell) const { return (cells_[cell] & 3) == 0; }
  // Returns true if the given move would win the game for player.
  // The neighborhood has the same order of bits as the result of
  // Position::GetImmediateNeighborhood(player, cell) but may be OR-ed
  // with kReverseNeighborhoods[]. The injected_chain -- if nonzero --
  // is the number of a chain that is not present in the position,
  // yet should be counted among chains that neighbor cell.
  bool MoveIsWinning(Player player, Cell cell, int neighborhood,
                     ChainNum injected_chain) const;
  // Returns true if player's move into cell would attack his opponent's
  // two-bridge without creating his own two-bridge.
  bool PlayerShouldNotMoveIntoCell(Player player, Cell cell) const {
    return cells_[cell] == (4 << player);
  }
  // Returns the six-bit immediate neighborhood of the given cell
  // composed of player's stones.
  unsigned Get6Neighbors(Player player, Cell cell) const {
    return player_position(player).Get6Neighbors(cell);
  }
  // TODO(mciura)
  uint64 Get18Neighbors(Player player, Cell cell) const {
    return (kEdgesCornersNeighbors[cell] >> 28) |
           (static_cast<uint64>(
               player_position(Opponent(player)).Get18Neighbors(cell)) << 18) |
           player_position(player).Get18Neighbors(cell);
  }
  // For testing. If s represents a valid Havannah board, sets this
  // to its internal representation and returns true. Otherwise clears
  // this and returns false;
  bool ParseString(const std::string& s);
  // Translates a dense cell index into an index to cells_.
  static Cell MoveIndexToCell(MoveIndex move) {
    return kMoveIndexToCell[move];
  }
  // Complementary to MoveIndexToCell().
  static MoveIndex CellToMoveIndex(Cell cell) {
    return kCellToMoveIndex[cell];
  }
  // TODO(mciura)
  static Cell GetNthNeighbor(Cell cell, int n) {
    return kNeighbors[cell][n];
  }
  // Returns the bit mask of edges and corners with a stone
  // put in the given cell.
  static unsigned GetMaskOfEdgesAndCorners(Cell cell) {
    return kEdgesCornersNeighbors[cell] & ~(~0ULL << 12);
  }
  // Returns the mask of cells that lie on the board.
  static const BoardBitmask& GetBoardBitmask() {
    return kIsCellOnBoardBitmask;
  }
  // Returns true if cell lies on an edge.
  static bool IsEdgeCell(Cell cell) {
    return (kEdgesCornersNeighbors[cell] & 0x3f);
  }
  // Returns true if cell lies in a corner.
  static bool IsCornerCell(Cell cell) {
    return (kEdgesCornersNeighbors[cell] & (0x3f << 6));
  }
  // Returns the hash of the given move of a given player.
  static Hash ModifyZobristHash(Hash hash, Player player, MoveIndex move) {
    return hash + kZobristHash[move][player];
  }
  // Returns the number of groups of nonadjacent stones
  // in the immediate neighborhood.
  static int CountNeighborGroups(int neighborhood) {
    return kGroupCount[neighborhood];
  }
  // Returns the number of groups of nonadjacent stones in the immediate
  // neighborhood or 9 when three adjacent cells are occupied by our stones.
  static int CountNeighborGroupsWithPossibleBenzeneRings(int neighborhood) {
    return kGroupCountWithPossibleBenzeneRings[neighborhood];
  }
  // Returns an edge chain for the given index.
  static const Chain* GetEdgeChain(int n) {
    assert(n >= 0 && n < 6);
    return &kEdgeChains[n];
  }
  // Returns a corner chain for the given index.
  static const Chain* GetCornerChain(int n) {
    assert(n >= 0 && n < 6);
    return &kCornerChains[n];
  }
  // Returns the distance between cell1 and cell2.
  static int GetDistance(Cell cell1, Cell cell2);

 private:
  // Returns 'x' or 'o' if the cell at coordinates (x, y) is occupied
  // by White's or Black's stone, or '.' if it's empty.
  virtual char GetCharForCell(XCoord x, YCoord y) const {
    return cells_[XYToCell(x, y)][".xo#........."];
  }

  // The configurations of stones of both players.
  PlayerPosition player_positions_[2];
  // The nth element is 0 when the corresponding cell is empty,
  // 1 or 2 when it is occupied by a white or black stone,
  // 3 when it lies outside the board, 4 or 8 when it is empty
  // but white or black should not move into it since this would
  // attack his opponent's two-bridge, or 12 when it belongs to
  // a two-bridge of both players (both of them can move into it).
  unsigned char cells_[kNumCellsWithSentinels];
  // Remembers changes to data structures related to subsequent moves.
  std::vector<Memento*> mementoes_;
  // Remembers moves in the game.
  std::vector<std::pair<Player, Cell> > past_moves_;
  // The number of moves made.
  unsigned move_count_;
  // Divides move_index_to_cell_ into random and historical parts.
  MoveIndex num_available_moves_;
  // True if the Position has been properly initialized.
  bool is_initialized_;

  // Tells whether a given cell lies on some edge or corner.
  // The meaning of the bits:
  //  0- 5: edges;
  //  6-11: corners;
  // 12-15: unused;
  // 16-21: set when one of six neighbors lies outside the board;
  // 22-27: a shifted copy of bits 16-21;
  // 28-45: set when one of 18 neighbors lies outside the board;
  // 46-63: a shifted copy of bits 28-45;
  // The numbering of edges, corners, 6-neighbors, and 18-neighbors:
  //      #00#     0##5
  //     1###5    #####               15 16 17
  //    1####5   ######    4 5     11 12 13 14
  //   #######  1#####4  2 . 3   7  8  .  9 10
  //   2####4   ######   0 1     3  4  5  6
  //   2###4    #####            0  1  2
  //   #33#     2##3
  static uint64 kEdgesCornersNeighbors[kNumCellsWithSentinels];
  // Translates a dense cell index into an index to cells_.
  static Cell kConstMoveIndexToCell[kNumMovesOnBoard];
  static Cell kMoveIndexToCell[kNumMovesOnBoard];
  // The Reverse transformation to kMoveIndexToCell.
  static MoveIndex kConstCellToMoveIndex[kNumCellsWithSentinels];
  static MoveIndex kCellToMoveIndex[kNumCellsWithSentinels];
  // TODO(mciura)
  static Cell kNeighbors[kNumCellsWithSentinels][8];
  // Random 64-bit numbers, used for computing fingerprints of positions.
  static Hash kZobristHash[kNumMovesOnBoard][2];
  // The xth bit of kIsCellOnBoardBitmask.Row(y) is set if cell (x, y)
  // lies on the board.
  static BoardBitmask kIsCellOnBoardBitmask;
  // Determines whether a cell lies on a virtual edge.
  static BoardBitmask kIsVirtualEdge;
  // Determines whether a cell lies in a virtual corner.
  static BoardBitmask kIsVirtualCorner;
  // Number of groups of stones adjacent to the cell with the given
  // neighborhood and nonadjacent to each other.
  static const unsigned char kGroupCount[64];
  // The same as Position::kGroupCount but with 9s when three adjacent
  // neighboring cells are occupied by player's stones.
  static const unsigned char kGroupCountWithPossibleBenzeneRings[64];
  // Chains that mask six edges right outside the board.
  static Chain kEdgeChains[6];
  // Chains that mask six corners right outside the board.
  static Chain kCornerChains[6];

  Position(const Position&);
  void operator=(const Position&);
};

// IV. AUXILIARIES

// Classes ChainAllocator, Arena, and RingDB should also belong here.

// Undoes assignments to memory locations and shrinks ChainSets.
class Memento {
 public:
  Memento() {}
  ~Memento() {}

  // Remembers pointers and their pointees.
  void Remember(unsigned* pointer) {
    words_.push_back(std::make_pair(pointer, *pointer));
  }
  void Remember(const unsigned* pointer) {
    Remember(const_cast<unsigned*>(pointer));
  }
  void Remember(unsigned char* pointer) {
    if (sizeof(ptrdiff_t) == sizeof pointer) {
      ptrdiff_t tmp = reinterpret_cast<ptrdiff_t>(pointer) & -sizeof(unsigned);
      Remember(reinterpret_cast<unsigned*>(tmp));
    } else {
      int64 tmp = reinterpret_cast<int64>(pointer) & -sizeof(unsigned);
      Remember(reinterpret_cast<unsigned*>(tmp));
    }
  }
  // Remembers the size of a ChainSet.
  void RememberSize(ChainSet* chain_set) {
    sizes_.push_back(std::make_pair(chain_set, chain_set->size()));
  }

  // Restores all remembered state and forgets the changes.
  void UndoAll();

 private:
  // Remembered pointers and their pointees.
  std::vector<std::pair<unsigned*, unsigned> > words_;
  // Remembered sizes of ChainSets;
  std::vector<std::pair<ChainSet*, int> > sizes_;

  Memento(const Memento&);
  void operator=(const Memento&);
};

}  // namespace lajkonik

#endif  // HAVANNAH_H_
