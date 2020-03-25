#define DUMP 0

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

// Definition of the game engine.

#include "engine.h"
#include "base.h"
#include "wfhashmap.h"

#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include <algorithm>

namespace lajkonik {

namespace {

const int kPotentialScale = 100;
const int kAdjustment = kPotentialScale / 2;
const int kWon = -101;
const int kLost = +10000;
const int kInfinity = 2 * kLost;
const int kDraw = 0.5 * kLost;

const uint64 kAttackerPassHash = -0xdeadbeefdeadbeefULL;
const uint64 kDefenderPassHash = +0xdeadbeefdeadbeefULL;

struct CellEval {
  CellEval(Cell cell, int value): cell(cell), value(value) {}
  ~CellEval() {}
  Cell cell : 16;
  int value : 16;
};

bool CellEvalCompareAsc(const CellEval& ce1, const CellEval& ce2) {
  if (ce1.value != ce2.value) {
    return ce1.value < ce2.value;
  } else {
    return ce1.cell > ce2.cell;
  }
}

bool CellEvalCompareDesc(const CellEval& ce1, const CellEval& ce2) {
  if (ce1.value != ce2.value) {
    return ce1.value > ce2.value;
  } else {
    return ce1.cell < ce2.cell;
  }
}

enum Kind {
  kExact, kAlpha, kBeta
};

struct EvalKindDepthMoves {
  int value : 16;
  unsigned kind : 2;
  unsigned depth : 14;
  unsigned moves_index: 32;
};

void EvaluateRingFrames(
    const PlayerPosition& pp, PositionEvaluation* evaluation) {
  for (int i = 0, size = pp.ring_frame_count(); i < size; ++i) {
    const unsigned* frame = pp.ring_frame(i);
    if (frame == NULL)
      continue;
    const int moves_to_win = frame[0] - 1;
    for (int j = 0; j <= moves_to_win; ++j) {
      MoveIndex m;
      m = Position::CellToMoveIndex(static_cast<Cell>(frame[2 * j + 1]));
      evaluation->set(m, std::min(evaluation->get(m), moves_to_win));
      m = Position::CellToMoveIndex(static_cast<Cell>(frame[2 * j + 2]));
      evaluation->set(m, std::min(evaluation->get(m), moves_to_win));
    }
  }
}

void EvaluateBridgeFrames(
    const PlayerPosition& pp,
    const PlayerPosition& op,
    PositionEvaluation* evaluation) {
  BfsResult from_corner[6];
  for (int i = 0; i < 6; ++i) {
    const Chain* corner_chain = Position::GetCornerChain(i);
    pp.ComputeTwoDistance(corner_chain, op, &from_corner[i]);
  }
  PositionEvaluation tmp;
  for (int i = 0; i < 6; ++i) {
    for (int j = i + 1; j < 6; ++j) {
      tmp.SetToCombination(
          from_corner[i], from_corner[j],
          Position::GetCornerChain(i), Position::GetCornerChain(j));
      evaluation->SetToMinimum(*evaluation, tmp);
    }
  }
}

void EvaluateForkFrames(
    const PlayerPosition& pp,
    const PlayerPosition& op,
    PositionEvaluation* evaluation) {
  std::set<const Chain*> current_chains;
  pp.GetCurrentChains(&current_chains);
  if (current_chains.empty()) {
    return;
  }
  BfsResult from_edge[6];
  for (int i = 0; i < 6; ++i) {
    const Chain* edge_chain = Position::GetEdgeChain(i);
    pp.ComputeTwoDistance(edge_chain, op, &from_edge[i]);
  }
  for (std::set<const Chain*>::const_iterator it = current_chains.begin();
       it != current_chains.end(); ++it) {
    BfsResult from_center;
    PositionEvaluation from_outside[6];
    pp.ComputeTwoDistance(*it, op, &from_center);
    for (int j = 0; j < 6; ++j) {
      from_outside[j].SetToCombination(
          from_center, from_edge[j],
          *it, Position::GetEdgeChain(j));
    }
    PositionEvaluation tmp1;
    PositionEvaluation tmp2;
    PositionEvaluation tmp3;
    tmp1.SetToMinimum(from_outside[0], from_outside[1]);
    tmp2.SetToMinimum(from_outside[2], from_outside[3]);
    tmp1.SetToSum(tmp1, tmp2);
    tmp2.SetToMinimum(from_outside[5], from_outside[4]);
    tmp3.SetToSum(tmp1, tmp2);
    tmp1.SetToMinimum(from_outside[0], from_outside[4]);
    tmp2.SetToMinimum(from_outside[2], from_outside[1]);
    tmp1.SetToSum(tmp1, tmp2);
    tmp2.SetToMinimum(from_outside[5], from_outside[3]);
    tmp1.SetToSum(tmp1, tmp2);
    tmp3.SetToMinimum(tmp3, tmp1);
    tmp1.SetToMinimum(from_outside[0], from_outside[3]);
    tmp2.SetToMinimum(from_outside[2], from_outside[4]);
    tmp1.SetToSum(tmp1, tmp2);
    tmp2.SetToMinimum(from_outside[5], from_outside[1]);
    tmp1.SetToSum(tmp1, tmp2);
    tmp3.SetToMinimum(tmp3, tmp1);
    evaluation->SetToMinimum(*evaluation, tmp3);
  }
}

void EvaluateForPlayer(
    const Position* position, Player player, PositionEvaluation* evaluation) {
  const PlayerPosition& pp = position->player_position(player);
  const PlayerPosition& op = position->player_position(Opponent(player));
  evaluation->SetAllMovesTo(BfsResult::kMaxDistance);
  EvaluateForkFrames(pp, op, evaluation);
  EvaluateBridgeFrames(pp, op, evaluation);
  EvaluateRingFrames(pp, evaluation);
}

// TODO.
class Logger {
 public:
  Logger() {
    gettimeofday(&start_time_, NULL);
    if (pthread_mutex_init(&write_mutex_, NULL) != 0) {
      fprintf(stderr, "Cannot initialize a mutex\n");
      exit(EXIT_FAILURE);
    }
  }
  ~Logger() {
    if (pthread_mutex_destroy(&write_mutex_) != 0) {
      fprintf(stderr, "Cannot destroy a mutex\n");
      exit(EXIT_FAILURE);
    }
  }

