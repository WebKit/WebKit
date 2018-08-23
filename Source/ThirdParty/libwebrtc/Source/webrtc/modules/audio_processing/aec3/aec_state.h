/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_

#include <math.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_audibility.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/erl_estimator.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/filter_analyzer.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/reverb_model_estimator.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/aec3/subtractor_output_analyzer.h"
#include "modules/audio_processing/aec3/suppression_gain_limiter.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

// Handles the state and the conditions for the echo removal functionality.
class AecState {
 public:
  explicit AecState(const EchoCanceller3Config& config);
  ~AecState();

  // Returns whether the echo subtractor can be used to determine the residual
  // echo.
  bool UsableLinearEstimate() const { return usable_linear_estimate_; }

  // Returns whether the echo subtractor output should be used as output.
  bool UseLinearFilterOutput() const { return use_linear_filter_output_; }

  // Returns the estimated echo path gain.
  float EchoPathGain() const { return filter_analyzer_.Gain(); }

  // Returns whether the render signal is currently active.
  bool ActiveRender() const { return blocks_with_active_render_ > 200; }

  // Returns the appropriate scaling of the residual echo to match the
  // audibility.
  void GetResidualEchoScaling(rtc::ArrayView<float> residual_scaling) const {
    echo_audibility_.GetResidualEchoScaling(residual_scaling);
  }

  // Returns whether the stationary properties of the signals are used in the
  // aec.
  bool UseStationaryProperties() const { return use_stationary_properties_; }

  // Returns the ERLE.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const {
    return erle_estimator_.Erle();
  }

  // Returns any uncertainty in the ERLE estimate.
  absl::optional<float> ErleUncertainty() const {
    if (allow_linear_mode_with_diverged_filter_ && diverged_linear_filter_) {
      return 10.f;
    }

    if (!filter_has_had_time_to_converge_ &&
        use_uncertainty_until_sufficiently_adapted_) {
      return 10.f;
    }
    return absl::nullopt;
  }

  // Returns the time-domain ERLE in log2 units.
  float ErleTimeDomainLog2() const {
    return erle_estimator_.ErleTimeDomainLog2();
  }

  // Returns the ERL.
  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    return erl_estimator_.Erl();
  }

  // Returns the time-domain ERL.
  float ErlTimeDomain() const { return erl_estimator_.ErlTimeDomain(); }

  // Returns the delay estimate based on the linear filter.
  int FilterDelayBlocks() const { return filter_delay_blocks_; }

  // Returns the internal delay estimate based on the linear filter.
  absl::optional<int> InternalDelay() const { return internal_delay_; }

  // Returns whether the capture signal is saturated.
  bool SaturatedCapture() const { return capture_signal_saturation_; }

  // Returns whether the echo signal is saturated.
  bool SaturatedEcho() const { return echo_saturation_; }

  // Updates the capture signal saturation.
  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  // Returns whether the transparent mode is active
  bool TransparentMode() const { return transparent_mode_; }

  // Takes appropriate action at an echo path change.
  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  // Returns the decay factor for the echo reverberation.
  float ReverbDecay() const { return reverb_model_estimator_.ReverbDecay(); }

  // Return the frequency response of the reverberant echo.
  rtc::ArrayView<const float> GetReverbFrequencyResponse() const {
    return reverb_model_estimator_.GetReverbFrequencyResponse();
  }

  // Returns the upper limit for the echo suppression gain.
  float SuppressionGainLimit() const {
    return suppression_gain_limiter_.Limit();
  }

  // Returns whether the suppression gain limiter is active.
  bool IsSuppressionGainLimitActive() const {
    return suppression_gain_limiter_.IsActive();
  }

  // Returns whether the linear filter should have been able to properly adapt.
  bool FilterHasHadTimeToConverge() const {
    return filter_has_had_time_to_converge_;
  }

  // Returns whether the filter adaptation is still in the initial state.
  bool InitialState() const { return initial_state_; }

  // Updates the aec state.
  void Update(const absl::optional<DelayEstimate>& external_delay,
              const std::vector<std::array<float, kFftLengthBy2Plus1>>&
                  adaptive_filter_frequency_response,
              const std::vector<float>& adaptive_filter_impulse_response,
              const RenderBuffer& render_buffer,
              const std::array<float, kFftLengthBy2Plus1>& E2_main,
              const std::array<float, kFftLengthBy2Plus1>& Y2,
              const SubtractorOutput& subtractor_output,
              rtc::ArrayView<const float> y);

  // Returns filter length in blocks.
  int FilterLengthBlocks() const {
    return filter_analyzer_.FilterLengthBlocks();
  }

 private:
  bool DetectActiveRender(rtc::ArrayView<const float> x) const;
  void UpdateSuppressorGainLimit(bool render_activity);
  bool DetectEchoSaturation(rtc::ArrayView<const float> x,
                            float echo_path_gain);

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const EchoCanceller3Config config_;
  const bool allow_transparent_mode_;
  const bool use_stationary_properties_;
  const bool enforce_delay_after_realignment_;
  const bool allow_linear_mode_with_diverged_filter_;
  const bool early_filter_usage_activated_;
  const bool use_short_initial_state_;
  const bool convergence_trigger_linear_mode_;
  const bool no_alignment_required_for_linear_mode_;
  const bool use_uncertainty_until_sufficiently_adapted_;
  const bool transparent_mode_enforces_nonlinear_mode_;
  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  size_t capture_block_counter_ = 0;
  size_t blocks_since_reset_ = 0;
  size_t blocks_with_proper_filter_adaptation_ = 0;
  size_t blocks_with_active_render_ = 0;
  bool usable_linear_estimate_ = false;
  bool diverged_linear_filter_ = false;
  bool capture_signal_saturation_ = false;
  bool echo_saturation_ = false;
  bool transparent_mode_ = false;
  bool render_received_ = false;
  int filter_delay_blocks_ = 0;
  size_t blocks_since_last_saturation_ = 1000;

  std::vector<float> max_render_;
  bool filter_has_had_time_to_converge_ = false;
  bool initial_state_ = true;
  const float gain_rampup_increase_;
  SuppressionGainUpperLimiter suppression_gain_limiter_;
  FilterAnalyzer filter_analyzer_;
  bool use_linear_filter_output_ = false;
  absl::optional<int> internal_delay_;
  size_t diverged_blocks_ = 0;
  bool filter_should_have_converged_ = false;
  size_t blocks_since_converged_filter_;
  size_t active_blocks_since_consistent_filter_estimate_;
  bool converged_filter_seen_ = false;
  bool consistent_filter_seen_ = false;
  bool external_delay_seen_ = false;
  absl::optional<DelayEstimate> external_delay_;
  size_t frames_since_external_delay_change_ = 0;
  size_t converged_filter_count_ = 0;
  bool finite_erl_ = false;
  size_t active_blocks_since_converged_filter_ = 0;
  EchoAudibility echo_audibility_;
  ReverbModelEstimator reverb_model_estimator_;
  SubtractorOutputAnalyzer subtractor_output_analyzer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AecState);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
