// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <sstream>
#include <tuple>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"
#include "y4m.h"

using testing::Combine;
using testing::Values;

namespace avif {
namespace {

class Y4mTest
    : public testing::TestWithParam<
          std::tuple</*width=*/int, /*height=*/int, /*bit_depth=*/int,
                     /*yuv_format=*/avifPixelFormat, /*yuv_range=*/avifRange,
                     /*create_alpha=*/bool>> {};

TEST_P(Y4mTest, EncodeDecode) {
  const int width = std::get<0>(GetParam());
  const int height = std::get<1>(GetParam());
  const int bit_depth = std::get<2>(GetParam());
  const avifPixelFormat yuv_format = std::get<3>(GetParam());
  const avifRange yuv_range = std::get<4>(GetParam());
  const bool create_alpha = std::get<5>(GetParam());
  std::ostringstream file_path;
  file_path << testing::TempDir() << "avify4mtest_encodedecode_" << width << "_"
            << height << "_" << bit_depth << "_" << yuv_format << "_"
            << yuv_range << "_" << create_alpha;

  ImagePtr image = testutil::CreateImage(
      width, height, bit_depth, yuv_format,
      create_alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV, yuv_range);
  ASSERT_NE(image, nullptr);
  const uint32_t yuva[] = {
      (yuv_range == AVIF_RANGE_LIMITED) ? (235u << (bit_depth - 8))
                                        : ((1u << bit_depth) - 1),
      (yuv_range == AVIF_RANGE_LIMITED) ? (240u << (bit_depth - 8))
                                        : ((1u << bit_depth) - 1),
      (yuv_range == AVIF_RANGE_LIMITED) ? (240u << (bit_depth - 8))
                                        : ((1u << bit_depth) - 1),
      (1u << bit_depth) - 1};
  testutil::FillImagePlain(image.get(), yuva);
  ASSERT_TRUE(y4mWrite(file_path.str().c_str(), image.get()));

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  ASSERT_TRUE(y4mRead(file_path.str().c_str(), AVIF_DEFAULT_IMAGE_SIZE_LIMIT,
                      decoded.get(), /*sourceTiming=*/nullptr,
                      /*iter=*/nullptr));

  EXPECT_TRUE(testutil::AreImagesEqual(*image, *decoded));
}

TEST_P(Y4mTest, OutOfRange) {
  const int width = std::get<0>(GetParam());
  const int height = std::get<1>(GetParam());
  const int bit_depth = std::get<2>(GetParam());
  const avifPixelFormat yuv_format = std::get<3>(GetParam());
  const avifRange yuv_range = std::get<4>(GetParam());
  const bool create_alpha = std::get<5>(GetParam());
  std::ostringstream file_path;
  file_path << testing::TempDir() << "avify4mtest_outofrange_" << width << "_"
            << height << "_" << bit_depth << "_" << yuv_format << "_"
            << yuv_range << "_" << create_alpha;

  ImagePtr image = testutil::CreateImage(
      width, height, bit_depth, yuv_format,
      create_alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV, yuv_range);
  ASSERT_NE(image, nullptr);

  // Insert values that may be out-of-range on purpose compared to the specified
  // bit_depth and yuv_range.
  const uint32_t yuva8[] = {255, 0, 255, 255};
  const uint32_t yuva16[] = {0, (1u << 16) - 1u, 0, (1u << 16) - 1u};
  testutil::FillImagePlain(image.get(),
                           avifImageUsesU16(image.get()) ? yuva16 : yuva8);
  ASSERT_TRUE(y4mWrite(file_path.str().c_str(), image.get()));

  // y4mRead() should clamp the values to respect the specified depth in order
  // to avoid computation with unexpected sample values. However, it does not
  // respect the limited ("video") range because the libavif API just passes
  // that tag along, it is ignored by the compression algorithm.
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  ASSERT_TRUE(y4mRead(file_path.str().c_str(), AVIF_DEFAULT_IMAGE_SIZE_LIMIT,
                      decoded.get(), /*sourceTiming=*/nullptr,
                      /*iter=*/nullptr));

  // Pass it through the libavif API to make sure reading a bad y4m does not
  // trigger undefined behavior.
  const testutil::AvifRwData encoded = testutil::Encode(decoded.get());
  EXPECT_NE(testutil::Decode(encoded.data, encoded.size), nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    OpaqueCombinations, Y4mTest,
    Combine(/*width=*/Values(1, 2, 3),
            /*height=*/Values(1, 2, 3),
            /*depths=*/Values(8, 10, 12),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400),
            Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
            /*create_alpha=*/Values(false)));

// Writing alpha is currently only supported in 8bpc YUV444.
INSTANTIATE_TEST_SUITE_P(AlphaCombinations, Y4mTest,
                         Combine(/*width=*/Values(1, 2, 3),
                                 /*height=*/Values(1, 2, 3),
                                 /*depths=*/Values(8),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
                                 /*create_alpha=*/Values(true)));

}  // namespace
}  // namespace avif
