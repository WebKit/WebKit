// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cmath>
#include <fstream>
#include <iostream>

#include "avif/internal.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

void ExpectMatrixNear(const double actual[3][3],
                      const std::array<std::array<double, 3>, 3>& expected,
                      const double epsilon) {
  EXPECT_NEAR(actual[0][0], expected[0][0], epsilon);
  EXPECT_NEAR(actual[0][1], expected[0][1], epsilon);
  EXPECT_NEAR(actual[0][2], expected[0][2], epsilon);
  EXPECT_NEAR(actual[1][0], expected[1][0], epsilon);
  EXPECT_NEAR(actual[1][1], expected[1][1], epsilon);
  EXPECT_NEAR(actual[1][2], expected[1][2], epsilon);
  EXPECT_NEAR(actual[2][0], expected[2][0], epsilon);
  EXPECT_NEAR(actual[2][1], expected[2][1], epsilon);
  EXPECT_NEAR(actual[2][2], expected[2][2], epsilon);
}

TEST(RgbToXyzD50Matrix, GoldenValues) {
  double coeffs[3][3];
  ASSERT_TRUE(avifColorPrimariesComputeRGBToXYZD50Matrix(
      AVIF_COLOR_PRIMARIES_BT709, coeffs));
  // Golden values from
  // http://brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
  const double kEpsilon = 0.00015;
  ExpectMatrixNear(coeffs,
                   {{{0.4360747, 0.3850649, 0.1430804},
                     {0.2225045, 0.7168786, 0.0606169},
                     {0.0139322, 0.0971045, 0.7141733}}},
                   kEpsilon);
}

TEST(XyzD50ToRgbMatrix, GoldenValues) {
  double coeffs[3][3];
  ASSERT_TRUE(avifColorPrimariesComputeXYZD50ToRGBMatrix(
      AVIF_COLOR_PRIMARIES_BT709, coeffs));
  // Golden values from
  // http://brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
  // Higher tolerance than for the RGB->XYZ matrix because of compounding
  // computation error (we compute the RGB->XYZ matrix then invert it which adds
  // more error).
  const double kEpsilon = 0.0005;
  ExpectMatrixNear(coeffs,
                   {{{3.1338561, -1.6168667, -0.4906146},
                     {-0.9787684, 1.9161415, 0.0334540},
                     {0.0719453, -0.2289914, 1.4052427}}},
                   kEpsilon);
}

TEST(RgbToRgbConversion, Identity) {
  const double kEpsilon = 0.000001;
  for (int primaries = AVIF_COLOR_PRIMARIES_UNKNOWN;
       primaries <= AVIF_COLOR_PRIMARIES_SMPTE432; ++primaries) {
    double coeffs[3][3];
    ASSERT_TRUE(avifColorPrimariesComputeRGBToRGBMatrix(
        (avifColorPrimaries)primaries, (avifColorPrimaries)primaries, coeffs));
    ExpectMatrixNear(coeffs,
                     {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}},
                     kEpsilon);
  }
}

TEST(ColorPrimariesComputeRGBToRGBMatrix, GoldenValues) {
  // Golden values from http://color.support/colorspacecalculator.html
  double coeffs[3][3];
  ASSERT_TRUE(avifColorPrimariesComputeRGBToRGBMatrix(
      AVIF_COLOR_PRIMARIES_BT709, AVIF_COLOR_PRIMARIES_BT2020, coeffs));
  const double kEpsilon = 0.0001;
  ExpectMatrixNear(coeffs,
                   {{{0.627404, 0.329283, 0.043313},
                     {0.069097, 0.919540, 0.011362},
                     {0.016391, 0.088013, 0.895595}}},
                   kEpsilon);

  ASSERT_TRUE(avifColorPrimariesComputeRGBToRGBMatrix(
      AVIF_COLOR_PRIMARIES_BT2020, AVIF_COLOR_PRIMARIES_BT709, coeffs));
  ExpectMatrixNear(coeffs,
                   {{{1.660491, -0.587641, -0.072850},
                     {-0.124550, 1.132900, -0.008349},
                     {-0.018151, -0.100579, 1.118730}}},
                   kEpsilon);

  ASSERT_TRUE(avifColorPrimariesComputeRGBToRGBMatrix(
      AVIF_COLOR_PRIMARIES_BT709, AVIF_COLOR_PRIMARIES_XYZ, coeffs));
  ExpectMatrixNear(coeffs,
                   {{{0.438449, 0.392176, 0.169375},
                     {0.222828, 0.708691, 0.068481},
                     {0.017314, 0.110445, 0.872241}}},
                   kEpsilon);
}

using ConvertImageColorspaceTest = ::testing::TestWithParam<
    std::tuple</*src_image_name=*/std::string,
               /*reference_image_name=*/std::string, /*min_psnr=*/float>>;

