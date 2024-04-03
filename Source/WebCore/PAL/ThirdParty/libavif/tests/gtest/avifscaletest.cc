// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <limits>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using testing::Combine;
using testing::Values;

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

constexpr bool kIgnoreMetadata = true;

//------------------------------------------------------------------------------

class ScaleTest
    : public testing::TestWithParam<
          std::tuple</*bit_depth=*/int, /*yuv_format=*/avifPixelFormat,
                     /*create_alpha=*/bool>> {};

TEST_P(ScaleTest, Roundtrip) {
  const int bit_depth = std::get<0>(GetParam());
  const avifPixelFormat yuv_format = std::get<1>(GetParam());
  const bool create_alpha = std::get<2>(GetParam());

  const ImagePtr image =
      testutil::ReadImage(data_path, "paris_exif_xmp_icc.jpg", yuv_format,
                          bit_depth, AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
                          kIgnoreMetadata, kIgnoreMetadata, kIgnoreMetadata);
  ASSERT_NE(image, nullptr);
  if (create_alpha && !image->alphaPlane) {
    // Simulate alpha plane with a view on luma.
    image->alphaPlane = image->yuvPlanes[AVIF_CHAN_Y];
    image->alphaRowBytes = image->yuvRowBytes[AVIF_CHAN_Y];
    image->imageOwnsAlphaPlane = false;
  }

  ImagePtr scaled_image(avifImageCreateEmpty());
  ASSERT_NE(scaled_image, nullptr);
  ASSERT_EQ(avifImageCopy(scaled_image.get(), image.get(), AVIF_PLANES_ALL),
            AVIF_RESULT_OK);

  const uint32_t scaled_width = static_cast<uint32_t>(image->width * 0.9);
  const uint32_t scaled_height = static_cast<uint32_t>(image->height * 2.14);
  avifDiagnostics diag;
  ASSERT_EQ(
      avifImageScale(scaled_image.get(), scaled_width, scaled_height, &diag),
      AVIF_RESULT_OK)
      << diag.error;
  EXPECT_EQ(scaled_image->width, scaled_width);
  EXPECT_EQ(scaled_image->height, scaled_height);

  ASSERT_EQ(
      avifImageScale(scaled_image.get(), image->width, image->height, &diag),
      AVIF_RESULT_OK)
      << diag.error;
  EXPECT_EQ(scaled_image->width, image->width);
  EXPECT_EQ(scaled_image->height, image->height);

  const double psnr = testutil::GetPsnr(*image, *scaled_image);
  EXPECT_GT(psnr, 30.0);
  EXPECT_LT(psnr, 45.0);
}

INSTANTIATE_TEST_SUITE_P(
    Some, ScaleTest,
    Combine(/*bit_depth=*/Values(8, 10, 12),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400),
            /*create_alpha=*/Values(true, false)));

TEST(ScaleTest, LargerThanDefaultLimits) {
  const ImagePtr image = testutil::ReadImage(
      data_path, "paris_exif_xmp_icc.jpg", AVIF_PIXEL_FORMAT_YUV420, 8,
      AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY, kIgnoreMetadata, kIgnoreMetadata,
      kIgnoreMetadata);
  ASSERT_NE(image, nullptr);
  avifDiagnostics diag;
  // dstWidth*dstHeight is larger than AVIF_DEFAULT_IMAGE_SIZE_LIMIT.
  EXPECT_NE(avifImageScale(image.get(), 20000, 20000, &diag), AVIF_RESULT_OK);
  // dstWidth is larger than AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT.
  EXPECT_NE(avifImageScale(image.get(), 40000, 2, &diag), AVIF_RESULT_OK);
  // dstHeight is larger than AVIF_DEFAULT_IMAGE_DIMENSION_LIMIT.
  EXPECT_NE(avifImageScale(image.get(), 2, 40000, &diag), AVIF_RESULT_OK);
}

//------------------------------------------------------------------------------

}  // namespace
}  // namespace avif

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  if (argc != 2) {
    std::cerr << "There must be exactly one argument containing the path to "
                 "the test data folder"
              << std::endl;
    return 1;
  }
  avif::data_path = argv[1];
  return RUN_ALL_TESTS();
}
