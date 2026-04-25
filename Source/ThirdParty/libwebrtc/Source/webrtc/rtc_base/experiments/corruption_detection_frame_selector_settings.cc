/*
 * Copyright 2026 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/corruption_detection_frame_selector_settings.h"

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr char kFieldTrialName[] = "WebRTC-CorruptionDetectionFrameSelector";
}  // namespace

CorruptionDetectionFrameSelectorSettings::
    CorruptionDetectionFrameSelectorSettings(
        const FieldTrialsView& field_trials) {
  FieldTrialParameter<bool> enabled("enabled", false);
  FieldTrialParameter<TimeDelta> low_overhead_lower_bound(
      "low_overhead_lower_bound", TimeDelta::Millis(1));
  FieldTrialParameter<TimeDelta> low_overhead_upper_bound(
      "low_overhead_upper_bound", TimeDelta::Millis(500));
  FieldTrialParameter<TimeDelta> high_overhead_lower_bound(
      "high_overhead_lower_bound", TimeDelta::Millis(33));
  FieldTrialParameter<TimeDelta> high_overhead_upper_bound(
      "high_overhead_upper_bound", TimeDelta::Millis(5000));

  ParseFieldTrial(
      {&enabled, &low_overhead_lower_bound, &low_overhead_upper_bound,
       &high_overhead_lower_bound, &high_overhead_upper_bound},
      field_trials.Lookup(kFieldTrialName));

  enabled_ = enabled.Get();
  low_overhead_lower_bound_ = low_overhead_lower_bound.Get();
  low_overhead_upper_bound_ = low_overhead_upper_bound.Get();
  high_overhead_lower_bound_ = high_overhead_lower_bound.Get();
  high_overhead_upper_bound_ = high_overhead_upper_bound.Get();

  // Validation
  if (low_overhead_lower_bound_ > low_overhead_upper_bound_) {
    RTC_LOG(LS_WARNING)
        << "WebRTC-CorruptionDetectionFrameSelector low_overhead_lower_bound "
           "must be <= low_overhead_upper_bound. Disabling experiment.";
    enabled_ = false;
  }
  if (high_overhead_lower_bound_ > high_overhead_upper_bound_) {
    RTC_LOG(LS_WARNING)
        << "WebRTC-CorruptionDetectionFrameSelector high_overhead_lower_bound "
           "must be <= high_overhead_upper_bound. Disabling experiment.";
    enabled_ = false;
  }
}

TimeDelta CorruptionDetectionFrameSelectorSettings::low_overhead_lower_bound()
    const {
  return low_overhead_lower_bound_;
}

TimeDelta CorruptionDetectionFrameSelectorSettings::low_overhead_upper_bound()
    const {
  return low_overhead_upper_bound_;
}

TimeDelta CorruptionDetectionFrameSelectorSettings::high_overhead_lower_bound()
    const {
  return high_overhead_lower_bound_;
}

TimeDelta CorruptionDetectionFrameSelectorSettings::high_overhead_upper_bound()
    const {
  return high_overhead_upper_bound_;
}

}  // namespace webrtc
