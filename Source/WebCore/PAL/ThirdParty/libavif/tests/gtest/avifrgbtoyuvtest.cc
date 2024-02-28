// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <algorithm>
#include <cmath>
#include <memory>
#include <tuple>

#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Range;
using ::testing::Values;

namespace avif {
namespace {

//------------------------------------------------------------------------------

constexpr uint32_t kModifierSize = 4 * 4;

// Modifies the pixel values of a channel in image by modifier[] (row-ordered).
template <typename PixelType>
void ModifyImageChannel(avifRGBImage* image, uint32_t channel_offset,
                        const uint8_t modifier[kModifierSize]) {
  const uint32_t channel_count = avifRGBFormatChannelCount(image->format);
  assert(channel_offset < channel_count);
  for (uint32_t y = 0, i = 0; y < image->height; ++y) {
    PixelType* pixel =
        reinterpret_cast<PixelType*>(image->pixels + image->rowBytes * y);
    for (uint32_t x = 0; x < image->width; ++x, ++i) {
      pixel[channel_offset] += modifier[i % kModifierSize];
      pixel += channel_count;
    }
  }
}

void ModifyImageChannel(avifRGBImage* image, uint32_t channel_offset,
                        const uint8_t modifier[kModifierSize]) {
  if (image->depth <= 8) {
    ModifyImageChannel<uint8_t>(image, channel_offset, modifier);
  } else {
    ModifyImageChannel<uint16_t>(image, channel_offset, modifier);
  }
}

// Accumulates stats about the differences between the images a and b.
template <typename PixelType>
void GetDiffSumAndSqDiffSum(const avifRGBImage& a, const avifRGBImage& b,
                            int64_t* diff_sum, int64_t* abs_diff_sum,
                            int64_t* sq_diff_sum, int64_t* max_abs_diff) {
  const uint32_t channel_count = avifRGBFormatChannelCount(a.format);
  for (uint32_t y = 0; y < a.height; ++y) {
    const PixelType* row_a =
        reinterpret_cast<PixelType*>(a.pixels + a.rowBytes * y);
    const PixelType* row_b =
        reinterpret_cast<PixelType*>(b.pixels + b.rowBytes * y);
    for (uint32_t x = 0; x < a.width * channel_count; ++x) {
      const int64_t diff =
          static_cast<int64_t>(row_b[x]) - static_cast<int64_t>(row_a[x]);
      *diff_sum += diff;
      *abs_diff_sum += std::abs(diff);
      *sq_diff_sum += diff * diff;
      *max_abs_diff = std::max(*max_abs_diff, std::abs(diff));
    }
  }
}

void GetDiffSumAndSqDiffSum(const avifRGBImage& a, const avifRGBImage& b,
                            int64_t* diff_sum, int64_t* abs_diff_sum,
                            int64_t* sq_diff_sum, int64_t* max_abs_diff) {
  (a.depth <= 8) ? GetDiffSumAndSqDiffSum<uint8_t>(a, b, diff_sum, abs_diff_sum,
                                                   sq_diff_sum, max_abs_diff)
                 : GetDiffSumAndSqDiffSum<uint16_t>(
                       a, b, diff_sum, abs_diff_sum, sq_diff_sum, max_abs_diff);
}

// Returns the Peak Signal-to-Noise Ratio from accumulated stats.
double GetPsnr(double sq_diff_sum, double num_diffs, double max_abs_diff) {
  if (sq_diff_sum == 0.) {
    return 99.;  // Lossless.
  }
  const double distortion =
      sq_diff_sum / (num_diffs * max_abs_diff * max_abs_diff);
  return (distortion > 0.) ? std::min(-10 * std::log10(distortion), 98.9)
                           : 98.9;  // Not lossless.
}

//------------------------------------------------------------------------------

// To exercise the chroma subsampling loss, the input samples must differ in
// each of the RGB channels. Chroma subsampling expects the input RGB channels
// to be correlated to minimize the quality loss.
constexpr uint8_t kRedNoise[kModifierSize] = {
    7,  14, 11, 5,   // Random permutation of 16 values.
    4,  6,  8,  15,  //
    2,  9,  13, 3,   //
    12, 1,  10, 0};
constexpr uint8_t kGreenNoise[kModifierSize] = {
    3,  2,  12, 15,  // Random permutation of 16 values
    14, 10, 7,  13,  // that is somewhat close to kRedNoise.
    5,  1,  9,  0,   //
    8,  4,  11, 6};
constexpr uint8_t kBlueNoise[kModifierSize] = {
    0,  8,  14, 9,   // Random permutation of 16 values
    13, 12, 2,  7,   // that is somewhat close to kGreenNoise.
    3,  1,  11, 10,  //
    6,  15, 5,  4};

//------------------------------------------------------------------------------

// Converts from RGB to YUV and back to RGB for all RGB combinations, separated
// by a color step for reasonable timing. If add_noise is true, also applies
// some noise to the input samples to exercise chroma subsampling.
void ConvertWholeRange(int rgb_depth, int yuv_depth, avifRGBFormat rgb_format,
                       avifPixelFormat yuv_format, avifRange yuv_range,
                       avifMatrixCoefficients matrix_coefficients,
                       avifChromaDownsampling chroma_downsampling,
                       bool add_noise, uint32_t rgb_step,
                       double max_abs_average_diff, double min_psnr, bool log) {
  // Deduced constants.
  const bool is_monochrome =
      (yuv_format == AVIF_PIXEL_FORMAT_YUV400);  // If so, only test grey input.
  const uint32_t rgb_max = (1 << rgb_depth) - 1;

  // The YUV upsampling treats the first and last rows and columns differently
  // than the remaining pairs of rows and columns. An image of 16 pixels is used
  // to test all these possibilities.
  static constexpr int width = 4;
  static constexpr int height = 4;
  ImagePtr yuv(avifImageCreate(width, height, yuv_depth, yuv_format));
  ASSERT_NE(yuv, nullptr);
  yuv->matrixCoefficients = matrix_coefficients;
  yuv->yuvRange = yuv_range;
  testutil::AvifRgbImage src_rgb(yuv.get(), rgb_depth, rgb_format);
  src_rgb.chromaDownsampling = chroma_downsampling;
  testutil::AvifRgbImage dst_rgb(yuv.get(), rgb_depth, rgb_format);
  const testutil::RgbChannelOffsets offsets =
      testutil::GetRgbChannelOffsets(rgb_format);

  // Alpha values are not tested here. Keep it opaque.
  if (avifRGBFormatHasAlpha(src_rgb.format)) {
    testutil::FillImageChannel(&src_rgb, offsets.a, rgb_max);
  }

  // Estimate the loss from converting RGB values to YUV and back.
  int64_t diff_sum = 0, abs_diff_sum = 0, sq_diff_sum = 0, max_abs_diff = 0;
  int64_t num_diffs = 0;
  const uint32_t max_value = rgb_max - (add_noise ? 15 : 0);
  for (uint32_t r = 0; r < max_value + rgb_step; r += rgb_step) {
    r = std::min(r, max_value);  // Test the maximum sample value even if it is
                                 // not a multiple of rgb_step.
    testutil::FillImageChannel(&src_rgb, offsets.r, r);
    if (add_noise) {
      ModifyImageChannel(&src_rgb, offsets.r, kRedNoise);
    }

    if (is_monochrome) {
      // Test only greyish input when converting to a single channel.
      testutil::FillImageChannel(&src_rgb, offsets.g, r);
      testutil::FillImageChannel(&src_rgb, offsets.b, r);
      if (add_noise) {
        ModifyImageChannel(&src_rgb, offsets.g, kGreenNoise);
        ModifyImageChannel(&src_rgb, offsets.b, kBlueNoise);
      }

      ASSERT_EQ(avifImageRGBToYUV(yuv.get(), &src_rgb), AVIF_RESULT_OK);
      ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
      GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &diff_sum, &abs_diff_sum,
                             &sq_diff_sum, &max_abs_diff);
      num_diffs += src_rgb.width * src_rgb.height * 3;  // Alpha is lossless.
    } else {
      for (uint32_t g = 0; g < max_value + rgb_step; g += rgb_step) {
        g = std::min(g, max_value);
        testutil::FillImageChannel(&src_rgb, offsets.g, g);
        if (add_noise) {
          ModifyImageChannel(&src_rgb, offsets.g, kGreenNoise);
        }
        for (uint32_t b = 0; b < max_value + rgb_step; b += rgb_step) {
          b = std::min(b, max_value);
          testutil::FillImageChannel(&src_rgb, offsets.b, b);
          if (add_noise) {
            ModifyImageChannel(&src_rgb, offsets.b, kBlueNoise);
          }

          const avifResult result = avifImageRGBToYUV(yuv.get(), &src_rgb);
          if (result == AVIF_RESULT_NOT_IMPLEMENTED &&
              src_rgb.chromaDownsampling ==
                  AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV) {
            GTEST_SKIP() << "libsharpyuv unavailable, skip test.";
          }
          ASSERT_EQ(result, AVIF_RESULT_OK);
          ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
          GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &diff_sum, &abs_diff_sum,
                                 &sq_diff_sum, &max_abs_diff);
          num_diffs +=
              src_rgb.width * src_rgb.height * 3;  // Alpha is lossless.
        }
      }
    }
  }

