#ifndef ENGINE_H_
#define ENGINE_H_

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

// Declaration of the game engine.

#include <pthread.h>
#include <string>
#include <vector>

#include "havannah.h"

namespace lajkonik {

enum {
  kNoneWon,
  kWhiteWon,
  kDraw,
  kBlackWon
};

class Engine {
 public:
  Engine();
  ~Engine();

  std::string SuggestMove(Player player, double thinking_time);
  bool Move(Player player, const std::string& move_string, int* result);
  void Reset();
  bool Undo();

  std::string GetBoardString() const;
  std::string GetPlayerEvaluationString(Player player) const;
  std::string GetPartialEvaluationString(
      Player player, Cell cell1, Cell cell2) const;
  int GetEvaluation(Player player) const;
  bool DumpEvaluations(const std::vector<MoveIndex>& variant);

  const Position* position() const { return &position_; }
  bool* use_lg_coordinates() { return &g_use_lg_coordinates; }
  double* seconds_per_move() { return &seconds_per_move_; }

 private:
  void EvaluatePartialGoal(
      Player player, Cell cell1, Cell cell2,
      PositionEvaluation* evaluation) const;
  std::string GetDebugInfo(Player player) const;

  Position position_;

  bool has_swapped_;
  double seconds_per_move_;

  std::vector<pthread_t> threads_;

  Engine(const Engine&);
  void operator=(const Engine&);
};

}  // namespace lajkonik

#endif  // ENGINE_H_
