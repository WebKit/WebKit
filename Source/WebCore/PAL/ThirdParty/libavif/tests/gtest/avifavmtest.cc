// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

using testing::Combine;
using testing::Values;

namespace avif {
namespace {

class AvmTest : public testing::TestWithParam<
                    std::tuple</*width=*/int, /*height=*/int, /*depth=*/int,
                               avifPixelFormat, /*alpha=*/bool>> {};

TEST_P(AvmTest, EncodeDecode) {
  const int width = std::get<0>(GetParam());
  const int height = std::get<1>(GetParam());
  const int depth = std::get<2>(GetParam());
  const avifPixelFormat format = std::get<3>(GetParam());
  const bool alpha = std::get<4>(GetParam());

  ImagePtr image = testutil::CreateImage(
      width, height, depth, format, alpha ? AVIF_PLANES_ALL : AVIF_PLANES_YUV);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = AVIF_CODEC_CHOICE_AVM;
  testutil::AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  // No need to set AVIF_CODEC_CHOICE_AVM. The decoder should recognize AV2.
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_OK);

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);

  // Forcing an AV1 decoding codec should fail.
  for (avifCodecChoice av1_codec :
       {AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_CHOICE_DAV1D,
        AVIF_CODEC_CHOICE_LIBGAV1}) {
    decoder->codecChoice = av1_codec;
    // An error is expected because av1_codec is not enabled or because we are
    // trying to decode an AV2 file with an AV1 codec.
    ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                    encoded.size),
              avifCodecName(av1_codec, AVIF_CODEC_FLAG_CAN_DECODE)
                  ? AVIF_RESULT_DECODE_COLOR_FAILED
                  : AVIF_RESULT_NO_CODEC_AVAILABLE);
  }
}

INSTANTIATE_TEST_SUITE_P(Basic, AvmTest,
                         Combine(/*width=*/Values(12), /*height=*/Values(34),
                                 /*depth=*/Values(8),
                                 Values(AVIF_PIXEL_FORMAT_YUV420,
                                        AVIF_PIXEL_FORMAT_YUV444),
                                 /*alpha=*/Values(true)));

INSTANTIATE_TEST_SUITE_P(Tiny, AvmTest,
                         Combine(/*width=*/Values(1), /*height=*/Values(1),
                                 /*depth=*/Values(8),
                                 Values(AVIF_PIXEL_FORMAT_YUV444),
                                 /*alpha=*/Values(false)));

// TODO(yguyon): Implement or fix in avm then test the following combinations.
INSTANTIATE_TEST_SUITE_P(DISABLED_Broken, AvmTest,
                         Combine(/*width=*/Values(1), /*height=*/Values(34),
                                 /*depth=*/Values(8, 10, 12),
                                 Values(AVIF_PIXEL_FORMAT_YUV400,
                                        AVIF_PIXEL_FORMAT_YUV420,
                                        AVIF_PIXEL_FORMAT_YUV444),
                                 /*alpha=*/Values(true)));

TEST(AvmTest, Av1StillWorksWhenAvmIsEnabled) {
  if (!testutil::Av1EncoderAvailable() || !testutil::Av1DecoderAvailable()) {
    GTEST_SKIP() << "AV1 codec unavailable, skip test.";
  }
  // avm is the only AV2 codec, so the default codec will be an AV1 one.

  ImagePtr image =
      testutil::CreateImage(/*width=*/64, /*height=*/64, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  testutil::AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_OK);

  // Verify that the input and decoded images are close.
  EXPECT_GT(testutil::GetPsnr(*image, *decoded), 40.0);

  // Forcing an AV2 decoding codec should fail.
  decoder->codecChoice = AVIF_CODEC_CHOICE_AVM;
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_DECODE_COLOR_FAILED);
}

}  // namespace
}  // namespace avif