  // Stats and thresholds.
  // Note: The thresholds defined in this test are calibrated for libyuv fast
  //       paths. See reformat_libyuv.c. Slower non-libyuv conversions in
  //       libavif have a higher precision (using floating point operations).
  const double average_diff =
      static_cast<double>(diff_sum) / static_cast<double>(num_diffs);
  const double average_abs_diff =
      static_cast<double>(abs_diff_sum) / static_cast<double>(num_diffs);
  const double psnr = GetPsnr(static_cast<double>(sq_diff_sum),
                              static_cast<double>(num_diffs), rgb_max);
  EXPECT_LE(std::abs(average_diff), max_abs_average_diff);
  EXPECT_GE(psnr, min_psnr);

  if (log) {
    // Print stats for convenience and easier threshold tuning.
    static constexpr const char* kAvifRgbFormatToString[] = {
        "RGB", "RGBA", "ARGB", "BGR", "BGRA", "ABGR"};
    std::cout << " RGB " << rgb_depth << " bits, YUV " << yuv_depth << " bits, "
              << kAvifRgbFormatToString[rgb_format] << ", "
              << avifPixelFormatToString(yuv_format) << ", "
              << (yuv_range ? "full" : "lmtd") << ", MC " << matrix_coefficients
              << ", " << (add_noise ? "noisy" : "plain") << ", avg "
              << average_diff << ", abs avg " << average_abs_diff << ", max "
              << max_abs_diff << ", PSNR " << psnr << "dB" << std::endl;
  }
}

