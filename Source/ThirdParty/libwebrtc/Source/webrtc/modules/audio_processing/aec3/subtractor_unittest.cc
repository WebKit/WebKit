/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/subtractor.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>

#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "modules/audio_processing/utility/cascaded_biquad_filter.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

std::vector<float> RunSubtractorTest(
    size_t num_render_channels,
    size_t num_capture_channels,
    int num_blocks_to_process,
    int delay_samples,
    int refined_filter_length_blocks,
    int coarse_filter_length_blocks,
    bool uncorrelated_inputs,
    const std::vector<int>& blocks_with_echo_path_changes) {
  ApmDataDumper data_dumper(42);
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  EchoCanceller3Config config;
  config.filter.refined.length_blocks = refined_filter_length_blocks;
  config.filter.coarse.length_blocks = coarse_filter_length_blocks;

  Subtractor subtractor(config, num_render_channels, num_capture_channels,
                        &data_dumper, DetectOptimization());
  std::optional<DelayEstimate> delay_estimate;
  Block x(kNumBands, num_render_channels);
  Block y(/*num_bands=*/1, num_capture_channels);
  std::array<float, kBlockSize> x_old;
  std::vector<SubtractorOutput> output(num_capture_channels);
  config.delay.default_delay = 1;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));
  RenderSignalAnalyzer render_signal_analyzer(config);
  Random random_generator(42U);
  Aec3Fft fft;
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      num_capture_channels);
  std::array<float, kFftLengthBy2Plus1> E2_coarse;
  AecState aec_state(config, num_capture_channels);
  x_old.fill(0.f);
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(0.f);
  }
  for (auto& E2_refined_ch : E2_refined) {
    E2_refined_ch.fill(0.f);
  }
  E2_coarse.fill(0.f);

  std::vector<std::vector<std::unique_ptr<DelayBuffer<float>>>> delay_buffer(
      num_capture_channels);
  for (size_t capture_ch = 0; capture_ch < num_capture_channels; ++capture_ch) {
    delay_buffer[capture_ch].resize(num_render_channels);
    for (size_t render_ch = 0; render_ch < num_render_channels; ++render_ch) {
      delay_buffer[capture_ch][render_ch] =
          std::make_unique<DelayBuffer<float>>(delay_samples);
    }
  }

  // [B,A] = butter(2,100/8000,'high')
  constexpr CascadedBiQuadFilter::BiQuadCoefficients
      kHighPassFilterCoefficients = {{0.97261f, -1.94523f, 0.97261f},
                                     {-1.94448f, 0.94598f}};
  std::vector<std::unique_ptr<CascadedBiQuadFilter>> x_hp_filter(
      num_render_channels);
  for (size_t ch = 0; ch < num_render_channels; ++ch) {
    x_hp_filter[ch] =
        std::make_unique<CascadedBiQuadFilter>(kHighPassFilterCoefficients, 1);
  }
  std::vector<std::unique_ptr<CascadedBiQuadFilter>> y_hp_filter(
      num_capture_channels);
  for (size_t ch = 0; ch < num_capture_channels; ++ch) {
    y_hp_filter[ch] =
        std::make_unique<CascadedBiQuadFilter>(kHighPassFilterCoefficients, 1);
  }

  for (int k = 0; k < num_blocks_to_process; ++k) {
    for (size_t render_ch = 0; render_ch < num_render_channels; ++render_ch) {
      RandomizeSampleVector(&random_generator, x.View(/*band=*/0, render_ch));
    }
    if (uncorrelated_inputs) {
      for (size_t capture_ch = 0; capture_ch < num_capture_channels;
           ++capture_ch) {
        RandomizeSampleVector(&random_generator,
                              y.View(/*band=*/0, capture_ch));
      }
    } else {
      for (size_t capture_ch = 0; capture_ch < num_capture_channels;
           ++capture_ch) {
        rtc::ArrayView<float> y_view = y.View(/*band=*/0, capture_ch);
        for (size_t render_ch = 0; render_ch < num_render_channels;
             ++render_ch) {
          std::array<float, kBlockSize> y_channel;
          delay_buffer[capture_ch][render_ch]->Delay(
              x.View(/*band=*/0, render_ch), y_channel);
          for (size_t k = 0; k < kBlockSize; ++k) {
            y_view[k] += y_channel[k] / num_render_channels;
          }
        }
      }
    }
    for (size_t ch = 0; ch < num_render_channels; ++ch) {
      x_hp_filter[ch]->Process(x.View(/*band=*/0, ch));
    }
    for (size_t ch = 0; ch < num_capture_channels; ++ch) {
      y_hp_filter[ch]->Process(y.View(/*band=*/0, ch));
    }

    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();
    render_signal_analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                                  aec_state.MinDirectPathFilterDelay());

    // Handle echo path changes.
    if (std::find(blocks_with_echo_path_changes.begin(),
                  blocks_with_echo_path_changes.end(),
                  k) != blocks_with_echo_path_changes.end()) {
      subtractor.HandleEchoPathChange(EchoPathVariability(
          true, EchoPathVariability::DelayAdjustment::kNewDetectedDelay,
          false));
    }
    subtractor.Process(*render_delay_buffer->GetRenderBuffer(), y,
                       render_signal_analyzer, aec_state, output);

    aec_state.HandleEchoPathChange(EchoPathVariability(
        false, EchoPathVariability::DelayAdjustment::kNone, false));
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponses(),
                     subtractor.FilterImpulseResponses(),
                     *render_delay_buffer->GetRenderBuffer(), E2_refined, Y2,
                     output);
  }

  std::vector<float> results(num_capture_channels);
  for (size_t ch = 0; ch < num_capture_channels; ++ch) {
    const float output_power = std::inner_product(
        output[ch].e_refined.begin(), output[ch].e_refined.end(),
        output[ch].e_refined.begin(), 0.f);
    const float y_power =
        std::inner_product(y.begin(/*band=*/0, ch), y.end(/*band=*/0, ch),
                           y.begin(/*band=*/0, ch), 0.f);
    if (y_power == 0.f) {
      ADD_FAILURE();
      results[ch] = -1.f;
    }
    results[ch] = output_power / y_power;
  }
  return results;
}

