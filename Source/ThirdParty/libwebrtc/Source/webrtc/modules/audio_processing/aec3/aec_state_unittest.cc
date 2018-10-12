/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec_state.h"

#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "test/gtest.h"

namespace webrtc {

// Verify the general functionality of AecState
TEST(AecState, NormalUsage) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  AecState state(config);
  absl::optional<DelayEstimate> delay_estimate =
      DelayEstimate(DelayEstimate::Quality::kRefined, 10);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));
  std::array<float, kFftLengthBy2Plus1> E2_main = {};
  std::array<float, kFftLengthBy2Plus1> Y2 = {};
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  SubtractorOutput output;
  output.Reset();
  std::array<float, kBlockSize> y;
  Aec3Fft fft;
  output.s_main.fill(100.f);
  output.e_main.fill(100.f);
  y.fill(1000.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>>
      converged_filter_frequency_response(10);
  for (auto& v : converged_filter_frequency_response) {
    v.fill(0.01f);
  }
  std::vector<std::array<float, kFftLengthBy2Plus1>>
      diverged_filter_frequency_response = converged_filter_frequency_response;
  converged_filter_frequency_response[2].fill(100.f);
  converged_filter_frequency_response[2][0] = 1.f;

  std::vector<float> impulse_response(
      GetTimeDomainLength(config.filter.main.length_blocks), 0.f);

  // Verify that linear AEC usability is true when the filter is converged
  std::fill(x[0].begin(), x[0].end(), 101.f);
  for (int k = 0; k < 3000; ++k) {
    render_delay_buffer->Insert(x);
    output.UpdatePowers(y);
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_main, Y2, output, y);
  }
  EXPECT_TRUE(state.UsableLinearEstimate());

  // Verify that linear AEC usability becomes false after an echo path change is
  // reported
  output.UpdatePowers(y);
  state.HandleEchoPathChange(EchoPathVariability(
      false, EchoPathVariability::DelayAdjustment::kBufferReadjustment, false));
  state.Update(delay_estimate, converged_filter_frequency_response,
               impulse_response, *render_delay_buffer->GetRenderBuffer(),
               E2_main, Y2, output, y);
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that the active render detection works as intended.
  std::fill(x[0].begin(), x[0].end(), 101.f);
  render_delay_buffer->Insert(x);
  output.UpdatePowers(y);
  state.HandleEchoPathChange(EchoPathVariability(
      true, EchoPathVariability::DelayAdjustment::kNewDetectedDelay, false));
  state.Update(delay_estimate, converged_filter_frequency_response,
               impulse_response, *render_delay_buffer->GetRenderBuffer(),
               E2_main, Y2, output, y);
  EXPECT_FALSE(state.ActiveRender());

  for (int k = 0; k < 1000; ++k) {
    render_delay_buffer->Insert(x);
    output.UpdatePowers(y);
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_main, Y2, output, y);
  }
  EXPECT_TRUE(state.ActiveRender());

  // Verify that the ERL is properly estimated
  for (auto& x_k : x) {
    x_k = std::vector<float>(kBlockSize, 0.f);
  }

  x[0][0] = 5000.f;
  for (size_t k = 0;
       k < render_delay_buffer->GetRenderBuffer()->GetFftBuffer().size(); ++k) {
    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();
  }

  Y2.fill(10.f * 10000.f * 10000.f);
  for (size_t k = 0; k < 1000; ++k) {
    output.UpdatePowers(y);
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_main, Y2, output, y);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  const std::array<float, kFftLengthBy2Plus1>& erl = state.Erl();
  EXPECT_EQ(erl[0], erl[1]);
  for (size_t k = 1; k < erl.size() - 1; ++k) {
    EXPECT_NEAR(k % 2 == 0 ? 10.f : 1000.f, erl[k], 0.1);
  }
  EXPECT_EQ(erl[erl.size() - 2], erl[erl.size() - 1]);

  // Verify that the ERLE is properly estimated
  E2_main.fill(1.f * 10000.f * 10000.f);
  Y2.fill(10.f * E2_main[0]);
  for (size_t k = 0; k < 1000; ++k) {
    output.UpdatePowers(y);
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_main, Y2, output, y);
  }
  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    // Note that the render spectrum is built so it does not have energy in the
    // odd bands but just in the even bands.
    const auto& erle = state.Erle();
    EXPECT_EQ(erle[0], erle[1]);
    constexpr size_t kLowFrequencyLimit = 32;
    for (size_t k = 2; k < kLowFrequencyLimit; k = k + 2) {
      EXPECT_NEAR(4.f, erle[k], 0.1);
    }
    for (size_t k = kLowFrequencyLimit; k < erle.size() - 1; k = k + 2) {
      EXPECT_NEAR(1.5f, erle[k], 0.1);
    }
    EXPECT_EQ(erle[erle.size() - 2], erle[erle.size() - 1]);
  }

  E2_main.fill(1.f * 10000.f * 10000.f);
  Y2.fill(5.f * E2_main[0]);
  for (size_t k = 0; k < 1000; ++k) {
    output.UpdatePowers(y);
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_main, Y2, output, y);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    const auto& erle = state.Erle();
    EXPECT_EQ(erle[0], erle[1]);
    constexpr size_t kLowFrequencyLimit = 32;
    for (size_t k = 1; k < kLowFrequencyLimit; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 4.f : 1.f, erle[k], 0.1);
    }
    for (size_t k = kLowFrequencyLimit; k < erle.size() - 1; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 1.5f : 1.f, erle[k], 0.1);
    }
    EXPECT_EQ(erle[erle.size() - 2], erle[erle.size() - 1]);
  }
}

// Verifies the delay for a converged filter is correctly identified.
TEST(AecState, ConvergedFilterDelay) {
  constexpr int kFilterLengthBlocks = 10;
  EchoCanceller3Config config;
  AecState state(config);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));
  absl::optional<DelayEstimate> delay_estimate;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  SubtractorOutput output;
  output.Reset();
  std::array<float, kBlockSize> y;
  output.s_main.fill(100.f);
  x.fill(0.f);
  y.fill(0.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(
      kFilterLengthBlocks);
  for (auto& v : frequency_response) {
    v.fill(0.01f);
  }

  std::vector<float> impulse_response(
      GetTimeDomainLength(config.filter.main.length_blocks), 0.f);

  // Verify that the filter delay for a converged filter is properly identified.
  for (int k = 0; k < kFilterLengthBlocks; ++k) {
    std::fill(impulse_response.begin(), impulse_response.end(), 0.f);
    impulse_response[k * kBlockSize + 1] = 1.f;

    state.HandleEchoPathChange(echo_path_variability);
    output.UpdatePowers(y);
    state.Update(delay_estimate, frequency_response, impulse_response,
                 *render_delay_buffer->GetRenderBuffer(), E2_main, Y2, output,
                 y);
  }
}

}  // namespace webrtc
