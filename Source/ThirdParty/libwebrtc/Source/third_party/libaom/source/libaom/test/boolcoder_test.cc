/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/acm_random.h"
#include "aom/aom_integer.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitwriter.h"

using libaom_test::ACMRandom;

namespace {
const int num_tests = 10;
}  // namespace

TEST(AV1, TestBitIO) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  for (int n = 0; n < num_tests; ++n) {
    for (int method = 0; method <= 7; ++method) {  // we generate various proba
      const int kBitsToTest = 1000;
      uint8_t probas[kBitsToTest];

      for (int i = 0; i < kBitsToTest; ++i) {
        const int parity = i & 1;
        /* clang-format off */
        probas[i] =
          (method == 0) ? 0 : (method == 1) ? 255 :
          (method == 2) ? 128 :
          (method == 3) ? rnd.Rand8() :
          (method == 4) ? (parity ? 0 : 255) :
            // alternate between low and high proba:
            (method == 5) ? (parity ? rnd(128) : 255 - rnd(128)) :
            (method == 6) ?
            (parity ? rnd(64) : 255 - rnd(64)) :
            (parity ? rnd(32) : 255 - rnd(32));
        /* clang-format on */
      }
      for (int bit_method = 0; bit_method <= 3; ++bit_method) {
        const int random_seed = 6432;
        const int kBufferSize = 10000;
        ACMRandom bit_rnd(random_seed);
        aom_writer bw;
        uint8_t bw_buffer[kBufferSize];
        aom_start_encode(&bw, bw_buffer);

        int bit = (bit_method == 0) ? 0 : (bit_method == 1) ? 1 : 0;
        for (int i = 0; i < kBitsToTest; ++i) {
          if (bit_method == 2) {
            bit = (i & 1);
          } else if (bit_method == 3) {
            bit = bit_rnd(2);
          }
          aom_write(&bw, bit, static_cast<int>(probas[i]));
        }

        GTEST_ASSERT_GE(aom_stop_encode(&bw), 0);

        aom_reader br;
        aom_reader_init(&br, bw_buffer, bw.pos);
        bit_rnd.Reset(random_seed);
        for (int i = 0; i < kBitsToTest; ++i) {
          if (bit_method == 2) {
            bit = (i & 1);
          } else if (bit_method == 3) {
            bit = bit_rnd(2);
          }
          GTEST_ASSERT_EQ(aom_read(&br, probas[i], nullptr), bit)
              << "pos: " << i << " / " << kBitsToTest
              << " bit_method: " << bit_method << " method: " << method;
        }
      }
    }
  }
}

#define FRAC_DIFF_TOTAL_ERROR 0.18

TEST(AV1, TestTell) {
  const int kBufferSize = 10000;
  aom_writer bw;
  uint8_t bw_buffer[kBufferSize];
  const int kSymbols = 1024;
  // Coders are noisier at low probabilities, so we start at p = 4.
  for (int p = 4; p < 256; p++) {
    double probability = p / 256.;
    aom_start_encode(&bw, bw_buffer);
    for (int i = 0; i < kSymbols; i++) {
      aom_write(&bw, 0, p);
    }
    GTEST_ASSERT_GE(aom_stop_encode(&bw), 0);
    aom_reader br;
    aom_reader_init(&br, bw_buffer, bw.pos);
    uint32_t last_tell = aom_reader_tell(&br);
    uint32_t last_tell_frac = aom_reader_tell_frac(&br);
    double frac_diff_total = 0;
    GTEST_ASSERT_GE(aom_reader_tell(&br), 0u);
    GTEST_ASSERT_LE(aom_reader_tell(&br), 1u);
    ASSERT_FALSE(aom_reader_has_overflowed(&br));
    for (int i = 0; i < kSymbols; i++) {
      aom_read(&br, p, nullptr);
      uint32_t tell = aom_reader_tell(&br);
      uint32_t tell_frac = aom_reader_tell_frac(&br);
      GTEST_ASSERT_GE(tell, last_tell)
          << "tell: " << tell << ", last_tell: " << last_tell;
      GTEST_ASSERT_GE(tell_frac, last_tell_frac)
          << "tell_frac: " << tell_frac
          << ", last_tell_frac: " << last_tell_frac;
      // Frac tell should round up to tell.
      GTEST_ASSERT_EQ(tell, (tell_frac + 7) >> 3);
      last_tell = tell;
      frac_diff_total +=
          fabs(((tell_frac - last_tell_frac) / 8.0) + log2(probability));
      last_tell_frac = tell_frac;
    }
    const uint32_t expected = (uint32_t)(-kSymbols * log2(probability));
    // Last tell should be close to the expected value.
    GTEST_ASSERT_LE(last_tell, expected + 20) << " last_tell: " << last_tell;
    // The average frac_diff error should be pretty small.
    GTEST_ASSERT_LE(frac_diff_total / kSymbols, FRAC_DIFF_TOTAL_ERROR)
        << " frac_diff_total: " << frac_diff_total;
    ASSERT_FALSE(aom_reader_has_overflowed(&br));
  }
}

TEST(AV1, TestHasOverflowed) {
  const int kBufferSize = 10000;
  aom_writer bw;
  uint8_t bw_buffer[kBufferSize];
  const int kSymbols = 1024;
  // Coders are noisier at low probabilities, so we start at p = 4.
  for (int p = 4; p < 256; p++) {
    aom_start_encode(&bw, bw_buffer);
    for (int i = 0; i < kSymbols; i++) {
      aom_write(&bw, 1, p);
    }
    GTEST_ASSERT_GE(aom_stop_encode(&bw), 0);
    aom_reader br;
    aom_reader_init(&br, bw_buffer, bw.pos);
    ASSERT_FALSE(aom_reader_has_overflowed(&br));
    for (int i = 0; i < kSymbols; i++) {
      GTEST_ASSERT_EQ(aom_read(&br, p, nullptr), 1);
      ASSERT_FALSE(aom_reader_has_overflowed(&br));
    }
    // In the worst case, the encoder uses just a tiny fraction of the last
    // byte in the buffer. So to guarantee that aom_reader_has_overflowed()
    // returns true, we have to consume very nearly 8 additional bits of data.
    // In the worse case, one of the bits in that byte will be 1, and the rest
    // will be zero. Once we are past that 1 bit, when the probability of
    // reading zero symbol from aom_read() is high, each additional symbol read
    // will consume very little additional data (in the case that p == 255,
    // approximately -log_2(255/256) ~= 0.0056 bits). In that case it would
    // take around 178 calls to consume more than 8 bits. That is only an upper
    // bound. In practice we are not guaranteed to hit the worse case and can
    // get away with 174 calls.
    for (int i = 0; i < 174; i++) {
      aom_read(&br, p, nullptr);
    }
    ASSERT_TRUE(aom_reader_has_overflowed(&br));
  }
}
