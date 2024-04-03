// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <tuple>

#include "avif/internal.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using ::testing::Combine;
using ::testing::Values;

namespace avif {
namespace {

class SetGetRGBATest
    : public testing::TestWithParam<std::tuple<
          /*rgb_depth=*/int, avifRGBFormat, /*is_float=*/bool>> {};

TEST_P(SetGetRGBATest, SetGetTest) {
  const int rgb_depth = std::get<0>(GetParam());
  const avifRGBFormat rgb_format = std::get<1>(GetParam());
  const bool is_float = std::get<2>(GetParam());

  // Unused yuv image, simply needed to initialize the rgb image.
  ImagePtr yuv(avifImageCreate(/*width=*/13, /*height=*/17, 8,
                               AVIF_PIXEL_FORMAT_YUV444));

  testutil::AvifRgbImage rgb(yuv.get(), rgb_depth, rgb_format);
  rgb.isFloat = is_float;

  avifRGBColorSpaceInfo color_space;
  ASSERT_TRUE(avifGetRGBColorSpaceInfo(&rgb, &color_space));

  float epsilon = 1.0f / color_space.maxChannelF;
  if (rgb_format == AVIF_RGB_FORMAT_RGB_565) {
    // Only 5 bits of information per channel except G which has 6.
    epsilon = 1.0f / (1 << 5);
  } else if (rgb.isFloat) {
    epsilon = 0.0005f;  // Half precision floats are not that precise.
  }

  float pixel_read[4];
  for (uint32_t j = 0; j < rgb.height; ++j) {
    for (uint32_t i = 0; i < rgb.width; ++i) {
      // Generate some arbitrary pixel values.
      const float pixel_to_write[4] = {
          0.0f + static_cast<float>(i) / rgb.width,
          0.5f + static_cast<float>(j) / (rgb.height * 2),
          1.0f - static_cast<float>(i + j) / ((rgb.width + rgb.height) * 2),
          1.0f - static_cast<float>(i) / rgb.width};

      avifSetRGBAPixel(&rgb, i, j, &color_space, pixel_to_write);
      avifGetRGBAPixel(&rgb, i, j, &color_space, pixel_read);
      EXPECT_NEAR(pixel_read[0], pixel_to_write[0], epsilon);
      EXPECT_NEAR(pixel_read[1], pixel_to_write[1], epsilon);
      EXPECT_NEAR(pixel_read[2], pixel_to_write[2], epsilon);
      if (avifRGBFormatHasAlpha(rgb_format)) {
        EXPECT_NEAR(pixel_read[3], pixel_to_write[3], epsilon);
      } else {
        EXPECT_EQ(pixel_read[3], 1.0f);
      }
    }
  }

  // Check that 0 maps to 0 and 1.0f maps to 1.0f.
  const float pixel_zero[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  avifSetRGBAPixel(&rgb, 0, 0, &color_space, pixel_zero);
  avifGetRGBAPixel(&rgb, 0, 0, &color_space, pixel_read);
  EXPECT_EQ(pixel_read[0], pixel_zero[0]);
  EXPECT_EQ(pixel_read[1], pixel_zero[1]);
  EXPECT_EQ(pixel_read[2], pixel_zero[2]);
  EXPECT_EQ(pixel_read[3], pixel_zero[3]);

  const float pixel_one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  avifSetRGBAPixel(&rgb, 0, 0, &color_space, pixel_one);
  avifGetRGBAPixel(&rgb, 0, 0, &color_space, pixel_read);
  EXPECT_EQ(pixel_read[0], pixel_one[0]);
  EXPECT_EQ(pixel_read[1], pixel_one[1]);
  EXPECT_EQ(pixel_read[2], pixel_one[2]);
  EXPECT_EQ(pixel_read[3], pixel_one[3]);
}

TEST_P(SetGetRGBATest, GradientTest) {
  const int rgb_depth = std::get<0>(GetParam());
  const avifRGBFormat rgb_format = std::get<1>(GetParam());
  const bool is_float = std::get<2>(GetParam());

  // Only used for convenience to generate RGB values.
  ImagePtr yuv =
      testutil::CreateImage(/*width=*/13, /*height=*/17, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV444, AVIF_PLANES_ALL);
  testutil::FillImageGradient(yuv.get());

  testutil::AvifRgbImage input_rgb(yuv.get(), rgb_depth, rgb_format);
  testutil::AvifRgbImage output_rgb(yuv.get(), rgb_depth, rgb_format);
  input_rgb.isFloat = is_float;
  output_rgb.isFloat = is_float;
  ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &input_rgb), AVIF_RESULT_OK);

  avifRGBColorSpaceInfo color_space;
  ASSERT_TRUE(avifGetRGBColorSpaceInfo(&input_rgb, &color_space));

  for (uint32_t j = 0; j < input_rgb.height; ++j) {
    for (uint32_t i = 0; i < input_rgb.width; ++i) {
      float pixel[4];
      avifGetRGBAPixel(&input_rgb, i, j, &color_space, pixel);
      avifSetRGBAPixel(&output_rgb, i, j, &color_space, pixel);
    }
  }
  EXPECT_TRUE(testutil::AreImagesEqual(input_rgb, output_rgb));
}

INSTANTIATE_TEST_SUITE_P(
    NonFloatNonRgb565, SetGetRGBATest,
    Combine(/*rgb_depth=*/Values(8, 10, 12, 16),
            Values(AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_RGBA,
                   AVIF_RGB_FORMAT_ARGB, AVIF_RGB_FORMAT_BGR,
                   AVIF_RGB_FORMAT_BGRA, AVIF_RGB_FORMAT_ABGR),
            /*is_float=*/Values(false)));

INSTANTIATE_TEST_SUITE_P(Rgb565, SetGetRGBATest,
                         Combine(/*rgb_depth=*/Values(8),
                                 Values(AVIF_RGB_FORMAT_RGB_565),
                                 /*is_float=*/Values(false)));

INSTANTIATE_TEST_SUITE_P(
    Float, SetGetRGBATest,
    Combine(/*rgb_depth=*/Values(16),
            Values(AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_RGBA,
                   AVIF_RGB_FORMAT_ARGB, AVIF_RGB_FORMAT_BGR,
                   AVIF_RGB_FORMAT_BGRA, AVIF_RGB_FORMAT_ABGR),
            /*is_float=*/Values(true)));

}  // namespace
}  // namespace avif
