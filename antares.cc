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

// The main module for Havannah playing via GTP.

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <utility>
#include <vector>

//#define USE_READLINE

#ifdef USE_READLINE
#include "readline/history.h"
#include "readline/readline.h"
#endif  // USE_READLINE

#include "engine.h"
#include "havannah.h"

namespace lajkonik {

namespace {

void RemoveUnderscores(char *s) {
  for (char* t = s; *t != '\0'; ++t) {
    if (*t != '_') {
      *s = *t;
      ++s;
    }
  }
  *s = '\0';
}

bool GetColor(const char* s, Player* color) {
  if (strcmp(s, "w") == 0 || strcmp(s, "white") == 0)
    *color = kWhite;
  else if (strcmp(s, "b") == 0 || strcmp(s, "black") == 0)
    *color = kBlack;
  else
    return false;
  return true;
}

}  // namespace

// A parser for the subset of GTP v2 applicable to Havannah.
class Frontend {
 public:
  static Frontend* Create(Engine* engine);
  static Frontend* get() {
    assert(frontend_instance_ != NULL);
    return frontend_instance_;
  }
  void HandleCommand(char* command);
  static char* CommandGenerator(const char* text, int state);

 private:
  static const char kSuccess = '=';
  static const char kFailure = '?';

  struct Command {
    const char* name;
    void (Frontend::*method)(const std::vector<char*>& args);
  };

  explicit Frontend(Engine* engine);
  ~Frontend();

  void StartAnswer(char indicator);
  void Answer(char indicator, const char* format, ...);

  bool StrToDouble(const char* str, double* v);
  bool StrToInt(const char* str, int* v);
  bool StrToBool(const char* str, bool* v);
  bool GetCellEdgeOrCorner(const char* str, Cell* cell);
  bool GetConnection(
      char* arg, Cell* cell1, Cell* cell2, bool* has_extra_move);

  void Boardsize(const std::vector<char*>& args);
  void ClearBoard(const std::vector<char*>& args);
  void Genmove(const std::vector<char*>& args);
  void Evaluate(const std::vector<char*>& args);
  void HavannahWinner(const std::vector<char*>& args);
  void KnownCommand(const std::vector<char*>& args);
  void Komi(const std::vector<char*>& args);
  void ListCommands(const std::vector<char*>& args);
  void ListOptions(const std::vector<char*>& args);
  void Name(const std::vector<char*>& args);
  void Play(const std::vector<char*>& args);
  void PlayGame(const std::vector<char*>& args);
  void ProtocolVersion(const std::vector<char*>& args);
  void PutStones(const std::vector<char*>& args);
  void SetOption(const std::vector<char*>& args);
  void Showboard(const std::vector<char*>& args);
  void Quit(const std::vector<char*>& args);
  void Undo(const std::vector<char*>& args);
  void Variant(const std::vector<char*>& args);
  void Version(const std::vector<char*>& args);

  static Frontend* frontend_instance_;
  //
  static const Command kCommands[];
  //
  std::vector<std::pair<const char*, double*> > double_options_;
  std::vector<std::pair<const char*, int*> > int_options_;
  std::vector<std::pair<const char*, bool*> > bool_options_;
  //
  Engine* engine_;
  //
  int result_;
  //
  int id_;
  //
  bool command_succeeded_;
  //
  Player player_;
  //
  volatile bool is_thinking_;

