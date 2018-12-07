/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/suppression_gain.h"

#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/aec3/subtractor.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output gains works.
TEST(SuppressionGain, NullOutputGains) {
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> R2;
  std::array<float, kFftLengthBy2Plus1> S2;
  std::array<float, kFftLengthBy2Plus1> N2;
  FftData E;
  FftData Y;
  E2.fill(0.f);
  R2.fill(0.f);
  S2.fill(0.1f);
  N2.fill(0.f);
  E.re.fill(0.f);
  E.im.fill(0.f);
  Y.re.fill(0.f);
  Y.im.fill(0.f);

  float high_bands_gain;
  AecState aec_state(EchoCanceller3Config{});
  EXPECT_DEATH(
      SuppressionGain(EchoCanceller3Config{}, DetectOptimization(), 16000)
          .GetGain(E2, E2, S2, R2, N2, E, Y,
                   RenderSignalAnalyzer((EchoCanceller3Config{})), aec_state,
                   std::vector<std::vector<float>>(
                       3, std::vector<float>(kBlockSize, 0.f)),
                   &high_bands_gain, nullptr),
      "");
}

#endif

// Does a sanity check that the gains are correctly computed.
TEST(SuppressionGain, BasicGainComputation) {
  SuppressionGain suppression_gain(EchoCanceller3Config(), DetectOptimization(),
                                   16000);
  RenderSignalAnalyzer analyzer(EchoCanceller3Config{});
  float high_bands_gain;
  std::array<float, kFftLengthBy2Plus1> E2;
  std::array<float, kFftLengthBy2Plus1> S2;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> R2;
  std::array<float, kFftLengthBy2Plus1> N2;
  std::array<float, kFftLengthBy2Plus1> g;
  SubtractorOutput output;
  std::array<float, kBlockSize> y;
  FftData E;
  FftData Y;
  std::vector<std::vector<float>> x(1, std::vector<float>(kBlockSize, 0.f));
  EchoCanceller3Config config;
  AecState aec_state(config);
  ApmDataDumper data_dumper(42);
  Subtractor subtractor(config, &data_dumper, DetectOptimization());
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create2(config, 3));
  absl::optional<DelayEstimate> delay_estimate;

  // Ensure that a strong noise is detected to mask any echoes.
  E2.fill(10.f);
  Y2.fill(10.f);
  R2.fill(0.1f);
  S2.fill(0.1f);
  N2.fill(100.f);
  output.Reset();
  y.fill(0.f);
  E.re.fill(sqrtf(E2[0]));
  E.im.fill(0.f);
  Y.re.fill(sqrtf(Y2[0]));
  Y.im.fill(0.f);

  // Ensure that the gain is no longer forced to zero.
  for (int k = 0; k <= kNumBlocksPerSecond / 5 + 1; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponse(),
                     subtractor.FilterImpulseResponse(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output,
                     y);
  }

  for (int k = 0; k < 100; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponse(),
                     subtractor.FilterImpulseResponse(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output,
                     y);
    suppression_gain.GetGain(E2, E2, S2, R2, N2, E, Y, analyzer, aec_state, x,
                             &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.f, a, 0.001); });

  // Ensure that a strong nearend is detected to mask any echoes.
  E2.fill(100.f);
  Y2.fill(100.f);
  R2.fill(0.1f);
  S2.fill(0.1f);
  N2.fill(0.f);
  E.re.fill(sqrtf(E2[0]));
  Y.re.fill(sqrtf(Y2[0]));

  for (int k = 0; k < 100; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponse(),
                     subtractor.FilterImpulseResponse(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output,
                     y);
    suppression_gain.GetGain(E2, E2, S2, R2, N2, E, Y, analyzer, aec_state, x,
                             &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.f, a, 0.001); });

  // Ensure that a strong echo is suppressed.
  E2.fill(1000000000.f);
  R2.fill(10000000000000.f);
  E.re.fill(sqrtf(E2[0]));

  for (int k = 0; k < 10; ++k) {
    suppression_gain.GetGain(E2, E2, S2, R2, N2, E, Y, analyzer, aec_state, x,
                             &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(0.f, a, 0.001); });
}

}  // namespace aec3
}  // namespace webrtc
