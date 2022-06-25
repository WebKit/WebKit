/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/auto_correlation.h"

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Checks that the auto correlation function produces output within tolerance
// given test input data.
TEST(RnnVadTest, PitchBufferAutoCorrelationWithinTolerance) {
  PitchTestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.PitchBuffer24kHzView(), pitch_buf_decimated);
  std::array<float, kNumLags12kHz> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    AutoCorrelationCalculator auto_corr_calculator;
    auto_corr_calculator.ComputeOnPitchBuffer(pitch_buf_decimated,
                                              computed_output);
  }
  auto auto_corr_view = test_data.AutoCorrelation12kHzView();
  ExpectNearAbsolute({auto_corr_view.data(), auto_corr_view.size()},
                     computed_output, 3e-3f);
}

// Checks that the auto correlation function computes the right thing for a
// simple use case.
TEST(RnnVadTest, CheckAutoCorrelationOnConstantPitchBuffer) {
  // Create constant signal with no pitch.
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  std::fill(pitch_buf_decimated.begin(), pitch_buf_decimated.end(), 1.f);
  std::array<float, kNumLags12kHz> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    AutoCorrelationCalculator auto_corr_calculator;
    auto_corr_calculator.ComputeOnPitchBuffer(pitch_buf_decimated,
                                              computed_output);
  }
  // The expected output is a vector filled with the same expected
  // auto-correlation value. The latter equals the length of a 20 ms frame.
  constexpr int kFrameSize20ms12kHz = kFrameSize20ms24kHz / 2;
  std::array<float, kNumLags12kHz> expected_output;
  std::fill(expected_output.begin(), expected_output.end(),
            static_cast<float>(kFrameSize20ms12kHz));
  ExpectNearAbsolute(expected_output, computed_output, 4e-5f);
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
