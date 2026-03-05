/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/quality_scaling_experiment.h"

#include "api/field_trials.h"
#include "api/video/video_codec_type.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
void ExpectEqualSettings(QualityScalingExperiment::Settings a,
                         QualityScalingExperiment::Settings b) {
  EXPECT_EQ(a.vp8_low, b.vp8_low);
  EXPECT_EQ(a.vp8_high, b.vp8_high);
  EXPECT_EQ(a.vp9_low, b.vp9_low);
  EXPECT_EQ(a.vp9_high, b.vp9_high);
  EXPECT_EQ(a.h264_low, b.h264_low);
  EXPECT_EQ(a.h264_high, b.h264_high);
  EXPECT_EQ(a.generic_low, b.generic_low);
  EXPECT_EQ(a.generic_high, b.generic_high);
  EXPECT_EQ(a.alpha_high, b.alpha_high);
  EXPECT_EQ(a.alpha_low, b.alpha_low);
  EXPECT_EQ(a.drop, b.drop);
}

void ExpectEqualConfig(QualityScalingExperiment::Config a,
                       QualityScalingExperiment::Config b) {
  EXPECT_EQ(a.alpha_high, b.alpha_high);
  EXPECT_EQ(a.alpha_low, b.alpha_low);
  EXPECT_EQ(a.use_all_drop_reasons, b.use_all_drop_reasons);
}
}  // namespace

#if !defined(WEBRTC_IOS)
// TODO(bugs.webrtc.org/12401): investigate why QualityScaler kicks in on iOS.
TEST(QualityScalingExperimentTest, DefaultEnabledWithoutFieldTrial) {
  FieldTrials field_trials("");
  EXPECT_TRUE(QualityScalingExperiment::Enabled(field_trials));
}
#else
TEST(QualityScalingExperimentTest, DefaultDisabledWithoutFieldTrialIOS) {
  FieldTrials field_trials("");
  EXPECT_FALSE(QualityScalingExperiment::Enabled(field_trials));
}
#endif

TEST(QualityScalingExperimentTest, EnabledWithFieldTrial) {
  FieldTrials field_trials("WebRTC-Video-QualityScaling/Enabled/");
  EXPECT_TRUE(QualityScalingExperiment::Enabled(field_trials));
}

TEST(QualityScalingExperimentTest, ParseSettings) {
  const QualityScalingExperiment::Settings kExpected = {.vp8_low = 1,
                                                        .vp8_high = 2,
                                                        .vp9_low = 3,
                                                        .vp9_high = 4,
                                                        .h264_low = 5,
                                                        .h264_high = 6,
                                                        .generic_low = 7,
                                                        .generic_high = 8,
                                                        .alpha_high = 0.9f,
                                                        .alpha_low = 0.99f,
                                                        .drop = 1};
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,1/");
  const auto settings = QualityScalingExperiment::ParseSettings(field_trials);
  EXPECT_TRUE(settings);
  ExpectEqualSettings(kExpected, *settings);
}

#if !defined(WEBRTC_IOS)
// TODO(bugs.webrtc.org/12401): investigate why QualityScaler kicks in on iOS.
TEST(QualityScalingExperimentTest, ParseSettingsUsesDefaultsWithoutFieldTrial) {
  FieldTrials field_trials("");
  // Uses some default hard coded values.
  EXPECT_TRUE(QualityScalingExperiment::ParseSettings(field_trials));
}
#else
TEST(QualityScalingExperimentTest, ParseSettingsFailsWithoutFieldTrial) {
  FieldTrials field_trials("");
  EXPECT_FALSE(QualityScalingExperiment::ParseSettings(field_trials));
}
#endif

TEST(QualityScalingExperimentTest, ParseSettingsFailsWithInvalidFieldTrial) {
  FieldTrials field_trials("WebRTC-Video-QualityScaling/Enabled-invalid/");
  EXPECT_FALSE(QualityScalingExperiment::ParseSettings(field_trials));
}

TEST(QualityScalingExperimentTest, GetConfig) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.9,0.99,0/");
  const auto config = QualityScalingExperiment::GetConfig(field_trials);
  EXPECT_EQ(0.9f, config.alpha_high);
  EXPECT_EQ(0.99f, config.alpha_low);
  EXPECT_FALSE(config.use_all_drop_reasons);
}

TEST(QualityScalingExperimentTest, GetsDefaultConfigForInvalidFieldTrial) {
  FieldTrials field_trials("WebRTC-Video-QualityScaling/Enabled-invalid/");
  const auto config = QualityScalingExperiment::GetConfig(field_trials);
  ExpectEqualConfig(config, QualityScalingExperiment::Config());
}

TEST(QualityScalingExperimentTest, GetsDefaultAlphaForInvalidValue) {
  QualityScalingExperiment::Config expected_config;
  expected_config.use_all_drop_reasons = true;
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,7,8,0.99,0.9,1/");
  const auto config = QualityScalingExperiment::GetConfig(field_trials);
  ExpectEqualConfig(config, expected_config);
}

TEST(QualityScalingExperimentTest, GetVp8Thresholds) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP8, field_trials);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(1, thresholds->low);
  EXPECT_EQ(2, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidVp8Value) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-0,0,3,4,5,6,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP8, field_trials);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetVp9Thresholds) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP9, field_trials);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(3, thresholds->low);
  EXPECT_EQ(4, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidVp9Value) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,0,0,5,6,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecVP9, field_trials);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetH264Thresholds) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecH264, field_trials);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(5, thresholds->low);
  EXPECT_EQ(6, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidH264Value) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,0,0,7,8,0.9,0.99,1/");
  const auto thresholds =
      QualityScalingExperiment::GetQpThresholds(kVideoCodecH264, field_trials);
  EXPECT_FALSE(thresholds);
}

TEST(QualityScalingExperimentTest, GetGenericThresholds) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,0,0,7,8,0.9,0.99,1/");
  const auto thresholds = QualityScalingExperiment::GetQpThresholds(
      kVideoCodecGeneric, field_trials);
  EXPECT_TRUE(thresholds);
  EXPECT_EQ(7, thresholds->low);
  EXPECT_EQ(8, thresholds->high);
}

TEST(QualityScalingExperimentTest, GetThresholdsFailsForInvalidGenericValue) {
  FieldTrials field_trials(
      "WebRTC-Video-QualityScaling/Enabled-1,2,3,4,5,6,0,0,0.9,0.99,1/");
  const auto thresholds = QualityScalingExperiment::GetQpThresholds(
      kVideoCodecGeneric, field_trials);
  EXPECT_FALSE(thresholds);
}
}  // namespace webrtc