// Converts from RGB to YUV and back to RGB for multiple buffer dimensions to
// exercise stride computation and subsampling edge cases.
void ConvertWholeBuffer(int rgb_depth, int yuv_depth, avifRGBFormat rgb_format,
                        avifPixelFormat yuv_format, avifRange yuv_range,
                        avifMatrixCoefficients matrix_coefficients,
                        avifChromaDownsampling chroma_downsampling,
                        bool add_noise, double min_psnr) {
  // Deduced constants.
  const bool is_monochrome =
      (yuv_format == AVIF_PIXEL_FORMAT_YUV400);  // If so, only test grey input.
  const uint32_t rgb_max = (1 << rgb_depth) - 1;

  // Estimate the loss from converting RGB values to YUV and back.
  int64_t diff_sum = 0, abs_diff_sum = 0, sq_diff_sum = 0, max_abs_diff = 0;
  int64_t num_diffs = 0;
  for (int width : {1, 2, 127}) {
    for (int height : {1, 2, 251}) {
      ImagePtr yuv(avifImageCreate(width, height, yuv_depth, yuv_format));
      ASSERT_NE(yuv, nullptr);
      yuv->matrixCoefficients = matrix_coefficients;
      yuv->yuvRange = yuv_range;
      testutil::AvifRgbImage src_rgb(yuv.get(), rgb_depth, rgb_format);
      src_rgb.chromaDownsampling = chroma_downsampling;
      testutil::AvifRgbImage dst_rgb(yuv.get(), rgb_depth, rgb_format);
      const testutil::RgbChannelOffsets offsets =
          testutil::GetRgbChannelOffsets(rgb_format);

      // Fill the input buffer with whatever content.
      testutil::FillImageChannel(&src_rgb, offsets.r, /*value=*/0);
      testutil::FillImageChannel(&src_rgb, offsets.g, /*value=*/0);
      testutil::FillImageChannel(&src_rgb, offsets.b, /*value=*/0);
      if (add_noise) {
        ModifyImageChannel(&src_rgb, offsets.r, kRedNoise);
        ModifyImageChannel(&src_rgb, offsets.g,
                           is_monochrome ? kRedNoise : kGreenNoise);
        ModifyImageChannel(&src_rgb, offsets.b,
                           is_monochrome ? kRedNoise : kBlueNoise);
      }
      // Alpha values are not tested here. Keep it opaque.
      if (avifRGBFormatHasAlpha(src_rgb.format)) {
        testutil::FillImageChannel(&src_rgb, offsets.a, rgb_max);
      }

      const avifResult result = avifImageRGBToYUV(yuv.get(), &src_rgb);
      if (result == AVIF_RESULT_NOT_IMPLEMENTED &&
          src_rgb.chromaDownsampling == AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV) {
        GTEST_SKIP() << "libsharpyuv unavailable, skip test.";
      }
      ASSERT_EQ(result, AVIF_RESULT_OK);
      ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &dst_rgb), AVIF_RESULT_OK);
      GetDiffSumAndSqDiffSum(src_rgb, dst_rgb, &diff_sum, &abs_diff_sum,
                             &sq_diff_sum, &max_abs_diff);
      num_diffs += src_rgb.width * src_rgb.height * 3;
    }
  }
  // max_abs_average_diff is not tested here because it is not meaningful for
  // only 3*3 conversions as it takes the maximum difference per conversion.
  // PSNR is averaged on all pixels so it can be tested here.
  EXPECT_GE(GetPsnr(static_cast<double>(sq_diff_sum),
                    static_cast<double>(num_diffs), rgb_max),
            min_psnr);
}

