/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/refined_filter_update_gain.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "modules/audio_processing/aec3/adaptive_fir_filter_erl.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/coarse_filter_update_gain.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/aec3/render_signal_analyzer.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

// Method for performing the simulations needed to test the refined filter
// update gain functionality.
void RunFilterUpdateTest(int num_blocks_to_process,
                         size_t delay_samples,
                         int filter_length_blocks,
                         const std::vector<int>& blocks_with_echo_path_changes,
                         const std::vector<int>& blocks_with_saturation,
                         bool use_silent_render_in_second_half,
                         std::array<float, kBlockSize>* e_last_block,
                         std::array<float, kBlockSize>* y_last_block,
                         FftData* G_last_block) {
  ApmDataDumper data_dumper(42);
  Aec3Optimization optimization = DetectOptimization();
  constexpr size_t kNumRenderChannels = 1;
  constexpr size_t kNumCaptureChannels = 1;
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  EchoCanceller3Config config;
  config.filter.refined.length_blocks = filter_length_blocks;
  config.filter.coarse.length_blocks = filter_length_blocks;
  AdaptiveFirFilter refined_filter(
      config.filter.refined.length_blocks, config.filter.refined.length_blocks,
      config.filter.config_change_duration_blocks, kNumRenderChannels,
      optimization, &data_dumper);
  AdaptiveFirFilter coarse_filter(
      config.filter.coarse.length_blocks, config.filter.coarse.length_blocks,
      config.filter.config_change_duration_blocks, kNumRenderChannels,
      optimization, &data_dumper);
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2(
      kNumCaptureChannels, std::vector<std::array<float, kFftLengthBy2Plus1>>(
                               refined_filter.max_filter_size_partitions(),
                               std::array<float, kFftLengthBy2Plus1>()));
  for (auto& H2_ch : H2) {
    for (auto& H2_k : H2_ch) {
      H2_k.fill(0.f);
    }
  }
  std::vector<std::vector<float>> h(
      kNumCaptureChannels,
      std::vector<float>(
          GetTimeDomainLength(refined_filter.max_filter_size_partitions()),
          0.f));

  Aec3Fft fft;
  std::array<float, kBlockSize> x_old;
  x_old.fill(0.f);
  CoarseFilterUpdateGain coarse_gain(
      config.filter.coarse, config.filter.config_change_duration_blocks);
  RefinedFilterUpdateGain refined_gain(
      config.filter.refined, config.filter.config_change_duration_blocks);
  Random random_generator(42U);
  Block x(kNumBands, kNumRenderChannels);
  std::vector<float> y(kBlockSize, 0.f);
  config.delay.default_delay = 1;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, kNumRenderChannels));
  AecState aec_state(config, kNumCaptureChannels);
  RenderSignalAnalyzer render_signal_analyzer(config);
  absl::optional<DelayEstimate> delay_estimate;
  std::array<float, kFftLength> s_scratch;
  std::array<float, kBlockSize> s;
  FftData S;
  FftData G;
  std::vector<SubtractorOutput> output(kNumCaptureChannels);
  for (auto& subtractor_output : output) {
    subtractor_output.Reset();
  }
  FftData& E_refined = output[0].E_refined;
  FftData E_coarse;
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      kNumCaptureChannels);
  std::array<float, kBlockSize>& e_refined = output[0].e_refined;
  std::array<float, kBlockSize>& e_coarse = output[0].e_coarse;
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(0.f);
  }

  constexpr float kScale = 1.0f / kFftLengthBy2;

  DelayBuffer<float> delay_buffer(delay_samples);
  for (int k = 0; k < num_blocks_to_process; ++k) {
    // Handle echo path changes.
    if (std::find(blocks_with_echo_path_changes.begin(),
                  blocks_with_echo_path_changes.end(),
                  k) != blocks_with_echo_path_changes.end()) {
      refined_filter.HandleEchoPathChange();
    }

    // Handle saturation.
    const bool saturation =
        std::find(blocks_with_saturation.begin(), blocks_with_saturation.end(),
                  k) != blocks_with_saturation.end();

    // Create the render signal.
    if (use_silent_render_in_second_half && k > num_blocks_to_process / 2) {
      for (int band = 0; band < x.NumBands(); ++band) {
        for (int channel = 0; channel < x.NumChannels(); ++channel) {
          std::fill(x.begin(band, channel), x.end(band, channel), 0.f);
        }
      }
    } else {
      for (int band = 0; band < x.NumChannels(); ++band) {
        for (int channel = 0; channel < x.NumChannels(); ++channel) {
          RandomizeSampleVector(&random_generator, x.View(band, channel));
        }
      }
    }
    delay_buffer.Delay(x.View(/*band=*/0, /*channel=*/0), y);

    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();

    render_signal_analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                                  aec_state.MinDirectPathFilterDelay());

    // Apply the refined filter.
    refined_filter.Filter(*render_delay_buffer->GetRenderBuffer(), &S);
    fft.Ifft(S, &s_scratch);
    std::transform(y.begin(), y.end(), s_scratch.begin() + kFftLengthBy2,
                   e_refined.begin(),
                   [&](float a, float b) { return a - b * kScale; });
    std::for_each(e_refined.begin(), e_refined.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
    fft.ZeroPaddedFft(e_refined, Aec3Fft::Window::kRectangular, &E_refined);
    for (size_t k = 0; k < kBlockSize; ++k) {
      s[k] = kScale * s_scratch[k + kFftLengthBy2];
    }

    // Apply the coarse filter.
    coarse_filter.Filter(*render_delay_buffer->GetRenderBuffer(), &S);
    fft.Ifft(S, &s_scratch);
    std::transform(y.begin(), y.end(), s_scratch.begin() + kFftLengthBy2,
                   e_coarse.begin(),
                   [&](float a, float b) { return a - b * kScale; });
    std::for_each(e_coarse.begin(), e_coarse.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
    fft.ZeroPaddedFft(e_coarse, Aec3Fft::Window::kRectangular, &E_coarse);

    // Compute spectra for future use.
    E_refined.Spectrum(Aec3Optimization::kNone, output[0].E2_refined);
    E_coarse.Spectrum(Aec3Optimization::kNone, output[0].E2_coarse);

    // Adapt the coarse filter.
    std::array<float, kFftLengthBy2Plus1> render_power;
    render_delay_buffer->GetRenderBuffer()->SpectralSum(
        coarse_filter.SizePartitions(), &render_power);
    coarse_gain.Compute(render_power, render_signal_analyzer, E_coarse,
                        coarse_filter.SizePartitions(), saturation, &G);
    coarse_filter.Adapt(*render_delay_buffer->GetRenderBuffer(), G);

    // Adapt the refined filter
    render_delay_buffer->GetRenderBuffer()->SpectralSum(
        refined_filter.SizePartitions(), &render_power);

    std::array<float, kFftLengthBy2Plus1> erl;
    ComputeErl(optimization, H2[0], erl);
    refined_gain.Compute(render_power, render_signal_analyzer, output[0], erl,
                         refined_filter.SizePartitions(), saturation, false,
                         &G);
    refined_filter.Adapt(*render_delay_buffer->GetRenderBuffer(), G, &h[0]);

    // Update the delay.
    aec_state.HandleEchoPathChange(EchoPathVariability(
        false, EchoPathVariability::DelayAdjustment::kNone, false));
    refined_filter.ComputeFrequencyResponse(&H2[0]);
    std::copy(output[0].E2_refined.begin(), output[0].E2_refined.end(),
              E2_refined[0].begin());
    aec_state.Update(delay_estimate, H2, h,
                     *render_delay_buffer->GetRenderBuffer(), E2_refined, Y2,
                     output);
  }

  std::copy(e_refined.begin(), e_refined.end(), e_last_block->begin());
  std::copy(y.begin(), y.end(), y_last_block->begin());
  std::copy(G.re.begin(), G.re.end(), G_last_block->re.begin());
  std::copy(G.im.begin(), G.im.end(), G_last_block->im.begin());
}

