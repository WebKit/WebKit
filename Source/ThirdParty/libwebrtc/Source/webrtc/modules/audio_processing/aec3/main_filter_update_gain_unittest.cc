/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/main_filter_update_gain.h"

#include <algorithm>
#include <numeric>
#include <string>

#include "webrtc/base/random.h"
#include "webrtc/base/safe_minmax.h"
#include "webrtc/modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"
#include "webrtc/modules/audio_processing/aec3/render_signal_analyzer.h"
#include "webrtc/modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "webrtc/modules/audio_processing/aec3/subtractor_output.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

// Method for performing the simulations needed to test the main filter update
// gain functionality.
void RunFilterUpdateTest(int num_blocks_to_process,
                         size_t delay_samples,
                         const std::vector<int>& blocks_with_echo_path_changes,
                         const std::vector<int>& blocks_with_saturation,
                         bool use_silent_render_in_second_half,
                         std::array<float, kBlockSize>* e_last_block,
                         std::array<float, kBlockSize>* y_last_block,
                         FftData* G_last_block) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter main_filter(9, DetectOptimization(), &data_dumper);
  AdaptiveFirFilter shadow_filter(9, DetectOptimization(), &data_dumper);
  Aec3Fft fft;
  RenderBuffer render_buffer(
      Aec3Optimization::kNone, 3, main_filter.SizePartitions(),
      std::vector<size_t>(1, main_filter.SizePartitions()));
  std::array<float, kBlockSize> x_old;
  x_old.fill(0.f);
  ShadowFilterUpdateGain shadow_gain;
  MainFilterUpdateGain main_gain;
  Random random_generator(42U);
  std::vector<std::vector<float>> x(3, std::vector<float>(kBlockSize, 0.f));
  std::vector<float> y(kBlockSize, 0.f);
  AecState aec_state;
  RenderSignalAnalyzer render_signal_analyzer;
  std::array<float, kFftLength> s;
  FftData S;
  FftData G;
  SubtractorOutput output;
  output.Reset();
  FftData& E_main = output.E_main;
  FftData E_shadow;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1>& E2_main = output.E2_main;
  std::array<float, kBlockSize>& e_main = output.e_main;
  std::array<float, kBlockSize>& e_shadow = output.e_shadow;
  Y2.fill(0.f);

  constexpr float kScale = 1.0f / kFftLengthBy2;

  DelayBuffer<float> delay_buffer(delay_samples);
  for (int k = 0; k < num_blocks_to_process; ++k) {
    // Handle echo path changes.
    if (std::find(blocks_with_echo_path_changes.begin(),
                  blocks_with_echo_path_changes.end(),
                  k) != blocks_with_echo_path_changes.end()) {
      main_filter.HandleEchoPathChange();
    }

    // Handle saturation.
    const bool saturation =
        std::find(blocks_with_saturation.begin(), blocks_with_saturation.end(),
                  k) != blocks_with_saturation.end();

    // Create the render signal.
    if (use_silent_render_in_second_half && k > num_blocks_to_process / 2) {
      std::fill(x[0].begin(), x[0].end(), 0.f);
    } else {
      RandomizeSampleVector(&random_generator, x[0]);
    }
    delay_buffer.Delay(x[0], y);
    render_buffer.Insert(x);
    render_signal_analyzer.Update(render_buffer, aec_state.FilterDelay());

    // Apply the main filter.
    main_filter.Filter(render_buffer, &S);
    fft.Ifft(S, &s);
    std::transform(y.begin(), y.end(), s.begin() + kFftLengthBy2,
                   e_main.begin(),
                   [&](float a, float b) { return a - b * kScale; });
    std::for_each(e_main.begin(), e_main.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
    fft.ZeroPaddedFft(e_main, &E_main);

    // Apply the shadow filter.
    shadow_filter.Filter(render_buffer, &S);
    fft.Ifft(S, &s);
    std::transform(y.begin(), y.end(), s.begin() + kFftLengthBy2,
                   e_shadow.begin(),
                   [&](float a, float b) { return a - b * kScale; });
    std::for_each(e_shadow.begin(), e_shadow.end(),
                  [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
    fft.ZeroPaddedFft(e_shadow, &E_shadow);

    // Compute spectra for future use.
    E_main.Spectrum(Aec3Optimization::kNone, &output.E2_main);
    E_shadow.Spectrum(Aec3Optimization::kNone, &output.E2_shadow);

    // Adapt the shadow filter.
    shadow_gain.Compute(render_buffer, render_signal_analyzer, E_shadow,
                        shadow_filter.SizePartitions(), saturation, &G);
    shadow_filter.Adapt(render_buffer, G);

    // Adapt the main filter
    main_gain.Compute(render_buffer, render_signal_analyzer, output,
                      main_filter, saturation, &G);
    main_filter.Adapt(render_buffer, G);

    // Update the delay.
    aec_state.HandleEchoPathChange(EchoPathVariability(false, false));
    aec_state.Update(main_filter.FilterFrequencyResponse(),
                     rtc::Optional<size_t>(), render_buffer, E2_main, Y2, x[0],
                     false);
  }

  std::copy(e_main.begin(), e_main.end(), e_last_block->begin());
  std::copy(y.begin(), y.end(), y_last_block->begin());
  std::copy(G.re.begin(), G.re.end(), G_last_block->re.begin());
  std::copy(G.im.begin(), G.im.end(), G_last_block->im.begin());
}

std::string ProduceDebugText(size_t delay) {
  std::ostringstream ss;
  ss << "Delay: " << delay;
  return ss.str();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output gain parameter works.
TEST(MainFilterUpdateGain, NullDataOutputGain) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, DetectOptimization(), &data_dumper);
  RenderBuffer render_buffer(Aec3Optimization::kNone, 3,
                             filter.SizePartitions(),
                             std::vector<size_t>(1, filter.SizePartitions()));
  RenderSignalAnalyzer analyzer;
  SubtractorOutput output;
  MainFilterUpdateGain gain;
  EXPECT_DEATH(
      gain.Compute(render_buffer, analyzer, output, filter, false, nullptr),
      "");
}

#endif

// Verifies that the gain formed causes the filter using it to converge.
TEST(MainFilterUpdateGain, GainCausesFilterToConverge) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;
  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    SCOPED_TRACE(ProduceDebugText(delay_samples));

    std::array<float, kBlockSize> e;
    std::array<float, kBlockSize> y;
    FftData G;

    RunFilterUpdateTest(500, delay_samples, blocks_with_echo_path_changes,
                        blocks_with_saturation, false, &e, &y, &G);

    // Verify that the main filter is able to perform well.
    EXPECT_LT(1000 * std::inner_product(e.begin(), e.end(), e.begin(), 0.f),
              std::inner_product(y.begin(), y.end(), y.begin(), 0.f));
  }
}