//------------------------------------------------------------------------------
// Exhaustive settings
// These tests would generate too many GoogleTest instances as parameterized
// tests (TEST_P) so loops are used instead.

TEST(RGBToYUVTest, ExhaustiveSettings) {
  // Coverage of all configurations with all min/max input combinations.
  for (int rgb_depth : {8, 10, 12, 16}) {
    for (int yuv_depth : {8, 10, 12, 16}) {
      for (avifRGBFormat rgb_format :
           {AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_RGBA, AVIF_RGB_FORMAT_ARGB,
            AVIF_RGB_FORMAT_BGR, AVIF_RGB_FORMAT_BGRA, AVIF_RGB_FORMAT_ABGR}) {
        for (avifPixelFormat yuv_format :
             {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
              AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
          for (avifRange yuv_range : {AVIF_RANGE_LIMITED, AVIF_RANGE_FULL}) {
            for (decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY)
                     matrix_coefficients : {AVIF_MATRIX_COEFFICIENTS_IDENTITY,
                                            AVIF_MATRIX_COEFFICIENTS_BT601}) {
              if (matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY &&
                  yuv_format != AVIF_PIXEL_FORMAT_YUV444) {
                // See avifPrepareReformatState().
                continue;
              }
              for (avifChromaDownsampling chroma_downsampling :
                   {AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
                    AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                    AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY,
                    AVIF_CHROMA_DOWNSAMPLING_AVERAGE,
                    AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV}) {
                if (chroma_downsampling == AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV &&
                    yuv_depth > 12) {
                  // SharpYuvConvert() only supports YUV bit depths up to 12.
                  continue;
                }
                ConvertWholeRange(
                    rgb_depth, yuv_depth, rgb_format, yuv_format, yuv_range,
                    static_cast<avifMatrixCoefficients>(matrix_coefficients),
                    chroma_downsampling,
                    /*add_noise=*/true,
                    // Just try min and max values.
                    /*rgb_step=*/(1u << rgb_depth) - 1u,
                    // Barely check the results, this is mostly for coverage.
                    /*max_abs_average_diff=*/(1u << rgb_depth) - 1u,
                    /*min_psnr=*/5.0,
                    // Avoid spam.
                    /*log=*/false);
              }
            }
          }
        }
      }
    }
  }
}

