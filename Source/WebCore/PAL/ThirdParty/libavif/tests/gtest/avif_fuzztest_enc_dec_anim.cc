// Copyright 2023 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include <cstdint>
#include <vector>

#include "avif/avif.h"
#include "avif_fuzztest_helpers.h"
#include "aviftest_helpers.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

namespace avif {
namespace testutil {
namespace {

::testing::Environment* const kStackLimitEnv = SetStackLimitTo512x1024Bytes();

struct FrameOptions {
  uint64_t duration;
  avifAddImageFlags flags;
};

// Encodes an animation and decodes it.
// For simplicity, there is only one source image, all frames are identical.
void EncodeDecodeAnimation(ImagePtr frame,
                           const std::vector<FrameOptions>& frame_options,
                           EncoderPtr encoder, DecoderPtr decoder) {
  ASSERT_NE(encoder, nullptr);
  ASSERT_NE(decoder, nullptr);
  ImagePtr decoded_image(avifImageCreateEmpty());
  ASSERT_NE(decoded_image, nullptr);

  const int num_frames = static_cast<int>(frame_options.size());
  int total_duration = 0;

  // Encode.
  for (const FrameOptions& options : frame_options) {
    total_duration += options.duration;
    const avifResult result = avifEncoderAddImage(
        encoder.get(), frame.get(), options.duration, options.flags);
    ASSERT_EQ(result, AVIF_RESULT_OK)
        << avifResultToString(result) << " " << encoder->diag.error;
  }
  AvifRwData encoded_data;
  avifResult result = avifEncoderFinish(encoder.get(), &encoded_data);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << encoder->diag.error;

  // Decode.
  result = avifDecoderSetIOMemory(decoder.get(), encoded_data.data,
                                  encoded_data.size);
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  result = avifDecoderParse(decoder.get());
  ASSERT_EQ(result, AVIF_RESULT_OK)
      << avifResultToString(result) << " " << decoder->diag.error;

  if (decoder->requestedSource == AVIF_DECODER_SOURCE_PRIMARY_ITEM ||
      num_frames == 1) {
    ASSERT_EQ(decoder->imageCount, 1);
  } else {
    ASSERT_EQ(decoder->imageCount, num_frames);
    EXPECT_EQ(decoder->durationInTimescales, total_duration);

    for (int i = 0; i < num_frames; ++i) {
      result = avifDecoderNextImage(decoder.get());
      ASSERT_EQ(result, AVIF_RESULT_OK)
          << " frame " << i << " " << avifResultToString(result) << " "
          << decoder->diag.error;
      EXPECT_EQ(decoder->image->width, frame->width);
      EXPECT_EQ(decoder->image->height, frame->height);
      EXPECT_EQ(decoder->image->depth, frame->depth);
      EXPECT_EQ(decoder->image->yuvFormat, frame->yuvFormat);
    }
    result = avifDecoderNextImage(decoder.get());
    ASSERT_EQ(result, AVIF_RESULT_NO_IMAGES_REMAINING)
        << avifResultToString(result) << " " << decoder->diag.error;
  }
}

constexpr int kMaxFrames = 10;

FUZZ_TEST(EncodeDecodeAvifFuzzTest, EncodeDecodeAnimation)
    .WithDomains(
        ArbitraryAvifImage(),
        fuzztest::VectorOf(
            fuzztest::StructOf<FrameOptions>(
                /*duration=*/fuzztest::InRange<uint64_t>(1, 10),
                // BitFlagCombinationOf() requires non zero flags as input.
                // AVIF_ADD_IMAGE_FLAG_NONE (0) is implicitly part of the set.
                fuzztest::BitFlagCombinationOf<avifAddImageFlags>(
                    {AVIF_ADD_IMAGE_FLAG_FORCE_KEYFRAME})))
            .WithMinSize(1)
            .WithMaxSize(kMaxFrames),
        ArbitraryAvifEncoder(), ArbitraryAvifDecoder());

}  // namespace
}  // namespace testutil
}  // namespace avif
