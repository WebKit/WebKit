/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_POWER_ECHO_MODEL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_POWER_ECHO_MODEL_H_

#include <array>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

namespace webrtc {

// Provides an echo model based on power spectral estimates that estimates the
// echo spectrum.
class PowerEchoModel {
 public:
  PowerEchoModel();
  ~PowerEchoModel();

  // Ajusts the model according to echo path changes.
  void HandleEchoPathChange(const EchoPathVariability& variability);

  // Updates the echo model and estimates the echo spectrum.
  void EstimateEcho(
      const FftBuffer& render_buffer,
      const std::array<float, kFftLengthBy2Plus1>& capture_spectrum,
      const AecState& aec_state,
      std::array<float, kFftLengthBy2Plus1>* echo_spectrum);

  // Returns the minimum required farend buffer length.
  size_t MinFarendBufferLength() const { return kRenderBufferSize; }

 private:
  // Provides a float value that is coupled with a counter.
  struct CountedFloat {
    CountedFloat() : value(0.f), counter(0) {}
    CountedFloat(float value, int counter) : value(value), counter(counter) {}
    float value;
    int counter;
  };

  const size_t kRenderBufferSize = 100;
  std::array<CountedFloat, kFftLengthBy2Plus1> H2_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PowerEchoModel);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_POWER_ECHO_MODEL_H_
