/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/comfort_noise_generator.h"

#include <algorithm>
#include <numeric>

#include "rtc_base/random.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
namespace aec3 {
namespace {

float Power(const FftData& N) {
  std::array<float, kFftLengthBy2Plus1> N2;
  N.Spectrum(Aec3Optimization::kNone, N2);
  return std::accumulate(N2.begin(), N2.end(), 0.f) / N2.size();
}

}  // namespace

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(ComfortNoiseGenerator, NullLowerBandNoise) {
  std::array<float, kFftLengthBy2Plus1> N2;
  FftData noise;
  EXPECT_DEATH(
      ComfortNoiseGenerator(DetectOptimization())
          .Compute(AecState(EchoCanceller3Config{}), N2, nullptr, &noise),
      "");
}

TEST(ComfortNoiseGenerator, NullUpperBandNoise) {
  std::array<float, kFftLengthBy2Plus1> N2;
  FftData noise;
  EXPECT_DEATH(
      ComfortNoiseGenerator(DetectOptimization())
          .Compute(AecState(EchoCanceller3Config{}), N2, &noise, nullptr),
      "");
}

#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods are bitexact to their reference
// counterparts.
TEST(ComfortNoiseGenerator, TestOptimizations) {
  if (WebRtc_GetCPUInfo(kSSE2) != 0) {
    Random random_generator(42U);
    uint32_t seed = 42;
    uint32_t seed_SSE2 = 42;
    std::array<float, kFftLengthBy2Plus1> N2;
    FftData lower_band_noise;
    FftData upper_band_noise;
    FftData lower_band_noise_SSE2;
    FftData upper_band_noise_SSE2;
    for (int k = 0; k < 10; ++k) {
      for (size_t j = 0; j < N2.size(); ++j) {
        N2[j] = random_generator.Rand<float>() * 1000.f;
      }

      EstimateComfortNoise(N2, &seed, &lower_band_noise, &upper_band_noise);
      EstimateComfortNoise_SSE2(N2, &seed_SSE2, &lower_band_noise_SSE2,
                                &upper_band_noise_SSE2);
      for (size_t j = 0; j < lower_band_noise.re.size(); ++j) {
        EXPECT_NEAR(lower_band_noise.re[j], lower_band_noise_SSE2.re[j],
                    0.00001f);
        EXPECT_NEAR(upper_band_noise.re[j], upper_band_noise_SSE2.re[j],
                    0.00001f);
      }
      for (size_t j = 1; j < lower_band_noise.re.size() - 1; ++j) {
        EXPECT_NEAR(lower_band_noise.im[j], lower_band_noise_SSE2.im[j],
                    0.00001f);
        EXPECT_NEAR(upper_band_noise.im[j], upper_band_noise_SSE2.im[j],
                    0.00001f);
      }
    }
  }
}

#endif

TEST(ComfortNoiseGenerator, CorrectLevel) {
  ComfortNoiseGenerator cng(DetectOptimization());
  AecState aec_state(EchoCanceller3Config{});

  std::array<float, kFftLengthBy2Plus1> N2;
  N2.fill(1000.f * 1000.f);

  FftData n_lower;
  FftData n_upper;
  n_lower.re.fill(0.f);
  n_lower.im.fill(0.f);
  n_upper.re.fill(0.f);
  n_upper.im.fill(0.f);

  // Ensure instantaneous updata to nonzero noise.
  cng.Compute(aec_state, N2, &n_lower, &n_upper);
  EXPECT_LT(0.f, Power(n_lower));
  EXPECT_LT(0.f, Power(n_upper));

  for (int k = 0; k < 10000; ++k) {
    cng.Compute(aec_state, N2, &n_lower, &n_upper);
  }
  EXPECT_NEAR(N2[0], Power(n_lower), N2[0] / 10.f);
  EXPECT_NEAR(N2[0], Power(n_upper), N2[0] / 10.f);
}

}  // namespace aec3
}  // namespace webrtc