  void Log(const std::string& message) {
    timeval current_time;
    gettimeofday(&current_time, NULL);
    int seconds = current_time.tv_sec - start_time_.tv_sec;
    int useconds = current_time.tv_usec - start_time_.tv_usec;
    if (useconds < 0) {
      useconds += 1000 * 1000;
      --seconds;
    }
    pthread_mutex_lock(&write_mutex_);
    fprintf(
        stderr, "%d:%02d.%03d %s\n",
        seconds / 60, seconds % 60, useconds / 1000, message.c_str());
    pthread_mutex_unlock(&write_mutex_);
  }

 private:
  timeval start_time_;
  pthread_mutex_t write_mutex_;

  Logger(const Logger&);
  void operator=(const Logger&);
};

// TODO.
typedef WaitFreeHashMap<Hash, EvalKindDepthMoves, 27> TranspositionTable;

// TODO.
class Searcher {
 public:
  Searcher(
      Logger* logger,
      volatile int* max_depth,
      const Position& position,
      Player attacker)
    : logger_(logger),
      max_depth_(max_depth),
      solved_(false),
      attacker_(attacker),
      defender_(Opponent(attacker)),
      tt_(new TranspositionTable) {
    position_.CopyFrom(position);
    vectors_.reserve(1000 * 1000);
    vectors_.push_back(NULL);
  }

  ~Searcher() {
    delete tt_;
    for (size_t i = 0; i < vectors_.size(); ++i) {
      delete vectors_[i];
    }
  }

