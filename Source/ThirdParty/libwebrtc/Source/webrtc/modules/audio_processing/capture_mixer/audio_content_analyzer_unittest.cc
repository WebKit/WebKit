/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/audio_content_analyzer.h"

#include <cstddef>
#include <tuple>
#include <vector>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::vector<float> CreateAndPopulateChannel(float amplitude,
                                            float dc_level,
                                            int num_samples_per_channel) {
  std::vector<float> x(num_samples_per_channel);

  for (size_t k = 0; k < x.size(); ++k) {
    x[k] = amplitude * (k % 2 == 0 ? 1 : -1) + dc_level;
  }
  return x;
}

}  // namespace

class AudioContentAnalyzerParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, float, float>> {};

INSTANTIATE_TEST_SUITE_P(
    MultiParameters,
    AudioContentAnalyzerParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(0.0f, -5.1f, 10.7f, 200.0f),
                       ::testing::Values(50.0f, 1000.0f)));

TEST_P(AudioContentAnalyzerParametrizedTest,
       VerifyReliableEstimatesAndRunAllCode) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const float dc_level = std::get<1>(GetParam());
  const float amplitude = std::get<2>(GetParam());

  constexpr int kFrameSizeMs = 10;
  const int num_samples_per_channel = sample_rate_hz * kFrameSizeMs / 1000;

  AudioContentAnalyzer analyzer(num_samples_per_channel);

  const std::vector<float> channel =
      CreateAndPopulateChannel(amplitude, dc_level, num_samples_per_channel);

  bool reliable_estimates = analyzer.Analyze(channel, channel);
  EXPECT_FALSE(reliable_estimates);

  constexpr int kNumFramesToAnalyze = 400;
  for (int k = 0; k < kNumFramesToAnalyze; ++k) {
    reliable_estimates = analyzer.Analyze(channel, channel);
  }
  EXPECT_TRUE(reliable_estimates);
}

}  // namespace webrtc
