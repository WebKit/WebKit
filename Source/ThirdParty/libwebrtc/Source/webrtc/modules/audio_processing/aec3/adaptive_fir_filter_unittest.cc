/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/adaptive_fir_filter.h"

#include <algorithm>
#include <numeric>
#include <string>
#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include "webrtc/base/arraysize.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/aec3/render_signal_analyzer.h"
#include "webrtc/modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {

std::string ProduceDebugText(size_t delay) {
  std::ostringstream ss;
  ss << ", Delay: " << delay;
  return ss.str();
}

}  // namespace

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods are bitexact to their reference
// counterparts.
TEST(AdaptiveFirFilter, TestOptimizations) {
  bool use_sse2 = (WebRtc_GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    FftBuffer X_buffer(Aec3Optimization::kNone, 12, std::vector<size_t>(1, 12));
    std::array<float, kBlockSize> x_old;
    x_old.fill(0.f);
    Random random_generator(42U);
    std::vector<float> x(kBlockSize, 0.f);
    FftData X;
    FftData S_C;
    FftData S_SSE2;
    FftData G;
    Aec3Fft fft;
    std::vector<FftData> H_C(10);
    std::vector<FftData> H_SSE2(10);
    for (auto& H_j : H_C) {
      H_j.Clear();
    }
    for (auto& H_j : H_SSE2) {
      H_j.Clear();
    }

    for (size_t k = 0; k < 500; ++k) {
      RandomizeSampleVector(&random_generator, x);
      fft.PaddedFft(x, x_old, &X);
      X_buffer.Insert(X);

      ApplyFilter_SSE2(X_buffer, H_SSE2, &S_SSE2);
      ApplyFilter(X_buffer, H_C, &S_C);
      for (size_t j = 0; j < S_C.re.size(); ++j) {
        EXPECT_FLOAT_EQ(S_C.re[j], S_SSE2.re[j]);
        EXPECT_FLOAT_EQ(S_C.im[j], S_SSE2.im[j]);
      }

      std::for_each(G.re.begin(), G.re.end(),
                    [&](float& a) { a = random_generator.Rand<float>(); });
      std::for_each(G.im.begin(), G.im.end(),
                    [&](float& a) { a = random_generator.Rand<float>(); });

      AdaptPartitions_SSE2(X_buffer, G, H_SSE2);
      AdaptPartitions(X_buffer, G, H_C);

      for (size_t k = 0; k < H_C.size(); ++k) {
        for (size_t j = 0; j < H_C[k].re.size(); ++j) {
          EXPECT_FLOAT_EQ(H_C[k].re[j], H_SSE2[k].re[j]);
          EXPECT_FLOAT_EQ(H_C[k].im[j], H_SSE2[k].im[j]);
        }
      }
    }
  }
}

#endif

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the check for non-null data dumper works.
TEST(AdaptiveFirFilter, NullDataDumper) {
  EXPECT_DEATH(AdaptiveFirFilter(9, true, DetectOptimization(), nullptr), "");
}

// Verifies that the check for non-null filter output works.
TEST(AdaptiveFirFilter, NullFilterOutput) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, true, DetectOptimization(), &data_dumper);
  FftBuffer X_buffer(Aec3Optimization::kNone, filter.SizePartitions(),
                     std::vector<size_t>(1, filter.SizePartitions()));
  EXPECT_DEATH(filter.Filter(X_buffer, nullptr), "");
}

// Verifies that the check for whether filter statistics are being generated
// works when retrieving the ERL.
TEST(AdaptiveFirFilter, ErlAccessWhenNoFilterStatistics) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, false, DetectOptimization(), &data_dumper);
  EXPECT_DEATH(filter.Erl(), "");
}

// Verifies that the check for whether filter statistics are being generated
// works when retrieving the filter frequencyResponse.
TEST(AdaptiveFirFilter, FilterFrequencyResponseAccessWhenNoFilterStatistics) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, false, DetectOptimization(), &data_dumper);
  EXPECT_DEATH(filter.FilterFrequencyResponse(), "");
}

#endif

// Verifies that the filter statistics can be accessed when filter statistics
// are turned on.
TEST(AdaptiveFirFilter, FilterStatisticsAccess) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, true, DetectOptimization(), &data_dumper);
  filter.Erl();
  filter.FilterFrequencyResponse();
}

// Verifies that the filter size if correctly repported.
TEST(AdaptiveFirFilter, FilterSize) {
  ApmDataDumper data_dumper(42);
  for (size_t filter_size = 1; filter_size < 5; ++filter_size) {
    AdaptiveFirFilter filter(filter_size, false, DetectOptimization(),
                             &data_dumper);
    EXPECT_EQ(filter_size, filter.SizePartitions());
  }
}

// Verifies that the filter is being able to properly filter a signal and to
// adapt its coefficients.
TEST(AdaptiveFirFilter, FilterAndAdapt) {
  constexpr size_t kNumBlocksToProcess = 500;
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, true, DetectOptimization(), &data_dumper);
  Aec3Fft fft;
  FftBuffer X_buffer(Aec3Optimization::kNone, filter.SizePartitions(),
                     std::vector<size_t>(1, filter.SizePartitions()));
  std::array<float, kBlockSize> x_old;
  x_old.fill(0.f);
  ShadowFilterUpdateGain gain;
  Random random_generator(42U);
  std::vector<float> x(kBlockSize, 0.f);
  std::vector<float> y(kBlockSize, 0.f);
  AecState aec_state;
  RenderSignalAnalyzer render_signal_analyzer;
  FftData X;
  std::vector<float> e(kBlockSize, 0.f);
  std::array<float, kFftLength> s;
  FftData S;
  FftData G;
  FftData E;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  Y2.fill(0.f);
  E2_main.fill(0.f);
  E2_shadow.fill(0.f);

  constexpr float kScale = 1.0f / kFftLengthBy2;

  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    DelayBuffer<float> delay_buffer(delay_samples);
    SCOPED_TRACE(ProduceDebugText(delay_samples));
    for (size_t k = 0; k < kNumBlocksToProcess; ++k) {
      RandomizeSampleVector(&random_generator, x);
      delay_buffer.Delay(x, y);

      fft.PaddedFft(x, x_old, &X);
      X_buffer.Insert(X);
      render_signal_analyzer.Update(X_buffer, aec_state.FilterDelay());

      filter.Filter(X_buffer, &S);
      fft.Ifft(S, &s);
      std::transform(y.begin(), y.end(), s.begin() + kFftLengthBy2, e.begin(),
                     [&](float a, float b) { return a - b * kScale; });
      std::for_each(e.begin(), e.end(), [](float& a) {
        a = std::max(std::min(a, 32767.0f), -32768.0f);
      });
      fft.ZeroPaddedFft(e, &E);

      gain.Compute(X_buffer, render_signal_analyzer, E, filter.SizePartitions(),
                   false, &G);
      filter.Adapt(X_buffer, G);
      aec_state.Update(filter.FilterFrequencyResponse(),
                       rtc::Optional<size_t>(), X_buffer, E2_main, E2_shadow,
                       Y2, x, EchoPathVariability(false, false), false);
    }
    // Verify that the filter is able to perform well.
    EXPECT_LT(1000 * std::inner_product(e.begin(), e.end(), e.begin(), 0.f),
              std::inner_product(y.begin(), y.end(), y.begin(), 0.f));
    ASSERT_TRUE(aec_state.FilterDelay());
    EXPECT_EQ(delay_samples / kBlockSize, *aec_state.FilterDelay());
  }
}
}  // namespace aec3
}  // namespace webrtc
