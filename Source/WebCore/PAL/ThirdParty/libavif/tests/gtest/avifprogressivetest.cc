// Copyright 2022 Yuan Tong. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"
#include "aviftest_helpers.h"
#include "gtest/gtest.h"

namespace avif {
namespace {

class ProgressiveTest : public testing::Test {
 protected:
  static constexpr uint32_t kImageSize = 256;

  void SetUp() override {
    if (avifCodecName(AVIF_CODEC_CHOICE_AOM, AVIF_CODEC_FLAG_CAN_ENCODE) ==
        nullptr) {
      GTEST_SKIP() << "ProgressiveTest requires the AOM encoder.";
    }

    ASSERT_NE(encoder_, nullptr);
    encoder_->codecChoice = AVIF_CODEC_CHOICE_AOM;
    // The fastest speed that uses AOM_USAGE_GOOD_QUALITY.
    encoder_->speed = 6;

    ASSERT_NE(decoder_, nullptr);
    decoder_->allowProgressive = true;

    ASSERT_NE(image_, nullptr);
    testutil::FillImageGradient(image_.get());
  }

  void TestDecode(uint32_t expect_width, uint32_t expect_height) {
    ASSERT_EQ(avifDecoderSetIOMemory(decoder_.get(), encoded_avif_.data,
                                     encoded_avif_.size),
              AVIF_RESULT_OK);
    ASSERT_EQ(avifDecoderParse(decoder_.get()), AVIF_RESULT_OK);
    ASSERT_EQ(decoder_->progressiveState, AVIF_PROGRESSIVE_STATE_ACTIVE);
    ASSERT_EQ(static_cast<uint32_t>(decoder_->imageCount),
              encoder_->extraLayerCount + 1);

    for (uint32_t layer = 0; layer < encoder_->extraLayerCount + 1; ++layer) {
      ASSERT_EQ(avifDecoderNextImage(decoder_.get()), AVIF_RESULT_OK);
      // libavif scales frame automatically.
      ASSERT_EQ(decoder_->image->width, expect_width);
      ASSERT_EQ(decoder_->image->height, expect_height);
      // TODO(wtc): Check avifDecoderNthImageMaxExtent().
    }

    // TODO(wtc): Check decoder_->image and image_ are similar, and better
    // quality layer is more similar.
  }

  EncoderPtr encoder_{avifEncoderCreate()};
  DecoderPtr decoder_{avifDecoderCreate()};

  ImagePtr image_ =
      testutil::CreateImage(kImageSize, kImageSize, 8, AVIF_PIXEL_FORMAT_YUV444,
                            AVIF_PLANES_YUV, AVIF_RANGE_FULL);

  testutil::AvifRwData encoded_avif_;
};

TEST_F(ProgressiveTest, QualityChange) {
  encoder_->extraLayerCount = 1;
  encoder_->minQuantizer = 50;
  encoder_->maxQuantizer = 50;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->minQuantizer = 0;
  encoder_->maxQuantizer = 0;
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(kImageSize, kImageSize);
}

// NOTE: This test requires libaom v3.6.0 or later, otherwise the following
// assertion in libaom fails:
//   av1/encoder/mcomp.c:1717: av1_full_pixel_search: Assertion
//   `ms_params->ms_buffers.ref->stride == ms_params->search_sites->stride'
//   failed.
// See https://aomedia.googlesource.com/aom/+/945edd671.
TEST_F(ProgressiveTest, DimensionChange) {
  if (avifLibYUVVersion() == 0) {
    GTEST_SKIP() << "libyuv not available, skip test.";
  }

  encoder_->extraLayerCount = 1;
  encoder_->minQuantizer = 0;
  encoder_->maxQuantizer = 0;
  encoder_->scalingMode = {{1, 2}, {1, 2}};

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->scalingMode = {{1, 1}, {1, 1}};
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(kImageSize, kImageSize);
}

TEST_F(ProgressiveTest, LayeredGrid) {
  encoder_->extraLayerCount = 1;
  encoder_->minQuantizer = 50;
  encoder_->maxQuantizer = 50;

  avifImage* image_grid[2] = {image_.get(), image_.get()};
  ASSERT_EQ(avifEncoderAddImageGrid(encoder_.get(), 2, 1, image_grid,
                                    AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  encoder_->minQuantizer = 0;
  encoder_->maxQuantizer = 0;
  ASSERT_EQ(avifEncoderAddImageGrid(encoder_.get(), 2, 1, image_grid,
                                    AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(2 * kImageSize, kImageSize);
}

TEST_F(ProgressiveTest, SameLayers) {
  encoder_->extraLayerCount = 3;
  for (uint32_t layer = 0; layer < encoder_->extraLayerCount + 1; ++layer) {
    ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                  AVIF_ADD_IMAGE_FLAG_NONE),
              AVIF_RESULT_OK);
  }
  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_), AVIF_RESULT_OK);

  TestDecode(kImageSize, kImageSize);
}

TEST_F(ProgressiveTest, TooManyLayers) {
  encoder_->extraLayerCount = 1;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);
  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_INVALID_ARGUMENT);
}

TEST_F(ProgressiveTest, TooFewLayers) {
  encoder_->extraLayerCount = 1;

  ASSERT_EQ(avifEncoderAddImage(encoder_.get(), image_.get(), 1,
                                AVIF_ADD_IMAGE_FLAG_NONE),
            AVIF_RESULT_OK);

  ASSERT_EQ(avifEncoderFinish(encoder_.get(), &encoded_avif_),
            AVIF_RESULT_INVALID_ARGUMENT);
}

}  // namespace
}  // namespace avif
