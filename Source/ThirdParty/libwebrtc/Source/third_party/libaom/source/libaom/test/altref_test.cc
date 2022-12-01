/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
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
typedef struct {
  const unsigned int min_kf_dist;
  const unsigned int max_kf_dist;
  const unsigned int min_gf_interval;
  const unsigned int max_gf_interval;
  const unsigned int lag_in_frames;
  libaom_test::TestMode encoding_mode;
} AltRefTestParams;

static const AltRefTestParams TestParams[] = {
  { 0, 10, 4, 8, 10, ::libaom_test::kOnePassGood },
  { 0, 30, 8, 12, 16, ::libaom_test::kOnePassGood },
  { 30, 30, 12, 16, 25, ::libaom_test::kOnePassGood },
  { 0, 60, 12, 20, 25, ::libaom_test::kOnePassGood },
  { 60, 60, 16, 28, 30, ::libaom_test::kOnePassGood },
  { 0, 100, 16, 32, 35, ::libaom_test::kOnePassGood },
  { 0, 10, 4, 8, 10, ::libaom_test::kTwoPassGood },
  { 0, 30, 8, 12, 16, ::libaom_test::kTwoPassGood },
  { 30, 30, 12, 16, 25, ::libaom_test::kTwoPassGood },
  { 0, 60, 16, 24, 25, ::libaom_test::kTwoPassGood },
  { 60, 60, 20, 28, 30, ::libaom_test::kTwoPassGood },
  { 0, 100, 24, 32, 35, ::libaom_test::kTwoPassGood },
};

std::ostream &operator<<(std::ostream &os, const AltRefTestParams &test_arg) {
  return os << "AltRefTestParams { min_kf_dist:" << test_arg.min_kf_dist
            << " max_kf_dist:" << test_arg.max_kf_dist
            << " min_gf_interval:" << test_arg.min_gf_interval
            << " max_gf_interval:" << test_arg.max_gf_interval
            << " lag_in_frames:" << test_arg.lag_in_frames
            << " encoding_mode:" << test_arg.encoding_mode << " }";
}

// This class is used to check the presence of altref frame.
class AltRefFramePresenceTestLarge
    : public ::libaom_test::CodecTestWith2Params<AltRefTestParams, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  AltRefFramePresenceTestLarge()
      : EncoderTest(GET_PARAM(0)), altref_test_params_(GET_PARAM(1)),
        rc_end_usage_(GET_PARAM(2)) {
    is_arf_frame_present_ = 0;
  }
  virtual ~AltRefFramePresenceTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(altref_test_params_.encoding_mode);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    cfg_.kf_min_dist = altref_test_params_.min_kf_dist;
    cfg_.kf_max_dist = altref_test_params_.max_kf_dist;
    cfg_.g_lag_in_frames = altref_test_params_.lag_in_frames;
  }

  virtual bool DoDecode() const { return 1; }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_MIN_GF_INTERVAL,
                       altref_test_params_.min_gf_interval);
      encoder->Control(AV1E_SET_MAX_GF_INTERVAL,
                       altref_test_params_.max_gf_interval);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (is_arf_frame_present_ != 1 && AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_ALTREF_PRESENT,
                                    &is_arf_frame_present_);
    }
    return AOM_CODEC_OK == res_dec;
  }

  const AltRefTestParams altref_test_params_;
  int is_arf_frame_present_;
  aom_rc_mode rc_end_usage_;
};

TEST_P(AltRefFramePresenceTestLarge, AltRefFrameEncodePresenceTest) {
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 100);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(is_arf_frame_present_, 1);
}

AV1_INSTANTIATE_TEST_SUITE(AltRefFramePresenceTestLarge,
                           ::testing::ValuesIn(TestParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));

typedef struct {
  const ::libaom_test::TestMode encoding_mode;
  const unsigned int min_gf_interval;
  const unsigned int max_gf_interval;
} gfIntervalParam;

const gfIntervalParam gfTestParams[] = {
  // single pass
  { ::libaom_test::kOnePassGood, 0, 6 },
  { ::libaom_test::kOnePassGood, 0, 8 },
  { ::libaom_test::kOnePassGood, 5, 10 },
  { ::libaom_test::kOnePassGood, 8, 16 },
  { ::libaom_test::kOnePassGood, 16, 16 },

  // two pass
  { ::libaom_test::kTwoPassGood, 0, 6 },
  { ::libaom_test::kTwoPassGood, 0, 8 },
  { ::libaom_test::kTwoPassGood, 5, 10 },
  { ::libaom_test::kTwoPassGood, 8, 16 },
  { ::libaom_test::kTwoPassGood, 16, 32 },
  { ::libaom_test::kTwoPassGood, 20, 32 },
};

// This class is used to test if the gf interval bounds configured by the user
// are respected by the encoder.
class GoldenFrameIntervalTestLarge
    : public ::libaom_test::CodecTestWith2Params<gfIntervalParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  GoldenFrameIntervalTestLarge()
      : EncoderTest(GET_PARAM(0)), gf_interval_param_(GET_PARAM(1)),
        rc_end_usage_(GET_PARAM(2)) {
    baseline_gf_interval_ = -1;
    limit_ = 60;
    frame_num_ = 0;
  }
  virtual ~GoldenFrameIntervalTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(gf_interval_param_.encoding_mode);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    // kf_min_dist is equal to kf_max_dist to make sure that there are no scene
    // cuts due to which the min_gf_interval may not be respected.
    cfg_.kf_min_dist = limit_;
    cfg_.kf_max_dist = limit_;
    cfg_.g_limit = limit_;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_target_bitrate = 1000;
  }

  virtual bool DoDecode() const { return 1; }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_MIN_GF_INTERVAL,
                       gf_interval_param_.min_gf_interval);
      encoder->Control(AV1E_SET_MAX_GF_INTERVAL,
                       gf_interval_param_.max_gf_interval);
    }
    if (frame_num_ > 0) {
      encoder->Control(AV1E_GET_BASELINE_GF_INTERVAL, &baseline_gf_interval_);
      ASSERT_LE(baseline_gf_interval_,
                (int)gf_interval_param_.max_gf_interval + 1);
      if ((frame_num_ + (int)gf_interval_param_.min_gf_interval) <= limit_) {
        ASSERT_GE(baseline_gf_interval_,
                  (int)gf_interval_param_.min_gf_interval);
      }
    }
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    (void)pkt;
    ++frame_num_;
  }

  const gfIntervalParam gf_interval_param_;
  int baseline_gf_interval_;
  int limit_;
  int frame_num_;
  aom_rc_mode rc_end_usage_;
};

TEST_P(GoldenFrameIntervalTestLarge, GoldenFrameIntervalTest) {
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, limit_);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(GoldenFrameIntervalTestLarge,
                           ::testing::ValuesIn(gfTestParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CQ, AOM_CBR));

}  // namespace