TEST(RGBToYUVTest, AllMatrixCoefficients) {
  // Coverage of all configurations with all min/max input combinations.
  for (int rgb_depth : {8, 10, 12, 16}) {
    for (int yuv_depth : {8, 10, 12, 16}) {
      for (avifPixelFormat yuv_format :
           {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
            AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400}) {
        for (avifRange yuv_range : {AVIF_RANGE_LIMITED, AVIF_RANGE_FULL}) {
          for (decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY) matrix_coefficients :
               {
                   AVIF_MATRIX_COEFFICIENTS_BT709,
                   AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED,
                   AVIF_MATRIX_COEFFICIENTS_FCC,
                   AVIF_MATRIX_COEFFICIENTS_BT470BG,
                   AVIF_MATRIX_COEFFICIENTS_BT601,
                   AVIF_MATRIX_COEFFICIENTS_SMPTE240,
                   AVIF_MATRIX_COEFFICIENTS_YCGCO,
                   AVIF_MATRIX_COEFFICIENTS_BT2020_NCL,
                   AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL
                   // These are unsupported. See avifPrepareReformatState().
                   // AVIF_MATRIX_COEFFICIENTS_BT2020_CL
                   // AVIF_MATRIX_COEFFICIENTS_SMPTE2085
                   // AVIF_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL
                   // AVIF_MATRIX_COEFFICIENTS_ICTCP
               }) {
            if (matrix_coefficients == AVIF_MATRIX_COEFFICIENTS_YCGCO &&
                yuv_range == AVIF_RANGE_LIMITED) {
              // See avifPrepareReformatState().
              continue;
            }
            for (avifChromaDownsampling chroma_downsampling :
                 {AVIF_CHROMA_DOWNSAMPLING_FASTEST,
                  AVIF_CHROMA_DOWNSAMPLING_BEST_QUALITY}) {
              ConvertWholeRange(
                  rgb_depth, yuv_depth, AVIF_RGB_FORMAT_RGBA, yuv_format,
                  yuv_range,
                  static_cast<avifMatrixCoefficients>(matrix_coefficients),
                  chroma_downsampling,
                  /*add_noise=*/true,
                  // Just try min and max values.
                  /*rgb_step=*/(1u << rgb_depth) - 1u,
                  // Barely check the results, this is mostly for coverage.
                  /*max_abs_average_diff=*/(1u << rgb_depth) - 1u,
                  /*min_psnr=*/5.0,
                  // Avoid spam.
                  /*log=*/false);
            }
          }
        }
      }
    }
  }
}

//------------------------------------------------------------------------------
// Selected configurations

class RGBToYUVTest
    : public testing::TestWithParam<std::tuple<
          /*rgb_depth=*/int, /*yuv_depth=*/int, avifRGBFormat, avifPixelFormat,
          avifRange, avifMatrixCoefficients, avifChromaDownsampling,
          /*add_noise=*/bool, /*rgb_step=*/uint32_t,
          /*max_abs_average_diff=*/double, /*min_psnr=*/double>> {};

TEST_P(RGBToYUVTest, ConvertWholeRange) {
  ConvertWholeRange(
      /*rgb_depth=*/std::get<0>(GetParam()),
      /*yuv_depth=*/std::get<1>(GetParam()),
      /*rgb_format=*/std::get<2>(GetParam()),
      /*yuv_format=*/std::get<3>(GetParam()),
      /*yuv_range=*/std::get<4>(GetParam()),
      /*matrix_coefficients=*/std::get<5>(GetParam()),
      /*chroma_downsampling=*/std::get<6>(GetParam()),
      // Whether to add noise to the input RGB samples.
      // Should only impact subsampled chroma (4:2:2 and 4:2:0).
      /*add_noise=*/std::get<7>(GetParam()),
      // Testing each RGB combination would be more accurate but results are
      // similar with faster settings.
      /*rgb_step=*/std::get<8>(GetParam()),
      // Thresholds to pass.
      /*max_abs_average_diff=*/std::get<9>(GetParam()),
      /*min_psnr=*/std::get<10>(GetParam()),
      // Useful to see surrounding results when there is a failure.
      /*log=*/true);
}

TEST_P(RGBToYUVTest, ConvertWholeBuffer) {
  ConvertWholeBuffer(
      /*rgb_depth=*/std::get<0>(GetParam()),
      /*yuv_depth=*/std::get<1>(GetParam()),
      /*rgb_format=*/std::get<2>(GetParam()),
      /*yuv_format=*/std::get<3>(GetParam()),
      /*yuv_range=*/std::get<4>(GetParam()),
      /*matrix_coefficients=*/std::get<5>(GetParam()),
      /*chroma_downsampling=*/std::get<6>(GetParam()),
      // Whether to add noise to the input RGB samples.
      /*add_noise=*/std::get<7>(GetParam()),
      // Threshold to pass.
      /*min_psnr=*/std::get<10>(GetParam()));
}

