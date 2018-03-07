/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_

#include <array>
#include <algorithm>
#include <vector>

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/main_filter_update_gain.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/shadow_filter_update_gain.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/utility/ooura_fft.h"
#include "rtc_base/constructormagic.h"

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

  // Returns the block-wise frequency response for the main adaptive filter.
  const std::vector<std::array<float, kFftLengthBy2Plus1>>&
  FilterFrequencyResponse() const {
    if (use_shadow_filter_frequency_response_) {
      return shadow_filter_.FilterFrequencyResponse();
    }
    return main_filter_.FilterFrequencyResponse();
  }

  // Returns the estimate of the impulse response for the main adaptive filter.
  const std::array<float, kAdaptiveFilterTimeDomainLength>&
  FilterImpulseResponse() const {
    return main_filter_.FilterImpulseResponse();
  }

  bool ConvergedFilter() const { return converged_filter_; }

 private:
  const Aec3Fft fft_;
  ApmDataDumper* data_dumper_;
  const Aec3Optimization optimization_;
  AdaptiveFirFilter main_filter_;
  AdaptiveFirFilter shadow_filter_;
  MainFilterUpdateGain G_main_;
  ShadowFilterUpdateGain G_shadow_;
  bool converged_filter_ = false;
  size_t converged_filter_counter_ = 0;
  bool use_shadow_filter_frequency_response_ = false;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Subtractor);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_H_