// Verifies that the magnitude of the gain on average decreases for a
// persistently exciting signal.
TEST(MainFilterUpdateGain, DecreasingGain) {
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

  RunFilterUpdateTest(100, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_a);
  RunFilterUpdateTest(200, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_b);
  RunFilterUpdateTest(300, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_c);

  G_a.Spectrum(Aec3Optimization::kNone, &G_a_power);
  G_b.Spectrum(Aec3Optimization::kNone, &G_b_power);
  G_c.Spectrum(Aec3Optimization::kNone, &G_c_power);

  EXPECT_GT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
            std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));

  EXPECT_GT(std::accumulate(G_b_power.begin(), G_b_power.end(), 0.),
            std::accumulate(G_c_power.begin(), G_c_power.end(), 0.));
}

// Verifies that the gain is zero when there is saturation and that the internal
// error estimates cause the gain to increase after a period of saturation.
TEST(MainFilterUpdateGain, SaturationBehavior) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;
  for (int k = 99; k < 200; ++k) {
    blocks_with_saturation.push_back(k);
  }

  std::array<float, kBlockSize> e;
  std::array<float, kBlockSize> y;
  FftData G_a;
  FftData G_b;
  FftData G_a_ref;
  G_a_ref.re.fill(0.f);
  G_a_ref.im.fill(0.f);

  std::array<float, kFftLengthBy2Plus1> G_a_power;
  std::array<float, kFftLengthBy2Plus1> G_b_power;

  RunFilterUpdateTest(100, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_a);

  EXPECT_EQ(G_a_ref.re, G_a.re);
  EXPECT_EQ(G_a_ref.im, G_a.im);

  RunFilterUpdateTest(99, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_a);
  RunFilterUpdateTest(201, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_b);

  G_a.Spectrum(Aec3Optimization::kNone, &G_a_power);
  G_b.Spectrum(Aec3Optimization::kNone, &G_b_power);

  EXPECT_LT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
            std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));
}

// Verifies that the gain increases after an echo path change.
TEST(MainFilterUpdateGain, EchoPathChangeBehavior) {
  std::vector<int> blocks_with_echo_path_changes;
  std::vector<int> blocks_with_saturation;
  blocks_with_echo_path_changes.push_back(99);

  std::array<float, kBlockSize> e;
  std::array<float, kBlockSize> y;
  FftData G_a;
  FftData G_b;
  std::array<float, kFftLengthBy2Plus1> G_a_power;
  std::array<float, kFftLengthBy2Plus1> G_b_power;

  RunFilterUpdateTest(99, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_a);
  RunFilterUpdateTest(100, 65, blocks_with_echo_path_changes,
                      blocks_with_saturation, false, &e, &y, &G_b);

  G_a.Spectrum(Aec3Optimization::kNone, &G_a_power);
  G_b.Spectrum(Aec3Optimization::kNone, &G_b_power);

  EXPECT_LT(std::accumulate(G_a_power.begin(), G_a_power.end(), 0.),
            std::accumulate(G_b_power.begin(), G_b_power.end(), 0.));
}

}  // namespace webrtc