  Frontend(const Frontend&);
  void operator=(const Frontend&);
};

const Frontend::Command Frontend::kCommands[] = {
  { "boardsize", &Frontend::Boardsize },
  { "clearboard", &Frontend::ClearBoard },
  { "eval", &Frontend::Evaluate },
  { "genmove", &Frontend::Genmove },
  { "havannahwinner", &Frontend::HavannahWinner },
  { "knowncommand", &Frontend::KnownCommand },
  { "komi", &Frontend::Komi },
  { "listcommands", &Frontend::ListCommands },
  { "listoptions", &Frontend::ListOptions },
  { "name", &Frontend::Name },
  { "play", &Frontend::Play },
  { "playgame", &Frontend::PlayGame },
  { "protocolversion", &Frontend::ProtocolVersion },
  { "putstones", &Frontend::PutStones },
  { "setoption", &Frontend::SetOption },
  { "showboard", &Frontend::Showboard },
  { "quit", &Frontend::Quit },
  { "undo", &Frontend::Undo },
  { "variant", &Frontend::Variant },
  { "version", &Frontend::Version },
  { NULL, NULL },
};

Frontend* Frontend::frontend_instance_ = NULL;

Frontend* Frontend::Create(Engine* engine) {
  frontend_instance_ = new Frontend(engine);
  return frontend_instance_;
}

Frontend::Frontend(Engine* engine)
    : engine_(engine),
      result_(kNoneWon),
      player_(kWhite),
      is_thinking_(false) {
#define ADD_OPTION(v, n) (v).push_back(std::make_pair(#n, engine->n()))
  ADD_OPTION(bool_options_, use_lg_coordinates);
  ADD_OPTION(double_options_, seconds_per_move);
#undef ADD_OPTION
}

void Frontend::StartAnswer(char indicator) {
  if (id_ >= 0)
    printf("%c%d ", indicator, id_);
  else
    printf("%c ", indicator);
  command_succeeded_ = (indicator == kSuccess);
}

void Frontend::Answer(char indicator, const char* format, ...) {
  StartAnswer(indicator);
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  va_end(ap);
  printf("\n\n");
  fflush(stdout);
}

bool Frontend::StrToDouble(const char* str, double* v) {
  char* endptr;
  *v = strtod(str, &endptr);
  if (*endptr != '\0') {
    Answer(kFailure, "invalid double %s", str);
    return false;
  } else {
    return true;
  }
}

bool Frontend::StrToInt(const char* str, int* v) {
  char* endptr;
  *v = strtol(str, &endptr, 10);
  if (*endptr != '\0') {
    Answer(kFailure, "invalid integer %s", str);
    return false;
  } else {
    return true;
  }
}

bool Frontend::StrToBool(const char* str, bool* v) {
  if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0) {
    *v = true;
    return true;
  } else if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0) {
    *v = false;
    return true;
  } else {
    Answer(kFailure, "invalid bool %s", str);
    return false;
  }
}

bool Frontend::GetCellEdgeOrCorner(const char* str, Cell* cell) {
  *cell = FromString(str);
  if (*cell != kZerothCell)
    return true;
  static const char* kEdgeCornerNames[12] = {
    "ne", "nwe", "swe", "se", "see", "nee",
    "nwc", "wc", "swc", "sec", "ec", "nec",
  };
  for (int i = 0; i < ARRAYSIZE(kEdgeCornerNames); ++i) {
    if (strcmp(str, kEdgeCornerNames[i]) == 0) {
      *cell = static_cast<Cell>(-i - 1);
      return true;
    }
  }
  return false;
}

bool Frontend::GetConnection(
    char* arg, Cell* cell1, Cell* cell2, bool* has_extra_move) {
  const int length = strlen(arg);
  if (length > 0 && arg[length - 1] == '\'') {
    *has_extra_move = true;
    arg[length - 1] = '\0';
  } else {
    *has_extra_move = false;
  }
  if (strcmp(arg, "ring") == 0) {
    *cell1 = kZerothCell;
    *cell2 = kZerothCell;
    return true;
  } else if (strcmp(arg, "bridge") == 0) {
    *cell1 = kZerothCell;
    *cell2 = static_cast<Cell>(-1);
    return true;
  } else if (strcmp(arg, "fork") == 0) {
    *cell1 = kZerothCell;
    *cell2 = static_cast<Cell>(-2);
    return true;
  } else if (strcmp(arg, "total") == 0) {
    *cell1 = kZerothCell;
    *cell2 = static_cast<Cell>(-3);
    return true;
  }
  char* saveptr;
  char* arg2 = strtok_r(arg, "-", &saveptr);
  arg2 = strtok_r(NULL, "-", &saveptr);
  if (arg2 == NULL)
    return false;
  if (!GetCellEdgeOrCorner(arg, cell1))
    return false;
  if (!GetCellEdgeOrCorner(arg2, cell2))
    return false;
  return true;
}

