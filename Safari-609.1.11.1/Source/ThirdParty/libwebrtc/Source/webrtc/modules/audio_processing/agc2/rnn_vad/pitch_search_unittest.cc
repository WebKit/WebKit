/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search.h"

#include <algorithm>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/pitch_info.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

// Checks that the computed pitch period is bit-exact and that the computed
// pitch gain is within tolerance given test input data.
TEST(RnnVadTest, PitchSearchWithinTolerance) {
  auto lp_residual_reader = CreateLpResidualAndPitchPeriodGainReader();
  const size_t num_frames = std::min(lp_residual_reader.second,
                                     static_cast<size_t>(300));  // Max 3 s.
  std::vector<float> lp_residual(kBufSize24kHz);
  float expected_pitch_period, expected_pitch_gain;
  PitchEstimator pitch_estimator;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      lp_residual_reader.first->ReadChunk(lp_residual);
      lp_residual_reader.first->ReadValue(&expected_pitch_period);
      lp_residual_reader.first->ReadValue(&expected_pitch_gain);
      PitchInfo pitch_info =
          pitch_estimator.Estimate({lp_residual.data(), kBufSize24kHz});
      EXPECT_EQ(static_cast<int>(expected_pitch_period), pitch_info.period);
      EXPECT_NEAR(expected_pitch_gain, pitch_info.gain, 1e-5f);
    }
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
