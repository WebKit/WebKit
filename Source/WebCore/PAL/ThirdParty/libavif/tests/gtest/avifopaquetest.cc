// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

TEST(OpaqueTest, AlphaAndNoAlpha) {
  for (bool alpha_is_opaque : {false, true}) {
    for (int depth : {8, 10, 12}) {
      ImagePtr alpha = testutil::CreateImage(
          1, 1, depth, AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
      ImagePtr no_alpha = testutil::CreateImage(
          1, 1, depth, AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_YUV);

      const uint32_t max_value = (1u << depth) - 1;
      const uint32_t yuva[] = {max_value, max_value, max_value,
                               alpha_is_opaque ? max_value : (max_value - 1)};
      testutil::FillImagePlain(alpha.get(), yuva);
      testutil::FillImagePlain(no_alpha.get(), yuva);

      EXPECT_EQ(testutil::AreImagesEqual(*alpha, *no_alpha), alpha_is_opaque);
    }
  }
}

}  // namespace
}  // namespace avif
