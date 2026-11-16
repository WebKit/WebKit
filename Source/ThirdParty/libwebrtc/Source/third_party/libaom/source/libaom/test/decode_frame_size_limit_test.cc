/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <ostream>

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/ivf_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

struct DecodeParam {
  const char *filename;
  unsigned int width;
  unsigned int height;
};

std::ostream &operator<<(std::ostream &os, const DecodeParam &dp) {
  return os << "file: " << dp.filename;
}

class DecodeFrameSizeLimitTest
    : public ::libaom_test::DecoderTest,
      public ::libaom_test::CodecTestWithParam<DecodeParam> {
 protected:
  DecodeFrameSizeLimitTest()
      : DecoderTest(GET_PARAM(0)), width_(GET_PARAM(1).width),
        height_(GET_PARAM(1).height) {}

  ~DecodeFrameSizeLimitTest() override = default;

  void PreDecodeFrameHook(const libaom_test::CompressedVideoSource &video,
                          libaom_test::Decoder *decoder) override {
    if (video.frame_number() == 0)
      decoder->Control(AOMD_SET_FRAME_SIZE_LIMIT, frame_size_limit_);
  }

  void DecompressedFrameHook(const aom_image_t &img,
                             const unsigned int /*frame_number*/) override {
    EXPECT_EQ(img.d_w, width_);
    EXPECT_EQ(img.d_h, height_);
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          const libaom_test::CompressedVideoSource & /*video*/,
                          libaom_test::Decoder * /*decoder*/) override {
    bool expect_failure =
        frame_size_limit_ && width_ * height_ > frame_size_limit_;
    if (expect_failure) {
      EXPECT_EQ(res_dec, AOM_CODEC_CORRUPT_FRAME);
    } else {
      EXPECT_EQ(res_dec, AOM_CODEC_OK);
    }

    return !HasFailure();
  }

  void RunTest() {
    const DecodeParam input = GET_PARAM(1);
    aom_codec_dec_cfg_t cfg = { 1, 0, 0, !FORCE_HIGHBITDEPTH_DECODING };
    libaom_test::IVFVideoSource decode_video(input.filename);
    decode_video.Init();

    ASSERT_NO_FATAL_FAILURE(RunLoop(&decode_video, cfg));
  }

 protected:
  unsigned int frame_size_limit_ = 0;

 private:
  unsigned int width_;
  unsigned int height_;
};

TEST_P(DecodeFrameSizeLimitTest, Unlimited) { RunTest(); }

TEST_P(DecodeFrameSizeLimitTest, LimitedBig) {
  frame_size_limit_ = 226 * 210;
  RunTest();
}

TEST_P(DecodeFrameSizeLimitTest, LimitedSmall) {
  frame_size_limit_ = 226 * 210 - 1;
  RunTest();
}

const DecodeParam kAV1DecodeFrameSizeLimitTests[] = {
  // { filename, width, height }
  { "av1-1-b8-01-size-16x16.ivf", 16, 16 },
  { "av1-1-b8-01-size-226x210.ivf", 226, 210 },
};

AV1_INSTANTIATE_TEST_SUITE(DecodeFrameSizeLimitTest,
                           ::testing::ValuesIn(kAV1DecodeFrameSizeLimitTests));

}  // namespace