std::string ProduceDebugText(int filter_length_blocks) {
  rtc::StringBuilder ss;
  ss << "Length: " << filter_length_blocks;
  return ss.Release();
}

std::string ProduceDebugText(size_t delay, int filter_length_blocks) {
  rtc::StringBuilder ss;
  ss << "Delay: " << delay << ", ";
  ss << ProduceDebugText(filter_length_blocks);
  return ss.Release();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output gain parameter works.
TEST(RefinedFilterUpdateGainDeathTest, NullDataOutputGain) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  RenderSignalAnalyzer analyzer(config);
  SubtractorOutput output;
  RefinedFilterUpdateGain gain(config.filter.refined,
                               config.filter.config_change_duration_blocks);
  std::array<float, kFftLengthBy2Plus1> render_power;
  render_power.fill(0.f);
  std::array<float, kFftLengthBy2Plus1> erl;
  erl.fill(0.f);
  EXPECT_DEATH(
      gain.Compute(render_power, analyzer, output, erl,
                   config.filter.refined.length_blocks, false, false, nullptr),
      "");
}

#endif

// Verifies that the gain formed causes the filter using it to converge.
TEST(RefinedFilterUpdateGain, GainCausesFilterToConverge) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, filter_length_blocks));

      std::array<float, kBlockSize> e;
      std::array<float, kBlockSize> y;
      FftData G;

      RunFilterUpdateTest(600, delay_samples, filter_length_blocks,
                          blocks_with_echo_path_changes, blocks_with_saturation,
                          false, &e, &y, &G);

      // Verify that the refined filter is able to perform well.
      // Use different criteria to take overmodelling into account.
      if (filter_length_blocks == 12) {
        EXPECT_LT(1000 * std::inner_product(e.begin(), e.end(), e.begin(), 0.f),
                  std::inner_product(y.begin(), y.end(), y.begin(), 0.f));
      } else {
        EXPECT_LT(std::inner_product(e.begin(), e.end(), e.begin(), 0.f),
                  std::inner_product(y.begin(), y.end(), y.begin(), 0.f));
      }
    }
  }
}