void Frontend::Boardsize(const std::vector<char*>& args) {
  int size;
  if (args.size() != 1)
    Answer(kFailure, "expected one argument to boardsize");
  else if (StrToInt(args[0], &size) && size == SIDE_LENGTH)
    Answer(kSuccess, "");
  else
    Answer(kFailure, "unacceptable size %s", args[0]);
}

void Frontend::ClearBoard(const std::vector<char*>& /*args*/) {
  engine_->Reset();
  result_ = kNoneWon;
  Answer(kSuccess, "");
}

void Frontend::Evaluate(const std::vector<char*>& args) {
  if (args.size() > 2) {
    Answer(kFailure, "expected at most two arguments to eval");
    return;
  }
  if (args.empty()) {
    Answer(kSuccess, "%d", engine_->GetEvaluation(player_));
    return;
  }
  Player player;
  if (!GetColor(args[0], &player)) {
    Answer(kFailure, "invalid color %s", args[0]);
    return;
  }
  if (args.size() == 1 || args[1][0] == '\'') {
    StartAnswer(kSuccess);
    printf("\n%s\n", engine_->GetPlayerEvaluationString(player).c_str());
    return;
  }
  Cell cell1;
  Cell cell2;
  bool has_extra_move;
  const std::string arg1_backup = args[1];
  if (!GetConnection(args[1], &cell1, &cell2, &has_extra_move)) {
    Answer(kFailure, "invalid connection %s", arg1_backup.c_str());
    return;
  }
  StartAnswer(kSuccess);
  printf("\n%s\n", engine_->GetPartialEvaluationString(
      player, cell1, cell2).c_str());
}

void Frontend::Genmove(const std::vector<char*>& args) {
  Player player = player_;
  int thinking_time_index = 0;
  if (!args.empty() && GetColor(args[0], &player))
    thinking_time_index = 1;
  const int last_arg_index = args.size() - 1;
  double thinking_time;
  if (thinking_time_index == last_arg_index) {
    if (!StrToDouble(args[thinking_time_index], &thinking_time)) {
      Answer(kFailure, "invalid arguments to genmove");
      return;
    }
  } else if (last_arg_index > thinking_time_index) {
    Answer(kFailure, "too many arguments to genmove");
    return;
  } else {
    thinking_time = 0.0;
  }
  if (result_ == kNoneWon) {
    is_thinking_ = true;
    const std::string move = engine_->SuggestMove(player, thinking_time);
    if (!engine_->Move(player, move, &result_)) {
      fprintf(stderr, "Unexpected move %s", move.c_str());
      exit(EXIT_FAILURE);
    }
    Answer(kSuccess, "%s", move.c_str());
    player_ = Opponent(player);
    is_thinking_ = false;
  } else {
    Answer(kSuccess, "none");
  }
}

void Frontend::HavannahWinner(const std::vector<char*>& /*args*/) {
  static const char* kWinner[4] = { "none", "white", "draw", "black" };
  assert(result_ >= 0);
  assert(result_ < 4);
  Answer(kSuccess, kWinner[result_]);
}

void Frontend::KnownCommand(const std::vector<char*>& args) {
  if (args.size() >= 1) {
    for (int i = 0; kCommands[i].name != NULL; ++i) {
      if (strcmp(args[0], kCommands[i].name) == 0) {
        Answer(kSuccess, "true");
        return;
      }
    }
  }
  Answer(kSuccess, "false");
}

