/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/energy_estimator.h"

#include <array>
#include <cstddef>
#include <tuple>
#include <vector>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::vector<float> CreateAndPopulateChannel(int sample_rate_hz,
                                            float amplitude,
                                            float dc_level) {
  constexpr int kFrameSizeMs = 10;
  std::vector<float> x(sample_rate_hz * kFrameSizeMs / 1000);

  for (size_t k = 0; k < x.size(); ++k) {
    x[k] = amplitude * (k % 2 == 0 ? 1 : -1) + dc_level;
  }
  return x;
}

}  // namespace

class AverageEnergyEstimatorParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, float>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiParameters,
    AverageEnergyEstimatorParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(0.0f, -5.1f, 10.7f)));

TEST_P(AverageEnergyEstimatorParametrizedTest, VerifyEstimates) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());

  constexpr float kAmplitudeChannel0 = 200.0f;
  constexpr float kAmplitudeChannel1 = 1000.0f;
  const std::vector<float> channel0 =
      CreateAndPopulateChannel(sample_rate_hz, kAmplitudeChannel0, dc_level);
  const std::vector<float> channel1 =
      CreateAndPopulateChannel(sample_rate_hz, kAmplitudeChannel1, dc_level);

  AverageEnergyEstimator estimator;

  std::array<float, 2> dc_levels = {dc_level, dc_level};
  constexpr int kNumFramesToAnalyze = 2000;
  for (int k = 0; k < kNumFramesToAnalyze; ++k) {
    estimator.Update(channel0, channel1, dc_levels);
  }

  ArrayView<const float, 2> energies = estimator.GetChannelEnergies();

  constexpr float kToleranceError = 0.0001f;
  const float expected_energy_channel_0 =
      kAmplitudeChannel0 * kAmplitudeChannel0 * channel0.size();
  const float expected_energy_channel_1 =
      kAmplitudeChannel1 * kAmplitudeChannel1 * channel0.size();
  EXPECT_NEAR(energies[0], expected_energy_channel_0,
              expected_energy_channel_0 * kToleranceError);
  EXPECT_NEAR(energies[1], expected_energy_channel_1,
              expected_energy_channel_1 * kToleranceError);
}

}  // namespace webrtc