// Verifies that the magnitude of the gain on average decreases for a
// persistently exciting signal.
TEST(RefinedFilterUpdateGain, DecreasingGain) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;

  std::array<float, kBlockSize> e;
  std::array<float, kBlockSize> y;
  FftData G_a;
  FftData G_b;
  FftData G_c;
  std::array<float, kFftLengthBy2Plus1> G_a_power;
  std::array<float, kFftLengthBy2Plus1> G_b_power;
  std::array<float, kFftLengthBy2Plus1> G_c_power;

  RunFilterUpdateTest(250, 65, 12, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_a);
  RunFilterUpdateTest(500, 65, 12, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_b);
  RunFilterUpdateTest(750, 65, 12, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_c);

  G_a.Spectrum(Aec3Optimization::kNone, G_a_power);
  G_b.Spectrum(Aec3Optimization::kNone, G_b_power);
  G_c.Spectrum(Aec3Optimization::kNone, G_c_power);

  EXPECT_GT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
            std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));

  EXPECT_GT(std::accumulate(G_b_power.begin(), G_b_power.end(), 0.),
            std::accumulate(G_c_power.begin(), G_c_power.end(), 0.));
}

// Verifies that the gain is zero when there is saturation and that the internal
// error estimates cause the gain to increase after a period of saturation.
TEST(RefinedFilterUpdateGain, SaturationBehavior) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;
  for (int k = 99; k < 200; ++k) {
    blocks_with_saturation.push_back(k);
  }

  for (size_t filter_length_blocks : {12, 20, 30}) {
    SCOPED_TRACE(ProduceDebugText(filter_length_blocks));
    std::array<float, kBlockSize> e;
    std::array<float, kBlockSize> y;
    FftData G_a;
    FftData G_b;
    FftData G_a_ref;
    G_a_ref.re.fill(0.f);
    G_a_ref.im.fill(0.f);

    std::array<float, kFftLengthBy2Plus1> G_a_power;
    std::array<float, kFftLengthBy2Plus1> G_b_power;

    RunFilterUpdateTest(100, 65, filter_length_blocks,
                        blocks_with_echo_path_changes, blocks_with_saturation,
                        false, &e, &y, &G_a);

    EXPECT_EQ(G_a_ref.re, G_a.re);
    EXPECT_EQ(G_a_ref.im, G_a.im);

    RunFilterUpdateTest(99, 65, filter_length_blocks,
                        blocks_with_echo_path_changes, blocks_with_saturation,
                        false, &e, &y, &G_a);
    RunFilterUpdateTest(201, 65, filter_length_blocks,
                        blocks_with_echo_path_changes, blocks_with_saturation,
                        false, &e, &y, &G_b);

    G_a.Spectrum(Aec3Optimization::kNone, G_a_power);
    G_b.Spectrum(Aec3Optimization::kNone, G_b_power);

    EXPECT_LT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
              std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));
  }
}

// Verifies that the gain increases after an echo path change.
// TODO(peah): Correct and reactivate this test.
TEST(RefinedFilterUpdateGain, DISABLED_EchoPathChangeBehavior) {
  for (size_t filter_length_blocks : {12, 20, 30}) {
    SCOPED_TRACE(ProduceDebugText(filter_length_blocks));
    std::vector<int> blocks_with_echo_path_changes;
    std::vector<int> blocks_with_saturation;
    blocks_with_echo_path_changes.push_back(99);

    std::array<float, kBlockSize> e;
    std::array<float, kBlockSize> y;
    FftData G_a;
    FftData G_b;
    std::array<float, kFftLengthBy2Plus1> G_a_power;
    std::array<float, kFftLengthBy2Plus1> G_b_power;

    RunFilterUpdateTest(100, 65, filter_length_blocks,
                        blocks_with_echo_path_changes, blocks_with_saturation,
                        false, &e, &y, &G_a);
    RunFilterUpdateTest(101, 65, filter_length_blocks,
                        blocks_with_echo_path_changes, blocks_with_saturation,
                        false, &e, &y, &G_b);

    G_a.Spectrum(Aec3Optimization::kNone, G_a_power);
    G_b.Spectrum(Aec3Optimization::kNone, G_b_power);

    EXPECT_LT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
              std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));
  }
}

}  // namespace webrtc
