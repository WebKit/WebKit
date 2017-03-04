/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/power_echo_model.h"

#include <array>
#include <string>
#include <vector>

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

std::string ProduceDebugText(size_t delay, bool known_delay) {
  std::ostringstream ss;
  ss << "True delay: " << delay;
  ss << ", Delay known: " << (known_delay ? "true" : "false");
  return ss.str();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output parameter works.
TEST(PowerEchoModel, NullEstimateEchoOutput) {
  PowerEchoModel model;
  std::array<float, kFftLengthBy2Plus1> Y2;
  AecState aec_state;
  FftBuffer X_buffer(Aec3Optimization::kNone, model.MinFarendBufferLength(),
                     std::vector<size_t>(1, model.MinFarendBufferLength()));

  EXPECT_DEATH(model.EstimateEcho(X_buffer, Y2, aec_state, nullptr), "");
}

#endif

TEST(PowerEchoModel, BasicSetup) {
  PowerEchoModel model;
  Random random_generator(42U);
  AecState aec_state;
  Aec3Fft fft;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> S2;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kBlockSize> x_old;
  std::array<float, kBlockSize> y;
  std::vector<float> x(kBlockSize, 0.f);
  FftData X;
  FftData Y;
  x_old.fill(0.f);

  FftBuffer X_buffer(Aec3Optimization::kNone, model.MinFarendBufferLength(),
                     std::vector<size_t>(1, model.MinFarendBufferLength()));

  for (size_t delay_samples : {0, 64, 301}) {
    DelayBuffer<float> delay_buffer(delay_samples);
    auto model_applier = [&](int num_iterations, float y_scale,
                             bool known_delay) {
      for (int k = 0; k < num_iterations; ++k) {
        RandomizeSampleVector(&random_generator, x);
        delay_buffer.Delay(x, y);
        std::for_each(y.begin(), y.end(), [&](float& a) { a *= y_scale; });

        fft.PaddedFft(x, x_old, &X);
        X_buffer.Insert(X);

        fft.ZeroPaddedFft(y, &Y);
        Y.Spectrum(Aec3Optimization::kNone, &Y2);

        aec_state.Update(std::vector<std::array<float, kFftLengthBy2Plus1>>(
                             10, std::array<float, kFftLengthBy2Plus1>()),
                         known_delay ? rtc::Optional<size_t>(delay_samples)
                                     : rtc::Optional<size_t>(),
                         X_buffer, E2_main, E2_shadow, Y2, x,
                         EchoPathVariability(false, false), false);

        model.EstimateEcho(X_buffer, Y2, aec_state, &S2);
      }
    };

    for (int j = 0; j < 2; ++j) {
      bool known_delay = j == 0;
      SCOPED_TRACE(ProduceDebugText(delay_samples, known_delay));
      // Verify that the echo path estimates converges downwards to a fairly
      // tight bound estimate.
      model_applier(600, 1.f, known_delay);
      for (size_t k = 1; k < S2.size() - 1; ++k) {
        EXPECT_LE(Y2[k], 2.f * S2[k]);
      }

      // Verify that stronger echo paths are detected immediately.
      model_applier(100, 10.f, known_delay);
      for (size_t k = 1; k < S2.size() - 1; ++k) {
        EXPECT_LE(Y2[k], 5.f * S2[k]);
      }

      // Verify that there is a delay until a weaker echo path is detected.
      model_applier(50, 100.f, known_delay);
      model_applier(50, 1.f, known_delay);
      for (size_t k = 1; k < S2.size() - 1; ++k) {
        EXPECT_LE(100.f * Y2[k], S2[k]);
      }

      // Verify that an echo path change causes the echo path estimate to be
      // reset.
      model_applier(600, 0.1f, known_delay);
      model.HandleEchoPathChange(EchoPathVariability(true, false));
      model_applier(50, 0.1f, known_delay);
      for (size_t k = 1; k < S2.size() - 1; ++k) {
        EXPECT_LE(10.f * Y2[k], S2[k]);
      }
    }
  }
}

}  // namespace webrtc
