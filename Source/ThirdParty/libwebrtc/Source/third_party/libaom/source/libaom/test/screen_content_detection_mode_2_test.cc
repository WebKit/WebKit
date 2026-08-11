/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "gtest/gtest.h"

#include "av1/encoder/encoder.h"

namespace {

/* clang-format off */
// Test the example taken from av1_dilate_block()
constexpr uint8_t kSource[] =   {   0,   0,   1,   2, 255,   3,   4,   0,   0,
                                    0,   5,   6, 255, 255, 255,   7,   8,   0,
                                    0, 255, 255, 255, 255, 255, 255, 255,   0,
                                    0, 255, 255, 255, 255, 255, 255, 255,   0,
                                    0,   9,  10, 255, 255, 255,  11,  12,   0,
                                    0,   0,  13,  14, 255,  15,  16,   0,   0};

constexpr uint8_t kExpected[] = {   0,   0, 255, 255, 255, 255, 255,   0,   0,
                                  255, 255, 255, 255, 255, 255, 255, 255, 255,
                                  255, 255, 255, 255, 255, 255, 255, 255, 255,
                                  255, 255, 255, 255, 255, 255, 255, 255, 255,
                                  255, 255, 255, 255, 255, 255, 255, 255, 255,
                                    0,   0, 255, 255, 255, 255, 255,   0,   0};
/* clang-format on */

constexpr int kWidth = 9;
constexpr int kHeight = 6;

TEST(ScreenContentDetectionMode2, FindDominantValue) {
  // Find the dominant value of kSource[], which should be 255,
  // as it appears 22 times. This is in contrast to 0 (16 times).
  EXPECT_EQ(av1_find_dominant_value(kSource, kWidth, /*rows=*/kHeight,
                                    /*cols=*/kWidth),
            255);
}

TEST(ScreenContentDetectionMode2, DilateBlock) {
  uint8_t dilated[kWidth * kHeight] = { 0 };

  av1_dilate_block(kSource, kWidth, dilated, kWidth, /*rows=*/kHeight,
                   /*cols=*/kWidth);

  // Compare values coming from av1_dilate_block() against the expected values
  for (int r = 0; r < kHeight; ++r) {
    for (int c = 0; c < kWidth; ++c) {
      EXPECT_EQ(kExpected[r * kHeight + c], dilated[r * kHeight + c]);
    }
  }
}

}  // namespace
