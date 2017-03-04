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
#include "webrtc/modules/audio_processing/aec3/erle_estimator.h"
#include "webrtc/modules/audio_processing/aec3/erl_estimator.h"
#include "webrtc/modules/audio_processing/aec3/fft_buffer.h"

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

  // Returns whether it is possible at all to use the model based echo removal
  // functionalities.
  bool ModelBasedAecFeasible() const { return model_based_aec_feasible_; }

  // Returns whether the render signal is currently active.
  bool ActiveRender() const { return active_render_counter_ > 0; }

  // Returns whether the number of active render blocks since an echo path
  // change.
  size_t ActiveRenderBlocks() const { return active_render_blocks_; }

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

  // Returns the bands where the linear filter is reliable.
  const std::array<bool, kFftLengthBy2Plus1>& BandsWithReliableFilter() const {
    return bands_with_reliable_filter_;
  }

  // Reports whether the filter is poorly aligned.
  bool PoorlyAlignedFilter() const {
    return FilterDelay() ? *FilterDelay() > 0.75f * filter_length_ : false;
  }

  // Returns the strength of the filter.
  const std::array<float, kFftLengthBy2Plus1>& FilterEstimateStrength() const {
    return filter_estimate_strength_;
  }

  // Returns whether the capture signal is saturated.
  bool SaturatedCapture() const { return capture_signal_saturation_; }

  // Updates the capture signal saturation.
  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  // Returns whether a probable headset setup has been detected.
  bool HeadsetDetected() const { return headset_detected_; }

  // Updates the aec state.
  void Update(const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  filter_frequency_response,
              const rtc::Optional<size_t>& external_delay_samples,
              const FftBuffer& X_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& E2_shadow,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              rtc::ArrayView<const float> x,
              const EchoPathVariability& echo_path_variability,
              bool echo_leakage_detected);

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  int echo_path_change_counter_;
  int active_render_counter_;
  size_t active_render_blocks_ = 0;
  bool usable_linear_estimate_ = false;
  bool echo_leakage_detected_ = false;
  bool model_based_aec_feasible_ = false;
  bool capture_signal_saturation_ = false;
  bool headset_detected_ = false;
  rtc::Optional<size_t> filter_delay_;
  rtc::Optional<size_t> external_delay_;
  std::array<bool, kFftLengthBy2Plus1> bands_with_reliable_filter_;
  std::array<float, kFftLengthBy2Plus1> filter_estimate_strength_;
  size_t filter_length_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
