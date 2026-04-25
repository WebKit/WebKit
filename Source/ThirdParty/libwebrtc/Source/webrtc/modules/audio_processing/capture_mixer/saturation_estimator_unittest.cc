/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/saturation_estimator.h"

#include <algorithm>
#include <array>
#include <tuple>
#include <vector>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int GetNumSamplesPerChannel(int sample_rate_hz) {
  constexpr int kFrameSizeMs = 10;
  return sample_rate_hz * kFrameSizeMs / 1000;
}

constexpr float kThresholdForActiveAudio = 100.0f;
constexpr float kThresholdForSaturatedAudio = 32000.0f;

}  // namespace

class SaturationDetectorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, float>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiDcLevels,
    SaturationDetectorParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(0.0f, -5.1f, 10.7f)));

TEST_P(SaturationDetectorParametrizedTest, VerifyLowValueActivityDetection) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};

  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);
  constexpr float kSampleValue = kThresholdForActiveAudio - 3.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    const std::vector<float> channel(num_samples_per_channel,
                                     dc_level + sign * kSampleValue);

    constexpr int kNumFramesToAnalyze = 10;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
      ArrayView<const int, 2> num_frames_since_activity =
          estimator.GetNumFramesSinceActivity();
      EXPECT_EQ(num_frames_since_activity[0], k + 1);
      EXPECT_EQ(num_frames_since_activity[1], k + 1);
    }
  }
}

TEST_P(SaturationDetectorParametrizedTest,
       VerifySufficientlyLargeValueActivityDetection) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValue = kThresholdForActiveAudio + 1.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    const std::vector<float> channel(num_samples_per_channel,
                                     dc_level + sign * kSampleValue);

    constexpr int kNumFramesToAnalyze = 10;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
      ArrayView<const int, 2> num_frames_since_activity =
          estimator.GetNumFramesSinceActivity();
      EXPECT_EQ(num_frames_since_activity[0], 0);
      EXPECT_EQ(num_frames_since_activity[1], 0);
    }
  }
}

TEST_P(SaturationDetectorParametrizedTest,
       VerifyActivityDetectionTransientBehavior) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValueNoActivity = kThresholdForActiveAudio - 3.0f;
  constexpr float kSampleValueActivity = kThresholdForActiveAudio + 3.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);

    {
      const std::vector<float> channel(
          num_samples_per_channel, dc_level + sign * kSampleValueNoActivity);
      estimator.Update(channel, channel, dc_levels);
    }

    ArrayView<const int, 2> num_frames_since_activity =
        estimator.GetNumFramesSinceActivity();
    EXPECT_EQ(num_frames_since_activity[0], 1);
    EXPECT_EQ(num_frames_since_activity[1], 1);

    {
      const std::vector<float> channel(num_samples_per_channel,
                                       dc_level + sign * kSampleValueActivity);
      estimator.Update(channel, channel, dc_levels);
    }

    num_frames_since_activity = estimator.GetNumFramesSinceActivity();
    EXPECT_EQ(num_frames_since_activity[0], 0);
    EXPECT_EQ(num_frames_since_activity[1], 0);

    constexpr int kNumFramesToAnalyze = 10;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      const std::vector<float> channel(
          num_samples_per_channel, dc_level + sign * kSampleValueNoActivity);
      estimator.Update(channel, channel, dc_levels);
      num_frames_since_activity = estimator.GetNumFramesSinceActivity();
      EXPECT_EQ(num_frames_since_activity[0], k + 1);
      EXPECT_EQ(num_frames_since_activity[1], k + 1);
    }
  }
}

TEST_P(SaturationDetectorParametrizedTest,
       VerifySaturationDecectionForNonSaturatingLevels) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValueNoSaturation = kThresholdForSaturatedAudio - 2.0f;

  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    const std::vector<float> channel(
        num_samples_per_channel, dc_level + sign * kSampleValueNoSaturation);

    constexpr int kNumFramesToAnalyze = 10;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
      ArrayView<const float, 2> saturation_factors =
          estimator.GetSaturationFactors();
      EXPECT_EQ(saturation_factors[0], 0.0f);
      EXPECT_EQ(saturation_factors[1], 0.0f);
    }
  }
}

TEST_P(SaturationDetectorParametrizedTest,
       VerifySaturationDetectionForSaturatingLevels) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValueSaturation = kThresholdForSaturatedAudio + 2.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    const std::vector<float> channel(num_samples_per_channel,
                                     dc_level + sign * kSampleValueSaturation);

    constexpr int kNumFramesToAnalyze = 10;
    std::vector<float> previous_factors(2, 0.0f);
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
      ArrayView<const float, 2> saturation_factors =
          estimator.GetSaturationFactors();
      EXPECT_GT(saturation_factors[0], 0.0f);
      EXPECT_GT(saturation_factors[1], 0.0f);
      EXPECT_GT(saturation_factors[0], previous_factors[0]);
      EXPECT_GT(saturation_factors[1], previous_factors[1]);
      std::copy(saturation_factors.begin(), saturation_factors.end(),
                previous_factors.begin());
    }
  }
}

TEST_P(SaturationDetectorParametrizedTest,
       VerifySaturationFactorComputationForSaturatingLevels) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValueSaturation = kThresholdForSaturatedAudio + 2.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    const std::vector<float> channel(num_samples_per_channel,
                                     dc_level + sign * kSampleValueSaturation);

    constexpr int kNumFramesToAnalyze = 100;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
    }
    ArrayView<const float, 2> saturation_factors =
        estimator.GetSaturationFactors();
    EXPECT_GT(saturation_factors[0], 0.99f);
    EXPECT_GT(saturation_factors[1], 0.99f);
  }
}

TEST_P(SaturationDetectorParametrizedTest, VerifyDecayingSaturationFactor) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  const std::array<float, 2> dc_levels = {dc_level, dc_level};
  const int num_samples_per_channel = GetNumSamplesPerChannel(sample_rate_hz);

  constexpr float kSampleValueNoSaturation = kThresholdForSaturatedAudio - 2.0f;
  constexpr float kSampleValueSaturation = kThresholdForSaturatedAudio + 2.0f;
  for (int sign = -1; sign <= 1; sign += 2) {
    SaturationEstimator estimator(num_samples_per_channel);
    std::vector<float> previous_factors(2, 0.0f);
    {
      const std::vector<float> channel(
          num_samples_per_channel, dc_level + sign * kSampleValueSaturation);
      estimator.Update(channel, channel, dc_levels);
    }
    ArrayView<const float, 2> saturation_factors =
        estimator.GetSaturationFactors();
    EXPECT_GT(saturation_factors[0], 0.0f);
    EXPECT_GT(saturation_factors[1], 0.0f);
    std::copy(saturation_factors.begin(), saturation_factors.end(),
              previous_factors.begin());

    const std::vector<float> channel(
        num_samples_per_channel, dc_level + sign * kSampleValueNoSaturation);
    constexpr int kNumFramesToAnalyze = 10;
    for (int k = 0; k < kNumFramesToAnalyze; ++k) {
      estimator.Update(channel, channel, dc_levels);
      saturation_factors = estimator.GetSaturationFactors();
      EXPECT_LT(saturation_factors[0], previous_factors[0]);
      EXPECT_LT(saturation_factors[1], previous_factors[1]);
      std::copy(saturation_factors.begin(), saturation_factors.end(),
                previous_factors.begin());
    }
  }
}

}  // namespace webrtc
