/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_FILTER_H_

#include <array>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"

namespace webrtc {

class SuppressionFilter {
 public:
  explicit SuppressionFilter(int sample_rate_hz);
  ~SuppressionFilter();
  void ApplyGain(const FftData& comfort_noise,
                 const FftData& comfort_noise_high_bands,
                 const std::array<float, kFftLengthBy2Plus1>& suppression_gain,
                 std::vector<std::vector<float>>* e);

 private:
  const int sample_rate_hz_;
  const OouraFft ooura_fft_;
  const Aec3Fft fft_;
  std::array<float, kFftLengthBy2> e_input_old_;
  std::vector<std::array<float, kFftLengthBy2>> e_output_old_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SuppressionFilter);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUPPRESSION_FILTER_H_
