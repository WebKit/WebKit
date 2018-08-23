/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/aec_state.h"

#include <math.h>

#include <numeric>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/atomicops.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

bool EnableTransparentMode() {
  return !field_trial::IsEnabled("WebRTC-Aec3TransparentModeKillSwitch");
}

bool EnableStationaryRenderImprovements() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3StationaryRenderImprovementsKillSwitch");
}

bool EnableEnforcingDelayAfterRealignment() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3EnforceDelayAfterRealignmentKillSwitch");
}

bool EnableLinearModeWithDivergedFilter() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3LinearModeWithDivergedFilterKillSwitch");
}

bool EnableEarlyFilterUsage() {
  return !field_trial::IsEnabled("WebRTC-Aec3EarlyLinearFilterUsageKillSwitch");
}

bool EnableShortInitialState() {
  return !field_trial::IsEnabled("WebRTC-Aec3ShortInitialStateKillSwitch");
}

bool EnableNoWaitForAlignment() {
  return !field_trial::IsEnabled("WebRTC-Aec3NoAlignmentWaitKillSwitch");
}

bool EnableConvergenceTriggeredLinearMode() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3ConvergenceTriggingLinearKillSwitch");
}

bool EnableUncertaintyUntilSufficientAdapted() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3ErleUncertaintyUntilSufficientlyAdaptedKillSwitch");
}

bool TreatTransparentModeAsNonlinear() {
  return !field_trial::IsEnabled(
      "WebRTC-Aec3TreatTransparentModeAsNonlinearKillSwitch");
}

float ComputeGainRampupIncrease(const EchoCanceller3Config& config) {
  const auto& c = config.echo_removal_control.gain_rampup;
  return powf(1.f / c.first_non_zero_gain, 1.f / c.non_zero_gain_blocks);
}

constexpr size_t kBlocksSinceConvergencedFilterInit = 10000;
constexpr size_t kBlocksSinceConsistentEstimateInit = 10000;

}  // namespace

int AecState::instance_count_ = 0;

AecState::AecState(const EchoCanceller3Config& config)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      config_(config),
      allow_transparent_mode_(EnableTransparentMode()),
      use_stationary_properties_(
          EnableStationaryRenderImprovements() &&
          config_.echo_audibility.use_stationary_properties),
      enforce_delay_after_realignment_(EnableEnforcingDelayAfterRealignment()),
      allow_linear_mode_with_diverged_filter_(
          EnableLinearModeWithDivergedFilter()),
      early_filter_usage_activated_(EnableEarlyFilterUsage()),
      use_short_initial_state_(EnableShortInitialState()),
      convergence_trigger_linear_mode_(EnableConvergenceTriggeredLinearMode()),
      no_alignment_required_for_linear_mode_(EnableNoWaitForAlignment()),
      use_uncertainty_until_sufficiently_adapted_(
          EnableUncertaintyUntilSufficientAdapted()),
      transparent_mode_enforces_nonlinear_mode_(
          TreatTransparentModeAsNonlinear()),
      erle_estimator_(config.erle.min, config.erle.max_l, config.erle.max_h),
      max_render_(config_.filter.main.length_blocks, 0.f),
      gain_rampup_increase_(ComputeGainRampupIncrease(config_)),
      suppression_gain_limiter_(config_),
      filter_analyzer_(config_),
      blocks_since_converged_filter_(kBlocksSinceConvergencedFilterInit),
      active_blocks_since_consistent_filter_estimate_(
          kBlocksSinceConsistentEstimateInit),
      reverb_model_estimator_(config) {}

AecState::~AecState() = default;

