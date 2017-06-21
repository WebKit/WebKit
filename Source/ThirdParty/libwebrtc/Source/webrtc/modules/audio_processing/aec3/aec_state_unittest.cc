/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/aec_state.h"

#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

// Verify the general functionality of AecState
TEST(AecState, NormalUsage) {
  ApmDataDumper data_dumper(42);
  AecState state;
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 30,
                             std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main = {};
  std::array<float, kFftLengthBy2Plus1> Y2 = {};
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  EchoPathVariability echo_path_variability(false, false);

  std::vector<std::array<float, kFftLengthBy2Plus1>>
      converged_filter_frequency_response(10);
  for (auto& v : converged_filter_frequency_response) {
    v.fill(0.01f);
  }
  std::vector<std::array<float, kFftLengthBy2Plus1>>
      diverged_filter_frequency_response = converged_filter_frequency_response;
  converged_filter_frequency_response[2].fill(100.f);
  converged_filter_frequency_response[2][0] = 1.f;

  // Verify that linear AEC usability is false when the filter is diverged and
  // there is no external delay reported.
  state.Update(diverged_filter_frequency_response, rtc::Optional<size_t>(),
               render_buffer, E2_main, Y2, x[0], false);
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that linear AEC usability is true when the filter is converged
  std::fill(x[0].begin(), x[0].end(), 101.f);
  for (int k = 0; k < 3000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 render_buffer, E2_main, Y2, x[0], false);
  }
  EXPECT_TRUE(state.UsableLinearEstimate());

  // Verify that linear AEC usability becomes false after an echo path change is
  // reported
  state.HandleEchoPathChange(EchoPathVariability(true, false));
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               render_buffer, E2_main, Y2, x[0], false);
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that the active render detection works as intended.
  std::fill(x[0].begin(), x[0].end(), 101.f);
  state.HandleEchoPathChange(EchoPathVariability(true, true));
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               render_buffer, E2_main, Y2, x[0], false);
  EXPECT_FALSE(state.ActiveRender());

  for (int k = 0; k < 1000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 render_buffer, E2_main, Y2, x[0], false);
  }
  EXPECT_TRUE(state.ActiveRender());

  // Verify that echo leakage is properly reported.
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               render_buffer, E2_main, Y2, x[0], false);
  EXPECT_FALSE(state.EchoLeakageDetected());

  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               render_buffer, E2_main, Y2, x[0], true);
  EXPECT_TRUE(state.EchoLeakageDetected());

  // Verify that the ERL is properly estimated
  for (auto& x_k : x) {
    x_k = std::vector<float>(kBlockSize, 0.f);
  }

  x[0][0] = 5000.f;
  for (size_t k = 0; k < render_buffer.Buffer().size(); ++k) {
    render_buffer.Insert(x);
  }

  Y2.fill(10.f * 10000.f * 10000.f);
  for (size_t k = 0; k < 1000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 render_buffer, E2_main, Y2, x[0], false);
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
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 render_buffer, E2_main, Y2, x[0], false);
  }
  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    const auto& erle = state.Erle();
    EXPECT_EQ(erle[0], erle[1]);
    constexpr size_t kLowFrequencyLimit = 32;
    for (size_t k = 1; k < kLowFrequencyLimit; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 8.f : 1.f, erle[k], 0.1);
    }
    for (size_t k = kLowFrequencyLimit; k < erle.size() - 1; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 1.5f : 1.f, erle[k], 0.1);
    }
    EXPECT_EQ(erle[erle.size() - 2], erle[erle.size() - 1]);
  }

  E2_main.fill(1.f * 10000.f * 10000.f);
  Y2.fill(5.f * E2_main[0]);
  for (size_t k = 0; k < 1000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 render_buffer, E2_main, Y2, x[0], false);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  {
    const auto& erle = state.Erle();
    EXPECT_EQ(erle[0], erle[1]);
    constexpr size_t kLowFrequencyLimit = 32;
    for (size_t k = 1; k < kLowFrequencyLimit; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 5.f : 1.f, erle[k], 0.1);
    }
    for (size_t k = kLowFrequencyLimit; k < erle.size() - 1; ++k) {
      EXPECT_NEAR(k % 2 == 0 ? 1.5f : 1.f, erle[k], 0.1);
    }
    EXPECT_EQ(erle[erle.size() - 2], erle[erle.size() - 1]);
  }
}

// Verifies the a non-significant delay is correctly identified.
TEST(AecState, NonSignificantDelay) {
  AecState state;
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 30,
                             std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(false, false);
  x.fill(0.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(30);
  for (auto& v : frequency_response) {
    v.fill(0.01f);
  }

  // Verify that a non-significant filter delay is identified correctly.
  state.HandleEchoPathChange(echo_path_variability);
  state.Update(frequency_response, rtc::Optional<size_t>(), render_buffer,
               E2_main, Y2, x, false);
  EXPECT_FALSE(state.FilterDelay());
}

// Verifies the delay for a converged filter is correctly identified.
TEST(AecState, ConvergedFilterDelay) {
  constexpr int kFilterLength = 10;
  AecState state;
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 30,
                             std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(false, false);
  x.fill(0.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(
      kFilterLength);

  // Verify that the filter delay for a converged filter is properly identified.
  for (int k = 0; k < kFilterLength; ++k) {
    for (auto& v : frequency_response) {
      v.fill(0.01f);
    }
    frequency_response[k].fill(100.f);
    frequency_response[k][0] = 0.f;
    state.HandleEchoPathChange(echo_path_variability);
    state.Update(frequency_response, rtc::Optional<size_t>(), render_buffer,
                 E2_main, Y2, x, false);
    EXPECT_TRUE(k == (kFilterLength - 1) || state.FilterDelay());
    if (k != (kFilterLength - 1)) {
      EXPECT_EQ(k, state.FilterDelay());
    }
  }
}

// Verify that the externally reported delay is properly reported and converted.
TEST(AecState, ExternalDelay) {
  AecState state;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  E2_main.fill(0.f);
  E2_shadow.fill(0.f);
  Y2.fill(0.f);
  x.fill(0.f);
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 30,
                             std::vector<size_t>(1, 30));
  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(30);
  for (auto& v : frequency_response) {
    v.fill(0.01f);
  }

  for (size_t k = 0; k < frequency_response.size() - 1; ++k) {
    state.HandleEchoPathChange(EchoPathVariability(false, false));
    state.Update(frequency_response, rtc::Optional<size_t>(k * kBlockSize + 5),
                 render_buffer, E2_main, Y2, x, false);
    EXPECT_TRUE(state.ExternalDelay());
    EXPECT_EQ(k, state.ExternalDelay());
  }

  // Verify that the externally reported delay is properly unset when it is no
  // longer present.
  state.HandleEchoPathChange(EchoPathVariability(false, false));
  state.Update(frequency_response, rtc::Optional<size_t>(), render_buffer,
               E2_main, Y2, x, false);
  EXPECT_FALSE(state.ExternalDelay());
}

}  // namespace webrtc
