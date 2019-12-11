/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/fft_util.h"
#include "rtc_base/checks.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

TEST(RnnVadTest, CheckBandAnalysisFftOutput) {
  // Input data.
  std::array<float, kFrameSize20ms24kHz> samples{};
  for (int i = 0; i < static_cast<int>(kFrameSize20ms24kHz); ++i) {
    samples[i] = i - static_cast<int>(kFrameSize20ms24kHz / 2);
  }
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  BandAnalysisFft fft;
  std::array<std::complex<float>, kFrameSize20ms24kHz> fft_coeffs;
  fft.ForwardFft(samples, fft_coeffs);
  // First coefficient is DC - i.e., real number.
  EXPECT_EQ(0.f, fft_coeffs[0].imag());
  // Check conjugated symmetry of the FFT output.
  for (size_t i = 1; i < fft_coeffs.size() / 2; ++i) {
    SCOPED_TRACE(i);
    const auto& a = fft_coeffs[i];
    const auto& b = fft_coeffs[fft_coeffs.size() - i];
    EXPECT_NEAR(a.real(), b.real(), 2e-6f);
    EXPECT_NEAR(a.imag(), -b.imag(), 2e-6f);
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