  static void* SearchForAttacker(void* self) {
    static_cast<Searcher*>(self)->SearchForAttackerInternal();
    return NULL;
  }

  static void* SearchForDefender(void* self) {
    static_cast<Searcher*>(self)->SearchForDefenderInternal();
    return NULL;
  }

  const PositionEvaluation& position_evaluation() const {
    return position_evaluation_;
  }

  bool solved() const { return solved_; }

  int tt_size() const { return tt_->num_elements(); }

 private:
  void SearchForAttackerInternal() {
    if (!setjmp(come_back_)) {
      int depth;
      for (depth = 0; depth < *max_depth_; ++depth) {
        Attack(0ULL, -kInfinity, +kInfinity, depth, 0, 2 * depth, false);
        std::string main_variation = PrincipalVariation(0ULL, attacker_);
        std::string pass_variation =
            PrincipalVariation(kAttackerPassHash, defender_);
        logger_->Log(StringPrintf(
            "A%d %d %s |%s",
            depth, tt_size(),
            main_variation.c_str(), pass_variation.c_str()));
        EvalKindDepthMoves* root = tt_->FindValue(0ULL);
        assert(root != NULL);
        assert(root->moves_index != 0);
        const std::vector<CellEval>& moves = *vectors_[root->moves_index];
        assert(!moves.empty());
        if (moves.begin()->value <= kWon + kPotentialScale * depth ||
            moves.size() == 1 || moves[1].value >= kDraw) {
          break;
        }
      }
      *max_depth_ = depth + 1;
    }
    FillEvaluation(0ULL);
    solved_ = true;
  }

  void SearchForDefenderInternal() {
    if (!setjmp(come_back_)) {
      int depth;
      for (depth = 0; depth < *max_depth_; ++depth) {
        Defend(0ULL, -kInfinity, +kInfinity, depth, 0, 2 * depth);
        std::string main_variation = PrincipalVariation(0ULL, defender_);
        std::string pass_variation =
            PrincipalVariation(kDefenderPassHash, attacker_);
        logger_->Log(StringPrintf(
            "D%d %d %s | %s",
            depth, tt_size(),
            main_variation.c_str(), pass_variation.c_str()));
        EvalKindDepthMoves* root = tt_->FindValue(0ULL);
        assert(root != NULL);
        const std::vector<CellEval>& moves = *vectors_[root->moves_index];
        assert(!moves.empty());
        if (moves.begin()->value >= kDraw ||
            moves.size() == 1 ||
            moves[1].value <= kWon + kPotentialScale * depth) {
          break;
        }
      }
      *max_depth_ = depth + 1;
    }
    FillEvaluation(0ULL);
    solved_ = true;
  }

