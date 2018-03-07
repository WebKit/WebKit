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

#include "typedefs.h"  // NOLINT(build/include)
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <math.h>
#include <algorithm>
#include <array>
#include <functional>
#include <numeric>

#include "common_audio/signal_processing/include/signal_processing_library.h"

namespace webrtc {

namespace {

// Creates an array of uniformly distributed variables.
void TableRandomValue(int16_t* vector, int16_t vector_length, uint32_t* seed) {
  for (int i = 0; i < vector_length; i++) {
    seed[0] = (seed[0] * ((int32_t)69069) + 1) & (0x80000000 - 1);
    vector[i] = (int16_t)(seed[0] >> 16);
  }
}

}  // namespace

namespace aec3 {

#if defined(WEBRTC_ARCH_X86_FAMILY)

void EstimateComfortNoise_SSE2(const std::array<float, kFftLengthBy2Plus1>& N2,
                               uint32_t* seed,
                               FftData* lower_band_noise,
                               FftData* upper_band_noise) {
  FftData* N_low = lower_band_noise;
  FftData* N_high = upper_band_noise;

  // Compute square root spectrum.
  std::array<float, kFftLengthBy2Plus1> N;
  for (size_t k = 0; k < kFftLengthBy2; k += 4) {
    __m128 v = _mm_loadu_ps(&N2[k]);
    v = _mm_sqrt_ps(v);
    _mm_storeu_ps(&N[k], v);
  }

  N[kFftLengthBy2] = sqrtf(N2[kFftLengthBy2]);

  // Compute the noise level for the upper bands.
  constexpr float kOneByNumBands = 1.f / (kFftLengthBy2Plus1 / 2 + 1);
  constexpr int kFftLengthBy2Plus1By2 = kFftLengthBy2Plus1 / 2;
  const float high_band_noise_level =
      std::accumulate(N.begin() + kFftLengthBy2Plus1By2, N.end(), 0.f) *
      kOneByNumBands;

  // Generate complex noise.
  std::array<int16_t, kFftLengthBy2 - 1> random_values_int;
  TableRandomValue(random_values_int.data(), random_values_int.size(), seed);

  std::array<float, kFftLengthBy2 - 1> sin;
  std::array<float, kFftLengthBy2 - 1> cos;
  constexpr float kScale = 6.28318530717959f / 32768.0f;
  std::transform(random_values_int.begin(), random_values_int.end(),
                 sin.begin(), [&](int16_t a) { return -sinf(kScale * a); });
  std::transform(random_values_int.begin(), random_values_int.end(),
                 cos.begin(), [&](int16_t a) { return cosf(kScale * a); });

  // Form low-frequency noise via spectral shaping.
  N_low->re[0] = N_low->re[kFftLengthBy2] = N_high->re[0] =
      N_high->re[kFftLengthBy2] = 0.f;
  std::transform(cos.begin(), cos.end(), N.begin() + 1, N_low->re.begin() + 1,
                 std::multiplies<float>());
  std::transform(sin.begin(), sin.end(), N.begin() + 1, N_low->im.begin() + 1,
                 std::multiplies<float>());

  // Form the high-frequency noise via simple levelling.
  std::transform(cos.begin(), cos.end(), N_high->re.begin() + 1,
                 [&](float a) { return high_band_noise_level * a; });
  std::transform(sin.begin(), sin.end(), N_high->im.begin() + 1,
                 [&](float a) { return high_band_noise_level * a; });
}

#endif

void EstimateComfortNoise(const std::array<float, kFftLengthBy2Plus1>& N2,
                          uint32_t* seed,
                          FftData* lower_band_noise,
                          FftData* upper_band_noise) {
  FftData* N_low = lower_band_noise;
  FftData* N_high = upper_band_noise;

  // Compute square root spectrum.
  std::array<float, kFftLengthBy2Plus1> N;
  std::transform(N2.begin(), N2.end(), N.begin(),
                 [](float a) { return sqrtf(a); });

  // Compute the noise level for the upper bands.
  constexpr float kOneByNumBands = 1.f / (kFftLengthBy2Plus1 / 2 + 1);
  constexpr int kFftLengthBy2Plus1By2 = kFftLengthBy2Plus1 / 2;
  const float high_band_noise_level =
      std::accumulate(N.begin() + kFftLengthBy2Plus1By2, N.end(), 0.f) *
      kOneByNumBands;

  // Generate complex noise.
  std::array<int16_t, kFftLengthBy2 - 1> random_values_int;
  TableRandomValue(random_values_int.data(), random_values_int.size(), seed);

  std::array<float, kFftLengthBy2 - 1> sin;
  std::array<float, kFftLengthBy2 - 1> cos;
  constexpr float kScale = 6.28318530717959f / 32768.0f;
  std::transform(random_values_int.begin(), random_values_int.end(),
                 sin.begin(), [&](int16_t a) { return -sinf(kScale * a); });
  std::transform(random_values_int.begin(), random_values_int.end(),
                 cos.begin(), [&](int16_t a) { return cosf(kScale * a); });

  // Form low-frequency noise via spectral shaping.
  N_low->re[0] = N_low->re[kFftLengthBy2] = N_high->re[0] =
      N_high->re[kFftLengthBy2] = 0.f;
  std::transform(cos.begin(), cos.end(), N.begin() + 1, N_low->re.begin() + 1,
                 std::multiplies<float>());
  std::transform(sin.begin(), sin.end(), N.begin() + 1, N_low->im.begin() + 1,
                 std::multiplies<float>());

  // Form the high-frequency noise via simple levelling.
  std::transform(cos.begin(), cos.end(), N_high->re.begin() + 1,
                 [&](float a) { return high_band_noise_level * a; });
  std::transform(sin.begin(), sin.end(), N_high->im.begin() + 1,
                 [&](float a) { return high_band_noise_level * a; });
}

}  // namespace aec3

ComfortNoiseGenerator::ComfortNoiseGenerator(Aec3Optimization optimization)
    : optimization_(optimization),
      seed_(42),
      N2_initial_(new std::array<float, kFftLengthBy2Plus1>()) {
  N2_initial_->fill(0.f);
  Y2_smoothed_.fill(0.f);
  N2_.fill(1.0e6f);
}

ComfortNoiseGenerator::~ComfortNoiseGenerator() = default;

void ComfortNoiseGenerator::Compute(
    const AecState& aec_state,
    const std::array<float, kFftLengthBy2Plus1>& capture_spectrum,
    FftData* lower_band_noise,
    FftData* upper_band_noise) {
  RTC_DCHECK(lower_band_noise);
  RTC_DCHECK(upper_band_noise);
  const auto& Y2 = capture_spectrum;

  if (!aec_state.SaturatedCapture()) {
    // Smooth Y2.
    std::transform(Y2_smoothed_.begin(), Y2_smoothed_.end(), Y2.begin(),
                   Y2_smoothed_.begin(),
                   [](float a, float b) { return a + 0.1f * (b - a); });

    if (N2_counter_ > 50) {
      // Update N2 from Y2_smoothed.
      std::transform(N2_.begin(), N2_.end(), Y2_smoothed_.begin(), N2_.begin(),
                     [](float a, float b) {
                       return b < a ? (0.9f * b + 0.1f * a) * 1.0002f
                                    : a * 1.0002f;
                     });
    }

    if (N2_initial_) {
      if (++N2_counter_ == 1000) {
        N2_initial_.reset();
      } else {
        // Compute the N2_initial from N2.
        std::transform(
            N2_.begin(), N2_.end(), N2_initial_->begin(), N2_initial_->begin(),
            [](float a, float b) { return a > b ? b + 0.001f * (a - b) : a; });
      }
    }
  }

  // Limit the noise to a floor of -96 dBFS.
  constexpr float kNoiseFloor = 440.f;
  for (auto& n : N2_) {
    n = std::max(n, kNoiseFloor);
  }
  if (N2_initial_) {
    for (auto& n : *N2_initial_) {
      n = std::max(n, kNoiseFloor);
    }
  }

  // Choose N2 estimate to use.
  const std::array<float, kFftLengthBy2Plus1>& N2 =
      N2_initial_ ? *N2_initial_ : N2_;

  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::EstimateComfortNoise_SSE2(N2, &seed_, lower_band_noise,
                                      upper_band_noise);
      break;
#endif
    default:
      aec3::EstimateComfortNoise(N2, &seed_, lower_band_noise,
                                 upper_band_noise);
  }
}

}  // namespace webrtc
