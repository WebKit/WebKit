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

#include <array>

#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {

TEST(RnnVadTest, LpResidualOfEmptyFrame) {
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;

  // Input frame (empty, i.e., all samples set to 0).
  std::array<float, kFrameSize10ms24kHz> empty_frame;
  empty_frame.fill(0.f);
  // Compute inverse filter coefficients.
  std::array<float, kNumLpcCoefficients> lpc_coeffs;
  ComputeAndPostProcessLpcCoefficients(empty_frame, lpc_coeffs);
  // Compute LP residual.
  std::array<float, kFrameSize10ms24kHz> lp_residual;
  ComputeLpResidual(lpc_coeffs, empty_frame, lp_residual);
}

// TODO(bugs.webrtc.org/9076): Remove when the issue is fixed.
TEST(RnnVadTest, LpResidualPipelineBitExactness) {
  // Pitch buffer 24 kHz data reader.
  auto pitch_buf_24kHz_reader = CreatePitchBuffer24kHzReader();
  const size_t num_frames = pitch_buf_24kHz_reader.second;
  std::array<float, kBufSize24kHz> pitch_buf_data;
  // Read ground-truth.
  auto lp_residual_reader = CreateLpResidualAndPitchPeriodGainReader();
  ASSERT_EQ(num_frames, lp_residual_reader.second);
  std::array<float, kBufSize24kHz> expected_lp_residual;
  rtc::ArrayView<float, kBufSize24kHz> expected_lp_residual_view(
      expected_lp_residual.data(), expected_lp_residual.size());
  // Init pipeline.
  std::array<float, kNumLpcCoefficients> lpc_coeffs;
  rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs_view(
      lpc_coeffs.data(), kNumLpcCoefficients);
  std::array<float, kBufSize24kHz> computed_lp_residual;
  rtc::ArrayView<float, kBufSize24kHz> computed_lp_residual_view(
      computed_lp_residual.data(), computed_lp_residual.size());
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    for (size_t i = 0; i < num_frames; ++i) {
      SCOPED_TRACE(i);
      // Read input and expected output.
      pitch_buf_24kHz_reader.first->ReadChunk(pitch_buf_data);
      lp_residual_reader.first->ReadChunk(expected_lp_residual_view);
      // Skip pitch gain and period.
      float unused;
      lp_residual_reader.first->ReadValue(&unused);
      lp_residual_reader.first->ReadValue(&unused);
      // Run pipeline.
      ComputeAndPostProcessLpcCoefficients(pitch_buf_data, lpc_coeffs_view);
      ComputeLpResidual(lpc_coeffs_view, pitch_buf_data,
                        computed_lp_residual_view);
      // Compare.
      ExpectNearAbsolute(expected_lp_residual_view, computed_lp_residual_view,
                         kFloatMin);
    }
  }
}

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