  int Attack(
      Hash hash, int alpha, int beta, int depth, int level, int max_level,
      bool last_move_was_defender_pass) {
    if (depth > *max_depth_) {
      longjmp(come_back_, 1);
    }
    EvalKindDepthMoves* node = tt_->FindValue(hash);
    int moves_index;
    if (node == NULL) {
      moves_index = 0;
    } else {
      if (node->depth == depth &&
          (node->kind == kExact ||
          (node->kind == kAlpha && node->value <= alpha) ||
          (node->kind == kBeta && node->value >= beta))) {
        return node->value;
      }
//      if (node->value - 2.5 * kPotentialScale > beta) {
//        return node->value;
//      }
      moves_index = node->moves_index;
      assert(moves_index != 0);
    }
    if (moves_index == 0) {
      moves_index = ExpandMoves(attacker_, level);
    }
    std::vector<CellEval>& moves = *vectors_[moves_index];
    int value = kDraw;
    Kind kind;
    if (depth == 0 || level > max_level) {
      if (moves.size() > last_move_was_defender_pass) {
        const CellEval tmp =
            CellEval(kZerothCell, moves[last_move_was_defender_pass].value);
        const int mobility =
            std::upper_bound(
                moves.begin(), moves.end(), tmp, CellEvalCompareAsc) -
            moves.begin();
        assert(mobility >= 1);
        value = moves[last_move_was_defender_pass].value - mobility;
      }
      kind = kExact;
    } else {
      kind = kBeta;
      Memento memento;
      size_t i;
      for (i = 0; i < moves.size(); ++i) {
        if (moves[i].cell == kZerothCell) {
          if (level == 0) {
#if DUMP
printf("pass\n");
#endif
            value = moves[i].value = Defend(
                hash + kAttackerPassHash,
                alpha, beta, depth, level + 1, max_level);
          }
        } else {
#if DUMP
if (level == 0) {
  printf("%s\n", ToString(moves[i].cell).c_str());
}
#endif
          if (position_.MakeMoveReversibly(
                  attacker_, moves[i].cell, &memento) != kNoWinningCondition) {
            memento.UndoAll();
            value = moves[i].value = kWon;
            kind = kAlpha;
            break;
          }
          value = moves[i].value = Defend(
              Position::ModifyZobristHash(
                  hash, attacker_, Position::CellToMoveIndex(moves[i].cell)),
              alpha - kPotentialScale, beta - kPotentialScale,
              depth - 1, level + 1, max_level) + kPotentialScale;
          memento.UndoAll();
        }
        if (value <= alpha) {
          if (level > 0) {
            kind = kAlpha;
            break;
          }
        }
        if (value < beta) {
          if (level > 0) {
            kind = kExact;
            beta = value;
          }
        }
      }
      if (i < moves.size()) {
        ++i;
      }
      std::sort(moves.begin(), moves.begin() + i, CellEvalCompareAsc);
      value = moves.empty() ? kDraw : moves[0].value;
    }

    if (node == NULL) {
      node = tt_->InsertKey(hash);
    }
    if (node != NULL) {
      node->value = value;
      node->kind = kind;
      node->depth = depth;
      node->moves_index = moves_index;
    }
#if DUMP
if (level <= 1) {
for (size_t i = 0; i < moves.size(); ++i) {
  if (moves[i].cell == kZerothCell) {
    Hash son = hash + kAttackerPassHash;
    printf("  Att %d %d pass(%d) %s\n", alpha, beta, moves[i].value, PrincipalVariation(son, defender_).c_str());
  } else {
    Hash son = Position::ModifyZobristHash(hash, attacker_, Position::CellToMoveIndex(moves[i].cell));
    printf("  Att %d %d %s(%d) %s\n", alpha, beta, ToString(moves[i].cell).c_str(), moves[i].value, PrincipalVariation(son, defender_).c_str());
  }
}
}
#endif
    return value;
  }

