// Copyright 2022 Google LLC. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <ostream>

#include "avif/avif.h"
#include "avif/internal.h"
#include "gtest/gtest.h"

namespace {

struct SetTileConfigurationTestParams {
  int threads;
  uint32_t width;
  uint32_t height;
  int expected_tile_rows_log2;
  int expected_tile_cols_log2;
};

std::ostream& operator<<(std::ostream& os,
                         const SetTileConfigurationTestParams& test) {
  return os << "SetTileConfigurationTestParams { threads:" << test.threads
            << " width:" << test.width << " height:" << test.height << " }";
}

TEST(TilingTest, SetTileConfiguration) {
  constexpr int kThreads = 8;
  int tile_rows_log2;
  int tile_cols_log2;

  constexpr SetTileConfigurationTestParams kTests[]{
      // 144p
      {kThreads, 256, 144, 0, 0},
      // 240p
      {kThreads, 426, 240, 0, 0},
      // 360p
      {kThreads, 640, 360, 0, 0},
      // 480p
      {kThreads, 854, 480, 0, 1},
      // 720p
      {kThreads, 1280, 720, 1, 1},
      // 1080p
      {kThreads, 1920, 1080, 1, 2},
      // 2K
      {kThreads, 2560, 1440, 1, 2},
      // 4K
      {32, 3840, 2160, 2, 3},
      // 8K
      {32, 7680, 4320, 2, 3},
      // Kodak image set: 768x512
      {kThreads, 768, 512, 0, 1},
      {kThreads, 16384, 64, 0, 2},
  };

  for (const auto& test : kTests) {
    avifSetTileConfiguration(test.threads, test.width, test.height,
                             &tile_rows_log2, &tile_cols_log2);
    EXPECT_EQ(tile_rows_log2, test.expected_tile_rows_log2) << test;
    EXPECT_EQ(tile_cols_log2, test.expected_tile_cols_log2) << test;
    // Swap width and height.
    avifSetTileConfiguration(test.threads, test.height, test.width,
                             &tile_rows_log2, &tile_cols_log2);
    EXPECT_EQ(tile_rows_log2, test.expected_tile_cols_log2) << test;
    EXPECT_EQ(tile_cols_log2, test.expected_tile_rows_log2) << test;
  }
}

}  // namespace
