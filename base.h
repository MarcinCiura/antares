#ifndef BASE_H_
#define BASE_H_

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

// General-purpose macros and function declarations.

#include <assert.h>
#include <stdarg.h>
#include <string>

namespace lajkonik {

// Returns the number of elements in an array.
#define ARRAYSIZE(a) (static_cast<int>(sizeof(a) / sizeof((a)[0])))

// Causes a compile-time error if condition is false.
#define STATIC_ASSERT(name, condition) typedef int name[1 / (condition)]

// Typedef long type names.
typedef long long int64;
typedef unsigned long long uint64;

// A modification of sprintf() that returns a string.
std::string StringPrintf(const char* format, ...);

// The count of set bits in numbers 0-63.
extern const unsigned char kBitsSet[64];

// Returns the number of set bits in the six lower bits of a number.
inline int CountSetBits(int n) { return kBitsSet[n & 63]; }

// An array of magic numbers.
extern const unsigned char kMultiplyDeBruijnBitPosition[32];

// Returns the number of zeroes at the end
// of the binary representation of mask.
// From http://graphics.stanford.edu/~seander/bithacks.html
inline int CountTrailingZeroes(unsigned mask) {
  return kMultiplyDeBruijnBitPosition[((mask & -mask) * 0x077CB531U) >> 27];
}

// Returns the index of the nth lowest set bit in mask.
inline int GetIndexOfNthBit(int n, unsigned mask) {
  assert(mask != 0);
  assert(n >= 0);
  for (/**/; n != 0; --n) {
    mask &= (mask - 1);
  }
  return CountTrailingZeroes(mask);
}

}  // namespace lajkonik

#endif  // BASE_H_
