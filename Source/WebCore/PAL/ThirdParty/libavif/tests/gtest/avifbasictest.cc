// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

TEST(BasicTest, EncodeDecode) {
  ImagePtr image =
      testutil::CreateImage(/*width=*/12, /*height=*/34, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  avifResult result = avifEncoderWrite(encoder.get(), image.get(), &encoded);
  ASSERT_EQ(result, AVIF_RESULT_OK) << avifResultToString(result);

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  result = avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                 encoded.size);
  ASSERT_EQ(result, AVIF_RESULT_OK) << avifResultToString(result);
  EXPECT_EQ(decoder->imageSequenceTrackPresent, AVIF_FALSE);

  // Verify that the input and decoded images are close.
  ASSERT_GT(testutil::GetPsnr(*image, *decoded), 40.0);

  // Uncomment the following to save the encoded image as an AVIF file.
  //  std::ofstream("/tmp/avifbasictest.avif", std::ios::binary)
  //      .write(reinterpret_cast<char*>(encoded.data), encoded.size);

  // Uncomment the following to save the decoded image as a PNG file.
  //  testutil::WriteImage(decoded.get(), "/tmp/avifbasictest.png");
}

}  // namespace
}  // namespace avif
