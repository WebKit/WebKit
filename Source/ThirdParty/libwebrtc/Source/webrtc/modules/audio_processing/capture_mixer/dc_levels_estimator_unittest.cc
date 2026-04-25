/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/dc_levels_estimator.h"

#include <math.h>

#include <algorithm>
#include <numbers>
#include <tuple>
#include <vector>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void PopulateStereoChannelsWithSinusoid(int sample_rate_hz,
                                        float dc_level,
                                        int& generated_sample_counter,
                                        ArrayView<float> channel0,
                                        ArrayView<float> channel1) {
  constexpr float kPi = std::numbers::pi;
  constexpr float kApmlitudeScaling = 1000.0f;
  constexpr float kBaseSinusoidFrequencyHz = 100.0f;

  int channel_index = 0;
  for (auto& channel : {channel0, channel1}) {
    for (float& channel_sample : channel) {
      ++generated_sample_counter;

      channel_sample =
          channel_index * kApmlitudeScaling *
              sin(2.0f * kPi * channel_index * kBaseSinusoidFrequencyHz *
                  generated_sample_counter / sample_rate_hz) +
          dc_level;
    }
    channel_index++;
  }
}

}  // namespace

class DcLevelsEstimatorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, float>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiParameters,
    DcLevelsEstimatorParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(0.0f, -5.1f, 10.7f, 200.0f)));

TEST_P(DcLevelsEstimatorParametrizedTest, VerifyEstimates) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float true_dc_level = std::get<1>(GetParam());

  constexpr int kFrameSizeMs = 10;
  const int num_samples_per_channel = sample_rate_hz * kFrameSizeMs / 1000;

  DcLevelsEstimator estimator(num_samples_per_channel);
  int generated_sample_counter = 0;

  std::vector<float> channel0(num_samples_per_channel);
  std::vector<float> channel1(num_samples_per_channel);
  for (int i = 0; i < 200; ++i) {
    PopulateStereoChannelsWithSinusoid(sample_rate_hz, true_dc_level,
                                       generated_sample_counter, channel0,
                                       channel1);

    estimator.Update(channel0, channel1);
  }

  ArrayView<const float> levels = estimator.GetLevels();

  for (const float level : levels) {
    EXPECT_NEAR(level, true_dc_level,
                std::max(fabs(true_dc_level) * 0.01f, 0.01f));
  }
}

}  // namespace webrtc