void AecState::HandleEchoPathChange(
    const EchoPathVariability& echo_path_variability) {
  const auto full_reset = [&]() {
    filter_analyzer_.Reset();
    blocks_since_last_saturation_ = 0;
    usable_linear_estimate_ = false;
    diverged_linear_filter_ = false;
    capture_signal_saturation_ = false;
    echo_saturation_ = false;
    std::fill(max_render_.begin(), max_render_.end(), 0.f);
    blocks_with_proper_filter_adaptation_ = 0;
    blocks_since_reset_ = 0;
    filter_has_had_time_to_converge_ = false;
    render_received_ = false;
    blocks_with_active_render_ = 0;
    initial_state_ = true;
    suppression_gain_limiter_.Reset();
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
    diverged_blocks_ = 0;
  };

  // TODO(peah): Refine the reset scheme according to the type of gain and
  // delay adjustment.

  if (echo_path_variability.delay_change !=
      EchoPathVariability::DelayAdjustment::kNone) {
    full_reset();
  }

  subtractor_output_analyzer_.HandleEchoPathChange();
}

void AecState::Update(
    const absl::optional<DelayEstimate>& external_delay,
    const std::vector<std::array<float, kFftLengthBy2Plus1>>&
        adaptive_filter_frequency_response,
    const std::vector<float>& adaptive_filter_impulse_response,
    const RenderBuffer& render_buffer,
    const std::array<float, kFftLengthBy2Plus1>& E2_main,
    const std::array<float, kFftLengthBy2Plus1>& Y2,
    const SubtractorOutput& subtractor_output,
    rtc::ArrayView<const float> y) {
  // Analyze the filter output.
  subtractor_output_analyzer_.Update(subtractor_output);

  const bool converged_filter = subtractor_output_analyzer_.ConvergedFilter();
  const bool diverged_filter = subtractor_output_analyzer_.DivergedFilter();

  // Analyze the filter and compute the delays.
  filter_analyzer_.Update(adaptive_filter_impulse_response,
                          adaptive_filter_frequency_response, render_buffer);
  filter_delay_blocks_ = filter_analyzer_.DelayBlocks();
  if (enforce_delay_after_realignment_) {
    if (external_delay &&
        (!external_delay_ || external_delay_->delay != external_delay->delay)) {
      frames_since_external_delay_change_ = 0;
      external_delay_ = external_delay;
    }
    if (blocks_with_proper_filter_adaptation_ < 2 * kNumBlocksPerSecond &&
        external_delay_) {
      filter_delay_blocks_ = config_.delay.delay_headroom_blocks;
    }
  }

  if (filter_analyzer_.Consistent()) {
    internal_delay_ = filter_analyzer_.DelayBlocks();
  } else {
    internal_delay_ = absl::nullopt;
  }

  external_delay_seen_ = external_delay_seen_ || external_delay;

  const std::vector<float>& x = render_buffer.Block(-filter_delay_blocks_)[0];

  // Update counters.
  ++capture_block_counter_;
  ++blocks_since_reset_;
  const bool active_render_block = DetectActiveRender(x);
  blocks_with_active_render_ += active_render_block ? 1 : 0;
  blocks_with_proper_filter_adaptation_ +=
      active_render_block && !SaturatedCapture() ? 1 : 0;

  // Update the limit on the echo suppression after an echo path change to avoid
  // an initial echo burst.
  suppression_gain_limiter_.Update(render_buffer.GetRenderActivity(),
                                   transparent_mode_);

  if (UseStationaryProperties()) {
    // Update the echo audibility evaluator.
    echo_audibility_.Update(
        render_buffer, FilterDelayBlocks(), external_delay_seen_,
        config_.ep_strength.reverb_based_on_render ? ReverbDecay() : 0.f);
  }

  // Update the ERL and ERLE measures.
  if (blocks_since_reset_ >= 2 * kNumBlocksPerSecond) {
    const auto& X2 = render_buffer.Spectrum(filter_delay_blocks_);
    erle_estimator_.Update(X2, Y2, E2_main, converged_filter);
    if (converged_filter) {
      erl_estimator_.Update(X2, Y2);
    }
  }

  // Detect and flag echo saturation.
  if (config_.ep_strength.echo_can_saturate) {
    echo_saturation_ = DetectEchoSaturation(x, EchoPathGain());
  }

  if (early_filter_usage_activated_) {
    filter_has_had_time_to_converge_ =
        blocks_with_proper_filter_adaptation_ >= 0.8f * kNumBlocksPerSecond;
  } else {
    filter_has_had_time_to_converge_ =
        blocks_with_proper_filter_adaptation_ >= 1.5f * kNumBlocksPerSecond;
  }

  if (!filter_should_have_converged_) {
    filter_should_have_converged_ =
        blocks_with_proper_filter_adaptation_ > 6 * kNumBlocksPerSecond;
  }

  // Flag whether the initial state is still active.
  if (use_short_initial_state_) {
    initial_state_ =
        blocks_with_proper_filter_adaptation_ < 2.5f * kNumBlocksPerSecond;
  } else {
    initial_state_ =
        blocks_with_proper_filter_adaptation_ < 5 * kNumBlocksPerSecond;
  }

  // Update counters for the filter divergence and convergence.
  diverged_blocks_ = diverged_filter ? diverged_blocks_ + 1 : 0;
  if (diverged_blocks_ >= 60) {
    blocks_since_converged_filter_ = kBlocksSinceConvergencedFilterInit;
  } else {
    blocks_since_converged_filter_ =
        converged_filter ? 0 : blocks_since_converged_filter_ + 1;
  }
  if (converged_filter) {
    active_blocks_since_converged_filter_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_converged_filter_;
  }

  bool recently_converged_filter =
      blocks_since_converged_filter_ < 60 * kNumBlocksPerSecond;

  if (blocks_since_converged_filter_ > 20 * kNumBlocksPerSecond) {
    converged_filter_count_ = 0;
  } else if (converged_filter) {
    ++converged_filter_count_;
  }
  if (converged_filter_count_ > 50) {
    finite_erl_ = true;
  }

  if (filter_analyzer_.Consistent() && filter_delay_blocks_ < 5) {
    consistent_filter_seen_ = true;
    active_blocks_since_consistent_filter_estimate_ = 0;
  } else if (active_render_block) {
    ++active_blocks_since_consistent_filter_estimate_;
  }

  bool consistent_filter_estimate_not_seen;
  if (!consistent_filter_seen_) {
    consistent_filter_estimate_not_seen =
        capture_block_counter_ > 5 * kNumBlocksPerSecond;
  } else {
    consistent_filter_estimate_not_seen =
        active_blocks_since_consistent_filter_estimate_ >
        30 * kNumBlocksPerSecond;
  }

  converged_filter_seen_ = converged_filter_seen_ || converged_filter;

  // If no filter convergence is seen for a long time, reset the estimated
  // properties of the echo path.
  if (active_blocks_since_converged_filter_ > 60 * kNumBlocksPerSecond) {
    converged_filter_seen_ = false;
    finite_erl_ = false;
  }

  // After an amount of active render samples for which an echo should have been
  // detected in the capture signal if the ERL was not infinite, flag that a
  // transparent mode should be entered.
  transparent_mode_ = !config_.ep_strength.bounded_erl && !finite_erl_;
  transparent_mode_ =
      transparent_mode_ &&
      (consistent_filter_estimate_not_seen || !converged_filter_seen_);
  transparent_mode_ = transparent_mode_ && filter_should_have_converged_;
  transparent_mode_ = transparent_mode_ && allow_transparent_mode_;

  usable_linear_estimate_ = !echo_saturation_;

  if (convergence_trigger_linear_mode_) {
    usable_linear_estimate_ =
        usable_linear_estimate_ &&
        ((filter_has_had_time_to_converge_ && external_delay) ||
         converged_filter_seen_);
  } else {
    usable_linear_estimate_ =
        usable_linear_estimate_ && filter_has_had_time_to_converge_;
  }

  if (!no_alignment_required_for_linear_mode_) {
    usable_linear_estimate_ = usable_linear_estimate_ && external_delay;
  }

  if (!config_.echo_removal_control.linear_and_stable_echo_path) {
    usable_linear_estimate_ =
        usable_linear_estimate_ && recently_converged_filter;
    if (!allow_linear_mode_with_diverged_filter_) {
      usable_linear_estimate_ = usable_linear_estimate_ && !diverged_filter;
    }
  }
  if (transparent_mode_enforces_nonlinear_mode_) {
    usable_linear_estimate_ = usable_linear_estimate_ && !TransparentMode();
  }

  use_linear_filter_output_ = usable_linear_estimate_ && !TransparentMode();
  diverged_linear_filter_ = diverged_filter;

  const bool stationary_block =
      use_stationary_properties_ && echo_audibility_.IsBlockStationary();

  reverb_model_estimator_.Update(
      filter_analyzer_.GetAdjustedFilter(), adaptive_filter_frequency_response,
      erle_estimator_.GetInstLinearQualityEstimate(), filter_delay_blocks_,
      usable_linear_estimate_, stationary_block);

  erle_estimator_.Dump(data_dumper_);
  reverb_model_estimator_.Dump(data_dumper_.get());
  data_dumper_->DumpRaw("aec3_erl", Erl());
  data_dumper_->DumpRaw("aec3_erl_time_domain", ErlTimeDomain());
  data_dumper_->DumpRaw("aec3_usable_linear_estimate", UsableLinearEstimate());
  data_dumper_->DumpRaw("aec3_transparent_mode", transparent_mode_);
  data_dumper_->DumpRaw("aec3_state_internal_delay",
                        internal_delay_ ? *internal_delay_ : -1);
  data_dumper_->DumpRaw("aec3_filter_delay", filter_analyzer_.DelayBlocks());

  data_dumper_->DumpRaw("aec3_consistent_filter",
                        filter_analyzer_.Consistent());
  data_dumper_->DumpRaw("aec3_suppression_gain_limit", SuppressionGainLimit());
  data_dumper_->DumpRaw("aec3_initial_state", InitialState());
  data_dumper_->DumpRaw("aec3_capture_saturation", SaturatedCapture());
  data_dumper_->DumpRaw("aec3_echo_saturation", echo_saturation_);
  data_dumper_->DumpRaw("aec3_converged_filter", converged_filter);
  data_dumper_->DumpRaw("aec3_diverged_filter", diverged_filter);

  data_dumper_->DumpRaw("aec3_external_delay_avaliable",
                        external_delay ? 1 : 0);
  data_dumper_->DumpRaw("aec3_consistent_filter_estimate_not_seen",
                        consistent_filter_estimate_not_seen);
  data_dumper_->DumpRaw("aec3_filter_should_have_converged",
                        filter_should_have_converged_);
  data_dumper_->DumpRaw("aec3_filter_has_had_time_to_converge",
                        filter_has_had_time_to_converge_);
  data_dumper_->DumpRaw("aec3_recently_converged_filter",
                        recently_converged_filter);
  data_dumper_->DumpRaw("aec3_suppresion_gain_limiter_running",
                        IsSuppressionGainLimitActive());
  data_dumper_->DumpRaw("aec3_filter_tail_freq_resp_est",
                        GetReverbFrequencyResponse());
}

bool AecState::DetectActiveRender(rtc::ArrayView<const float> x) const {
  const float x_energy = std::inner_product(x.begin(), x.end(), x.begin(), 0.f);
  return x_energy > (config_.render_levels.active_render_limit *
                     config_.render_levels.active_render_limit) *
                        kFftLengthBy2;
}

bool AecState::DetectEchoSaturation(rtc::ArrayView<const float> x,
                                    float echo_path_gain) {
  RTC_DCHECK_LT(0, x.size());
  const float max_sample = fabs(*std::max_element(
      x.begin(), x.end(), [](float a, float b) { return a * a < b * b; }));

  // Set flag for potential presence of saturated echo
  const float kMargin = 10.f;
  float peak_echo_amplitude = max_sample * echo_path_gain * kMargin;
  if (SaturatedCapture() && peak_echo_amplitude > 32000) {
    blocks_since_last_saturation_ = 0;
  } else {
    ++blocks_since_last_saturation_;
  }

  return blocks_since_last_saturation_ < 5;
}

}  // namespace webrtc