  int Defend(
      Hash hash, int alpha, int beta, int depth, int level, int max_level) {
    if (depth > *max_depth_) {
      longjmp(come_back_, 1);
    }
    EvalKindDepthMoves* node = tt_->FindValue(hash);
    int moves_index;
    if (node == NULL) {
      moves_index = 0;
    } else {
      if (node->depth == depth &&
          (node->kind == kExact ||
           (node->kind == kAlpha && node->value <= alpha) ||
           (node->kind == kBeta && node->value >= beta))) {
        return node->value;
      }
//      if (node->value < alpha) {
//        return node->value;
//      }
      moves_index = node->moves_index;
      assert(moves_index != 0);
    }
    if (moves_index == 0) {
      moves_index = vectors_.size();
      vectors_.push_back(new std::vector<CellEval>);
      vectors_[moves_index]->push_back(CellEval(kZerothCell, alpha));
    }
    std::vector<CellEval>& moves = *vectors_[moves_index];
    int value;
    Kind kind = kAlpha;
    Memento memento;
    size_t i;
    for (i = 0; i < moves.size(); ++i) {
      if (moves[i].cell == kZerothCell) {
#if DUMP
if (level == 0) {
  printf("pass\n");
}
#endif
        value = moves[i].value = Attack(
            hash + kDefenderPassHash,
            alpha - kPotentialScale, beta - kPotentialScale,
            depth, level + 1, max_level, true);
        if (value < beta) {
          AppendInterestingNodesIfNotPresent(hash + kDefenderPassHash, &moves);
        }
      } else {
#if DUMP
if (level == 0) {
  printf("%s\n", ToString(moves[i].cell).c_str());
}
#endif
        if (position_.MakeMoveReversibly(defender_, moves[i].cell, &memento) !=
            kNoWinningCondition) {
          memento.UndoAll();
          value = moves[i].value = kLost;
          kind = kBeta;
          break;
        }
        value = moves[i].value = Attack(
            Position::ModifyZobristHash(
                hash, defender_, Position::CellToMoveIndex(moves[i].cell)),
            alpha + kPotentialScale, beta + kPotentialScale,
            depth + 1, level + 1, max_level, false) - kPotentialScale;
        memento.UndoAll();
      }
      if (value >= beta) {
        if (level > 0) {
          kind = kBeta;
          break;
        }
      }
      if (value > alpha) {
        if (level > 0) {
          kind = kExact;
          alpha = value;
        }
      }
    }
    if (i < moves.size()) {
      ++i;
    }
    std::sort(moves.begin(), moves.begin() + i, CellEvalCompareDesc);
    value = moves.empty() ? kDraw : moves[0].value;
    if (node == NULL) {
      node = tt_->InsertKey(hash);
    }
    if (node != NULL) {
      node->value = value;
      node->kind = kind;
      node->depth = depth;
      node->moves_index = moves_index;
    }
#if DUMP
if (level <= 1) {
for (size_t i = 0; i < moves.size(); ++i) {
  if (moves[i].cell == kZerothCell) {
    Hash son = hash + kDefenderPassHash;
    printf("  Def %d %d pass(%d) %s\n", alpha, beta, moves[i].value, PrincipalVariation(son, attacker_).c_str());
  } else {
    Hash son = Position::ModifyZobristHash(hash, defender_, Position::CellToMoveIndex(moves[i].cell));
    printf("  Def %d %d %s(%d) %s\n", alpha, beta, ToString(moves[i].cell).c_str(), moves[i].value, PrincipalVariation(son, attacker_).c_str());
  }
}
}
#endif
    return value;
  }

  static bool SubvectorContainsCell(
      std::vector<CellEval>::const_iterator begin,
      std::vector<CellEval>::const_iterator end,
      Cell cell) {
    while (begin != end) {
      if (begin->cell == cell)
        return true;
      ++begin;
    }
    return false;
  }

  void AppendInterestingNodesIfNotPresent(
      Hash hash, std::vector<CellEval>* moves) {
    EvalKindDepthMoves* attack_node = tt_->FindValue(hash);
    assert(attack_node != NULL);
    assert(attack_node->moves_index != 0);
    const std::vector<CellEval>& attacks = *vectors_[attack_node->moves_index];
    const int size = moves->size();
    if (size == 1) {
      for (size_t i = 0; i < attacks.size(); ++i) {
        if (attacks[i].value > attacks[0].value)
          break;
        moves->push_back(attacks[i]);
      }
    } else {
      assert(size > 1);
      for (size_t i = 0; i < attacks.size(); ++i) {
        if (attacks[i].value > attacks[0].value)
          break;
        if (!SubvectorContainsCell(
            moves->begin(), moves->begin() + size, attacks[i].cell)) {
          moves->push_back(attacks[i]);
        }
      }
    }
  }

  static bool IsInMaskOrTwiceAdjacent(Cell cell, const BoardBitmask& mask) {
    const XCoord x = CellToX(cell);
    const YCoord y = CellToY(cell);
    return (mask.get(x, y) || CountSetBits(mask.Get6Neighbors(x, y)) >= 2);
  }

