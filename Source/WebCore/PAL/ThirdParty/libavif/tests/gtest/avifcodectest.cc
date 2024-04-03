// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

class CodecTest : public testing::TestWithParam<
                      std::tuple</*encoding_codec=*/avifCodecChoice,
                                 /*decoding_codec=*/avifCodecChoice>> {};

TEST_P(CodecTest, EncodeDecode) {
  const avifCodecChoice encoding_codec = std::get<0>(GetParam());
  const avifCodecChoice decoding_codec = std::get<1>(GetParam());

  if (avifCodecName(encoding_codec, AVIF_CODEC_FLAG_CAN_ENCODE) == nullptr ||
      avifCodecName(decoding_codec, AVIF_CODEC_FLAG_CAN_DECODE) == nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  // AVIF_CODEC_CHOICE_SVT requires dimensions to be at least 64 pixels.
  ImagePtr image =
      testutil::CreateImage(/*width=*/64, /*height=*/64, /*depth=*/8,
                            AVIF_PIXEL_FORMAT_YUV420, AVIF_PLANES_ALL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = encoding_codec;
  encoder->quality = encoder->qualityAlpha = 90;  // Small loss.
  testutil::AvifRwData encoded;
  ASSERT_EQ(avifEncoderWrite(encoder.get(), image.get(), &encoded),
            AVIF_RESULT_OK);

  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);
  decoder->codecChoice = decoding_codec;
  ImagePtr decoded(avifImageCreateEmpty());
  ASSERT_NE(decoded, nullptr);
  ASSERT_EQ(avifDecoderReadMemory(decoder.get(), decoded.get(), encoded.data,
                                  encoded.size),
            AVIF_RESULT_OK);

  ASSERT_GT(testutil::GetPsnr(*image, *decoded), 32.0);
}

INSTANTIATE_TEST_SUITE_P(
    All, CodecTest,
    testing::Combine(/*encoding_codec=*/testing::Values(AVIF_CODEC_CHOICE_AOM,
                                                        AVIF_CODEC_CHOICE_RAV1E,
                                                        AVIF_CODEC_CHOICE_SVT),
                     /*decoding_codec=*/testing::Values(
                         AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_CHOICE_DAV1D,
                         AVIF_CODEC_CHOICE_LIBGAV1)));

}  // namespace
}  // namespace avif
