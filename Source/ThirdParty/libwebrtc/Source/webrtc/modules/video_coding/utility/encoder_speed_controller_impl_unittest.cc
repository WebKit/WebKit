/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Eq;
using ReferenceClass = webrtc::EncoderSpeedController::ReferenceClass;

namespace webrtc {
namespace {
constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1.0 / 30.0);

EncoderSpeedController::Config GetDefaultConfig() {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 5, 5, 5}},
                         {.speeds = {6, 6, 6, 6}},
                         {.speeds = {7, 7, 7, 7}}};
  config.start_speed_index = 1;
  return config;
}
}  // namespace

TEST(EncoderSpeedControllerTest, CreateFailsWithEmptySpeedLevels) {
  EncoderSpeedController::Config config;
  config.speed_levels = {};
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);
}

TEST(EncoderSpeedControllerTest, CreateFailsWithInvalidStartSpeedIndex) {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 5, 5, 5}}};
  config.start_speed_index = -1;  // Invalid index

  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);

  config.start_speed_index = 1;
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);
}

TEST(EncoderSpeedControllerTest, CreateFailsWithInvalidFrameInterval) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::Zero()), nullptr);
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::Millis(-1)),
            nullptr);
  EXPECT_EQ(EncoderSpeedController::Create(config, TimeDelta::PlusInfinity()),
            nullptr);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsBaseLayers) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[0].min_qp = 25;  // Prevent dropping to speed 5 easily
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  // Starts at index 1 (speed 6)
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);

  // Simulate high encode time to increase speed
  for (int i = 0; i < 10; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.90,
                                .qp = 30,
                                .frame_info = frame_info},
                               /*baseline_results=*/std::nullopt);
  }
  // Speed should increase to 7
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 7);

  // Simulate low encode time to decrease speed
  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.10,
                                .qp = 20,
                                .frame_info = frame_info},
                               /*baseline_results=*/std::nullopt);
  }
  // Speed should decrease to 6
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsKeyFrame) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EXPECT_EQ(controller
                ->GetEncodeSettings({.reference_type = ReferenceClass::kKey,
                                     .timestamp = Timestamp::Zero()})
                .speed,
            6);
}

TEST(EncoderSpeedControllerTest, GetEncodeSettingsWithTemporalLayers) {
  EncoderSpeedController::Config config;
  config.speed_levels = {{.speeds = {5, 6, 7, 8}}, {.speeds = {9, 10, 11, 12}}};
  config.start_speed_index = 0;
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EXPECT_EQ(controller
                ->GetEncodeSettings({.reference_type = ReferenceClass::kKey,
                                     .timestamp = Timestamp::Zero()})
                .speed,
            5);
  EXPECT_EQ(controller
                ->GetEncodeSettings({.reference_type = ReferenceClass::kMain,
                                     .timestamp = Timestamp::Zero()})
                .speed,
            6);
  EXPECT_EQ(
      controller
          ->GetEncodeSettings({.reference_type = ReferenceClass::kIntermediate,
                               .timestamp = Timestamp::Zero()})
          .speed,
      7);
  EXPECT_EQ(
      controller
          ->GetEncodeSettings({.reference_type = ReferenceClass::kNoneReference,
                               .timestamp = Timestamp::Zero()})
          .speed,
      8);
}

TEST(EncoderSpeedControllerTest, StaysAtMaxSpeed) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.start_speed_index = 2;  // Start at max speed
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.95,
                                .qp = 30,
                                .frame_info = frame_info},
                               /*baseline_results=*/std::nullopt);
  }

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed,
            7);  // Still at max speed
}

TEST(EncoderSpeedControllerTest, StaysAtMinSpeed) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.start_speed_index = 0;  // Start at min speed
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.speed = 5, .frame_info = frame_info},
                               /*baseline_results=*/std::nullopt);
  }

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed,
            5);  // Still at min speed
}

TEST(EncoderSpeedControllerTest, IncreasesSpeedOnLowQp) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[1].min_qp = 20;
  config.start_speed_index = 1;
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);

  // Simulate low QP, normal encode time
  for (int i = 0; i < 20; ++i) {
    controller->OnEncodedFrame({.encode_time = kFrameInterval * 0.60,
                                .qp = 10,
                                .frame_info = frame_info},
                               /*baseline_results=*/std::nullopt);
  }
  // Speed should increase to 7 due to low QP
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 7);
}

TEST(EncoderSpeedControllerTest, TriggersRegularPsnrSampling) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kRegularBaseLayerSampling,
      .sampling_interval = TimeDelta::Seconds(5)};
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  // First frame should always trigger PSNR if configured.
  EXPECT_TRUE(controller->GetEncodeSettings(frame_info).calculate_psnr);

  // Complete the frame.
  controller->OnEncodedFrame(
      {.encode_time = kFrameInterval * 0.5, .qp = 30, .frame_info = frame_info},
      /*baseline_results=*/std::nullopt);

  // Next frame within interval should not trigger PSNR.
  frame_info.timestamp += kFrameInterval;
  EXPECT_FALSE(controller->GetEncodeSettings(frame_info).calculate_psnr);

  // Advance to sampling interval.
  frame_info.timestamp += config.psnr_probing_settings->sampling_interval;
  EXPECT_TRUE(controller->GetEncodeSettings(frame_info).calculate_psnr);
}