void Frontend::Komi(const std::vector<char*>& /*args*/) {
  Answer(kSuccess, "");
}

void Frontend::ListCommands(const std::vector<char*>& /*args*/) {
  StartAnswer(kSuccess);
  for (int i = 0; kCommands[i].name != NULL; ++i) {
    printf("%s\n", kCommands[i].name);
  }
  printf("\n");
}

void Frontend::ListOptions(const std::vector<char*>& /*args*/) {
  StartAnswer(kSuccess);
  printf("\n");
  for (int i = 0, size = double_options_.size(); i < size; ++i) {
    printf("%s = %f\n", double_options_[i].first, *double_options_[i].second);
  }
  for (int i = 0, size = int_options_.size(); i < size; ++i) {
    printf("%s = %d\n", int_options_[i].first, *int_options_[i].second);
  }
  for (int i = 0, size = bool_options_.size(); i < size; ++i) {
    printf("%s = %s\n", bool_options_[i].first,
           *bool_options_[i].second ? "true" : "false");
  }
  printf("\n");
}

void Frontend::Name(const std::vector<char*>& /*args*/) {
  Answer(kSuccess, "Antares");
}

void Frontend::Play(const std::vector<char*>& args) {
  Player player;
  if (args.size() != 2) {
    Answer(kFailure, "expected two arguments to play");
  } else if (!GetColor(args[0], &player)) {
    Answer(kFailure, "invalid color %s", args[0]);
  } else {
    if (engine_->Move(player, args[1], &result_)) {
      Answer(kSuccess, "");
      player_ = Opponent(player);
    } else {
      Answer(kFailure, "invalid move %s", args[1]);
    }
  }
}

void Frontend::PlayGame(const std::vector<char*>& args) {
  Player player_copy = player_;
  for (int i = 0, size = args.size(); i < size; ++i) {
    int unused;
    if (!engine_->Move(player_, args[i], &unused)) {
      Answer(kFailure, "invalid move %s", args[i]);
      for (/**/; i > 0; --i) {
        engine_->Undo();
      }
      player_ = player_copy;
      return;
    }
    player_ = Opponent(player_);
  }
  Answer(kSuccess, "");
}

void Frontend::ProtocolVersion(const std::vector<char*>& /*args*/) {
  Answer(kSuccess, "2");
}

void Frontend::PutStones(const std::vector<char*>& args) {
  Player player;
  if (args.size() <= 1) {
    Answer(kFailure, "expected at least one argument to putstones");
  } else if (!GetColor(args[0], &player)) {
    Answer(kFailure, "invalid color %s", args[0]);
  } else {
    for (int i = 1, size = args.size(); i < size; ++i) {
      int unused;
      if (!engine_->Move(player, args[i], &unused)) {
        Answer(kFailure, "invalid move %s", args[i]);
        for (/**/; i > 0; --i) {
          engine_->Undo();
        }
        return;
      }
    }
    Answer(kSuccess, "");
  }
}

void Frontend::Quit(const std::vector<char*>& /*args*/) {
  Answer(kSuccess, "");
  exit(EXIT_SUCCESS);
}

void Frontend::SetOption(const std::vector<char*>& args) {
  if (args.size() != 2) {
    Answer(kFailure, "expected two arguments to set_option");
  } else {
    for (int i = 0, size = double_options_.size(); i < size; ++i) {
      if (strcmp(args[0], double_options_[i].first) == 0) {
        if (StrToDouble(args[1], double_options_[i].second))
          Answer(kSuccess, "");
        return;
      }
    }
    for (int i = 0, size = int_options_.size(); i < size; ++i) {
      if (strcmp(args[0], int_options_[i].first) == 0) {
        if (StrToInt(args[1], int_options_[i].second))
          Answer(kSuccess, "");
        return;
      }
    }
    for (int i = 0, size = bool_options_.size(); i < size; ++i) {
      if (strcmp(args[0], bool_options_[i].first) == 0) {
        if (StrToBool(args[1], bool_options_[i].second))
          Answer(kSuccess, "");
        return;
      }
    }
    Answer(kFailure, "unknown option %s", args[0]);
  }
}

