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

#include "config/aom_config.h"

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const libaom_test::TestMode kTestModeParams[] =
#if CONFIG_REALTIME_ONLY
    { ::libaom_test::kRealTime };
#else
    { ::libaom_test::kRealTime, ::libaom_test::kOnePassGood };
#endif

class AqSegmentTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  AqSegmentTest() : EncoderTest(GET_PARAM(0)) {}
  ~AqSegmentTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
    aq_mode_ = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_DELTAQ_MODE, deltaq_mode_);
      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT, 100);
      if (mode_ == ::libaom_test::kRealTime) {
        encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      }
    }
  }

  void DoTest(int aq_mode) {
    aq_mode_ = aq_mode;
    deltaq_mode_ = 0;
    cfg_.kf_max_dist = 12;
    cfg_.rc_min_quantizer = 8;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 6;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_target_bitrate = 300;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 15);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int set_cpu_used_;
  int aq_mode_;
  int deltaq_mode_;
};

// Validate that this AQ segmentation mode (1-variance_aq, 2-complexity_aq,
// 3-cyclic_refresh_aq) encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatch) { DoTest(GET_PARAM(3)); }

#if !CONFIG_REALTIME_ONLY
// Validate that this delta q mode
// encodes and decodes without a mismatch.
TEST_P(AqSegmentTest, TestNoMisMatchExtDeltaQ) {
  cfg_.rc_end_usage = AOM_CQ;
  aq_mode_ = 0;
  deltaq_mode_ = 2;
  ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 15);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

AV1_INSTANTIATE_TEST_SUITE(AqSegmentTest, ::testing::ValuesIn(kTestModeParams),
                           ::testing::Range(5, 9), ::testing::Range(0, 4));

#if !CONFIG_REALTIME_ONLY
class AqSegmentTestLarge : public AqSegmentTest {};

TEST_P(AqSegmentTestLarge, TestNoMisMatch) { DoTest(GET_PARAM(3)); }

AV1_INSTANTIATE_TEST_SUITE(AqSegmentTestLarge,
                           ::testing::Values(::libaom_test::kOnePassGood),
                           ::testing::Range(3, 5), ::testing::Range(0, 4));
#endif
}  // namespace