TEST(EncoderSpeedControllerTest, TriggersPsnrProbeForSpeedChange) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  // Default speed levels = {5, 6, 7}.
  // To move from speed 6 to 5, we check speed 5's requirements.
  config.speed_levels[0].min_psnr_gain = {
      .baseline_speed = 6,  // Compare against current speed (6)
      .psnr_threshold = 1.0,
  };
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kOnlyWhenProbing,
      .sampling_interval = TimeDelta::Seconds(1)};
  config.start_speed_index = 1;  // Start at speed 6.

  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  // Initial state: Speed 6 (index 1).
  EXPECT_EQ(controller
                ->GetEncodeSettings({.reference_type = ReferenceClass::kKey,
                                     .timestamp = Timestamp::Zero()})
                .speed,
            6);

  // Simulate low utilization to trigger speed decrease attempt.
  // We need multiple samples to trigger the filter.
  constexpr int kNumFrames = 10;
  for (int i = 0; i < kNumFrames; ++i) {
    controller->OnEncodedFrame(
        {.encode_time = kFrameInterval * 0.1,
         .qp = 20,
         .frame_info = {.reference_type = ReferenceClass::kMain,
                        .timestamp =
                            Timestamp::Zero() + (i + 1) * kFrameInterval}},
        /*baseline_results=*/std::nullopt);
  }

  // Next frame should be a probe.
  // We expect it to try Speed 5.
  EncoderSpeedController::EncodeSettings settings =
      controller->GetEncodeSettings(
          {.reference_type = ReferenceClass::kMain,
           .timestamp = Timestamp::Zero() + kNumFrames * kFrameInterval});
  EXPECT_EQ(settings.speed, 5);
  EXPECT_TRUE(settings.calculate_psnr);
  EXPECT_EQ(settings.baseline_comparison_speed, 6);
}

TEST(EncoderSpeedControllerTest, DecreasesSpeedOnSufficientPsnrGain) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  // Default speed levels = {5, 6, 7}.
  // To move to Speed 5, we need 1.0dB gain over Speed 6.
  config.speed_levels[0].min_psnr_gain = {
      .baseline_speed = 6,
      .psnr_threshold = 1.0,
  };
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kOnlyWhenProbing,
      .sampling_interval = TimeDelta::Seconds(1)};
  config.start_speed_index = 1;  // Start at speed 6.

  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  // Trigger probe.
  constexpr int kNumFrames = 10;
  for (int i = 0; i < kNumFrames; ++i) {
    controller->OnEncodedFrame(
        {.encode_time = kFrameInterval * 0.1,
         .qp = 20,
         .frame_info = {.reference_type = ReferenceClass::kMain,
                        .timestamp =
                            Timestamp::Zero() + (i + 1) * kFrameInterval}},
        /*baseline_results=*/std::nullopt);
  }

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain,
      .timestamp = Timestamp::Zero() + kNumFrames * kFrameInterval};

  // Get settings (verify it's a probe).
  EncoderSpeedController::EncodeSettings settings =
      controller->GetEncodeSettings(frame_info);
  ASSERT_TRUE(settings.baseline_comparison_speed.has_value());
  EXPECT_EQ(settings.speed, 5);
  EXPECT_EQ(settings.baseline_comparison_speed, 6);

  // Feed probe results.
  // Result (Speed 5): 37.0dB (Higher quality)
  // Baseline (Speed 6): 35.0dB (Lower quality)
  // Gain: 2.0dB >= 1.0dB threshold.
  EncoderSpeedController::EncodeResults results = {
      .speed = settings.speed,  // 5
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = 37.0,
      .frame_info = frame_info};
  EncoderSpeedController::EncodeResults baseline_results = {
      .speed = *settings.baseline_comparison_speed,  // 6
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = 35.0,
      .frame_info = frame_info};

  controller->OnEncodedFrame(results, baseline_results);

  // Speed should decrease to 5.
  frame_info.timestamp += kFrameInterval;
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 5);
}