void Frontend::Showboard(const std::vector<char*>& /*args*/) {
  StartAnswer(kSuccess);
  printf("\n%s\n", engine_->GetBoardString().c_str());
}

void Frontend::Undo(const std::vector<char*>& /*args*/) {
  if (engine_->Undo()) {
    player_ = Opponent(player_);
    result_ = kNoneWon;
    Answer(kSuccess, "");
  } else {
    Answer(kFailure, "cannot undo");
  }
}

void Frontend::Variant(const std::vector<char*>& args) {
  std::vector<MoveIndex> variant;
  for (int i = 0, size = args.size(); i < size; ++i) {
    const Cell cell = FromString(args[i]);
    if (cell == kZerothCell) {
      Answer(kFailure, "invalid move %s", args[i]);
      return;
    }
    variant.push_back(Position::CellToMoveIndex(cell));
  }
  if (engine_->DumpEvaluations(variant))
    Answer(kSuccess, "");
  else
    Answer(kFailure, "cannot execute moves");
}

void Frontend::Version(const std::vector<char*>& /*args*/) {
  Answer(kSuccess, __DATE__ ", " __TIME__);
}

void Frontend::HandleCommand(char* input) {
  for (char* p = input; *p != '\0'; ++p) {
    *p = tolower(*p);
  }
  if (isdigit(*input))
    id_ = strtol(input, &input, 10);
  else
    id_ = -1;
  std::vector<char*> args;
  char* saveptr;
  char* command = strtok_r(input, " \t\n", &saveptr);
  if (command == NULL)
    return;
  RemoveUnderscores(command);
  while (true) {
    char* word = strtok_r(NULL, " \t\n", &saveptr);
    if (word == NULL)
      break;
    args.push_back(word);
  }
  for (int i = 0; kCommands[i].name != NULL; ++i) {
    if (strcmp(command, kCommands[i].name) == 0) {
      (this->*kCommands[i].method)(args);
      fflush(stdout);
      return;
    }
  }
  Answer(kFailure, "unknown command %s", command);
}

char* Frontend::CommandGenerator(const char* text, int state) {
  static int list_index;
  static int len;
  const char* name;
  if (state == 0) {
    list_index = 0;
    len = strlen(text);
  }
  while ((name = kCommands[list_index].name) != NULL) {
    ++list_index;
    if (strncmp(name, text, len) == 0)
      return strdup(name);
  }
  return NULL;
}

}  // namespace lajkonik

namespace {

#ifdef USE_READLINE
char** AntaresCompletion(const char* text, int start, int /*end*/) {
  rl_attempted_completion_over = 1;
  if (start == 0)
    return rl_completion_matches(text, lajkonik::Frontend::CommandGenerator);
  else
    return NULL;
}
#endif  // USE_READLINE

char* GetLine() {
  static char buffer[1024];
#ifdef USE_READLINE
  if (isatty(fileno(stdout)))
    return readline(NULL);
  else
#endif  // USE_READLINE
    return fgets(buffer, sizeof buffer, stdin);
}

}  // namespace

int main() {
  srand(time(NULL));
  lajkonik::Engine engine;
  lajkonik::Frontend* frontend = lajkonik::Frontend::Create(&engine);
#ifdef USE_READLINE
  rl_attempted_completion_function = AntaresCompletion;
#endif  // USE_READLINE
  char* command;
  while ((command = GetLine()) != NULL) {
#ifdef USE_READLINE
    if (command[0] != '\0')
      add_history(command);
#endif  // USE_RADLINE
    frontend->HandleCommand(command);
  }
  return 0;
}
