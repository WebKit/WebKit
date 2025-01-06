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
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void RunNormalUsageTest(size_t num_render_channels,
                        size_t num_capture_channels) {
  // TODO(bugs.webrtc.org/10913): Test with different content in different
  // channels.
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  AecState state(config, num_capture_channels);
  std::optional<DelayEstimate> delay_estimate =
      DelayEstimate(DelayEstimate::Quality::kRefined, 10);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  Block x(kNumBands, num_render_channels);
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  std::vector<std::array<float, kBlockSize>> y(num_capture_channels);
  std::vector<SubtractorOutput> subtractor_output(num_capture_channels);
  for (size_t ch = 0; ch < num_capture_channels; ++ch) {
    subtractor_output[ch].Reset();
    subtractor_output[ch].s_refined.fill(100.f);
    subtractor_output[ch].e_refined.fill(100.f);
    y[ch].fill(1000.f);
    E2_refined[ch].fill(0.f);
    Y2[ch].fill(0.f);
  }
  Aec3Fft fft;
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>>
      converged_filter_frequency_response(
          num_capture_channels,
          std::vector<std::array<float, kFftLengthBy2Plus1>>(10));
  for (auto& v_ch : converged_filter_frequency_response) {
    for (auto& v : v_ch) {
      v.fill(0.01f);
    }
  }
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>>
      diverged_filter_frequency_response = converged_filter_frequency_response;
  converged_filter_frequency_response[0][2].fill(100.f);
  converged_filter_frequency_response[0][2][0] = 1.f;
  std::vector<std::vector<float>> impulse_response(
      num_capture_channels,
      std::vector<float>(
          GetTimeDomainLength(config.filter.refined.length_blocks), 0.f));

  // Verify that linear AEC usability is true when the filter is converged
  for (size_t band = 0; band < kNumBands; ++band) {
    for (size_t ch = 0; ch < num_render_channels; ++ch) {
      std::fill(x.begin(band, ch), x.end(band, ch), 101.f);
    }
  }
  for (int k = 0; k < 3000; ++k) {
    render_delay_buffer->Insert(x);
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      subtractor_output[ch].ComputeMetrics(y[ch]);
    }
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_refined, Y2, subtractor_output);
  }
  EXPECT_TRUE(state.UsableLinearEstimate());

  // Verify that linear AEC usability becomes false after an echo path
  // change is reported
  for (size_t ch = 0; ch < num_capture_channels; ++ch) {
    subtractor_output[ch].ComputeMetrics(y[ch]);
  }
  state.HandleEchoPathChange(EchoPathVariability(
      false, EchoPathVariability::DelayAdjustment::kNewDetectedDelay, false));
  state.Update(delay_estimate, converged_filter_frequency_response,
               impulse_response, *render_delay_buffer->GetRenderBuffer(),
               E2_refined, Y2, subtractor_output);
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that the active render detection works as intended.
  for (size_t ch = 0; ch < num_render_channels; ++ch) {
    std::fill(x.begin(0, ch), x.end(0, ch), 101.f);
  }
  render_delay_buffer->Insert(x);
  for (size_t ch = 0; ch < num_capture_channels; ++ch) {
    subtractor_output[ch].ComputeMetrics(y[ch]);
  }
  state.HandleEchoPathChange(EchoPathVariability(
      true, EchoPathVariability::DelayAdjustment::kNewDetectedDelay, false));
  state.Update(delay_estimate, converged_filter_frequency_response,
               impulse_response, *render_delay_buffer->GetRenderBuffer(),
               E2_refined, Y2, subtractor_output);
  EXPECT_FALSE(state.ActiveRender());

  for (int k = 0; k < 1000; ++k) {
    render_delay_buffer->Insert(x);
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      subtractor_output[ch].ComputeMetrics(y[ch]);
    }
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_refined, Y2, subtractor_output);
  }
  EXPECT_TRUE(state.ActiveRender());

  // Verify that the ERL is properly estimated
  for (int band = 0; band < x.NumBands(); ++band) {
    for (int channel = 0; channel < x.NumChannels(); ++channel) {
      std::fill(x.begin(band, channel), x.end(band, channel), 0.0f);
    }
  }

  for (size_t ch = 0; ch < num_render_channels; ++ch) {
    x.View(/*band=*/0, ch)[0] = 5000.f;
  }
  for (size_t k = 0;
       k < render_delay_buffer->GetRenderBuffer()->GetFftBuffer().size(); ++k) {
    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();
  }

  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(10.f * 10000.f * 10000.f);
  }
  for (size_t k = 0; k < 1000; ++k) {
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      subtractor_output[ch].ComputeMetrics(y[ch]);
    }
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_refined, Y2, subtractor_output);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  const std::array<float, kFftLengthBy2Plus1>& erl = state.Erl();
  EXPECT_EQ(erl[0], erl[1]);
  for (size_t k = 1; k < erl.size() - 1; ++k) {
    EXPECT_NEAR(k % 2 == 0 ? 10.f : 1000.f, erl[k], 0.1);
  }
  EXPECT_EQ(erl[erl.size() - 2], erl[erl.size() - 1]);

  // Verify that the ERLE is properly estimated
  for (auto& E2_refined_ch : E2_refined) {
    E2_refined_ch.fill(1.f * 10000.f * 10000.f);
  }
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(10.f * E2_refined[0][0]);
  }
  for (size_t k = 0; k < 1000; ++k) {
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      subtractor_output[ch].ComputeMetrics(y[ch]);
    }
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_refined, Y2, subtractor_output);
  }
  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    // Note that the render spectrum is built so it does not have energy in
    // the odd bands but just in the even bands.
    const auto& erle = state.Erle(/*onset_compensated=*/true)[0];
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
  for (auto& E2_refined_ch : E2_refined) {
    E2_refined_ch.fill(1.f * 10000.f * 10000.f);
  }
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(5.f * E2_refined[0][0]);
  }
  for (size_t k = 0; k < 1000; ++k) {
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      subtractor_output[ch].ComputeMetrics(y[ch]);
    }
    state.Update(delay_estimate, converged_filter_frequency_response,
                 impulse_response, *render_delay_buffer->GetRenderBuffer(),
                 E2_refined, Y2, subtractor_output);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    const auto& erle = state.Erle(/*onset_compensated=*/true)[0];
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

}  // namespace

class AecStateMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         AecStateMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 8),
                                            ::testing::Values(1, 2, 8)));

// Verify the general functionality of AecState
TEST_P(AecStateMultiChannel, NormalUsage) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  RunNormalUsageTest(num_render_channels, num_capture_channels);
}

// Verifies the delay for a converged filter is correctly identified.
TEST(AecState, ConvergedFilterDelay) {
  constexpr int kFilterLengthBlocks = 10;
  constexpr size_t kNumCaptureChannels = 1;
  EchoCanceller3Config config;
  AecState state(config, kNumCaptureChannels);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 48000, 1));
  std::optional<DelayEstimate> delay_estimate;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(
      false, EchoPathVariability::DelayAdjustment::kNone, false);
  std::vector<SubtractorOutput> subtractor_output(kNumCaptureChannels);
  for (auto& output : subtractor_output) {
    output.Reset();
    output.s_refined.fill(100.f);
  }
  std::array<float, kBlockSize> y;
  x.fill(0.f);
  y.fill(0.f);

  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>>
      frequency_response(kNumCaptureChannels,
                         std::vector<std::array<float, kFftLengthBy2Plus1>>(
                             kFilterLengthBlocks));
  for (auto& v_ch : frequency_response) {
    for (auto& v : v_ch) {
      v.fill(0.01f);
    }
  }

  std::vector<std::vector<float>> impulse_response(
      kNumCaptureChannels,
      std::vector<float>(
          GetTimeDomainLength(config.filter.refined.length_blocks), 0.f));

  // Verify that the filter delay for a converged filter is properly
  // identified.
  for (int k = 0; k < kFilterLengthBlocks; ++k) {
    for (auto& ir : impulse_response) {
      std::fill(ir.begin(), ir.end(), 0.f);
      ir[k * kBlockSize + 1] = 1.f;
    }

    state.HandleEchoPathChange(echo_path_variability);
    subtractor_output[0].ComputeMetrics(y);
    state.Update(delay_estimate, frequency_response, impulse_response,
                 *render_delay_buffer->GetRenderBuffer(), E2_refined, Y2,
                 subtractor_output);
  }
}

}  // namespace webrtc
