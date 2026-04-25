// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "modules/video_coding/codecs/av1/libaom_speed_config_factory.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>

#include "api/environment/environment.h"
#include "api/field_trials.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {

// Helper to allow SpeedLevel to be used in a set.
bool operator<(const EncoderSpeedController::Config::SpeedLevel& lhs,
               const EncoderSpeedController::Config::SpeedLevel& rhs) {
  if (lhs.speeds != rhs.speeds) {
    return lhs.speeds < rhs.speeds;
  }
  return lhs.min_qp < rhs.min_qp;
}

namespace {

using ::testing::Values;

// Test that the number of speed levels increases with complexity.
TEST(LibaomSpeedConfigFactoryTest, NumLevelsIncreaseWithComplexity) {
  FieldTrials empty_trial("");
  LibaomSpeedConfigFactory factory_low(VideoCodecComplexity::kComplexityLow,
                                       VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config_low =
      factory_low.GetSpeedConfig(640, 360, 3, empty_trial);

  LibaomSpeedConfigFactory factory_normal(
      VideoCodecComplexity::kComplexityNormal, VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config_normal =
      factory_normal.GetSpeedConfig(640, 360, 3, empty_trial);

  LibaomSpeedConfigFactory factory_high(VideoCodecComplexity::kComplexityHigh,
                                        VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config_high =
      factory_high.GetSpeedConfig(640, 360, 3, empty_trial);

  LibaomSpeedConfigFactory factory_higher(
      VideoCodecComplexity::kComplexityHigher, VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config_higher =
      factory_higher.GetSpeedConfig(640, 360, 3, empty_trial);

  LibaomSpeedConfigFactory factory_max(VideoCodecComplexity::kComplexityMax,
                                       VideoCodecMode::kRealtimeVideo);

  EncoderSpeedController::Config config_max =
      factory_max.GetSpeedConfig(640, 360, 3, empty_trial);

  EXPECT_GE(config_normal.speed_levels.size(), config_low.speed_levels.size());
  EXPECT_GE(config_high.speed_levels.size(), config_normal.speed_levels.size());
  EXPECT_GE(config_higher.speed_levels.size(), config_high.speed_levels.size());
  EXPECT_GE(config_max.speed_levels.size(), config_higher.speed_levels.size());
}

// Test that speeds within each level are monotonic.
TEST(LibaomSpeedConfigFactoryTest, SpeedsAreMonotonic) {
  FieldTrials empty_trial("");
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config =
      factory.GetSpeedConfig(1280, 720, 3, empty_trial);

  for (const auto& level : config.speed_levels) {
    // Lower reference class index means more important, so speed should be
    // lower or equal.
    EXPECT_LE(level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kKey)],
              level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kMain)]);
    EXPECT_LE(level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kMain)],
              level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kIntermediate)]);
    EXPECT_LE(level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kIntermediate)],
              level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kNoneReference)]);
  }
}

// Test that keyframe and base layer speeds between levels are monotonic.
TEST(LibaomSpeedConfigFactoryTest, KeyAndMainSpeedsIncreaseBetweenLevels) {
  FieldTrials empty_trial("");
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config =
      factory.GetSpeedConfig(1280, 720, 3, empty_trial);

  for (size_t i = 0; i < config.speed_levels.size() - 1; ++i) {
    const auto& current_level = config.speed_levels[i];
    const auto& next_level = config.speed_levels[i + 1];
    EXPECT_LE(current_level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kKey)],
              next_level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kKey)]);
    EXPECT_LE(current_level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kMain)],
              next_level.speeds[static_cast<int>(
                  EncoderSpeedController::ReferenceClass::kMain)]);
  }
}

struct ResolutionParams {
  int width;
  int height;
  int expected_start_index_offset;  // Offset from the last index
};

class LibaomSpeedConfigFactoryResolutionTest
    : public ::testing::TestWithParam<ResolutionParams> {};

INSTANTIATE_TEST_SUITE_P(All,
                         LibaomSpeedConfigFactoryResolutionTest,
                         Values(ResolutionParams{320, 180, 1},
                                ResolutionParams{640, 360, 1},
                                ResolutionParams{1280, 720, 2},
                                ResolutionParams{1920, 1080, 3},
                                ResolutionParams{2560, 1440, 4}));

TEST_P(LibaomSpeedConfigFactoryResolutionTest, GetSpeedConfigStartSpeedIndex) {
  const ResolutionParams& params = GetParam();
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  FieldTrials empty_trial("");
  EncoderSpeedController::Config config =
      factory.GetSpeedConfig(params.width, params.height, 3, empty_trial);
  int expected_index =
      std::max(0, static_cast<int>(config.speed_levels.size()) -
                      params.expected_start_index_offset);
  EXPECT_EQ(config.start_speed_index, expected_index);
}

void CheckDistinctConfigs(LibaomSpeedConfigFactory& factory,
                          int num_temporal_layers) {
  RTC_DCHECK_GT(num_temporal_layers, 0);
  RTC_DCHECK_LE(num_temporal_layers, 3);

  FieldTrials empty_trial("");
  EncoderSpeedController::Config config =
      factory.GetSpeedConfig(640, 360, num_temporal_layers, empty_trial);

  std::set<EncoderSpeedController::Config::SpeedLevel> unique_configs(
      config.speed_levels.begin(), config.speed_levels.end());
  EXPECT_EQ(unique_configs.size(), config.speed_levels.size());
}

TEST(LibaomSpeedConfigFactoryTest, DistinctConfigs1Tl) {
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  CheckDistinctConfigs(factory, 1);
}

TEST(LibaomSpeedConfigFactoryTest, DistinctConfigs2Tl) {
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  CheckDistinctConfigs(factory, 2);
}

TEST(LibaomSpeedConfigFactoryTest, DistinctConfigs3Tl) {
  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  CheckDistinctConfigs(factory, 3);
}

TEST(LibaomSpeedConfigFactoryTest, PropagatesPsnrExperimentSettings) {
  const std::string kFieldTrials =
      "WebRTC-Video-CalculatePsnr/Enabled,sampling_interval:3000ms/";
  Environment env = CreateTestEnvironment({.field_trials = kFieldTrials});

  LibaomSpeedConfigFactory factory(VideoCodecComplexity::kComplexityMax,
                                   VideoCodecMode::kRealtimeVideo);
  EncoderSpeedController::Config config =
      factory.GetSpeedConfig(1280, 720, 2, env.field_trials());

  ASSERT_TRUE(config.psnr_probing_settings.has_value());
  EXPECT_EQ(config.psnr_probing_settings->mode,
            EncoderSpeedController::Config::PsnrProbingSettings::Mode::
                kRegularBaseLayerSampling);
  EXPECT_EQ(config.psnr_probing_settings->sampling_interval,
            TimeDelta::Seconds(3));
  EXPECT_EQ(config.psnr_probing_settings->average_base_layer_ratio, 0.5);
}

}  // namespace
}  // namespace webrtc
