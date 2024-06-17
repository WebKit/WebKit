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

#include "config/aom_config.h"

#include "test/acm_random.h"
#include "aom/aom_integer.h"
#include "aom_dsp/bitreader.h"
#include "aom_dsp/bitwriter.h"
#include "aom_dsp/binary_codes_reader.h"
#include "aom_dsp/binary_codes_writer.h"

#define ACCT_STR __func__

using libaom_test::ACMRandom;

namespace {

// Test for Finite subexponential code with reference
TEST(AV1, TestPrimitiveRefsubexpfin) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());
  const int kBufferSize = 65536;
  aom_writer bw;
  uint8_t bw_buffer[kBufferSize];
  const uint16_t kRanges = 8;
  const uint16_t kSubexpParams = 6;
  const uint16_t kReferences = 8;
  const uint16_t kValues = 16;
  uint16_t enc_values[kRanges][kSubexpParams][kReferences][kValues][4];
  const uint16_t range_vals[kRanges] = { 1, 13, 64, 120, 230, 420, 1100, 8000 };
  aom_start_encode(&bw, bw_buffer);
  for (int n = 0; n < kRanges; ++n) {
    const uint16_t range = range_vals[n];
    for (int k = 0; k < kSubexpParams; ++k) {
      for (int r = 0; r < kReferences; ++r) {
        const uint16_t ref = rnd(range);
        for (int v = 0; v < kValues; ++v) {
          const uint16_t value = rnd(range);
          enc_values[n][k][r][v][0] = range;
          enc_values[n][k][r][v][1] = k;
          enc_values[n][k][r][v][2] = ref;
          enc_values[n][k][r][v][3] = value;
          aom_write_primitive_refsubexpfin(&bw, range, k, ref, value);
        }
      }
    }
  }
  GTEST_ASSERT_GE(aom_stop_encode(&bw), 0);
  aom_reader br;
  aom_reader_init(&br, bw_buffer, bw.pos);
  GTEST_ASSERT_GE(aom_reader_tell(&br), 0u);
  GTEST_ASSERT_LE(aom_reader_tell(&br), 1u);
  for (int n = 0; n < kRanges; ++n) {
    for (int k = 0; k < kSubexpParams; ++k) {
      for (int r = 0; r < kReferences; ++r) {
        for (int v = 0; v < kValues; ++v) {
          const uint16_t range = enc_values[n][k][r][v][0];
          assert(k == enc_values[n][k][r][v][1]);
          const uint16_t ref = enc_values[n][k][r][v][2];
          const uint16_t value =
              aom_read_primitive_refsubexpfin(&br, range, k, ref, ACCT_STR);
          GTEST_ASSERT_EQ(value, enc_values[n][k][r][v][3]);
        }
      }
    }
  }
}
// TODO(debargha): Adds tests for other primitives
}  // namespace
