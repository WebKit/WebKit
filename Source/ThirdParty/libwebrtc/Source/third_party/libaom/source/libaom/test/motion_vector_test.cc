/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/yuv_video_source.h"

namespace {
#define MAX_EXTREME_MV 1
#define MIN_EXTREME_MV 2

// Encoding modes
const libaom_test::TestMode kEncodingModeVectors[] = {
  ::libaom_test::kTwoPassGood,
  ::libaom_test::kOnePassGood,
};

// Encoding speeds
const int kCpuUsedVectors[] = { 1, 5 };

// MV test modes: 1 - always use maximum MV; 2 - always use minimum MV.
const int kMVTestModes[] = { MAX_EXTREME_MV, MIN_EXTREME_MV };

class MotionVectorTestLarge
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode, int,
                                                 int>,
      public ::libaom_test::EncoderTest {
 protected:
  MotionVectorTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), mv_test_mode_(GET_PARAM(3)) {}

  ~MotionVectorTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    if (encoding_mode_ != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 3;
    } else {
      cfg_.rc_buf_sz = 1000;
      cfg_.rc_buf_initial_sz = 500;
      cfg_.rc_buf_optimal_sz = 600;
    }
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_ENABLE_MOTION_VECTOR_UNIT_TEST, mv_test_mode_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }
  }

  libaom_test::TestMode encoding_mode_;
  int cpu_used_;
  int mv_test_mode_;
};

TEST_P(MotionVectorTestLarge, OverallTest) {
  int width = 3840;
  int height = 2160;

  // Reduce the test clip's resolution while testing on 32-bit system.
  if (sizeof(void *) == 4) {
    width = 2048;
    height = 360;
  }

  cfg_.rc_target_bitrate = 24000;
  cfg_.g_profile = 0;
  init_flags_ = AOM_CODEC_USE_PSNR;

  std::unique_ptr<libaom_test::VideoSource> video;
  video.reset(new libaom_test::YUVVideoSource(
      "niklas_640_480_30.yuv", AOM_IMG_FMT_I420, width, height, 30, 1, 0, 3));

  ASSERT_NE(video, nullptr);
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

AV1_INSTANTIATE_TEST_SUITE(MotionVectorTestLarge,
                           ::testing::ValuesIn(kEncodingModeVectors),
                           ::testing::ValuesIn(kCpuUsedVectors),
                           ::testing::ValuesIn(kMVTestModes));
}  // namespace
