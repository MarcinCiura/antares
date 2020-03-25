.PHONY: clean gendeps
.PRECIOUS: base.o engine%.o havannah%.o antares%.o

CC := g++
#CFLAGS := -x c -O2 -fomit-frame-pointer -std=c99 -pedantic -W -Wall -Wextra -DNDEBUG
CXXFLAGS := -O3 -fomit-frame-pointer -ansi -W -Wall -Wextra
LDFLAGS := -lreadline -stdlib=libc++ -m64 -pthread  # -ldl -pthread

# Execute 'make DEBUG=...' to change the debug level.
DEBUG ?= 1
ifeq "$(DEBUG)" "0"
  CXXFLAGS += -DNDEBUG
else ifeq "$(DEBUG)" "2"
  CXXFLAGS := -g
else ifeq "$(DEBUG)" "3"
  CXXFLAGS := -O3 -pg -fprofile-arcs -ftest-coverage
  LDFLAGS += -pg -fprofile-arcs
else ifeq "$(DEBUG)" "4"
  CXXFLAGS := -g -O0 -coverage
  LDFLAGS += -coverage
endif

# Use awk instead of bash arays since sh does not have them.
GCC_VERSION := $(shell $(CC) --version | head -1 | awk '{print $$3}')
# 1 means that the inequality is false.
GCC_HAS_ATOMIC_OPS := $(shell [[ "$(GCC_VERSION)" < "4.1" ]]; echo $$?)
GCC_HAS_MARCH_NATIVE := $(shell [[ "$(GCC_VERSION)" < "4.3" ]]; echo $$?)

OS_TYPE := $(shell uname -s)
ifeq "$(OS_TYPE)" "Darwin"
  CC := clang++
  CPU_COUNT := $(shell sysctl -n hw.ncpu)
  RAM_SIZE := $(shell sysctl -n hw.usermem)
else ifeq "$(OS_TYPE)" "Linux"
  CPU_COUNT := $(shell grep -c ^processor /proc/cpuinfo)
  RAM_SIZE := $(shell free -b | head -2 | tail -1 | awk '{print $$2}')
endif

NUM_THREADS ?= $(shell echo $(CPU_COUNT) | awk '{print int(0.7*$$1+1)}')
LOG2_NUM_ENTRIES ?= $(shell echo $(RAM_SIZE) | awk '{x=int(log($$1/32/2)/log(2));if(x>26)x=26;print x}')

ifeq "$(GCC_HAS_MARCH_NATIVE)" "1"
  CFLAGS += -march=native
  CXXFLAGS += -march=native
else
  CFLAGS += -m64
  CXXFLAGS += -m64
endif

all: antares-4

antares-%: base.o antares%.o engine%.o havannah%.o
	$(CC) $(LDFLAGS) $^ -o $@

test: test10.o base.o havannah10.o
	$(CC) $(LDFLAGS) $^ -o $@

# Edited output of make gendeps.
base.o: base.cc base.h
	$(CC) $(CXXFLAGS) -c $< -o $@

antares%.o: antares.cc engine.h havannah.h base.h
	$(CC) $(CXXFLAGS) -DSIDE_LENGTH=$* -c $< -o $@

engine%.o: engine.cc engine.h havannah.h base.h wfhashmap.h
	$(CC) $(CXXFLAGS) -DSIDE_LENGTH=$* -c $< -o $@

havannah%.o: havannah.cc havannah.h base.h
	$(CC) $(CXXFLAGS) -DSIDE_LENGTH=$* -c $< -o $@

test%.o: test.cc fct.h havannah.h base.h
	$(CC) $(CXXFLAGS) -DSIDE_LENGTH=$* -c $< -o $@

clean:
	$(RM) *.o *.gcda *.gcno *.gcov gmon.out antares-* test

fresh: clean all

gendeps:
	ls -1 *.cc *.c | xargs -L 1 cc -M -MM
