/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/spectral_features_internal.h"

#include <algorithm>
#include <array>
#include <complex>
#include <numeric>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "modules/audio_processing/utility/pffft_wrapper.h"
#include "rtc_base/numerics/safe_compare.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Generates the values for the array named `kOpusBandWeights24kHz20ms` in the
// anonymous namespace of the .cc file, which is the array of FFT coefficient
// weights for the Opus scale triangular filters.
std::vector<float> ComputeTriangularFiltersWeights() {
  constexpr auto kOpusScaleNumBins24kHz20ms = GetOpusScaleNumBins24kHz20ms();
  const auto& v = kOpusScaleNumBins24kHz20ms;  // Alias.
  const int num_weights = std::accumulate(kOpusScaleNumBins24kHz20ms.begin(),
                                          kOpusScaleNumBins24kHz20ms.end(), 0);
  std::vector<float> weights(num_weights);
  int next_fft_coeff_index = 0;
  for (int band = 0; rtc::SafeLt(band, v.size()); ++band) {
    const int band_size = v[band];
    for (int j = 0; rtc::SafeLt(j, band_size); ++j) {
      weights[next_fft_coeff_index + j] = static_cast<float>(j) / band_size;
    }
    next_fft_coeff_index += band_size;
  }
  return weights;
}

// Checks that the values returned by GetOpusScaleNumBins24kHz20ms() match the
// Opus scale frequency boundaries.
TEST(RnnVadTest, TestOpusScaleBoundaries) {
  constexpr int kBandFrequencyBoundariesHz[kNumBands - 1] = {
      200,  400,  600,  800,  1000, 1200, 1400, 1600,  2000,  2400, 2800,
      3200, 4000, 4800, 5600, 6800, 8000, 9600, 12000, 15600, 20000};
  constexpr auto kOpusScaleNumBins24kHz20ms = GetOpusScaleNumBins24kHz20ms();
  int prev = 0;
  for (int i = 0; rtc::SafeLt(i, kOpusScaleNumBins24kHz20ms.size()); ++i) {
    int boundary =
        kBandFrequencyBoundariesHz[i] * kFrameSize20ms24kHz / kSampleRate24kHz;
    EXPECT_EQ(kOpusScaleNumBins24kHz20ms[i], boundary - prev);
    prev = boundary;
  }
}

// Checks that the computed triangular filters weights for the Opus scale are
// monotonic withing each Opus band. This test should only be enabled when
// ComputeTriangularFiltersWeights() is changed and `kOpusBandWeights24kHz20ms`
// is updated accordingly.
TEST(RnnVadTest, DISABLED_TestOpusScaleWeights) {
  auto weights = ComputeTriangularFiltersWeights();
  int i = 0;
  for (int band_size : GetOpusScaleNumBins24kHz20ms()) {
    SCOPED_TRACE(band_size);
    rtc::ArrayView<float> band_weights(weights.data() + i, band_size);
    float prev = -1.f;
    for (float weight : band_weights) {
      EXPECT_LT(prev, weight);
      prev = weight;
    }
    i += band_size;
  }
}

// Checks that the computed band-wise auto-correlation is non-negative for a
// simple input vector of FFT coefficients.
TEST(RnnVadTest, SpectralCorrelatorValidOutput) {
  // Input: vector of (1, 1j) values.
  Pffft fft(kFrameSize20ms24kHz, Pffft::FftType::kReal);
  auto in = fft.CreateBuffer();
  std::array<float, kOpusBands24kHz> out;
  auto in_view = in->GetView();
  std::fill(in_view.begin(), in_view.end(), 1.f);
  in_view[1] = 0.f;  // Nyquist frequency.
  // Compute and check output.
  SpectralCorrelator e;
  e.ComputeAutoCorrelation(in_view, out);
  for (int i = 0; i < kOpusBands24kHz; ++i) {
    SCOPED_TRACE(i);
    EXPECT_GT(out[i], 0.f);
  }
}

// Checks that the computed smoothed log magnitude spectrum is within tolerance
// given hard-coded test input data.
TEST(RnnVadTest, ComputeSmoothedLogMagnitudeSpectrumWithinTolerance) {
  constexpr std::array<float, kNumBands> input = {
      {86.060539245605f, 275.668334960938f, 43.406528472900f, 6.541896820068f,
       17.964015960693f, 8.090919494629f,   1.261920094490f,  1.212702631950f,
       1.619154453278f,  0.508935272694f,   0.346316039562f,  0.237035423517f,
       0.172424271703f,  0.271657168865f,   0.126088857651f,  0.139967113733f,
       0.207200810313f,  0.155893072486f,   0.091090843081f,  0.033391401172f,
       0.013879744336f,  0.011973354965f}};
  constexpr std::array<float, kNumBands> expected_output = {
      {1.934854507446f,  2.440402746201f,  1.637655138969f,  0.816367030144f,
       1.254645109177f,  0.908534288406f,  0.104459829628f,  0.087320849299f,
       0.211962252855f,  -0.284886807203f, -0.448164641857f, -0.607240796089f,
       -0.738917350769f, -0.550279200077f, -0.866177439690f, -0.824003994465f,
       -0.663138568401f, -0.780171751976f, -0.995288193226f, -1.362596273422f,
       -1.621970295906f, -1.658103585243f}};
  std::array<float, kNumBands> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    ComputeSmoothedLogMagnitudeSpectrum(input, computed_output);
    ExpectNearAbsolute(expected_output, computed_output, 1e-5f);
  }
}

// Checks that the computed DCT is within tolerance given hard-coded test input
// data.
TEST(RnnVadTest, ComputeDctWithinTolerance) {
  constexpr std::array<float, kNumBands> input = {
      {0.232155621052f,  0.678957760334f, 0.220818966627f,  -0.077363930643f,
       -0.559227049351f, 0.432545185089f, 0.353900641203f,  0.398993015289f,
       0.409774333239f,  0.454977899790f, 0.300520688295f,  -0.010286616161f,
       0.272525429726f,  0.098067551851f, 0.083649002016f,  0.046226885170f,
       -0.033228103071f, 0.144773483276f, -0.117661058903f, -0.005628800020f,
       -0.009547689930f, -0.045382082462f}};
  constexpr std::array<float, kNumBands> expected_output = {
      {0.697072803974f,  0.442710995674f,  -0.293156713247f, -0.060711503029f,
       0.292050391436f,  0.489301353693f,  0.402255415916f,  0.134404733777f,
       -0.086305990815f, -0.199605688453f, -0.234511867166f, -0.413774639368f,
       -0.388507157564f, -0.032798115164f, 0.044605545700f,  0.112466648221f,
       -0.050096966326f, 0.045971218497f,  -0.029815061018f, -0.410366982222f,
       -0.209233760834f, -0.128037497401f}};
  auto dct_table = ComputeDctTable();
  std::array<float, kNumBands> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    ComputeDct(input, dct_table, computed_output);
    ExpectNearAbsolute(expected_output, computed_output, 1e-5f);
  }
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
