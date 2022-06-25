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
TEST(SuppressionGainDeathTest, NullOutputGains) {
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(1, {0.0f});
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(1, {0.0f});
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded(1, {0.0f});
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2(1);
  std::vector<std::array<float, kFftLengthBy2Plus1>> N2(1, {0.0f});
  for (auto& S2_k : S2) {
    S2_k.fill(0.1f);
  }
  FftData E;
  FftData Y;
  E.re.fill(0.0f);
  E.im.fill(0.0f);
  Y.re.fill(0.0f);
  Y.im.fill(0.0f);

  float high_bands_gain;
  AecState aec_state(EchoCanceller3Config{}, 1);
  EXPECT_DEATH(
      SuppressionGain(EchoCanceller3Config{}, DetectOptimization(), 16000, 1)
          .GetGain(E2, S2, R2, R2_unbounded, N2,
                   RenderSignalAnalyzer((EchoCanceller3Config{})), aec_state,
                   std::vector<std::vector<std::vector<float>>>(
                       3, std::vector<std::vector<float>>(
                              1, std::vector<float>(kBlockSize, 0.0f))),
                   false, &high_bands_gain, nullptr),
      "");
}

#endif

// Does a sanity check that the gains are correctly computed.
TEST(SuppressionGain, BasicGainComputation) {
  constexpr size_t kNumRenderChannels = 1;
  constexpr size_t kNumCaptureChannels = 2;
  constexpr int kSampleRateHz = 16000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  SuppressionGain suppression_gain(EchoCanceller3Config(), DetectOptimization(),
                                   kSampleRateHz, kNumCaptureChannels);
  RenderSignalAnalyzer analyzer(EchoCanceller3Config{});
  float high_bands_gain;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2(kNumCaptureChannels,
                                                        {0.0f});
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2(kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded(
      kNumCaptureChannels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> N2(kNumCaptureChannels);
  std::array<float, kFftLengthBy2Plus1> g;
  std::vector<SubtractorOutput> output(kNumCaptureChannels);
  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     kNumRenderChannels, std::vector<float>(kBlockSize, 0.0f)));
  EchoCanceller3Config config;
  AecState aec_state(config, kNumCaptureChannels);
  ApmDataDumper data_dumper(42);
  Subtractor subtractor(config, kNumRenderChannels, kNumCaptureChannels,
                        &data_dumper, DetectOptimization());
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, kNumRenderChannels));
  absl::optional<DelayEstimate> delay_estimate;

  // Ensure that a strong noise is detected to mask any echoes.
  for (size_t ch = 0; ch < kNumCaptureChannels; ++ch) {
    E2[ch].fill(10.f);
    Y2[ch].fill(10.f);
    R2[ch].fill(0.1f);
    R2_unbounded[ch].fill(0.1f);
    N2[ch].fill(100.0f);
  }
  for (auto& subtractor_output : output) {
    subtractor_output.Reset();
  }

  // Ensure that the gain is no longer forced to zero.
  for (int k = 0; k <= kNumBlocksPerSecond / 5 + 1; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponses(),
                     subtractor.FilterImpulseResponses(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output);
  }

  for (int k = 0; k < 100; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponses(),
                     subtractor.FilterImpulseResponses(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output);
    suppression_gain.GetGain(E2, S2, R2, R2_unbounded, N2, analyzer, aec_state,
                             x, false, &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.0f, a, 0.001f); });

  // Ensure that a strong nearend is detected to mask any echoes.
  for (size_t ch = 0; ch < kNumCaptureChannels; ++ch) {
    E2[ch].fill(100.f);
    Y2[ch].fill(100.f);
    R2[ch].fill(0.1f);
    R2_unbounded[ch].fill(0.1f);
    S2[ch].fill(0.1f);
    N2[ch].fill(0.f);
  }

  for (int k = 0; k < 100; ++k) {
    aec_state.Update(delay_estimate, subtractor.FilterFrequencyResponses(),
                     subtractor.FilterImpulseResponses(),
                     *render_delay_buffer->GetRenderBuffer(), E2, Y2, output);
    suppression_gain.GetGain(E2, S2, R2, R2_unbounded, N2, analyzer, aec_state,
                             x, false, &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(1.0f, a, 0.001f); });

  // Add a strong echo to one of the channels and ensure that it is suppressed.
  E2[1].fill(1000000000.0f);
  R2[1].fill(10000000000000.0f);
  R2_unbounded[1].fill(10000000000000.0f);

  for (int k = 0; k < 10; ++k) {
    suppression_gain.GetGain(E2, S2, R2, R2_unbounded, N2, analyzer, aec_state,
                             x, false, &high_bands_gain, &g);
  }
  std::for_each(g.begin(), g.end(),
                [](float a) { EXPECT_NEAR(0.0f, a, 0.001f); });
}

}  // namespace aec3
}  // namespace webrtc
