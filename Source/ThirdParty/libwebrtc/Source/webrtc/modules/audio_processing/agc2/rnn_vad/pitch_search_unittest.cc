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
#include <array>

#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {

// Checks that the computed pitch period is bit-exact and that the computed
// pitch gain is within tolerance given test input data.
TEST(RnnVadTest, PitchSearchWithinTolerance) {
  ChunksFileReader reader = CreateLpResidualAndPitchInfoReader();
  const int num_frames = std::min(reader.num_chunks, 300);  // Max 3 s.
  std::array<float, kBufSize24kHz> lp_residual = {};
  float expected_pitch_period, expected_pitch_strength;
  const AvailableCpuFeatures cpu_features = GetAvailableCpuFeatures();
  PitchEstimator pitch_estimator(cpu_features);
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    for (int i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      ASSERT_TRUE(reader.reader->ReadChunk(lp_residual));
      ASSERT_TRUE(reader.reader->ReadValue(expected_pitch_period));
      ASSERT_TRUE(reader.reader->ReadValue(expected_pitch_strength));
      int pitch_period = pitch_estimator.Estimate(lp_residual);
      EXPECT_EQ(expected_pitch_period, pitch_period);
      EXPECT_NEAR(expected_pitch_strength,
                  pitch_estimator.GetLastPitchStrengthForTesting(), 15e-6f);
    }
  }
}

}  // namespace rnn_vad
}  // namespace webrtc