TEST(EncoderSpeedControllerTest, MaintainsSpeedOnInsufficientPsnrGain) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  // Default speed levels = {5, 6, 7}.
  // To move to Speed 5, we need 1.0dB gain over Speed 6.
  config.speed_levels[0].min_psnr_gain = {
      .baseline_speed = 6,
      .psnr_threshold = 1.0,
  };
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kOnlyWhenProbing,
      .sampling_interval = TimeDelta::Seconds(1)};
  config.start_speed_index = 1;  // Start at speed 6.

  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  // Trigger probe.
  constexpr int kNumFrames = 10;
  for (int i = 0; i < kNumFrames; ++i) {
    controller->OnEncodedFrame(
        {.encode_time = kFrameInterval * 0.1,
         .qp = 20,
         .frame_info = {.reference_type = ReferenceClass::kMain,
                        .timestamp =
                            Timestamp::Zero() + (i + 1) * kFrameInterval}},
        /*baseline_results=*/std::nullopt);
  }

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain,
      .timestamp = Timestamp::Zero() + kNumFrames * kFrameInterval};

  // Get settings (verify it's a probe).
  EncoderSpeedController::EncodeSettings settings =
      controller->GetEncodeSettings(frame_info);
  ASSERT_TRUE(settings.baseline_comparison_speed.has_value());
  EXPECT_EQ(settings.speed, 5);
  EXPECT_EQ(settings.baseline_comparison_speed, 6);

  // Feed probe results.
  // Result (Speed 5): 35.5dB
  // Baseline (Speed 6): 35.0dB
  // Gain: 0.5dB < 1.0dB threshold.
  EncoderSpeedController::EncodeResults results = {
      .speed = settings.speed,
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = 35.5,
      .frame_info = frame_info};
  EncoderSpeedController::EncodeResults baseline_results = {
      .speed = *settings.baseline_comparison_speed,
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = 35.0,
      .frame_info = frame_info};

  controller->OnEncodedFrame(results, baseline_results);

  // Speed should stay at 6 (reset to current index because gain was
  // insufficient).
  frame_info.timestamp += kFrameInterval;
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}
TEST(EncoderSpeedControllerTest, CreateFailsWithInvalidPsnrSamplingInterval) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kRegularBaseLayerSampling,
      .sampling_interval = TimeDelta::Zero()};
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);

  config.psnr_probing_settings->sampling_interval = TimeDelta::PlusInfinity();
  EXPECT_EQ(EncoderSpeedController::Create(config, kFrameInterval), nullptr);
}

TEST(EncoderSpeedControllerTest, OnEncodedFrameIgnoresResultWithMissingPsnr) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  config.speed_levels[0].min_psnr_gain = {
      .baseline_speed = 6,
      .psnr_threshold = 1.0,
  };
  config.psnr_probing_settings = {
      .mode = EncoderSpeedController::Config::PsnrProbingSettings::Mode::
          kOnlyWhenProbing,
      .sampling_interval = TimeDelta::Seconds(1)};
  config.start_speed_index = 1;

  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain, .timestamp = Timestamp::Zero()};

  // Trigger probe.
  constexpr int kNumFrames = 10;
  for (int i = 0; i < kNumFrames; ++i) {
    controller->OnEncodedFrame(
        {.encode_time = kFrameInterval * 0.1,
         .qp = 20,
         .frame_info = {.reference_type = ReferenceClass::kMain,
                        .timestamp =
                            Timestamp::Zero() + (i + 1) * kFrameInterval}},
        /*baseline_results=*/std::nullopt);
  }

  // Get settings (verify it's a probe).
  EncoderSpeedController::EncodeSettings settings =
      controller->GetEncodeSettings(
          {.reference_type = ReferenceClass::kMain,
           .timestamp = Timestamp::Zero() + kNumFrames * kFrameInterval});
  ASSERT_TRUE(settings.baseline_comparison_speed.has_value());
  EXPECT_EQ(settings.speed, 5);

  // Feed probe results with missing PSNR.
  frame_info.timestamp = Timestamp::Zero() + kNumFrames * kFrameInterval;
  EncoderSpeedController::EncodeResults results = {
      .speed = settings.speed,
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = std::nullopt,  // Missing PSNR
      .frame_info = frame_info};
  EncoderSpeedController::EncodeResults baseline_results = {
      .speed = *settings.baseline_comparison_speed,
      .encode_time = kFrameInterval * 0.1,
      .qp = 20,
      .psnr = 35.0,
      .frame_info = frame_info};

  controller->OnEncodedFrame(results, baseline_results);

  // Speed should stay at 6 (reset to current index because probe invalid).
  frame_info.timestamp += kFrameInterval;
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}

TEST(EncoderSpeedControllerTest, WorksWithDefaultInfiniteTimestamp) {
  EncoderSpeedController::Config config = GetDefaultConfig();
  auto controller = EncoderSpeedController::Create(config, kFrameInterval);
  ASSERT_NE(controller, nullptr);

  // Default frame_info has timestamp = Timestamp::MinusInfinity().
  EncoderSpeedController::FrameEncodingInfo frame_info = {
      .reference_type = ReferenceClass::kMain};
  EXPECT_TRUE(frame_info.timestamp.IsMinusInfinity());

  // Should return a valid speed (start speed 6).
  // PSNR calculation should be false because timestamp is not finite.
  EncoderSpeedController::EncodeSettings settings =
      controller->GetEncodeSettings(frame_info);
  EXPECT_EQ(settings.speed, 6);
  EXPECT_FALSE(settings.calculate_psnr);

  // OnEncodedFrame should also handle it gracefully.
  controller->OnEncodedFrame(
      {.encode_time = kFrameInterval * 0.5, .qp = 30, .frame_info = frame_info},
      /*baseline_results=*/std::nullopt);

  // Speed should remain same.
  EXPECT_EQ(controller->GetEncodeSettings(frame_info).speed, 6);
}

}  // namespace webrtc
