/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vad_with_level.h"

#include "rtc_base/gunit.h"

namespace webrtc {
namespace test {

TEST(AutomaticGainController2VadWithLevelEstimator,
     PeakLevelGreaterThanRmsLevel) {
  constexpr size_t kSampleRateHz = 8000;

  // 10 ms input frame, constant except for one peak value.
  // Handcrafted so that the average is lower than the peak value.
  std::array<float, kSampleRateHz / 100> frame;
  frame.fill(1000.f);
  frame[10] = 2000.f;
  float* const channel0 = frame.data();
  AudioFrameView<float> frame_view(&channel0, 1, frame.size());

  // Compute audio frame levels (the VAD result is ignored).
  VadWithLevel vad_with_level;
  auto levels_and_vad_prob = vad_with_level.AnalyzeFrame(frame_view);

  // Compare peak and RMS levels.
  EXPECT_LT(levels_and_vad_prob.speech_rms_dbfs,
            levels_and_vad_prob.speech_peak_dbfs);
}

}  // namespace test
}  // namespace webrtc
