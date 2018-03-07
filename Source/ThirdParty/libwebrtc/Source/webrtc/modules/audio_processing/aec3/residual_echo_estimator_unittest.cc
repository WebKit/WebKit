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

#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output residual echo power works.
TEST(ResidualEchoEstimator, NullResidualEchoPowerOutput) {
  AecState aec_state(EchoCanceller3Config{});
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 10,
                             std::vector<size_t>(1, 10));
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> Y2;
  EXPECT_DEATH(ResidualEchoEstimator(EchoCanceller3Config{})
                   .Estimate(aec_state, render_buffer, S2_linear, Y2, nullptr),
               "");
}

#endif

TEST(ResidualEchoEstimator, BasicTest) {
  ResidualEchoEstimator estimator(EchoCanceller3Config{});
  EchoCanceller3Config config;
  config.ep_strength.default_len = 0.f;
  AecState aec_state(config);
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3, 10,
                             std::vector<size_t>(1, 10));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> S2_fallback;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> R2;
  EchoPathVariability echo_path_variability(false, false);
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(10);
  Random random_generator(42U);
  FftData X;
  std::array<float, kBlockSize> x_old;
  std::array<float, kBlockSize> s;
  Aec3Fft fft;

  for (auto& H2_k : H2) {
    H2_k.fill(0.01f);
  }
  H2[2].fill(10.f);
  H2[2][0] = 0.1f;

  std::array<float, kAdaptiveFilterTimeDomainLength> h;
  h.fill(0.f);

  s.fill(100.f);

  constexpr float kLevel = 10.f;
  E2_shadow.fill(kLevel);
  E2_main.fill(kLevel);
  S2_linear.fill(kLevel);
  S2_fallback.fill(kLevel);
  Y2.fill(kLevel);

  for (int k = 0; k < 2000; ++k) {
    RandomizeSampleVector(&random_generator, x[0]);
    std::for_each(x[0].begin(), x[0].end(), [](float& a) { a /= 30.f; });
    fft.PaddedFft(x[0], x_old, &X);
    render_buffer.Insert(x);

    aec_state.HandleEchoPathChange(echo_path_variability);
    aec_state.Update(H2, h, true, 2, render_buffer, E2_main, Y2, x[0], s,
                     false);

    estimator.Estimate(aec_state, render_buffer, S2_linear, Y2, &R2);
  }
  std::for_each(R2.begin(), R2.end(),
                [&](float a) { EXPECT_NEAR(kLevel, a, 0.1f); });
}

}  // namespace webrtc