// avifMatrixCoefficients-typed constants for testing::Values() to work on MSVC.
// typedef or using decltype(AVIF_MATRIX_COEFFICIENTS_IDENTITY) does not work
// (GTest template "declared using unnamed type, is used but never defined").
constexpr avifMatrixCoefficients kMatrixCoefficientsBT601 =
    AVIF_MATRIX_COEFFICIENTS_BT601;
constexpr avifMatrixCoefficients kMatrixCoefficientsBT709 =
    AVIF_MATRIX_COEFFICIENTS_BT709;
constexpr avifMatrixCoefficients kMatrixCoefficientsIdentity =
    AVIF_MATRIX_COEFFICIENTS_IDENTITY;

// This is the default avifenc setup when encoding from 8b PNG files to AVIF.
INSTANTIATE_TEST_SUITE_P(
    DefaultFormat, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(8),
            /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV420), Values(AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Values(true),
            /*rgb_step=*/Values(3),
            /*max_abs_average_diff=*/Values(0.1),  // The color drift is almost
                                                   // centered.
            /*min_psnr=*/Values(36.)  // Subsampling distortion is acceptable.
            ));

// Keeping RGB samples in full range and same or higher bit depth should not
// bring any loss in the roundtrip.
INSTANTIATE_TEST_SUITE_P(Identity8b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(8),
                                 /*yuv_depth=*/Values(8, 10, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(31),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity10b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(10),
                                 /*yuv_depth=*/Values(10, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(101),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity12b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(12),
                                 /*yuv_depth=*/Values(12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(401),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(Identity16b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(16),
                                 /*yuv_depth=*/Values(16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsIdentity),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(true),
                                 /*rgb_step=*/Values(6421),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));

// 4:4:4 and chroma subsampling have similar distortions on plain color inputs.
INSTANTIATE_TEST_SUITE_P(
    PlainAnySubsampling8b, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
               AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_FULL), Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
        /*add_noise=*/Values(false),
        /*rgb_step=*/Values(17),
        /*max_abs_average_diff=*/Values(0.02),  // The color drift is centered.
        /*min_psnr=*/Values(45.)  // RGB>YUV>RGB distortion is barely
                                  // noticeable.
        ));

// Converting grey RGB samples to full-range monochrome of same or greater bit
// depth should be lossless.
INSTANTIATE_TEST_SUITE_P(MonochromeLossless8b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(8),
                                 /*yuv_depth=*/Values(8, 10, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless10b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(10),
                                 /*yuv_depth=*/Values(10, 12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless12b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(12),
                                 /*yuv_depth=*/Values(12, 16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(1),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));
INSTANTIATE_TEST_SUITE_P(MonochromeLossless16b, RGBToYUVTest,
                         Combine(/*rgb_depth=*/Values(16),
                                 /*yuv_depth=*/Values(16),
                                 Values(AVIF_RGB_FORMAT_RGBA),
                                 Values(AVIF_PIXEL_FORMAT_YUV400),
                                 Values(AVIF_RANGE_FULL),
                                 Values(kMatrixCoefficientsBT601),
                                 Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
                                 /*add_noise=*/Values(false),
                                 /*rgb_step=*/Values(401),
                                 /*max_abs_average_diff=*/Values(0.),
                                 /*min_psnr=*/Values(99.)));

// Coverage for reformat_libsharpyuv.c.
INSTANTIATE_TEST_SUITE_P(
    SharpYuv8Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(8),
        /*yuv_depth=*/Values(8, 10, 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601, kMatrixCoefficientsBT709),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(17),
        /*max_abs_average_diff=*/Values(1.2),  // Sharp YUV introduces some
                                               // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv10Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(10),
        /*yuv_depth=*/Values(8, 10, 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(211),              // High or it would be too slow.
        /*max_abs_average_diff=*/Values(1.2),  // Sharp YUV introduces some
                                               // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv12Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(12),
        /*yuv_depth=*/Values(8, 10, 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(840),              // High or it would be too slow.
        /*max_abs_average_diff=*/Values(1.2),  // Sharp YUV introduces some
                                               // color shift.
        /*min_psnr=*/Values(34.)  // SharpYuv distortion is acceptable.
        ));
INSTANTIATE_TEST_SUITE_P(
    SharpYuv16Bit, RGBToYUVTest,
    Combine(
        /*rgb_depth=*/Values(16),
        // TODO(yguyon): Why max_abs_average_diff>28 if RGB16 to YUV10 full rng?
        /*yuv_depth=*/Values(8, /*10,*/ 12), Values(AVIF_RGB_FORMAT_RGBA),
        Values(AVIF_PIXEL_FORMAT_YUV420),
        Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
        Values(kMatrixCoefficientsBT601),
        Values(AVIF_CHROMA_DOWNSAMPLING_SHARP_YUV),
        /*add_noise=*/Values(true),
        /*rgb_step=*/Values(4567),             // High or it would be too slow.
        /*max_abs_average_diff=*/Values(2.5),  // Sharp YUV introduces some
                                               // color shift.
        /*min_psnr=*/Values(49.)  // SharpYuv distortion is acceptable.
        ));

// Can be used to print the drift of all RGB to YUV conversion possibilities.
// Also used for coverage.
INSTANTIATE_TEST_SUITE_P(
    All8b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(8),
            /*yuv_depth=*/Values(8, 10, 12, 16),
            Values(AVIF_RGB_FORMAT_RGBA, AVIF_RGB_FORMAT_BGR),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(61),  // High or it would be too slow.
            /*max_abs_average_diff=*/Values(1.),  // Not very accurate because
                                                  // of high rgb_step.
            /*min_psnr=*/Values(36.)));
INSTANTIATE_TEST_SUITE_P(
    All10b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(10),
            /*yuv_depth=*/Values(8, 10, 12, 16), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(211),  // High or it would be too slow.
            /*max_abs_average_diff=*/Values(0.2),  // Not very accurate because
                                                   // of high rgb_step.
            /*min_psnr=*/Values(47.)));
INSTANTIATE_TEST_SUITE_P(
    All12b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(12),
            /*yuv_depth=*/Values(8, 10, 12, 16), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(809),  // High or it would be too slow.
            /*max_abs_average_diff=*/Values(0.3),  // Not very accurate because
                                                   // of high rgb_step.
            /*min_psnr=*/Values(52.)));
INSTANTIATE_TEST_SUITE_P(
    All16b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(16),
            /*yuv_depth=*/Values(16), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420),
            Values(AVIF_RANGE_LIMITED, AVIF_RANGE_FULL),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(16001),  // High or it would be too slow.
            /*max_abs_average_diff=*/Values(0.05),
            /*min_psnr=*/Values(80.)));

