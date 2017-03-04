/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_

#include <algorithm>
#include <array>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

namespace webrtc {

class ResidualEchoEstimator {
 public:
  ResidualEchoEstimator();
  ~ResidualEchoEstimator();

  void Estimate(bool using_subtractor_output,
                const AecState& aec_state,
                const FftBuffer& X_buffer,
                const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
                const std::array<float, kFftLengthBy2Plus1>& E2_main,
                const std::array<float, kFftLengthBy2Plus1>& E2_shadow,
                const std::array<float, kFftLengthBy2Plus1>& S2_linear,
                const std::array<float, kFftLengthBy2Plus1>& S2_fallback,
                const std::array<float, kFftLengthBy2Plus1>& Y2,
                std::array<float, kFftLengthBy2Plus1>* R2);

  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

 private:
  std::array<float, kFftLengthBy2Plus1> echo_path_gain_;
  size_t blocks_since_last_saturation_ = 1000;

  RTC_DISALLOW_COPY_AND_ASSIGN(ResidualEchoEstimator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
