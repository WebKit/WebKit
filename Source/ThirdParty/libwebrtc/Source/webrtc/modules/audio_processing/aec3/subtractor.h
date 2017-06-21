/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_

#include <array>
#include <algorithm>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/aec3/main_filter_update_gain.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"
#include "webrtc/modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "webrtc/modules/audio_processing/aec3/subtractor_output.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/utility/ooura_fft.h"

namespace webrtc {

// Proves linear echo cancellation functionality
class Subtractor {
 public:
  Subtractor(ApmDataDumper* data_dumper, Aec3Optimization optimization);
  ~Subtractor();

  // Performs the echo subtraction.
  void Process(const RenderBuffer& render_buffer,
               const rtc::ArrayView<const float> capture,
               const RenderSignalAnalyzer& render_signal_analyzer,
               const AecState& aec_state,
               SubtractorOutput* output);

  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Returns the block-wise frequency response of the main adaptive filter.
  const std::vector<std::array<float, kFftLengthBy2Plus1>>&
  FilterFrequencyResponse() const {
    return main_filter_.FilterFrequencyResponse();
  }

 private:
  const Aec3Fft fft_;
  ApmDataDumper* data_dumper_;
  const Aec3Optimization optimization_;
  AdaptiveFirFilter main_filter_;
  AdaptiveFirFilter shadow_filter_;
  MainFilterUpdateGain G_main_;
  ShadowFilterUpdateGain G_shadow_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Subtractor);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
