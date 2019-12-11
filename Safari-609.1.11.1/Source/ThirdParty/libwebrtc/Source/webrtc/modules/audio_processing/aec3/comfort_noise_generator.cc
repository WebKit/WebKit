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

// Defines WEBRTC_ARCH_X86_FAMILY, used below.
#include "rtc_base/system/arch.h"

#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_processing/aec3/vector_math.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Table of sqrt(2) * sin(2*pi*i/32).
constexpr float kSqrt2Sin[32] = {
    +0.0000000f, +0.2758994f, +0.5411961f, +0.7856950f, +1.0000000f,
    +1.1758756f, +1.3065630f, +1.3870398f, +1.4142136f, +1.3870398f,
    +1.3065630f, +1.1758756f, +1.0000000f, +0.7856950f, +0.5411961f,
    +0.2758994f, +0.0000000f, -0.2758994f, -0.5411961f, -0.7856950f,
    -1.0000000f, -1.1758756f, -1.3065630f, -1.3870398f, -1.4142136f,
    -1.3870398f, -1.3065630f, -1.1758756f, -1.0000000f, -0.7856950f,
    -0.5411961f, -0.2758994f};

void GenerateComfortNoise(Aec3Optimization optimization,
                          const std::array<float, kFftLengthBy2Plus1>& N2,
                          uint32_t* seed,
                          FftData* lower_band_noise,
                          FftData* upper_band_noise) {
  FftData* N_low = lower_band_noise;
  FftData* N_high = upper_band_noise;

  // Compute square root spectrum.
  std::array<float, kFftLengthBy2Plus1> N;
  std::copy(N2.begin(), N2.end(), N.begin());
  aec3::VectorMath(optimization).Sqrt(N);

  // Compute the noise level for the upper bands.
  constexpr float kOneByNumBands = 1.f / (kFftLengthBy2Plus1 / 2 + 1);
  constexpr int kFftLengthBy2Plus1By2 = kFftLengthBy2Plus1 / 2;
  const float high_band_noise_level =
      std::accumulate(N.begin() + kFftLengthBy2Plus1By2, N.end(), 0.f) *
      kOneByNumBands;

  // The analysis and synthesis windowing cause loss of power when
  // cross-fading the noise where frames are completely uncorrelated
  // (generated with random phase), hence the factor sqrt(2).
  // This is not the case for the speech signal where the input is overlapping
  // (strong correlation).
  N_low->re[0] = N_low->re[kFftLengthBy2] = N_high->re[0] =
      N_high->re[kFftLengthBy2] = 0.f;
  for (size_t k = 1; k < kFftLengthBy2; k++) {
    constexpr int kIndexMask = 32 - 1;
    // Generate a random 31-bit integer.
    seed[0] = (seed[0] * 69069 + 1) & (0x80000000 - 1);
    // Convert to a 5-bit index.
    int i = seed[0] >> 26;

    // y = sqrt(2) * sin(a)
    const float x = kSqrt2Sin[i];
    // x = sqrt(2) * cos(a) = sqrt(2) * sin(a + pi/2)
    const float y = kSqrt2Sin[(i + 8) & kIndexMask];

    // Form low-frequency noise via spectral shaping.
    N_low->re[k] = N[k] * x;
    N_low->im[k] = N[k] * y;

    // Form the high-frequency noise via simple levelling.
    N_high->re[k] = high_band_noise_level * x;
    N_high->im[k] = high_band_noise_level * y;
  }
}

}  // namespace

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

  // Limit the noise to a floor matching a WGN input of -96 dBFS.
  constexpr float kNoiseFloor = 17.1267f;

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

  GenerateComfortNoise(optimization_, N2, &seed_, lower_band_noise,
                       upper_band_noise);
}

}  // namespace webrtc
