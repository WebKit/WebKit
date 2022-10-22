// Copyright 2022 Yuan Tong. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include <map>
#include <string>

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace libavif {
namespace {

void TestEncodeDecode(avifCodecChoice codec,
                      const std::map<std::string, std::string>& init_cs_options,
                      bool can_encode, bool use_cq) {
  if (avifCodecName(codec, AVIF_CODEC_FLAG_CAN_ENCODE) == nullptr) {
    GTEST_SKIP() << "Codec unavailable, skip test.";
  }

  const uint32_t image_size = 512;
  testutil::AvifImagePtr image =
      testutil::CreateImage(image_size, image_size, 8, AVIF_PIXEL_FORMAT_YUV420,
                            AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  // Encode
  testutil::AvifEncoderPtr encoder(avifEncoderCreate(), avifEncoderDestroy);
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = codec;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;

  for (const auto& option : init_cs_options) {
    avifEncoderSetCodecSpecificOption(encoder.get(), option.first.c_str(),
                                      option.second.c_str());
  }

  if (use_cq) {
    encoder->minQuantizer = 0;
    encoder->maxQuantizer = 63;
    avifEncoderSetCodecSpecificOption(encoder.get(), "end-usage", "q");
    avifEncoderSetCodecSpecificOption(encoder.get(), "cq-level", "63");
  } else {
    encoder->minQuantizer = 63;
    encoder->maxQuantizer = 63;
  }

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  if (use_cq) {
    avifEncoderSetCodecSpecificOption(encoder.get(), "cq-level", "0");
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
  testutil::AvifDecoderPtr decoder(avifDecoderCreate(), avifDecoderDestroy);
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

  const uint32_t image_size = 512;
  testutil::AvifImagePtr image =
      testutil::CreateImage(image_size, image_size, 8, AVIF_PIXEL_FORMAT_YUV420,
                            AVIF_PLANES_YUV, AVIF_RANGE_FULL);
  ASSERT_NE(image, nullptr);
  testutil::FillImageGradient(image.get());

  // Encode
  testutil::AvifEncoderPtr encoder(avifEncoderCreate(), avifEncoderDestroy);
  ASSERT_NE(encoder, nullptr);
  encoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
  encoder->speed = AVIF_SPEED_FASTEST;
  encoder->timescale = 1;
  encoder->minQuantizer = 63;
  encoder->maxQuantizer = 63;

  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_OK);

  encoder->timescale = 2;
  ASSERT_EQ(avifEncoderAddImage(encoder.get(), image.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME),
            AVIF_RESULT_CANNOT_CHANGE_SETTING);
}

}  // namespace
}  // namespace libavif