// This was used to estimate the quality loss of libyuv for RGB-to-YUV.
// Disabled because it takes a few minutes.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_All8bTo8b, RGBToYUVTest,
    Combine(/*rgb_depth=*/Values(8),
            /*yuv_depth=*/Values(8), Values(AVIF_RGB_FORMAT_RGBA),
            Values(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                   AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400),
            Values(AVIF_RANGE_FULL, AVIF_RANGE_LIMITED),
            Values(kMatrixCoefficientsBT601),
            Values(AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC),
            /*add_noise=*/Bool(),
            /*rgb_step=*/Values(3),  // way faster and 99% similar to rgb_step=1
            /*max_abs_average_diff=*/Values(10.),
            /*min_psnr=*/Values(10.)));

// Converts YUV pixels to RGB using one thread and multiple threads and checks
// whether the results of both are identical.
class YUVToRGBThreadingTest
    : public testing::TestWithParam<std::tuple<
          /*rgb_depth=*/int, /*yuv_depth=*/int,
          /*width=*/int, /*height=*/int, avifRGBFormat, avifPixelFormat,
          /*threads=*/int, /*avoidLibYUV=*/bool, avifChromaUpsampling,
          /*has_alpha=*/bool>> {};

TEST_P(YUVToRGBThreadingTest, TestIdentical) {
  const int rgb_depth = std::get<0>(GetParam());
  const int yuv_depth = std::get<1>(GetParam());
  const int width = std::get<2>(GetParam());
  const int height = std::get<3>(GetParam());
  const avifRGBFormat rgb_format = std::get<4>(GetParam());
  const avifPixelFormat yuv_format = std::get<5>(GetParam());
  const int maxThreads = std::get<6>(GetParam());
  const bool avoidLibYUV = std::get<7>(GetParam());
  const avifChromaUpsampling chromaUpsampling = std::get<8>(GetParam());
  const bool has_alpha = std::get<9>(GetParam());

  if (rgb_depth > 8 && rgb_format == AVIF_RGB_FORMAT_RGB_565) {
    return;
  }

  ImagePtr yuv(avifImageCreate(width, height, yuv_depth, yuv_format));
  ASSERT_NE(yuv, nullptr);
  yuv->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT601;
  yuv->yuvRange = AVIF_RANGE_FULL;

  // Fill YUVA planes with random values.
  srand(0xAABBCCDD);
  const int yuv_max = (1 << yuv_depth);
  ASSERT_EQ(avifImageAllocatePlanes(
                yuv.get(), has_alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV),
            AVIF_RESULT_OK);
  for (int plane = AVIF_CHAN_Y; plane <= AVIF_CHAN_A; ++plane) {
    const uint32_t plane_width = avifImagePlaneWidth(yuv.get(), plane);
    if (plane_width == 0) continue;
    const uint32_t plane_height = avifImagePlaneHeight(yuv.get(), plane);
    const uint32_t rowBytes = avifImagePlaneRowBytes(yuv.get(), plane);
    uint8_t* row = avifImagePlane(yuv.get(), plane);
    for (uint32_t y = 0; y < plane_height; ++y, row += rowBytes) {
      for (uint32_t x = 0; x < plane_width; ++x) {
        if (yuv_depth == 8) {
          row[x] = (uint8_t)(rand() % yuv_max);
        } else {
          ((uint16_t*)row)[x] = (uint16_t)(rand() % yuv_max);
        }
      }
    }
  }

  // Convert to RGB with 1 thread.
  testutil::AvifRgbImage rgb(yuv.get(), rgb_depth, rgb_format);
  rgb.avoidLibYUV = avoidLibYUV;
  rgb.chromaUpsampling = chromaUpsampling;
  ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &rgb), AVIF_RESULT_OK);

  // Convert to RGB with multiple threads.
  testutil::AvifRgbImage rgb_threaded(yuv.get(), rgb_depth, rgb_format);
  rgb_threaded.avoidLibYUV = avoidLibYUV;
  rgb_threaded.chromaUpsampling = chromaUpsampling;
  rgb_threaded.maxThreads = maxThreads;
  ASSERT_EQ(avifImageYUVToRGB(yuv.get(), &rgb_threaded), AVIF_RESULT_OK);

  EXPECT_TRUE(testutil::AreImagesEqual(rgb, rgb_threaded));
}

