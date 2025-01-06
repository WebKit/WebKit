/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/stable_target_rate_experiment.h"

#include "api/field_trials_view.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {
namespace {
constexpr char kFieldTrialName[] = "WebRTC-StableTargetRate";
}  // namespace

StableTargetRateExperiment::StableTargetRateExperiment(
    const FieldTrialsView& key_value_config)
    : enabled_("enabled", false),
      video_hysteresis_factor_("video_hysteresis_factor",
                               /*default_value=*/1.2),
      screenshare_hysteresis_factor_("screenshare_hysteresis_factor",
                                     /*default_value=*/1.35) {
  ParseFieldTrial(
      {&enabled_, &video_hysteresis_factor_, &screenshare_hysteresis_factor_},
      key_value_config.Lookup(kFieldTrialName));
}

StableTargetRateExperiment::StableTargetRateExperiment(
    const StableTargetRateExperiment&) = default;
StableTargetRateExperiment::StableTargetRateExperiment(
    StableTargetRateExperiment&&) = default;

bool StableTargetRateExperiment::IsEnabled() const {
  return enabled_.Get();
}

double StableTargetRateExperiment::GetVideoHysteresisFactor() const {
  return video_hysteresis_factor_.Get();
}

double StableTargetRateExperiment::GetScreenshareHysteresisFactor() const {
  return screenshare_hysteresis_factor_.Get();
}

}  // namespace webrtc
