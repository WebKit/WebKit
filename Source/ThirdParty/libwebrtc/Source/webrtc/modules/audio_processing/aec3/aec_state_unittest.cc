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
  FftBuffer X_buffer(Aec3Optimization::kNone, 30, std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(false, false);
  x.fill(0.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>>
      converged_filter_frequency_response(10);
  for (auto& v : converged_filter_frequency_response) {
    v.fill(0.01f);
  }
  std::vector<std::array<float, kFftLengthBy2Plus1>>
      diverged_filter_frequency_response = converged_filter_frequency_response;
  converged_filter_frequency_response[2].fill(100.f);

  // Verify that model based aec feasibility and linear AEC usability are false
  // when the filter is diverged and there is no external delay reported.
  state.Update(diverged_filter_frequency_response, rtc::Optional<size_t>(),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_FALSE(state.ModelBasedAecFeasible());
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that model based aec feasibility is true and that linear AEC
  // usability is false when the filter is diverged and there is an external
  // delay reported.
  state.Update(diverged_filter_frequency_response, rtc::Optional<size_t>(),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_FALSE(state.ModelBasedAecFeasible());
  for (int k = 0; k < 50; ++k) {
    state.Update(diverged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }
  EXPECT_TRUE(state.ModelBasedAecFeasible());
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that linear AEC usability is true when the filter is converged
  for (int k = 0; k < 50; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }
  EXPECT_TRUE(state.UsableLinearEstimate());

  // Verify that linear AEC usability becomes false after an echo path change is
  // reported
  echo_path_variability = EchoPathVariability(true, false);
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_FALSE(state.UsableLinearEstimate());

  // Verify that the active render detection works as intended.
  x.fill(101.f);
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_TRUE(state.ActiveRender());

  x.fill(0.f);
  for (int k = 0; k < 200; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }
  EXPECT_FALSE(state.ActiveRender());

  x.fill(101.f);
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_TRUE(state.ActiveRender());

  // Verify that echo leakage is properly reported.
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);
  EXPECT_FALSE(state.EchoLeakageDetected());

  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               true);
  EXPECT_TRUE(state.EchoLeakageDetected());

  // Verify that the bands containing reliable filter estimates are properly
  // reported.
  echo_path_variability = EchoPathVariability(false, false);
  for (int k = 0; k < 200; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }

  FftData X;
  X.re.fill(10000.f);
  X.im.fill(0.f);
  for (size_t k = 0; k < X_buffer.Buffer().size(); ++k) {
    X_buffer.Insert(X);
  }

  Y2.fill(10.f * 1000.f * 1000.f);
  E2_main.fill(100.f * Y2[0]);
  E2_shadow.fill(100.f * Y2[0]);
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);

  E2_main.fill(0.1f * Y2[0]);
  E2_shadow.fill(E2_main[0]);
  for (size_t k = 0; k < Y2.size(); k += 2) {
    E2_main[k] = Y2[k];
    E2_shadow[k] = Y2[k];
  }
  state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
               X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
               false);

  const std::array<bool, kFftLengthBy2Plus1>& reliable_bands =
      state.BandsWithReliableFilter();

  EXPECT_EQ(reliable_bands[0], reliable_bands[1]);
  for (size_t k = 1; k < kFftLengthBy2 - 5; ++k) {
    EXPECT_TRUE(reliable_bands[k]);
  }
  for (size_t k = kFftLengthBy2 - 5; k < reliable_bands.size(); ++k) {
    EXPECT_EQ(reliable_bands[kFftLengthBy2 - 6], reliable_bands[k]);
  }

  // Verify that the ERL is properly estimated
  Y2.fill(10.f * X.re[0] * X.re[0]);
  for (size_t k = 0; k < 100000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }

  ASSERT_TRUE(state.UsableLinearEstimate());
  const std::array<float, kFftLengthBy2Plus1>& erl = state.Erl();
  std::for_each(erl.begin(), erl.end(),
                [](float a) { EXPECT_NEAR(10.f, a, 0.1); });

  // Verify that the ERLE is properly estimated
  E2_main.fill(1.f * X.re[0] * X.re[0]);
  Y2.fill(10.f * E2_main[0]);
  for (size_t k = 0; k < 10000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }
  ASSERT_TRUE(state.UsableLinearEstimate());
  std::for_each(state.Erle().begin(), state.Erle().end(),
                [](float a) { EXPECT_NEAR(8.f, a, 0.1); });

  E2_main.fill(1.f * X.re[0] * X.re[0]);
  Y2.fill(5.f * E2_main[0]);
  for (size_t k = 0; k < 10000; ++k) {
    state.Update(converged_filter_frequency_response, rtc::Optional<size_t>(2),
                 X_buffer, E2_main, E2_shadow, Y2, x, echo_path_variability,
                 false);
  }
  ASSERT_TRUE(state.UsableLinearEstimate());
  std::for_each(state.Erle().begin(), state.Erle().end(),
                [](float a) { EXPECT_NEAR(5.f, a, 0.1); });
}

// Verifies the a non-significant delay is correctly identified.
TEST(AecState, NonSignificantDelay) {
  AecState state;
  FftBuffer X_buffer(Aec3Optimization::kNone, 30, std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kBlockSize> x;
  EchoPathVariability echo_path_variability(false, false);
  x.fill(0.f);

  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(30);
  for (auto& v : frequency_response) {
    v.fill(0.01f);
  }

  // Verify that a non-significant filter delay is identified correctly.
  state.Update(frequency_response, rtc::Optional<size_t>(), X_buffer, E2_main,
               E2_shadow, Y2, x, echo_path_variability, false);
  EXPECT_FALSE(state.FilterDelay());
}

// Verifies the delay for a converged filter is correctly identified.
TEST(AecState, ConvergedFilterDelay) {
  constexpr int kFilterLength = 10;
  AecState state;
  FftBuffer X_buffer(Aec3Optimization::kNone, 30, std::vector<size_t>(1, 30));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
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

    state.Update(frequency_response, rtc::Optional<size_t>(), X_buffer, E2_main,
                 E2_shadow, Y2, x, echo_path_variability, false);
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
  FftBuffer X_buffer(Aec3Optimization::kNone, 30, std::vector<size_t>(1, 30));
  std::vector<std::array<float, kFftLengthBy2Plus1>> frequency_response(30);
  for (auto& v : frequency_response) {
    v.fill(0.01f);
  }

  for (size_t k = 0; k < frequency_response.size() - 1; ++k) {
    state.Update(frequency_response, rtc::Optional<size_t>(k * kBlockSize + 5),
                 X_buffer, E2_main, E2_shadow, Y2, x,
                 EchoPathVariability(false, false), false);
    EXPECT_TRUE(state.ExternalDelay());
    EXPECT_EQ(k, state.ExternalDelay());
  }

  // Verify that the externally reported delay is properly unset when it is no
  // longer present.
  state.Update(frequency_response, rtc::Optional<size_t>(), X_buffer, E2_main,
               E2_shadow, Y2, x, EchoPathVariability(false, false), false);
  EXPECT_FALSE(state.ExternalDelay());
}

}  // namespace webrtc
