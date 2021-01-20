/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/residual_echo_estimator.h"

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

class ResidualEchoEstimatorMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         ResidualEchoEstimatorMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 4),
                                            ::testing::Values(1, 2, 4)));

TEST_P(ResidualEchoEstimatorMultiChannel, BasicTest) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  EchoCanceller3Config config;
  ResidualEchoEstimator estimator(config, num_render_channels);
  AecState aec_state(config, num_capture_channels);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));

  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2_linear(
      num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(num_capture_channels);
  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     num_render_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2(
      num_capture_channels,
      std::vector<std::array<float, kFftLengthBy2Plus1>>(10));
  Random random_generator(42U);
  std::vector<SubtractorOutput> output(num_capture_channels);
  std::array<float, kBlockSize> y;
  absl::optional<DelayEstimate> delay_estimate;

  for (auto& H2_ch : H2) {
    for (auto& H2_k : H2_ch) {
      H2_k.fill(0.01f);
    }
    H2_ch[2].fill(10.f);
    H2_ch[2][0] = 0.1f;
  }

  std::vector<std::vector<float>> h(
      num_capture_channels,
      std::vector<float>(
          GetTimeDomainLength(config.filter.refined.length_blocks), 0.f));

  for (auto& subtractor_output : output) {
    subtractor_output.Reset();
    subtractor_output.s_refined.fill(100.f);
  }
  y.fill(0.f);

  constexpr float kLevel = 10.f;
  for (auto& E2_refined_ch : E2_refined) {
    E2_refined_ch.fill(kLevel);
  }
  S2_linear[0].fill(kLevel);
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(kLevel);
  }

  for (int k = 0; k < 1993; ++k) {
    RandomizeSampleVector(&random_generator, x[0][0]);
    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();

    aec_state.Update(delay_estimate, H2, h,
                     *render_delay_buffer->GetRenderBuffer(), E2_refined, Y2,
                     output);

    estimator.Estimate(aec_state, *render_delay_buffer->GetRenderBuffer(),
                       S2_linear, Y2, R2);
  }
}

}  // namespace webrtc
