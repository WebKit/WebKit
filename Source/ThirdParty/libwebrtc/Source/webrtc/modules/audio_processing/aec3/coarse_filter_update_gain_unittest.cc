/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/coarse_filter_update_gain.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
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
                         size_t num_render_channels,
                         int filter_length_blocks,
                         const std::vector<int>& blocks_with_saturation,
                         std::array<float, kBlockSize>* e_last_block,
                         std::array<float, kBlockSize>* y_last_block,
                         FftData* G_last_block) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  config.filter.refined.length_blocks = filter_length_blocks;
  AdaptiveFirFilter refined_filter(
      config.filter.refined.length_blocks, config.filter.refined.length_blocks,
      config.filter.config_change_duration_blocks, num_render_channels,
      DetectOptimization(), &data_dumper);
  AdaptiveFirFilter coarse_filter(
      config.filter.coarse.length_blocks, config.filter.coarse.length_blocks,
      config.filter.config_change_duration_blocks, num_render_channels,
      DetectOptimization(), &data_dumper);
  Aec3Fft fft;

  constexpr int kSampleRateHz = 48000;
  config.delay.default_delay = 1;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));

  CoarseFilterUpdateGain coarse_gain(
      config.filter.coarse, config.filter.config_change_duration_blocks);
  Random random_generator(42U);
  std::vector<std::vector<std::vector<float>>> x(
      NumBandsForRate(kSampleRateHz),
      std::vector<std::vector<float>>(num_render_channels,
                                      std::vector<float>(kBlockSize, 0.f)));
  std::array<float, kBlockSize> y;
  RenderSignalAnalyzer render_signal_analyzer(config);
  std::array<float, kFftLength> s;
  FftData S;
  FftData G;
  FftData E_coarse;
  std::array<float, kBlockSize> e_coarse;

  constexpr float kScale = 1.0f / kFftLengthBy2;

  DelayBuffer<float> delay_buffer(delay_samples);
  for (int k = 0; k < num_blocks_to_process; ++k) {
    // Handle saturation.
    bool saturation =
        std::find(blocks_with_saturation.begin(), blocks_with_saturation.end(),
                  k) != blocks_with_saturation.end();

    // Create the render signal.
    for (size_t band = 0; band < x.size(); ++band) {
      for (size_t channel = 0; channel < x[band].size(); ++channel) {
        RandomizeSampleVector(&random_generator, x[band][channel]);
      }
    }
    delay_buffer.Delay(x[0][0], y);

    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();

    render_signal_analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                                  delay_samples / kBlockSize);

    coarse_filter.Filter(*render_delay_buffer->GetRenderBuffer(), &S);
    fft.Ifft(S, &s);
    std::transform(y.begin(), y.end(), s.begin() + kFftLengthBy2,
                   e_coarse.begin(),
                   [&](float a, float b) { return a - b * kScale; });
    std::for_each(e_coarse.begin(), e_coarse.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
    fft.ZeroPaddedFft(e_coarse, Aec3Fft::Window::kRectangular, &E_coarse);

    std::array<float, kFftLengthBy2Plus1> render_power;
    render_delay_buffer->GetRenderBuffer()->SpectralSum(
        coarse_filter.SizePartitions(), &render_power);
    coarse_gain.Compute(render_power, render_signal_analyzer, E_coarse,
                        coarse_filter.SizePartitions(), saturation, &G);
    coarse_filter.Adapt(*render_delay_buffer->GetRenderBuffer(), G);
  }

  std::copy(e_coarse.begin(), e_coarse.end(), e_last_block->begin());
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
TEST(CoarseFilterUpdateGainDeathTest, NullDataOutputGain) {
  ApmDataDumper data_dumper(42);
  FftBuffer fft_buffer(1, 1);
  RenderSignalAnalyzer analyzer(EchoCanceller3Config{});
  FftData E;
  const EchoCanceller3Config::Filter::CoarseConfiguration& config = {
      12, 0.5f, 220075344.f};
  CoarseFilterUpdateGain gain(config, 250);
  std::array<float, kFftLengthBy2Plus1> render_power;
  render_power.fill(0.f);
  EXPECT_DEATH(gain.Compute(render_power, analyzer, E, 1, false, nullptr), "");
}

#endif

class CoarseFilterUpdateGainOneTwoEightRenderChannels
    : public ::testing::Test,
      public ::testing::WithParamInterface<size_t> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         CoarseFilterUpdateGainOneTwoEightRenderChannels,
                         ::testing::Values(1, 2, 8));

// Verifies that the gain formed causes the filter using it to converge.
TEST_P(CoarseFilterUpdateGainOneTwoEightRenderChannels,
       GainCausesFilterToConverge) {
  const size_t num_render_channels = GetParam();
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;

  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, filter_length_blocks));

      std::array<float, kBlockSize> e;
      std::array<float, kBlockSize> y;
      FftData G;

      RunFilterUpdateTest(5000, delay_samples, num_render_channels,
                          filter_length_blocks, blocks_with_saturation, &e, &y,
                          &G);

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

// Verifies that the gain is zero when there is saturation.
TEST_P(CoarseFilterUpdateGainOneTwoEightRenderChannels, SaturationBehavior) {
  const size_t num_render_channels = GetParam();
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
    FftData G_a_ref;
    G_a_ref.re.fill(0.f);
    G_a_ref.im.fill(0.f);

    RunFilterUpdateTest(100, 65, num_render_channels, filter_length_blocks,
                        blocks_with_saturation, &e, &y, &G_a);

    EXPECT_EQ(G_a_ref.re, G_a.re);
    EXPECT_EQ(G_a_ref.im, G_a.im);
  }
}

class CoarseFilterUpdateGainOneTwoFourRenderChannels
    : public ::testing::Test,
      public ::testing::WithParamInterface<size_t> {};

INSTANTIATE_TEST_SUITE_P(
    MultiChannel,
    CoarseFilterUpdateGainOneTwoFourRenderChannels,
    ::testing::Values(1, 2, 4),
    [](const ::testing::TestParamInfo<
        CoarseFilterUpdateGainOneTwoFourRenderChannels::ParamType>& info) {
      return (rtc::StringBuilder() << "Render" << info.param).str();
    });

// Verifies that the magnitude of the gain on average decreases for a
// persistently exciting signal.
TEST_P(CoarseFilterUpdateGainOneTwoFourRenderChannels, DecreasingGain) {
  const size_t num_render_channels = GetParam();
  for (size_t filter_length_blocks : {12, 20, 30}) {
    SCOPED_TRACE(ProduceDebugText(filter_length_blocks));
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

    RunFilterUpdateTest(100, 65, num_render_channels, filter_length_blocks,
                        blocks_with_saturation, &e, &y, &G_a);
    RunFilterUpdateTest(200, 65, num_render_channels, filter_length_blocks,
                        blocks_with_saturation, &e, &y, &G_b);
    RunFilterUpdateTest(300, 65, num_render_channels, filter_length_blocks,
                        blocks_with_saturation, &e, &y, &G_c);

    G_a.Spectrum(Aec3Optimization::kNone, G_a_power);
    G_b.Spectrum(Aec3Optimization::kNone, G_b_power);
    G_c.Spectrum(Aec3Optimization::kNone, G_c_power);

    EXPECT_GT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
              std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));

    EXPECT_GT(std::accumulate(G_b_power.begin(), G_b_power.end(), 0.),
              std::accumulate(G_c_power.begin(), G_c_power.end(), 0.));
  }
}
}  // namespace webrtc