INSTANTIATE_TEST_SUITE_P(
    Parameterized, ConvertImageColorspaceTest,
    ::testing::Values(
        std::make_tuple(/*src_image_name=*/"colors_hdr_rec2020.avif",
                        /*reference_image_name=*/"colors_hdr_srgb.avif",
                        /*min_psnr=*/44.0f),
        std::make_tuple(/*src_image_name=*/"colors_hdr_srgb.avif",
                        /*reference_image_name=*/"colors_hdr_rec2020.avif",
                        /*min_psnr=*/44.0f),
        std::make_tuple(/*src_image_name=*/"colors_hdr_rec2020.avif",
                        /*reference_image_name=*/"colors_hdr_p3.avif",
                        /*min_psnr=*/60.0f),
        std::make_tuple(/*src_image_name=*/"colors_hdr_p3.avif",
                        /*reference_image_name=*/"colors_hdr_rec2020.avif",
                        /*min_psnr=*/44.0f),
        std::make_tuple(/*src_image_name=*/"colors_hdr_p3.avif",
                        /*reference_image_name=*/"colors_hdr_srgb.avif",
                        /*min_psnr=*/44.0f),
        std::make_tuple(/*src_image_name=*/"colors_hdr_srgb.avif",
                        /*reference_image_name=*/"colors_hdr_p3.avif",
                        /*min_psnr=*/60.0f)));

TEST_P(ConvertImageColorspaceTest, ConvertImage) {
  const std::string src_image_name = std::get<0>(GetParam());
  const std::string reference_image_name = std::get<1>(GetParam());
  const float min_psnr = std::get<2>(GetParam());

  const ImagePtr src_image =
      testutil::DecodeFile(std::string(data_path) + src_image_name);
  ASSERT_NE(src_image, nullptr)
      << "Failed to read " << std::string(data_path) + src_image_name;
  testutil::AvifRgbImage src_image_rgb(src_image.get(), src_image->depth,
                                       AVIF_RGB_FORMAT_RGB);
  ASSERT_EQ(avifImageYUVToRGB(src_image.get(), &src_image_rgb), AVIF_RESULT_OK);

  const ImagePtr reference_image =
      testutil::DecodeFile(std::string(data_path) + reference_image_name);
  ASSERT_NE(reference_image, nullptr);
  testutil::AvifRgbImage reference_image_rgb(
      reference_image.get(), reference_image->depth, AVIF_RGB_FORMAT_RGB);
  ASSERT_EQ(avifImageYUVToRGB(reference_image.get(), &reference_image_rgb),
            AVIF_RESULT_OK);

  ASSERT_EQ(reference_image_rgb.width, src_image_rgb.width);
  ASSERT_EQ(reference_image_rgb.height, src_image_rgb.height);

  avifRGBColorSpaceInfo src_info;
  ASSERT_TRUE(avifGetRGBColorSpaceInfo(&src_image_rgb, &src_info));

  testutil::AvifRgbImage src_image_converted_rgb(
      reference_image.get(), reference_image_rgb.depth, AVIF_RGB_FORMAT_RGB);
  avifRGBColorSpaceInfo dst_info;
  ASSERT_TRUE(avifGetRGBColorSpaceInfo(&src_image_converted_rgb, &dst_info));

  const avifTransferFunction gammaToLinear =
      avifTransferCharacteristicsGetGammaToLinearFunction(
          src_image->transferCharacteristics);
  const avifTransferFunction linearToGamma =
      avifTransferCharacteristicsGetLinearToGammaFunction(
          reference_image->transferCharacteristics);

  double coeffs[3][3];
  ASSERT_TRUE(avifColorPrimariesComputeRGBToRGBMatrix(
      src_image->colorPrimaries, reference_image->colorPrimaries, coeffs));

  for (uint32_t j = 0; j < src_image_rgb.height; ++j) {
    for (uint32_t i = 0; i < src_image_rgb.width; ++i) {
      float rgba[4];
      avifGetRGBAPixel(&src_image_rgb, i, j, &src_info, rgba);
      for (int c = 0; c < 3; ++c) rgba[c] = gammaToLinear(rgba[c]);
      avifLinearRGBConvertColorSpace(rgba, coeffs);
      for (int c = 0; c < 3; ++c) rgba[c] = linearToGamma(rgba[c]);
      avifSetRGBAPixel(&src_image_converted_rgb, i, j, &dst_info, rgba);
    }
  }

  ImagePtr src_image_converted(
      avifImageCreate(reference_image->width, reference_image->height,
                      reference_image->depth, reference_image->yuvFormat));
  ASSERT_EQ(
      avifImageRGBToYUV(src_image_converted.get(), &src_image_converted_rgb),
      AVIF_RESULT_OK);
  src_image_converted->colorPrimaries = AVIF_COLOR_PRIMARIES_BT709;
  src_image_converted->transferCharacteristics =
      src_image->transferCharacteristics;
  src_image_converted->clli = src_image->clli;

  const double psnr = testutil::GetPsnr(*reference_image, *src_image_converted);
  std::cout << "PSNR: " << psnr << "\n";
  EXPECT_GT(psnr, min_psnr);

  // Uncomment the following to save the encoded image as an AVIF file.
  // const auto encoded =
  //     testutil::Encode(src_image_converted.get(), 9, 100);
  // std::ofstream("/tmp/colrtest.avif", std::ios::binary)
  //     .write(reinterpret_cast<char*>(encoded.data), encoded.size);
}

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