std::string ProduceDebugText(size_t num_render_channels,
                             size_t num_capture_channels,
                             size_t delay,
                             int filter_length_blocks) {
  rtc::StringBuilder ss;
  ss << "delay: " << delay << ", ";
  ss << "filter_length_blocks:" << filter_length_blocks << ", ";
  ss << "num_render_channels:" << num_render_channels << ", ";
  ss << "num_capture_channels:" << num_capture_channels;
  return ss.Release();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non data dumper works.
TEST(SubtractorDeathTest, NullDataDumper) {
  EXPECT_DEATH(
      Subtractor(EchoCanceller3Config(), 1, 1, nullptr, DetectOptimization()),
      "");
}

#endif

// Verifies that the subtractor is able to converge on correlated data.
TEST(Subtractor, Convergence) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(1, 1, delay_samples, filter_length_blocks));
      std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
          1, 1, 2500, delay_samples, filter_length_blocks, filter_length_blocks,
          false, blocks_with_echo_path_changes);

      for (float echo_to_nearend_power : echo_to_nearend_powers) {
        EXPECT_GT(0.1f, echo_to_nearend_power);
      }
    }
  }
}

// Verifies that the subtractor is able to handle the case when the refined
// filter is longer than the coarse filter.
TEST(Subtractor, RefinedFilterLongerThanCoarseFilter) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
      1, 1, 400, 64, 20, 15, false, blocks_with_echo_path_changes);
  for (float echo_to_nearend_power : echo_to_nearend_powers) {
    EXPECT_GT(0.5f, echo_to_nearend_power);
  }
}

// Verifies that the subtractor is able to handle the case when the coarse
// filter is longer than the refined filter.
TEST(Subtractor, CoarseFilterLongerThanRefinedFilter) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
      1, 1, 400, 64, 15, 20, false, blocks_with_echo_path_changes);
  for (float echo_to_nearend_power : echo_to_nearend_powers) {
    EXPECT_GT(0.5f, echo_to_nearend_power);
  }
}

// Verifies that the subtractor does not converge on uncorrelated signals.
TEST(Subtractor, NonConvergenceOnUncorrelatedSignals) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(1, 1, delay_samples, filter_length_blocks));

      std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
          1, 1, 3000, delay_samples, filter_length_blocks, filter_length_blocks,
          true, blocks_with_echo_path_changes);
      for (float echo_to_nearend_power : echo_to_nearend_powers) {
        EXPECT_NEAR(1.f, echo_to_nearend_power, 0.1);
      }
    }
  }
}

class SubtractorMultiChannelUpToEightRender
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

#if defined(NDEBUG)
INSTANTIATE_TEST_SUITE_P(NonDebugMultiChannel,
                         SubtractorMultiChannelUpToEightRender,
                         ::testing::Combine(::testing::Values(1, 2, 8),
                                            ::testing::Values(1, 2, 4)));
#else
INSTANTIATE_TEST_SUITE_P(DebugMultiChannel,
                         SubtractorMultiChannelUpToEightRender,
                         ::testing::Combine(::testing::Values(1, 2),
                                            ::testing::Values(1, 2)));
#endif

// Verifies that the subtractor is able to converge on correlated data.
TEST_P(SubtractorMultiChannelUpToEightRender, Convergence) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());

  std::vector<int> blocks_with_echo_path_changes;
  size_t num_blocks_to_process = 2500 * num_render_channels;
  std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
      num_render_channels, num_capture_channels, num_blocks_to_process, 64, 20,
      20, false, blocks_with_echo_path_changes);

  for (float echo_to_nearend_power : echo_to_nearend_powers) {
    EXPECT_GT(0.1f, echo_to_nearend_power);
  }
}

class SubtractorMultiChannelUpToFourRender
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

#if defined(NDEBUG)
INSTANTIATE_TEST_SUITE_P(NonDebugMultiChannel,
                         SubtractorMultiChannelUpToFourRender,
                         ::testing::Combine(::testing::Values(1, 2, 4),
                                            ::testing::Values(1, 2, 4)));
#else
INSTANTIATE_TEST_SUITE_P(DebugMultiChannel,
                         SubtractorMultiChannelUpToFourRender,
                         ::testing::Combine(::testing::Values(1, 2),
                                            ::testing::Values(1, 2)));
#endif

// Verifies that the subtractor does not converge on uncorrelated signals.
TEST_P(SubtractorMultiChannelUpToFourRender,
       NonConvergenceOnUncorrelatedSignals) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());

  std::vector<int> blocks_with_echo_path_changes;
  size_t num_blocks_to_process = 5000 * num_render_channels;
  std::vector<float> echo_to_nearend_powers = RunSubtractorTest(
      num_render_channels, num_capture_channels, num_blocks_to_process, 64, 20,
      20, true, blocks_with_echo_path_changes);
  for (float echo_to_nearend_power : echo_to_nearend_powers) {
    EXPECT_LT(.8f, echo_to_nearend_power);
    EXPECT_NEAR(1.f, echo_to_nearend_power, 0.25f);
  }
}
}  // namespace webrtc