INSTANTIATE_TEST_SUITE_P(
    YUVToRGBThreadingTestInstance, YUVToRGBThreadingTest,
    Combine(/*rgb_depth=*/Values(8, 16),
            /*yuv_depth=*/Values(8, 10),
            /*width=*/Values(1, 2, 127, 200),
            /*height=*/Values(1, 2, 127, 200),
            Values(AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_RGBA),
            Range(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_COUNT),
            // Test an odd and even number for threads. Not adding all possible
            // thread values to keep the number of test instances low.
            /*threads=*/Values(2, 7),
            /*avoidLibYUV=*/Bool(),
            Values(AVIF_CHROMA_UPSAMPLING_FASTEST,
                   AVIF_CHROMA_UPSAMPLING_BILINEAR),
            /*has_alpha=*/Bool()));

// This will generate a large number of test instances and hence it is disabled
// by default. It can be run manually if necessary.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_ExhaustiveYUVToRGBThreadingTestInstance, YUVToRGBThreadingTest,
    Combine(/*rgb_depth=*/Values(8, 10, 12, 16),
            /*yuv_depth=*/Values(8, 10, 12),
            /*width=*/Values(1, 2, 127, 200),
            /*height=*/Values(1, 2, 127, 200),
            Range(AVIF_RGB_FORMAT_RGB, AVIF_RGB_FORMAT_COUNT),
            Range(AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_COUNT),
            /*threads=*/Range(0, 9),
            /*avoidLibYUV=*/Bool(),
            Values(AVIF_CHROMA_UPSAMPLING_AUTOMATIC,
                   AVIF_CHROMA_UPSAMPLING_FASTEST,
                   AVIF_CHROMA_UPSAMPLING_NEAREST,
                   AVIF_CHROMA_UPSAMPLING_BILINEAR),
            /*has_alpha=*/Bool()));

}  // namespace
}  // namespace avif
