/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/aec3/erl_estimator.h"
#include "webrtc/modules/audio_processing/aec3/erle_estimator.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"

namespace webrtc {

class ApmDataDumper;

// Handles the state and the conditions for the echo removal functionality.
class AecState {
 public:
  AecState();
  ~AecState();

  // Returns whether the linear filter estimate is usable.
  bool UsableLinearEstimate() const { return usable_linear_estimate_; }

  // Returns whether there has been echo leakage detected.
  bool EchoLeakageDetected() const { return echo_leakage_detected_; }

  // Returns whether the render signal is currently active.
  // TODO(peah): Deprecate this in an upcoming CL.
  bool ActiveRender() const { return blocks_with_filter_adaptation_ > 200; }

  // Returns the ERLE.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_estimator_.Erle();
  }

  // Returns the ERL.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    return erl_estimator_.Erl();
  }

  // Returns the delay estimate based on the linear filter.
  rtc::Optional<size_t> FilterDelay() const { return filter_delay_; }

  // Returns the externally provided delay.
  rtc::Optional<size_t> ExternalDelay() const { return external_delay_; }

  // Returns whether the capture signal is saturated.
  bool SaturatedCapture() const { return capture_signal_saturation_; }

  // Returns whether the echo signal is saturated.
  bool SaturatedEcho() const { return echo_saturation_; }

  // Updates the capture signal saturation.
  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  // Returns whether a probable headset setup has been detected.
  bool HeadsetDetected() const { return headset_detected_; }

  // Takes appropriate action at an echo path change.
  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Returns the decay factor for the echo reverberation.
  // TODO(peah): Make this adaptive.
  float ReverbDecayFactor() const { return 0.f; }

  // Returns whether the echo suppression gain should be forced to zero.
  bool ForcedZeroGain() const { return force_zero_gain_; }

  // Updates the aec state.
  void Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  adaptive_filter_frequency_response,
              const rtc::Optional<size_t>& external_delay_samples,
              const RenderBuffer& render_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              rtc::ArrayView<const float> x,
              bool echo_leakage_detected);

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  int echo_path_change_counter_;
  size_t blocks_with_filter_adaptation_ = 0;
  bool usable_linear_estimate_ = false;
  bool echo_leakage_detected_ = false;
  bool capture_signal_saturation_ = false;
  bool echo_saturation_ = false;
  bool headset_detected_ = false;
  float previous_max_sample_ = 0.f;
  bool force_zero_gain_ = false;
  bool render_received_ = false;
  size_t force_zero_gain_counter_ = 0;
  rtc::Optional<size_t> filter_delay_;
  rtc::Optional<size_t> external_delay_;
  size_t blocks_since_last_saturation_ = 1000;

  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
