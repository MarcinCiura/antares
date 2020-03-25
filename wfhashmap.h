#ifndef WFHASHMAP_H_
#define WFHASHMAP_H_

// Copyright (c) 2011-2012 Marcin Ciura, Piotr Wieczorek
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

// Definitions of atomic operations and the WaitFreeHashMap class.

#include <assert.h>
#include <string.h>

namespace lajkonik {

#if NUM_THREADS > 1
template<typename T>
T AtomicIncrement(volatile T* ptr, T increment) {
  return __sync_add_and_fetch(ptr, increment);
}

template<typename T>
T AtomicCompareAndSwap(volatile T* ptr, T oldval, T newval) {
  return __sync_val_compare_and_swap(ptr, oldval, newval);
}

template<typename T>
bool AtomicIncrementIfFalse(
    volatile T* ptr, T increment, bool(*predicate)(T)) {
  while (true) {
    const T oldval = *ptr;
    if (predicate(oldval))
      return false;
    if (AtomicCompareAndSwap(ptr, oldval, oldval + increment) == oldval)
      return true;
  }
}
#else
template<typename T>
T AtomicIncrement(T* ptr, T increment) {
  return (*ptr += increment);
}

template<typename T>
T AtomicCompareAndSwap(T* ptr, T oldval, T newval) {
  const T ptrval = *ptr;
  if (ptrval == oldval)
    *ptr = newval;
  return ptrval;
}

template<typename T>
bool AtomicIncrementIfFalse(T* ptr, T increment, bool(*predicate)(T)) {
  if (predicate(*ptr))
    return false;
  *ptr += increment;
  return true;
}
#endif  // NUM_THREADS > 1

template<typename Key, typename Value, int kLogCapacity>
class WaitFreeHashMap {
 public:
  WaitFreeHashMap() {}
  ~WaitFreeHashMap() {}

  void Clear() {
    for (int i = 0; i < kCapacity; ++i) {
      *keys(i) = kEmptyKey;
      values(i)->Init();
    }
    memset(num_elements_, 0, sizeof num_elements_);
  }

  Value* InsertKey(Key key) {
    if (num_elements_[0] > kLimit / ARRAYSIZE(num_elements_))
      return NULL;
    int hash = PrimaryHash(key);
    Key old_key;
    if (key != kEmptyKey) {
      old_key = AtomicCompareAndSwap(keys(hash), kEmptyKey, key);
      if (old_key == kEmptyKey) {
        increment_num_elements(key);
        return values(hash);
      } else if (old_key == key) {
        return values(hash);
      }
      const int jump = SecondaryHash(key);
      while (true) {
        hash = (hash + jump) % kCapacity;
        old_key = AtomicCompareAndSwap(keys(hash), kEmptyKey, key);
        if (old_key == kEmptyKey) {
          increment_num_elements(key);
          return values(hash);
        } else if (old_key == key) {
          return values(hash);
        }
      }
    } else {
      old_key = AtomicCompareAndSwap(keys(hash), kEmptyKey, kEmptyKey + 1);
      if (old_key == kEmptyKey) {
        increment_num_elements(key);
        return values(PrimaryHash(kEmptyKey));
      } else {
        assert(old_key == kEmptyKey + 1);
        return values(PrimaryHash(kEmptyKey));
      }
    }
  }

  Value* FindValue(Key key) {
    int hash = PrimaryHash(key);
    Key found_key = *keys(hash);
    if (key != kEmptyKey) {
      if (found_key == key) {
        return values(hash);
      } else if (found_key == kEmptyKey) {
        return NULL;
      }
      const int jump = SecondaryHash(key);
      while (true) {
        hash = (hash + jump) % kCapacity;
        found_key = *keys(hash);
        if (found_key == key) {
          return values(hash);
        } else if (found_key == kEmptyKey) {
          return NULL;
        }
      }
    } else {
      if (found_key == kEmptyKey + 1) {
        return values(PrimaryHash(kEmptyKey));
      } else {
        assert(found_key == kEmptyKey);
        return NULL;
      }
    }
  }

  // Getter for num_elements_.
  int num_elements() const {
    int size = num_elements_[0];
    for (int i = 1; i < ARRAYSIZE(num_elements_); ++i) {
      size += num_elements_[i];
    }
    return size;
  }

 private:
  // Check assumptions about template arguments.
  STATIC_ASSERT(Key_must_be_an_unsigned_type, static_cast<Key>(-1) > 0);
  STATIC_ASSERT(the_number_of_elements_must_fit_in_int,
                kLogCapacity < 8 * sizeof(int));
  STATIC_ASSERT(the_hash_functions_must_be_independent,
                2 * kLogCapacity <= 8 * sizeof(Key));

  void increment_num_elements(Key key) {
    AtomicIncrement(&num_elements_[key % ARRAYSIZE(num_elements_)], 1);
  }

  static int PrimaryHash(Key key) { return key % kCapacity; }
  static int SecondaryHash(Key key) { return (key >> kShift) | 1; }

  static const int kCapacity = 1 << kLogCapacity;
  static const int kLimit = kCapacity * 3 / 4;
  static const int kShift = 8 * sizeof(Key) - kLogCapacity;
  static const Key kEmptyKey = 0;

#ifdef USE_SEPARATE_ARRAYS_FOR_KEYS_AND_VALUES
  Key* keys(int n) { return &keys_[n]; }
  Value* values(int n) { return &values_[n]; }
  Key keys_[kCapacity];
  Value values_[kCapacity];
#else
  Key* keys(int n) { return &array_[n].key; }
  Value* values(int n) { return &array_[n].value; }
  struct {
    Key key;
    Value value;
  } array_[kCapacity];
#endif  // USE_SEPARATE_ARRAYS_FOR_KEYS_AND_VALUES

  // The number of filled elements in this WaitFreeHashMap.
  int num_elements_[16];

  WaitFreeHashMap<Key, Value, kLogCapacity>(
      const WaitFreeHashMap<Key, Value, kLogCapacity>&);
  void operator=(const WaitFreeHashMap<Key, Value, kLogCapacity>&);
};

}  // namespace lajkonik

#endif  // WFHASHMAP_H
