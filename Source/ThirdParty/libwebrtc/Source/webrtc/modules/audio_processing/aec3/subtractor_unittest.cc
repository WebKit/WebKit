/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/subtractor.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

float RunSubtractorTest(int num_blocks_to_process,
                        int delay_samples,
                        bool uncorrelated_inputs,
                        const std::vector<int>& blocks_with_echo_path_changes) {
  ApmDataDumper data_dumper(42);
  Subtractor subtractor(&data_dumper, DetectOptimization());
  std::vector<float> x(kBlockSize, 0.f);
  std::vector<float> y(kBlockSize, 0.f);
  std::array<float, kBlockSize> x_old;
  SubtractorOutput output;
  FftBuffer X_buffer(
      Aec3Optimization::kNone, subtractor.MinFarendBufferLength(),
      std::vector<size_t>(1, subtractor.MinFarendBufferLength()));
  RenderSignalAnalyzer render_signal_analyzer;
  Random random_generator(42U);
  Aec3Fft fft;
  FftData X;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  AecState aec_state;
  x_old.fill(0.f);
  Y2.fill(0.f);
  E2_main.fill(0.f);
  E2_shadow.fill(0.f);

  DelayBuffer<float> delay_buffer(delay_samples);
  for (int k = 0; k < num_blocks_to_process; ++k) {
    RandomizeSampleVector(&random_generator, x);
    if (uncorrelated_inputs) {
      RandomizeSampleVector(&random_generator, y);
    } else {
      delay_buffer.Delay(x, y);
    }
    fft.PaddedFft(x, x_old, &X);
    X_buffer.Insert(X);
    render_signal_analyzer.Update(X_buffer, aec_state.FilterDelay());

    // Handle echo path changes.
    if (std::find(blocks_with_echo_path_changes.begin(),
                  blocks_with_echo_path_changes.end(),
                  k) != blocks_with_echo_path_changes.end()) {
      subtractor.HandleEchoPathChange(EchoPathVariability(true, true));
    }
    subtractor.Process(X_buffer, y, render_signal_analyzer, false, &output);

    aec_state.Update(subtractor.FilterFrequencyResponse(),
                     rtc::Optional<size_t>(delay_samples / kBlockSize),
                     X_buffer, E2_main, E2_shadow, Y2, x,
                     EchoPathVariability(false, false), false);
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

std::string ProduceDebugText(size_t delay) {
  std::ostringstream ss;
  ss << "Delay: " << delay;
  return ss.str();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non data dumper works.
TEST(Subtractor, NullDataDumper) {
  EXPECT_DEATH(Subtractor(nullptr, DetectOptimization()), "");
}

// Verifies the check for null subtractor output.
// TODO(peah): Re-enable the test once the issue with memory leaks during DEATH
// tests on test bots has been fixed.
TEST(Subtractor, DISABLED_NullOutput) {
  ApmDataDumper data_dumper(42);
  Subtractor subtractor(&data_dumper, DetectOptimization());
  FftBuffer X_buffer(
      Aec3Optimization::kNone, subtractor.MinFarendBufferLength(),
      std::vector<size_t>(1, subtractor.MinFarendBufferLength()));
  RenderSignalAnalyzer render_signal_analyzer;
  std::vector<float> y(kBlockSize, 0.f);

  EXPECT_DEATH(
      subtractor.Process(X_buffer, y, render_signal_analyzer, false, nullptr),
      "");
}

// Verifies the check for the capture signal size.
TEST(Subtractor, WrongCaptureSize) {
  ApmDataDumper data_dumper(42);
  Subtractor subtractor(&data_dumper, DetectOptimization());
  FftBuffer X_buffer(
      Aec3Optimization::kNone, subtractor.MinFarendBufferLength(),
      std::vector<size_t>(1, subtractor.MinFarendBufferLength()));
  RenderSignalAnalyzer render_signal_analyzer;
  std::vector<float> y(kBlockSize - 1, 0.f);
  SubtractorOutput output;

  EXPECT_DEATH(
      subtractor.Process(X_buffer, y, render_signal_analyzer, false, &output),
      "");
}

#endif

// Verifies that the subtractor is able to converge on correlated data.
TEST(Subtractor, Convergence) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    SCOPED_TRACE(ProduceDebugText(delay_samples));

    float echo_to_nearend_power = RunSubtractorTest(
        100, delay_samples, false, blocks_with_echo_path_changes);
    EXPECT_GT(0.1f, echo_to_nearend_power);
  }
}

// Verifies that the subtractor does not converge on uncorrelated signals.
TEST(Subtractor, NonConvergenceOnUncorrelatedSignals) {
  std::vector<int> blocks_with_echo_path_changes;
  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    SCOPED_TRACE(ProduceDebugText(delay_samples));

    float echo_to_nearend_power = RunSubtractorTest(
        100, delay_samples, true, blocks_with_echo_path_changes);
    EXPECT_NEAR(1.f, echo_to_nearend_power, 0.05);
  }
}

// Verifies that the subtractor is properly reset when there is an echo path
// change.
TEST(Subtractor, EchoPathChangeReset) {
  std::vector<int> blocks_with_echo_path_changes;
  blocks_with_echo_path_changes.push_back(99);
  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    SCOPED_TRACE(ProduceDebugText(delay_samples));

    float echo_to_nearend_power = RunSubtractorTest(
        100, delay_samples, false, blocks_with_echo_path_changes);
    EXPECT_NEAR(1.f, echo_to_nearend_power, 0.0000001f);
  }
}

}  // namespace webrtc
