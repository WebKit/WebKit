// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "avifutil.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

// Tests encode/decode round trips under different matrix coefficients.
TEST(BasicTest, EncodeDecodeMatrixCoefficients) {
  for (const auto& file_name :
       {"paris_icc_exif_xmp.png", "paris_exif_xmp_icc.jpg"}) {
    // Read a ground truth image with default matrix coefficients.
    const std::string file_path = std::string(data_path) + file_name;
    const ImagePtr ground_truth_image =
        testutil::ReadImage(data_path, file_name);

    for (auto matrix_coefficient : {
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
           AVIF_MATRIX_COEFFICIENTS_YCGCO_RE, AVIF_MATRIX_COEFFICIENTS_YCGCO_RO,
#endif
               AVIF_MATRIX_COEFFICIENTS_IDENTITY, AVIF_MATRIX_COEFFICIENTS_YCGCO
         }) {
      // Read a ground truth image but ask for certain matrix coefficients.
      ImagePtr image(avifImageCreateEmpty());
      ASSERT_NE(image, nullptr);
      image->matrixCoefficients = (avifMatrixCoefficients)matrix_coefficient;
      const avifAppFileFormat file_format = avifReadImage(
          file_path.c_str(),
          /*requestedFormat=*/AVIF_PIXEL_FORMAT_NONE,
          /*requestedDepth=*/0,
          /*chromaDownsampling=*/AVIF_CHROMA_DOWNSAMPLING_AUTOMATIC,
          /*ignoreColorProfile=*/false, /*ignoreExif=*/false,
          /*ignoreXMP=*/false, /*allowChangingCicp=*/true,
          /*ignoreGainMap=*/true, AVIF_DEFAULT_IMAGE_SIZE_LIMIT, image.get(),
          /*outDepth=*/nullptr, /*sourceTiming=*/nullptr,
          /*frameIter=*/nullptr);
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
      if (matrix_coefficient == AVIF_MATRIX_COEFFICIENTS_YCGCO_RO) {
        // AVIF_MATRIX_COEFFICIENTS_YCGCO_RO does not work because the input
        // depth is not odd.
        ASSERT_EQ(file_format, AVIF_APP_FILE_FORMAT_UNKNOWN);
        continue;
      }
#endif
      ASSERT_NE(file_format, AVIF_APP_FILE_FORMAT_UNKNOWN);

      // Encode.
      EncoderPtr encoder(avifEncoderCreate());
      ASSERT_NE(encoder, nullptr);
      encoder->speed = AVIF_SPEED_FASTEST;
      encoder->quality = AVIF_QUALITY_LOSSLESS;
      encoder->qualityAlpha = AVIF_QUALITY_LOSSLESS;
      testutil::AvifRwData encoded;
      avifResult result =
          avifEncoderWrite(encoder.get(), image.get(), &encoded);
      ASSERT_EQ(result, AVIF_RESULT_OK) << avifResultToString(result);

      // Decode to RAM.
      ImagePtr decoded(avifImageCreateEmpty());
      ASSERT_NE(decoded, nullptr);
      DecoderPtr decoder(avifDecoderCreate());
      ASSERT_NE(decoder, nullptr);
      result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                     encoded.size);
      ASSERT_EQ(result, AVIF_RESULT_OK) << avifResultToString(result);

      // Convert to a temporary PNG and read back, to have default matrix
      // coefficients.
      const std::string temp_dir = testing::TempDir();
      const std::string temp_file = "decoded_default.png";
      const std::string tmp_path = temp_dir + temp_file;
      ASSERT_TRUE(testutil::WriteImage(decoded.get(), tmp_path.c_str()));
      const ImagePtr decoded_default =
          testutil::ReadImage(temp_dir.c_str(), temp_file.c_str());

      // Verify that the ground truth and decoded images are the same.
      const bool are_images_equal =
          testutil::AreImagesEqual(*ground_truth_image, *decoded_default);

      if (matrix_coefficient == AVIF_MATRIX_COEFFICIENTS_IDENTITY
#if defined(AVIF_ENABLE_EXPERIMENTAL_YCGCO_R)
          || matrix_coefficient == AVIF_MATRIX_COEFFICIENTS_YCGCO_RE
#endif
      ) {
        ASSERT_TRUE(are_images_equal);
      } else {
        // AVIF_MATRIX_COEFFICIENTS_YCGCO is not lossless because it does not
        // expand the input bit range for YCgCo values.
        ASSERT_FALSE(are_images_equal);
      }
    }
  }
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
