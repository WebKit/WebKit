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
#include "config/aom_config.h"

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const ::libaom_test::TestMode kTestMode[] =
#if CONFIG_REALTIME_ONLY
    { ::libaom_test::kRealTime };
#else
    { ::libaom_test::kRealTime, ::libaom_test::kOnePassGood };
#endif

const int kUseFixedQPOffsetsMode[] = { 1, 2 };

class UseFixedQPOffsetsTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  UseFixedQPOffsetsTest() : EncoderTest(GET_PARAM(0)) {}
  ~UseFixedQPOffsetsTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cfg_.kf_max_dist = 9999;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.use_fixed_qp_offsets = GET_PARAM(2);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 6);
      encoder->Control(AOME_SET_CQ_LEVEL, frame_qp_);
    }
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    int qp = 0;
    encoder->Control(AOME_GET_LAST_QUANTIZER_64, &qp);

    // If a call to encoder->EncodeFrame() results in a last QP of 0,
    // interpret as the frame being read into the lookahead buffer.
    if (qp == 0) return;

    if (cfg_.use_fixed_qp_offsets == 2) {
      // Setting use_fixed_qp_offsets = 2 means every frame should use the same
      // QP
      ASSERT_EQ(qp, frame_qp_);
    } else {
      ASSERT_LE(qp, frame_qp_);
    }
  }

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 33);
    frame_qp_ = 35;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int frame_qp_;
};

TEST_P(UseFixedQPOffsetsTest, TestQPOffsets) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(UseFixedQPOffsetsTest,
                           ::testing::ValuesIn(kTestMode),
                           ::testing::ValuesIn(kUseFixedQPOffsetsMode));
}  // namespace
