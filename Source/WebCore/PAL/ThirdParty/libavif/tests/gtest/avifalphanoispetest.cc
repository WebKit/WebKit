// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

// Used to pass the data folder path to the GoogleTest suites.
const char* data_path = nullptr;

TEST(AvifDecodeTest, AlphaNoIspe) {
  if (!testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 Codec unavailable, skip test.";
  }
  // See https://github.com/AOMediaCodec/libavif/pull/745.
  const char* file_name = "alpha_noispe.avif";
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderSetIOFile(decoder.get(),
                                 (std::string(data_path) + file_name).c_str()),
            AVIF_RESULT_OK);
  // By default, loose files are refused. Cast to avoid C4389 Windows warning.
  EXPECT_EQ(decoder->strictFlags, (avifStrictFlags)AVIF_STRICT_ENABLED);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_BMFF_PARSE_FAILED);
  // Allow this kind of file specifically.
  decoder->strictFlags = (avifStrictFlags)AVIF_STRICT_ENABLED &
                         ~(avifStrictFlags)AVIF_STRICT_ALPHA_ISPE_REQUIRED;
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  EXPECT_EQ(decoder->alphaPresent, AVIF_TRUE);
  EXPECT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  EXPECT_NE(decoder->image->alphaPlane, nullptr);
  EXPECT_GT(decoder->image->alphaRowBytes, 0u);
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
