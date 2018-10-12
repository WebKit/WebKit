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
#include <numeric>
#include <string>

#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float RunSubtractorTest(int num_blocks_to_process,
                        int delay_samples,
                        int main_filter_length_blocks,
                        int shadow_filter_length_blocks,
                        bool uncorrelated_inputs,
                        const std::vector<int>& blocks_with_echo_path_changes) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  config.filter.main.length_blocks = main_filter_length_blocks;
  config.filter.shadow.length_blocks = shadow_filter_length_blocks;

  Subtractor subtractor(config, &data_dumper, DetectOptimization());
  absl::optional<DelayEstimate> delay_estimate;
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<float> y(kBlockSize, 0.f);
  std::array<float, kBlockSize> x_old;
  SubtractorOutput output;
  config.delay.min_echo_path_delay_blocks = 0;
  config.delay.default_delay = 1;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));
  RenderSignalAnalyzer render_signal_analyzer(config);
  Random random_generator(42U);
  Aec3Fft fft;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  AecState aec_state(config);
  x_old.fill(0.f);
  Y2.fill(0.f);
  E2_main.fill(0.f);
  E2_shadow.fill(0.f);

  DelayBuffer<float> delay_buffer(delay_samples);
  for (int k = 0; k < num_blocks_to_process; ++k) {
    RandomizeSampleVector(&random_generator, x[0]);
    if (uncorrelated_inputs) {
      RandomizeSampleVector(&random_generator, y);
    } else {
      delay_buffer.Delay(x[0], y);
    }
    render_delay_buffer->Insert(x);
    if (k == 0) {
      render_delay_buffer->Reset();
    }
    render_delay_buffer->PrepareCaptureProcessing();
    render_signal_analyzer.Update(*render_delay_buffer->GetRenderBuffer(),
                                  aec_state.FilterDelayBlocks());

    // Handle echo path changes.
    if (std::find(blocks_with_echo_path_changes.begin(),
                  blocks_with_echo_path_changes.end(),
                  k) != blocks_with_echo_path_changes.end()) {
      subtractor.HandleEchoPathChange(EchoPathVariability(
          true, EchoPathVariability::DelayAdjustment::kNewDetectedDelay,
          false));
    }
    subtractor.Process(*render_delay_buffer->GetRenderBuffer(), y,
                       render_signal_analyzer, aec_state, &output);

    aec_state.HandleEchoPathChange(EchoPathVariability(
        false, EchoPathVariability::DelayAdjustment::kNone, false));
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponse(),
                     subtractor.FilterImpulseResponse(),
                     *render_delay_buffer->GetRenderBuffer(), E2_main, Y2,
                     output, y);
  }

  const float output_power = std::inner_product(
      output.e_main.begin(), output.e_main.end(), output.e_main.begin(), 0.f);
  const float y_power = std::inner_product(y.begin(), y.end(), y.begin(), 0.f);
  if (y_power == 0.f) {
    ADD_FAILURE();
    return -1.0;
  }
  return output_power / y_power;
}

std::string ProduceDebugText(size_t delay, int filter_length_blocks) {
  rtc::StringBuilder ss;
  ss << "Delay: " << delay << ", ";
  ss << "Length: " << filter_length_blocks;
  return ss.Release();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non data dumper works.
TEST(Subtractor, NullDataDumper) {
  EXPECT_DEATH(
      Subtractor(EchoCanceller3Config(), nullptr, DetectOptimization()), "");
}

// Verifies the check for null subtractor output.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(Subtractor, DISABLED_NullOutput) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  Subtractor subtractor(config, &data_dumper, DetectOptimization());
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));
  RenderSignalAnalyzer render_signal_analyzer(config);
  std::vector<float> y(kBlockSize, 0.f);

  EXPECT_DEATH(
      subtractor.Process(*render_delay_buffer->GetRenderBuffer(), y,
                         render_signal_analyzer, AecState(config), nullptr),
      "");
}

// Verifies the check for the capture signal size.
TEST(Subtractor, WrongCaptureSize) {
  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;
  Subtractor subtractor(config, &data_dumper, DetectOptimization());
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, 3));
  RenderSignalAnalyzer render_signal_analyzer(config);
  std::vector<float> y(kBlockSize - 1, 0.f);
  SubtractorOutput output;

  EXPECT_DEATH(
      subtractor.Process(*render_delay_buffer->GetRenderBuffer(), y,
                         render_signal_analyzer, AecState(config), &output),
      "");
}

#endif

// Verifies that the subtractor is able to converge on correlated data.
TEST(Subtractor, Convergence) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, filter_length_blocks));

      float echo_to_nearend_power = RunSubtractorTest(
          400, delay_samples, filter_length_blocks, filter_length_blocks, false,
          blocks_with_echo_path_changes);

      // Use different criteria to take overmodelling into account.
      if (filter_length_blocks == 12) {
        EXPECT_GT(0.1f, echo_to_nearend_power);
      } else {
        EXPECT_GT(1.f, echo_to_nearend_power);
      }
    }
  }
}

// Verifies that the subtractor is able to handle the case when the main filter
// is longer than the shadow filter.
TEST(Subtractor, MainFilterLongerThanShadowFilter) {
  std::vector<int> blocks_with_echo_path_changes;
  float echo_to_nearend_power =
      RunSubtractorTest(400, 64, 20, 15, false, blocks_with_echo_path_changes);
  EXPECT_GT(0.5f, echo_to_nearend_power);
}

// Verifies that the subtractor is able to handle the case when the shadow
// filter is longer than the main filter.
TEST(Subtractor, ShadowFilterLongerThanMainFilter) {
  std::vector<int> blocks_with_echo_path_changes;
  float echo_to_nearend_power =
      RunSubtractorTest(400, 64, 15, 20, false, blocks_with_echo_path_changes);
  EXPECT_GT(0.5f, echo_to_nearend_power);
}

// Verifies that the subtractor does not converge on uncorrelated signals.
TEST(Subtractor, NonConvergenceOnUncorrelatedSignals) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, filter_length_blocks));

      float echo_to_nearend_power = RunSubtractorTest(
          300, delay_samples, filter_length_blocks, filter_length_blocks, true,
          blocks_with_echo_path_changes);
      EXPECT_NEAR(1.f, echo_to_nearend_power, 0.1);
    }
  }
}

// Verifies that the subtractor is properly reset when there is an echo path
// change.
TEST(Subtractor, EchoPathChangeReset) {
  std::vector<int> blocks_with_echo_path_changes;
  blocks_with_echo_path_changes.push_back(99);
  for (size_t filter_length_blocks : {12, 20, 30}) {
    for (size_t delay_samples : {0, 64, 150, 200, 301}) {
      SCOPED_TRACE(ProduceDebugText(delay_samples, filter_length_blocks));

      float echo_to_nearend_power = RunSubtractorTest(
          100, delay_samples, filter_length_blocks, filter_length_blocks, false,
          blocks_with_echo_path_changes);
      EXPECT_NEAR(1.f, echo_to_nearend_power, 0.0000001f);
    }
  }
}

}  // namespace webrtc
