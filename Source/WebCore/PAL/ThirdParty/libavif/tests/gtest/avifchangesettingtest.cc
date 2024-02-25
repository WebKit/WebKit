// Copyright 2022 Yuan Tong. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <map>
#include <string>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

void TestEncodeDecode(avifCodecChoice codec,
                      const std::map<std::string, std::string>& init_cs_options,
                      bool can_encode, bool use_cq) {
  if (avifCodecName(codec, AVIF_CODEC_FLAG_CAN_ENCODE) == nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  constexpr uint32_t kImageSize = 512;
  ImagePtr image = testutil::CreateImage(kImageSize, kImageSize, /*depth=*/8,
                                         AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  // Encode
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = codec;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;

  for (const auto& option : init_cs_options) {
    ASSERT_EQ(avifEncoderSetCodecSpecificOption(
                  encoder.get(), option.first.c_str(), option.second.c_str()),
              AVIF_RESULT_OK);
  }

  if (use_cq) {
    encoder->minQuantizer = 0;
    encoder->maxQuantizer = 63;
    ASSERT_EQ(
        avifEncoderSetCodecSpecificOption(encoder.get(), "end-usage", "q"),
        AVIF_RESULT_OK);
    ASSERT_EQ(
        avifEncoderSetCodecSpecificOption(encoder.get(), "cq-level", "63"),
        AVIF_RESULT_OK);
  } else {
    encoder->minQuantizer = 63;
    encoder->maxQuantizer = 63;
  }

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  if (use_cq) {
    ASSERT_EQ(avifEncoderSetCodecSpecificOption(encoder.get(), "cq-level", "0"),
              AVIF_RESULT_OK);
  } else {
    encoder->minQuantizer = 0;
    encoder->maxQuantizer = 0;
  }

  if (!can_encode) {
    ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                  AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
              AVIF_RESULT_NOT_IMPLEMENTED);

    return;
  }

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  testutil::AvifRwData encodedAvif;
  ASSERT_EQ(avifEncoderFinish(encoder.get(), &encodedAvif), AVIF_RESULT_OK);

  // Decode
  DecoderPtr decoder(avifDecoderCreate());
  ASSERT_NE(decoder, nullptr);

  // The second frame is set to have far better quality,
  // and should be much bigger, so small amount of data at beginning
  // should be enough to decode the first frame.
  avifIO* io = testutil::AvifIOCreateLimitedReader(
      avifIOCreateMemoryReader(encodedAvif.data, encodedAvif.size),
      encodedAvif.size / 10);
  ASSERT_NE(io, nullptr);
  avifDecoderSetIO(decoder.get(), io);
  ASSERT_EQ(avifDecoderParse(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_WAITING_ON_IO);
  reinterpret_cast<testutil::AvifIOLimitedReader*>(io)->clamp =
      testutil::AvifIOLimitedReader::kNoClamp;
  ASSERT_EQ(avifDecoderNextImage(decoder.get()), AVIF_RESULT_OK);
  ASSERT_EQ(avifDecoderNextImage(decoder.get()),
            AVIF_RESULT_NO_IMAGES_REMAINING);
}

TEST(ChangeSettingTest, AOM) {
  // Test if changes to AV1 encode settings are detected.
  TestEncodeDecode(AVIF_CODEC_CHOICE_AOM, {{"end-usage", "cbr"}}, true, false);

  // Test if changes to codec specific options are detected.
  TestEncodeDecode(AVIF_CODEC_CHOICE_AOM, {}, true, true);
}

TEST(ChangeSettingTest, RAV1E) {
  TestEncodeDecode(AVIF_CODEC_CHOICE_RAV1E, {}, false, false);
}

TEST(ChangeSettingTest, SVT) {
  TestEncodeDecode(AVIF_CODEC_CHOICE_SVT, {}, false, false);
}

TEST(ChangeSettingTest, UnchangeableSetting) {
  if (avifCodecName(AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_FLAG_CAN_ENCODE) ==
      nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  constexpr uint32_t kImageSize = 512;
  ImagePtr image = testutil::CreateImage(kImageSize, kImageSize, /*depth=*/8,
                                         AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  // Encode
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;
  ASSERT_EQ(encoder->repetitionCount, AVIF_REPETITION_COUNT_INFINITE);
  encoder->minQuantizer = 63;
  encoder->maxQuantizer = 63;

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  encoder->timescale = 2;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_CANNOT_CHANGE_SETTING);

  encoder->timescale = 1;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  encoder->repetitionCount = 0;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_CANNOT_CHANGE_SETTING);
}

TEST(ChangeSettingTest, UnchangeableImageColorRange) {
  if (avifCodecName(AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_FLAG_CAN_ENCODE) ==
      nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  constexpr uint32_t kImageSize = 512;
  ImagePtr image = testutil::CreateImage(kImageSize, kImageSize, /*depth=*/8,
                                         AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  const uint32_t yuva[] = {128, 128, 128, 255};
  testutil::FillImagePlain(image.get(), yuva);

  // Encode
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;
  ASSERT_EQ(encoder->repetitionCount, AVIF_REPETITION_COUNT_INFINITE);
  encoder->quality = AVIF_QUALITY_WORST;

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  image->yuvRange = AVIF_RANGE_LIMITED;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_INCOMPATIBLE_IMAGE);
}

TEST(ChangeSettingTest, UnchangeableImageChromaSamplePosition) {
  if (avifCodecName(AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_FLAG_CAN_ENCODE) ==
      nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  constexpr uint32_t kImageSize = 512;
  ImagePtr image = testutil::CreateImage(kImageSize, kImageSize, /*depth=*/8,
                                         AVIF_PIXEL_FORMAT_YUV420,
                                         AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  const uint32_t yuva[] = {128, 128, 128, 255};
  testutil::FillImagePlain(image.get(), yuva);

  // Encode
  EncoderPtr encoder(avifEncoderCreate());
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;
  ASSERT_EQ(encoder->repetitionCount, AVIF_REPETITION_COUNT_INFINITE);
  encoder->quality = AVIF_QUALITY_WORST;

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(image->yuvChromaSamplePosition,
            AVIF_CHROMA_SAMPLE_POSITION_UNKNOWN);
  image->yuvChromaSamplePosition = AVIF_CHROMA_SAMPLE_POSITION_VERTICAL;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_INCOMPATIBLE_IMAGE);
}

}  // namespace
}  // namespace avif
