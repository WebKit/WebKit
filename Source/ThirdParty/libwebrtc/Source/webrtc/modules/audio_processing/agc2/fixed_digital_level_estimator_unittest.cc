/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/fixed_digital_level_estimator.h"

#include <limits>

#include "api/audio/audio_frame.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInputLevel = 10000.f;

// Run audio at specified settings through the level estimator, and
// verify that the output level falls within the bounds.
void TestLevelEstimator(size_t samples_per_channel,
                        int num_channels,
                        float input_level_linear_scale,
                        float expected_min,
                        float expected_max) {
  ApmDataDumper apm_data_dumper(0);
  FixedDigitalLevelEstimator level_estimator(samples_per_channel,
                                             &apm_data_dumper);

  const VectorFloatFrame vectors_with_float_frame(
      num_channels, samples_per_channel, input_level_linear_scale);

  for (int i = 0; i < 500; ++i) {
    const auto level =
        level_estimator.ComputeLevel(vectors_with_float_frame.view());

    // Give the estimator some time to ramp up.
    if (i < 50) {
      continue;
    }

    for (const auto& x : level) {
      EXPECT_LE(expected_min, x);
      EXPECT_LE(x, expected_max);
    }
  }
}

// Returns time it takes for the level estimator to decrease its level
// estimate by 'level_reduction_db'.
float TimeMsToDecreaseLevel(size_t samples_per_channel,
                            int num_channels,
                            float input_level_db,
                            float level_reduction_db) {
  const float input_level = DbfsToFloatS16(input_level_db);
  RTC_DCHECK_GT(level_reduction_db, 0);

  const VectorFloatFrame vectors_with_float_frame(
      num_channels, samples_per_channel, input_level);

  ApmDataDumper apm_data_dumper(0);
  FixedDigitalLevelEstimator level_estimator(samples_per_channel,
                                             &apm_data_dumper);

  // Give the LevelEstimator plenty of time to ramp up and stabilize
  float last_level = 0.f;
  for (int i = 0; i < 500; ++i) {
    const auto level_envelope =
        level_estimator.ComputeLevel(vectors_with_float_frame.view());
    last_level = *level_envelope.rbegin();
  }

  // Set input to 0.
  VectorFloatFrame vectors_with_zero_float_frame(num_channels,
                                                 samples_per_channel, 0);

  const float reduced_level_linear =
      DbfsToFloatS16(input_level_db - level_reduction_db);
  int sub_frames_until_level_reduction = 0;
  while (last_level > reduced_level_linear) {
    const auto level_envelope =
        level_estimator.ComputeLevel(vectors_with_zero_float_frame.view());
    for (const auto& v : level_envelope) {
      EXPECT_LT(v, last_level);
      sub_frames_until_level_reduction++;
      last_level = v;
      if (last_level <= reduced_level_linear) {
        break;
      }
    }
  }
  return static_cast<float>(sub_frames_until_level_reduction) *
         kFrameDurationMs / kSubFramesInFrame;
}
}  // namespace

TEST(GainController2FixedDigitalLevelEstimator, EstimatorShouldNotCrash) {
  TestLevelEstimator(SampleRateToDefaultChannelSize(8000u), 1, 0,
                     std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::max());
}

TEST(GainController2FixedDigitalLevelEstimator,
     EstimatorShouldEstimateConstantLevel) {
  TestLevelEstimator(SampleRateToDefaultChannelSize(10000u), 1, kInputLevel,
                     kInputLevel * 0.99, kInputLevel * 1.01);
}

TEST(GainController2FixedDigitalLevelEstimator,
     EstimatorShouldEstimateConstantLevelForManyChannels) {
  constexpr size_t num_channels = 10;
  TestLevelEstimator(SampleRateToDefaultChannelSize(20000u), num_channels,
                     kInputLevel, kInputLevel * 0.99, kInputLevel * 1.01);
}

TEST(GainController2FixedDigitalLevelEstimator, TimeToDecreaseForLowLevel) {
  constexpr float kLevelReductionDb = 25;
  constexpr float kInitialLowLevel = -40;
  constexpr float kExpectedTime = kLevelReductionDb * test::kDecayMs;

  const float time_to_decrease =
      TimeMsToDecreaseLevel(SampleRateToDefaultChannelSize(22000u), 1,
                            kInitialLowLevel, kLevelReductionDb);

  EXPECT_LE(kExpectedTime * 0.9, time_to_decrease);
  EXPECT_LE(time_to_decrease, kExpectedTime * 1.1);
}

TEST(GainController2FixedDigitalLevelEstimator,
     TimeToDecreaseForFullScaleLevel) {
  constexpr float kLevelReductionDb = 25;
  constexpr float kExpectedTime = kLevelReductionDb * test::kDecayMs;

  const float time_to_decrease = TimeMsToDecreaseLevel(
      SampleRateToDefaultChannelSize(26000u), 1, 0, kLevelReductionDb);

  EXPECT_LE(kExpectedTime * 0.9, time_to_decrease);
  EXPECT_LE(time_to_decrease, kExpectedTime * 1.1);
}

TEST(GainController2FixedDigitalLevelEstimator,
     TimeToDecreaseForMultipleChannels) {
  constexpr float kLevelReductionDb = 25;
  constexpr float kExpectedTime = kLevelReductionDb * test::kDecayMs;
  constexpr size_t kNumChannels = 10;

  const float time_to_decrease =
      TimeMsToDecreaseLevel(SampleRateToDefaultChannelSize(28000u),
                            kNumChannels, 0, kLevelReductionDb);

  EXPECT_LE(kExpectedTime * 0.9, time_to_decrease);
  EXPECT_LE(time_to_decrease, kExpectedTime * 1.1);
}

}  // namespace webrtc