  int ExpandMoves(Player player, int level) {
    const int moves_index = vectors_.size();
    vectors_.push_back(new std::vector<CellEval>);
    std::vector<CellEval>& moves = *vectors_[moves_index];
    int baseline_value;
    // TODO(mciura): take into account symmetries.
    if (position_.MoveCount() == 0) {
      baseline_value = (SIDE_LENGTH + 1) * (SIDE_LENGTH + 1) / 3;
      for (YCoord y = kMiddleRow; y < kPastRows; y = NextY(y)) {
        for (XCoord x = kMiddleColumn; x <= static_cast<XCoord>(y);
             x = NextX(x)) {
          if (!Position::GetBoardBitmask().get(x, y))
            continue;
          const Cell cell = XYToCell(x, y);
          assert(position_.CellIsEmpty(cell));
          moves.push_back(CellEval(cell, kPotentialScale * baseline_value));
        }
      }
    } else if (position_.MoveCount() == 1) {
      baseline_value = (SIDE_LENGTH + 1) * (SIDE_LENGTH + 1) / 3;
      for (MoveIndex move = kZerothMove,
           num_moves = position_.NumAvailableMoves();
           move < num_moves; move = NextMove(move)) {
        const Cell cell = Position::MoveIndexToCell(move);
        if (position_.CellIsEmpty(cell)) {
          moves.push_back(CellEval(cell, kPotentialScale * baseline_value));
        }
      }
    } else {
      EvaluateForPlayer(&position_, player, &position_evaluation_);
      baseline_value = position_evaluation_.get_baseline_distance();
      const BoardBitmask& player_stones =
          position_.player_position(player).stone_mask();
      const BoardBitmask& opponent_stones =
          position_.player_position(Opponent(player)).stone_mask();
      BoardBitmask player_neighbors;
      player_neighbors.FillWithNeighborMask(player_stones, opponent_stones);
      for (MoveIndex move = kZerothMove,
           num_moves = position_.NumAvailableMoves();
           move < num_moves; move = NextMove(move)) {
        const Cell cell = Position::MoveIndexToCell(move);
        if (position_.CellIsEmpty(cell)) {
          const int value = position_evaluation_.get(move);
          if (value < baseline_value ||
              IsInMaskOrTwiceAdjacent(cell, player_neighbors)) {
            moves.push_back(CellEval(cell, kPotentialScale * value));
          }
        }
      }
    }
    if (level == 0) {
      moves.push_back(CellEval(kZerothCell, kPotentialScale * baseline_value));
    }
    sort(moves.begin(), moves.end(), CellEvalCompareAsc);
    return moves_index;
  }

  std::string PrincipalVariation(Hash hash, Player player) {
    std::string result;
    for (int i = 0; i < 20; ++i) {
      EvalKindDepthMoves* node = tt_->FindValue(hash);
      if (node == NULL)
        break;
      const std::vector<CellEval>& moves = *vectors_[node->moves_index];
      if (moves.empty())
        break;
      const Cell cell = moves[0].cell;
      if (cell == kZerothCell) {
        result += StringPrintf(" (%d)pass(%d)", node->value, moves[0].value);
        hash += (player == attacker_ ? kAttackerPassHash : kDefenderPassHash);
      } else {
        result += StringPrintf(
            " (%d)%s(%d)",
            node->value, ToString(cell).c_str(), moves[0].value);
        hash = Position::ModifyZobristHash(
            hash, player, Position::CellToMoveIndex(cell));
      }
      player = Opponent(player);
    }
    return result;
  }

  void FillEvaluation(Hash hash) {
    EvalKindDepthMoves* root = tt_->FindValue(hash);
    assert(root != NULL);
    assert(root->moves_index != 0);
    const std::vector<CellEval>& moves = *vectors_[root->moves_index];
    assert(!moves.empty());
    int null_value = kLost;
    for (size_t i = 0; i < moves.size(); ++i) {
      if (moves[i].cell == kZerothCell) {
        null_value = moves[i].value;
      }
    }
    position_evaluation_.SetAllMovesTo(null_value);
    for (size_t i = 0; i < moves.size(); ++i) {
      const Cell cell = moves[i].cell;
      if (cell != kZerothCell) {
        const MoveIndex move = Position::CellToMoveIndex(cell);
        position_evaluation_.set(move, moves[i].value);
      }
    }
  }

