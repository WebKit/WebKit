/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <memory>
#include <string>

#include "gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {
const int kLevelMin = 0;
const int kLevelMax = 31;
const int kLevelKeepStats = 32;
// Speed settings tested
static const int kCpuUsedVectors[] = {
  1,
  2,
  3,
  4,
};

class LevelTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  LevelTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), target_level_(31) {}

  ~LevelTest() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    if (encoding_mode_ != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 5;
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
      encoder->Control(AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level_);
      if (encoding_mode_ != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }

    int num_operating_points;
    encoder->Control(AV1E_GET_NUM_OPERATING_POINTS, &num_operating_points);
    ASSERT_EQ(num_operating_points, 1);
    encoder->Control(AV1E_GET_SEQ_LEVEL_IDX, level_);
    ASSERT_LE(level_[0], kLevelMax);
    ASSERT_GE(level_[0], kLevelMin);
  }

  libaom_test::TestMode encoding_mode_;
  int cpu_used_;
  int target_level_;
  int level_[32];
};

TEST(LevelTest, TestTargetLevelApi) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_config_default(codec, &cfg, 0));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_enc_init(&enc, codec, &cfg, 0));
  for (int operating_point = 0; operating_point <= 32; ++operating_point) {
    for (int level = 0; level <= 32; ++level) {
      const int target_level = operating_point * 100 + level;
      if (operating_point <= 31 &&
          ((level < (CONFIG_CWG_C013 ? 28 : 20) && level != 2 && level != 3 &&
            level != 6 && level != 7 && level != 10 && level != 11) ||
           level == kLevelMax || level == kLevelKeepStats)) {
        EXPECT_EQ(AOM_CODEC_OK,
                  AOM_CODEC_CONTROL_TYPECHECKED(
                      &enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level));
      } else {
        EXPECT_EQ(AOM_CODEC_INVALID_PARAM,
                  AOM_CODEC_CONTROL_TYPECHECKED(
                      &enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, target_level));
      }
    }
  }
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

TEST(LevelTest, InvalidOperatingPointIndexErrorDetail) {
  aom_codec_iface_t *codec = aom_codec_av1_cx();
  aom_codec_ctx_t enc;
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(codec, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_enc_init(&enc, codec, &cfg, 0), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TARGET_SEQ_LEVEL_IDX, 3219),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_error_detail(&enc),
            std::string("Invalid operating point index: 32"));
  EXPECT_EQ(aom_codec_set_option(&enc, "target-seq-level-idx", "3319"),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_error_detail(&enc),
            std::string("Invalid operating point index: 33"));
  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

TEST_P(LevelTest, TestTargetLevel19) {
  std::unique_ptr<libaom_test::VideoSource> video;
  video.reset(new libaom_test::Y4mVideoSource("park_joy_90p_8_420.y4m", 0, 10));
  ASSERT_NE(video, nullptr);
  // Level index 19 corresponding to level 6.3.
  target_level_ = 19;
  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

TEST_P(LevelTest, TestLevelMonitoringLowBitrate) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 30);
    target_level_ = kLevelKeepStats;
    cfg_.rc_target_bitrate = 1000;
    cfg_.g_limit = 30;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(level_[0], 0);
  }
}

TEST_P(LevelTest, TestLevelMonitoringHighBitrate) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 30);
    target_level_ = kLevelKeepStats;
    cfg_.rc_target_bitrate = 4000;
    cfg_.g_limit = 30;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(level_[0], 4);
  }
}

TEST_P(LevelTest, TestTargetLevel0) {
  // To save run time, we only test speed 4.
  if (cpu_used_ == 4) {
    libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                       30, 1, 0, 50);
    const int target_level = 0;
    target_level_ = target_level;
    cfg_.rc_target_bitrate = 4000;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_LE(level_[0], target_level);
  }
}

TEST_P(LevelTest, TestTargetLevelRecode) {
  if (cpu_used_ == 4 && encoding_mode_ == ::libaom_test::kTwoPassGood) {
    libaom_test::I420VideoSource video("rand_noise_w1280h720.yuv", 1280, 720,
                                       25, 1, 0, 10);
    const int target_level = 0005;
    target_level_ = target_level;
    cfg_.rc_target_bitrate = 5000;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }
}

AV1_INSTANTIATE_TEST_SUITE(LevelTest,
                           ::testing::Values(::libaom_test::kTwoPassGood,
                                             ::libaom_test::kOnePassGood),
                           ::testing::ValuesIn(kCpuUsedVectors));
}  // namespace
