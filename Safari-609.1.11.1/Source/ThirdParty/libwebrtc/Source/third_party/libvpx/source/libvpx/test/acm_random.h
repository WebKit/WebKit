/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_ACM_RANDOM_H_
#define TEST_ACM_RANDOM_H_

#include <assert.h>

#include <limits>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "vpx/vpx_integer.h"

namespace libvpx_test {

class ACMRandom {
 public:
  ACMRandom() : random_(DeterministicSeed()) {}

  explicit ACMRandom(int seed) : random_(seed) {}

  void Reset(int seed) { random_.Reseed(seed); }
  uint16_t Rand16(void) {
    const uint32_t value =
        random_.Generate(testing::internal::Random::kMaxRange);
    return (value >> 15) & 0xffff;
  }

  int16_t Rand9Signed(void) {
    // Use 9 bits: values between 255 (0x0FF) and -256 (0x100).
    const uint32_t value = random_.Generate(512);
    return static_cast<int16_t>(value) - 256;
  }

  uint8_t Rand8(void) {
    const uint32_t value =
        random_.Generate(testing::internal::Random::kMaxRange);
    // There's a bit more entropy in the upper bits of this implementation.
    return (value >> 23) & 0xff;
  }

  uint8_t Rand8Extremes(void) {
    // Returns a random value near 0 or near 255, to better exercise
    // saturation behavior.
    const uint8_t r = Rand8();
    return r < 128 ? r << 4 : r >> 4;
  }

  uint32_t RandRange(const uint32_t range) {
    // testing::internal::Random::Generate provides values in the range
    // testing::internal::Random::kMaxRange.
    assert(range <= testing::internal::Random::kMaxRange);
    return random_.Generate(range);
  }

  int PseudoUniform(int range) { return random_.Generate(range); }

  int operator()(int n) { return PseudoUniform(n); }

  static int DeterministicSeed(void) { return 0xbaba; }

 private:
  testing::internal::Random random_;
};

}  // namespace libvpx_test

#endif  // TEST_ACM_RANDOM_H_
