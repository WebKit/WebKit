// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause
//
// Tests the jpeg/png/y4m reading code from avifenc.

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "aviftest_helpers.h"
#include "avifutil.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

using ::fuzztest::Arbitrary;
using ::fuzztest::ElementOf;

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

//------------------------------------------------------------------------------

std::string FileFormatToString(avifAppFileFormat file_format) {
  switch (file_format) {
    case AVIF_APP_FILE_FORMAT_PNG:
      return "PNG";
    case AVIF_APP_FILE_FORMAT_JPEG:
      return "JPEG";
    case AVIF_APP_FILE_FORMAT_Y4M:
      return "Y4M";
    default:
      assert(false);
      return "unknown";
  }
}

void ReadImageFile(const std::string& arbitrary_bytes,
                   avifPixelFormat requested_format, int requested_depth,
                   avifChromaDownsampling chroma_downsampling,
                   bool ignore_color_profile, bool ignore_exif, bool ignore_xmp,
                   bool allow_changing_cicp, bool ignore_gain_map,
                   avifMatrixCoefficients matrix_coefficients) {
  ASSERT_FALSE(GetSeedDataDirs().empty());  // Make sure seeds are available.

  // Write the byte stream to a temp file since avifReadImage() takes a file
  // path as input.
  const std::string file_path = testing::TempDir() + "inputimage";
  std::ofstream out(file_path);
  out << arbitrary_bytes;
  out.close();

  uint32_t out_depth;
  avifAppSourceTiming timing;
  ImagePtr avif_image(avifImageCreateEmpty());
  avif_image->matrixCoefficients = matrix_coefficients;

  // OSS-Fuzz limits the allocated memory to 2560 MB. Consider 16-bit samples.
  constexpr uint32_t kImageSizeLimit =
      2560u * 1024 * 1024 / AVIF_MAX_AV1_LAYER_COUNT / sizeof(uint16_t);
  // SharpYUV is computationally expensive. Avoid timeouts.
  const uint32_t imageSizeLimit =
      (chroma_downsampling == AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV &&
       requested_format == AVIF_PIXEL_FORMAT_YUV420)
          ? kImageSizeLimit / 4
          : kImageSizeLimit;

  const avifAppFileFormat file_format = avifReadImage(
      file_path.c_str(), requested_format, requested_depth, chroma_downsampling,
      ignore_color_profile, ignore_exif, ignore_xmp, allow_changing_cicp,
      ignore_gain_map, imageSizeLimit, avif_image.get(), &out_depth, &timing,
      /*frameIter=*/nullptr);

  if (file_format != AVIF_APP_FILE_FORMAT_UNKNOWN) {
    EXPECT_GT(avif_image->width, 0);
    EXPECT_GT(avif_image->height, 0);

    if (requested_depth != 0 && file_format != AVIF_APP_FILE_FORMAT_Y4M) {
      EXPECT_EQ(avif_image->depth, requested_depth);
    }
    if (file_format != AVIF_APP_FILE_FORMAT_Y4M) {
      EXPECT_EQ(avif_image->yuvFormat, requested_format);
    }
    if (ignore_color_profile) {
      EXPECT_EQ(avif_image->icc.size, 0);
    }
    if (ignore_exif) {
      EXPECT_EQ(avif_image->exif.size, 0);
    }
    if (ignore_xmp) {
      EXPECT_EQ(avif_image->xmp.size, 0);
    }
    std::cout << "Decode successful (" << FileFormatToString(file_format)
              << ")\n";
  }
}

FUZZ_TEST(ReadImageFuzzTest, ReadImageFile)
    .WithDomains(ArbitraryImageWithSeeds({AVIF_APP_FILE_FORMAT_JPEG,
                                          AVIF_APP_FILE_FORMAT_PNG,
                                          AVIF_APP_FILE_FORMAT_Y4M}),
                 ArbitraryPixelFormat(),
                 /*requested_depth=*/ElementOf({0, 8, 10, 12}),
                 ElementOf({AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                            AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                            AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
                            AVIF_CHROMA_DOWNSAMPLING_AVERAGE,
                            AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV}),
                 /*ignore_color_profile=*/Arbitrary<bool>(),
                 /*ignore_exif=*/Arbitrary<bool>(),
                 /*ignore_xmp=*/Arbitrary<bool>(),
                 /*allow_changing_cicp=*/Arbitrary<bool>(),
                 /*ignore_gain_map=*/Arbitrary<bool>(),
                 ElementOf({AVIF_MATRIX_COEFFICIENTS_IDENTITY,
                            AVIF_MATRIX_COEFFICIENTS_BT709,
                            AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED,
                            AVIF_MATRIX_COEFFICIENTS_FCC,
                            AVIF_MATRIX_COEFFICIENTS_BT470BG,
                            AVIF_MATRIX_COEFFICIENTS_BT601,
                            AVIF_MATRIX_COEFFICIENTS_SMPTE240,
                            AVIF_MATRIX_COEFFICIENTS_YCGCO,
                            AVIF_MATRIX_COEFFICIENTS_BT2020_NCL,
                            AVIF_MATRIX_COEFFICIENTS_BT2020_CL,
                            AVIF_MATRIX_COEFFICIENTS_SMPTE2085,
                            AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL,
                            AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL,
                            AVIF_MATRIX_COEFFICIENTS_ICTCP}));

//------------------------------------------------------------------------------

}  // namespace
}  // namespace testutil
}  // namespace avif
