/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>

#include "aom_ports/bitops.h"
#include "av1/common/entropymode.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

TEST(Log2Test, GetMsb) {
  // Test small numbers exhaustively.
  for (unsigned int n = 1; n < 10000; n++) {
    EXPECT_EQ(get_msb(n), static_cast<int>(floor(log2(n))));
  }

  // Test every power of 2 and the two adjacent numbers.
  for (int exponent = 2; exponent < 32; exponent++) {
    const unsigned int power_of_2 = 1U << exponent;
    EXPECT_EQ(get_msb(power_of_2 - 1), exponent - 1);
    EXPECT_EQ(get_msb(power_of_2), exponent);
    EXPECT_EQ(get_msb(power_of_2 + 1), exponent);
  }
}

TEST(Log2Test, Av1CeilLog2) {
  // Test small numbers exhaustively.
  EXPECT_EQ(av1_ceil_log2(0), 0);
  for (int n = 1; n < 10000; n++) {
    EXPECT_EQ(av1_ceil_log2(n), static_cast<int>(ceil(log2(n))));
  }

  // Test every power of 2 and the two adjacent numbers.
  for (int exponent = 2; exponent < 31; exponent++) {
    const int power_of_2 = 1 << exponent;
    EXPECT_EQ(av1_ceil_log2(power_of_2 - 1), exponent);
    EXPECT_EQ(av1_ceil_log2(power_of_2), exponent);
    // The current implementation of av1_ceil_log2 only works up to 2^30.
    if (exponent < 30) {
      EXPECT_EQ(av1_ceil_log2(power_of_2 + 1), exponent + 1);
    }
  }
}
