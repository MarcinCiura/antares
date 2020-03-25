#ifndef RNG_H_
#define RNG_H_

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

// George Marsaglia's ultra-fast XorShift random number generator.

#include <algorithm>
#include <vector>

namespace lajkonik {

// Thread-unsafe.
class Rng {
 public:
  Rng() {}
  ~Rng() {}

  // Initializes the seed_.
  void Init(unsigned seed) { seed_ = seed; }
  // Generates a random integer in the [0...N - 1] range.
  int operator()(int n) {
    return (static_cast<unsigned long long>(XorShift()) * n) >> 32ULL;
  }
  // Shuffles a container.
  template<class T>
  void Shuffle(T begin, T end) {
    std::random_shuffle(begin, end, *this);
  }
  // Picks a random element of a vector.
  template<class T>
  const T& GetRandomElement(const std::vector<T>& v) {
    return v[operator()(v.size())];
  }

 private:
  // The underlying 32-bit XorShift algorithm.
  unsigned XorShift() {
    unsigned tmp = seed_;
    tmp ^= tmp << 13;
    tmp ^= tmp >> 17;
    tmp ^= tmp << 5;
    seed_ = tmp;
    return tmp;
  }

  // The seed of the random number generator.
  unsigned seed_;

  Rng(const Rng&);
  void operator=(const Rng&);
};

}  // namespace lajkonik

#endif  // RNG_H_
