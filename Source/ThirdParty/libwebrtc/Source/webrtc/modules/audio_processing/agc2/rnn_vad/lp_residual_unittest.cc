/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/lp_residual.h"

#include <algorithm>
#include <array>
#include <vector>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

// Checks that the LP residual can be computed on an empty frame.
TEST(RnnVadTest, LpResidualOfEmptyFrame) {
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // Input frame (empty, i.e., all samples set to 0).
  std::array<float, kFrameSize10ms24kHz> empty_frame;
  empty_frame.fill(0.f);
  // Compute inverse filter coefficients.
  std::array<float, kNumLpcCoefficients> lpc;
  ComputeAndPostProcessLpcCoefficients(empty_frame, lpc);
  // Compute LP residual.
  std::array<float, kFrameSize10ms24kHz> lp_residual;
  ComputeLpResidual(lpc, empty_frame, lp_residual);
}

// Checks that the computed LP residual is bit-exact given test input data.
TEST(RnnVadTest, LpResidualPipelineBitExactness) {
  // Input and expected output readers.
  ChunksFileReader pitch_buffer_reader = CreatePitchBuffer24kHzReader();
  ChunksFileReader lp_pitch_reader = CreateLpResidualAndPitchInfoReader();

  // Buffers.
  std::vector<float> pitch_buffer_24kHz(kBufSize24kHz);
  std::array<float, kNumLpcCoefficients> lpc;
  std::vector<float> computed_lp_residual(kBufSize24kHz);
  std::vector<float> expected_lp_residual(kBufSize24kHz);

  // Test length.
  const int num_frames =
      std::min(pitch_buffer_reader.num_chunks, 300);  // Max 3 s.
  ASSERT_GE(lp_pitch_reader.num_chunks, num_frames);

  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  for (int i = 0; i < num_frames; ++i) {
    SCOPED_TRACE(i);
    // Read input.
    ASSERT_TRUE(pitch_buffer_reader.reader->ReadChunk(pitch_buffer_24kHz));
    // Read expected output (ignore pitch gain and period).
    ASSERT_TRUE(lp_pitch_reader.reader->ReadChunk(expected_lp_residual));
    lp_pitch_reader.reader->SeekForward(2);  // Pitch period and strength.
    // Check every 200 ms.
    if (i % 20 == 0) {
      ComputeAndPostProcessLpcCoefficients(pitch_buffer_24kHz, lpc);
      ComputeLpResidual(lpc, pitch_buffer_24kHz, computed_lp_residual);
      ExpectNearAbsolute(expected_lp_residual, computed_lp_residual, kFloatMin);
    }
  }
}

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
