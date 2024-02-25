// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <limits>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

TEST(AvifImageTest, CreateEmpty) {
  ImagePtr empty(avifImageCreateEmpty());
  EXPECT_NE(empty, nullptr);
}

bool IsValidAvifImageCreate(uint32_t width, uint32_t height, uint32_t depth,
                            avifPixelFormat format) {
  ImagePtr image(avifImageCreate(width, height, depth, format));
  return image != nullptr;
}

TEST(AvifImageTest, Create) {
  EXPECT_TRUE(IsValidAvifImageCreate(0, 0, 0, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_TRUE(
      IsValidAvifImageCreate(1, 1, /*depth=*/1, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_TRUE(
      IsValidAvifImageCreate(64, 64, /*depth=*/8, AVIF_PIXEL_FORMAT_NONE));
  EXPECT_TRUE(IsValidAvifImageCreate(std::numeric_limits<uint32_t>::max(),
                                     std::numeric_limits<uint32_t>::max(),
                                     /*depth=*/16, AVIF_PIXEL_FORMAT_NONE));
}

TEST(AvifImageTest, Invalid) {
  EXPECT_FALSE(IsValidAvifImageCreate(0, 0, 0, AVIF_PIXEL_FORMAT_COUNT));
  EXPECT_FALSE(
      IsValidAvifImageCreate(0, 0, /*depth=*/17, AVIF_PIXEL_FORMAT_YUV400));
}

TEST(AvifImageTest, WriteImage) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/10,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());
  ASSERT_TRUE(testutil::WriteImage(
      image.get(), (testing::TempDir() + "/avifimagetest.png").c_str()));
}

}  // namespace
}  // namespace avif
