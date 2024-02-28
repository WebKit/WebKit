// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstring>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "aviftest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

using ::fuzztest::Arbitrary;
using ::fuzztest::ElementOf;
using ::fuzztest::InRange;

namespace avif {
namespace testutil {
namespace {

void Convert(ImagePtr image, int rgb_depth, int rgb_format,
             avifChromaUpsampling upsampling,
             avifChromaDownsampling downsampling, bool avoid_libyuv,
             bool ignore_alpha, bool alpha_premultiplied, bool is_float,
             int max_threads) {
  AvifRgbImage rgb(image.get(), rgb_depth,
                   static_cast<avifRGBFormat>(rgb_format));
  rgb.chromaUpsampling = upsampling;
  rgb.chromaDownsampling = downsampling;
  rgb.avoidLibYUV = avoid_libyuv;
  rgb.ignoreAlpha = ignore_alpha;
  rgb.alphaPremultiplied = alpha_premultiplied;
  rgb.isFloat = is_float;
  rgb.maxThreads = max_threads;

  // See avifRGBFormat and avifRGBImage.
  const avifResult expected_yuv_to_rgb_result =
      (rgb_format == AVIF_RGB_FORMAT_RGB_565 && rgb_depth != 8)
          ? AVIF_RESULT_REFORMAT_FAILED
      : (is_float && rgb_depth != 16) ? AVIF_RESULT_REFORMAT_FAILED
      : (max_threads < 0)             ? AVIF_RESULT_REFORMAT_FAILED
                                      : AVIF_RESULT_OK;
  const avifResult expected_rgb_to_yuv_result =
      (rgb_format == AVIF_RGB_FORMAT_RGB_565) ? AVIF_RESULT_REFORMAT_FAILED
      : (is_float && rgb_depth != 16)         ? AVIF_RESULT_REFORMAT_FAILED
      : (is_float)                            ? AVIF_RESULT_NOT_IMPLEMENTED
                                              : AVIF_RESULT_OK;

  ASSERT_EQ(avifImageYUVToRGB(image.get(), &rgb), expected_yuv_to_rgb_result);
  if (expected_yuv_to_rgb_result != AVIF_RESULT_OK) {
    // Fill pixels with something, so that avifImageRGBToYUV() can be called.
    AvifRgbImage rgb_ok(image.get(), rgb_depth, AVIF_RGB_FORMAT_RGBA);
    ASSERT_EQ(avifImageYUVToRGB(image.get(), &rgb_ok), AVIF_RESULT_OK);
    std::memcpy(rgb.pixels, rgb_ok.pixels, rgb.rowBytes * rgb.height);
  }

  ASSERT_EQ(avifImageRGBToYUV(image.get(), &rgb), expected_rgb_to_yuv_result);
}

FUZZ_TEST(YuvRgbFuzzTest, Convert)
    .WithDomains(ArbitraryAvifImage(),
                 /*rgb_depth=*/ElementOf({8, 10, 12, 16}),
                 InRange(0, int{AVIF_RGB_FORMAT_COUNT} - 1),
                 ElementOf({AVIF_CHROMA_UPSAMPLING_AUTOMATIC,
                            AVIF_CHROMA_UPSAMPLING_FASTEST,
                            AVIF_CHROMA_UPSAMPLING_BEST_QUALITY,
                            AVIF_CHROMA_UPSAMPLING_NEAREST,
                            AVIF_CHROMA_UPSAMPLING_BILINEAR}),
                 ElementOf({AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                            AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                            AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
                            AVIF_CHROMA_DOWNSAMPLING_AVERAGE,
                            AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV}),
                 /*avoid_libyuv=*/Arbitrary<bool>(),
                 /*ignore_alpha=*/Arbitrary<bool>(),
                 /*alpha_premultiplied=*/Arbitrary<bool>(),
                 /*is_float=*/Arbitrary<bool>(),
                 /*max_threads=*/ElementOf({-1, 0, 1, 2, 8}));

//------------------------------------------------------------------------------

}  // namespace
}  // namespace testutil
}  // namespace avif
