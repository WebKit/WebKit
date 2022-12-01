/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

// This class is used to test the presence of still picture feature.
class StillPicturePresenceTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  StillPicturePresenceTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        enable_full_header_(GET_PARAM(2)) {
    still_picture_coding_violated_ = false;
  }
  virtual ~StillPicturePresenceTest() {}

  virtual void SetUp() {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.g_threads = 1;
    cfg_.full_still_picture_hdr = enable_full_header_;
    cfg_.g_limit = 1;
  }

  virtual bool DoDecode() const { return 1; }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AV1E_SET_FORCE_VIDEO_MODE, 0);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_STILL_PICTURE,
                                    &still_pic_info_);
      if (still_pic_info_.is_still_picture != 1) {
        still_picture_coding_violated_ = true;
      }
      if (still_pic_info_.is_reduced_still_picture_hdr == enable_full_header_) {
        /* If full_still_picture_header is enabled in encoder config but
         * bitstream contains reduced_still_picture_header set, then set
         * still_picture_coding_violated_ to true.
         * Similarly, if full_still_picture_header is disabled in encoder config
         * but bitstream contains reduced_still_picture_header not set, then set
         * still_picture_coding_violated_ to true.
         */
        still_picture_coding_violated_ = true;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  bool still_picture_coding_violated_;
  int enable_full_header_;
  aom_still_picture_info still_pic_info_;
  aom_rc_mode end_usage_check_;
};

TEST_P(StillPicturePresenceTest, StillPictureEncodePresenceTest) {
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 1);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(still_picture_coding_violated_, false);
}

AV1_INSTANTIATE_TEST_SUITE(StillPicturePresenceTest,
                           ::testing::Values(::libaom_test::kOnePassGood,
                                             ::libaom_test::kTwoPassGood),
                           ::testing::Values(1, 0));
}  // namespace