  // TODO(mciura).
  Logger* logger_;

  volatile int* max_depth_;
  bool solved_;

  Position position_;
  Player attacker_;
  Player defender_;
  PositionEvaluation position_evaluation_;

  // The underlying transposition table.
  TranspositionTable* tt_;

  std::vector<std::vector<CellEval>*> vectors_;

  jmp_buf come_back_;

  Searcher(const Searcher&);
  void operator=(const Searcher&);
};

void create_thread(
    std::vector<pthread_t>* threads,
    void *(*start_routine)(void *),
    Searcher* searcher) {
  threads->push_back(pthread_t());
  if (pthread_create(&threads->back(), NULL, start_routine, searcher) != 0) {
    fprintf(stderr, "Cannot create background thread\n");
    exit(EXIT_FAILURE);
  }
}

}  // namespace

Engine::Engine()
    : has_swapped_(false),
      seconds_per_move_(20.0) {
  position_.InitToStartPosition();
}

Engine::~Engine() {}

std::string Engine::SuggestMove(Player player_to_move, double thinking_time) {
  if (thinking_time <= 0.0)
    thinking_time = seconds_per_move_;
  volatile int max_depth = 100;
  PositionEvaluation attacker_evaluation;
  PositionEvaluation defender_evaluation;
  Logger logger;
  Searcher attack(&logger, &max_depth, position_, player_to_move);
  Searcher defend(&logger, &max_depth, position_, Opponent(player_to_move));
  create_thread(&threads_, Searcher::SearchForAttacker, &attack);
  create_thread(&threads_, Searcher::SearchForDefender, &defend);
  for (int i = 1; i <= thinking_time; ++i) {
    sleep(1);
    if (i % 10 == 0) {
      logger.Log(StringPrintf("%d %d", attack.tt_size(), defend.tt_size()));
    }
    if (attack.solved() && defend.solved()) {
      break;
    }
  }
  max_depth = 0;
  void* ignored;
  for (size_t i = 0; i < threads_.size(); ++i) {
    if (pthread_join(threads_[i], &ignored) != 0) {
      fprintf(stderr, "Cannot join background thread\n");
      exit(EXIT_FAILURE);
    }
  }
  threads_.clear();
  const PositionEvaluation& attack_evaluation = attack.position_evaluation();
  const PositionEvaluation& defend_evaluation = defend.position_evaluation();
  printf("%s", attack_evaluation.MakeString(&position_).c_str());
  printf("%s", defend_evaluation.MakeString(&position_).c_str());
  int best_value = -kInfinity;
  MoveIndex best_move = kInvalidMove;
  for (MoveIndex move = kZerothMove; move < position_.NumAvailableMoves();
       move = NextMove(move)) {
    const int value = defend_evaluation.get(move) - attack_evaluation.get(move);
    if (value > best_value) {
      best_value = value;
      best_move = move;
    }
  }
  printf(
      "%.2f moves ahead\n",
      static_cast<double>(best_value) / kPotentialScale);
  return ToString(Position::MoveIndexToCell(best_move));
}

void Engine::Reset() {
  while (Undo()) {
    continue;
  }
  has_swapped_ = false;
}

bool Engine::Undo() {
  return position_.UndoPermanentMove();
}

bool Engine::Move(
    Player player, const std::string& move_string, int* result) {
  if (move_string == "pass") {
    *result = kNoneWon;
    return true;
  } else if (move_string == "swap") {
    position_.SwapPlayers();
    has_swapped_ = true;
    *result = kNoneWon;
    return true;
  }
  const Cell cell = FromString(move_string);
  if (cell == kZerothCell)
    return false;
  if (!position_.CellIsEmpty(cell))
    return false;
  const int move_result = position_.MakePermanentMove(player, cell);
  if (move_result != 0) {
    *result = (player == kWhite) ? kWhiteWon : kBlackWon;
    return true;
  }
  assert(move_result == 0);
  *result = kNoneWon;
  return true;
}

void Engine::EvaluatePartialGoal(
    Player player,
    Cell cell1,
    Cell cell2,
    PositionEvaluation* evaluation) const {
  const PlayerPosition& pp = position_.player_position(player);
  if (cell1 == kZerothCell && cell2 == kZerothCell) {
    evaluation->SetAllMovesTo(BfsResult::kMaxDistance);
    EvaluateRingFrames(pp, evaluation);
    return;
  } else if (cell1 == kZerothCell && cell2 == static_cast<Cell>(-1)) {
    const PlayerPosition& op = position_.player_position(Opponent(player));
    evaluation->SetAllMovesTo(BfsResult::kMaxDistance);
    EvaluateBridgeFrames(pp, op, evaluation);
    return;
  } else if (cell1 == kZerothCell && cell2 == static_cast<Cell>(-2)) {
    const PlayerPosition& op = position_.player_position(Opponent(player));
    evaluation->SetAllMovesTo(BfsResult::kMaxDistance);
    EvaluateForkFrames(pp, op, evaluation);
    return;
  } else if (cell1 == kZerothCell && cell2 == static_cast<Cell>(-3)) {
    EvaluateForPlayer(&position_, player, evaluation);
    return;
  }
  const Chain* chain1p = NULL;
  if (cell1 >= kZerothCell) {
    Chain chain1;
    chain1.InitWithStone(CellToX(cell1), CellToY(cell1));
    chain1p = &chain1;
  } else if (cell1 >= static_cast<Cell>(-6)) {
    chain1p = Position::GetEdgeChain(-cell1 - 1);
  } else if (cell1 >= static_cast<Cell>(-12)) {
    chain1p = Position::GetCornerChain(-cell1 - 1 - 6);
  } else {
    assert(false);
  }
  const Chain* chain2p = NULL;
  if (cell2 >= kZerothCell) {
    Chain chain2;
    chain2.InitWithStone(CellToX(cell2), CellToY(cell2));
    chain2p = &chain2;
  } else if (cell2 >= static_cast<Cell>(-6)) {
    chain2p = Position::GetEdgeChain(-cell2 - 1);
  } else if (cell2 >= static_cast<Cell>(-12)) {
    chain2p = Position::GetCornerChain(-cell2 - 1 - 6);
  } else {
    assert(false);
  }
  const PlayerPosition& op = position_.player_position(Opponent(player));
  BfsResult tmp1;
  BfsResult tmp2;
  pp.ComputeTwoDistance(chain1p, op, &tmp1);
  pp.ComputeTwoDistance(chain2p, op, &tmp2);
  evaluation->SetToCombination(tmp1, tmp2, chain1p, chain2p);
}

std::string Engine::GetBoardString() const {
  return position_.MakeString(position_.MoveNPliesAgo(0));
}

std::string Engine::GetPlayerEvaluationString(Player player) const {
  PositionEvaluation evaluation;
  EvaluateForPlayer(&position_, player, &evaluation);
  return evaluation.MakeString(&position_);
}

std::string Engine::GetPartialEvaluationString(
    Player player, Cell cell1, Cell cell2) const {
  PositionEvaluation evaluation;
  EvaluatePartialGoal(player, cell1, cell2, &evaluation);
  return evaluation.MakeString(&position_);
}

int Engine::GetEvaluation(Player player) const {
  PositionEvaluation tmp;
  EvaluateForPlayer(&position_, player, &tmp);
  return tmp.GetEvaluation(position_);
}

bool Engine::DumpEvaluations(const std::vector<MoveIndex>& variant) {
  int size = variant.size();
  return size > 0;
}

}  // namespace lajkonik
