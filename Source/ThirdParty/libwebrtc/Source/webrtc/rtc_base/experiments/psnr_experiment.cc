// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "rtc_base/experiments/psnr_experiment.h"

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {
constexpr char kFieldTrialName[] = "WebRTC-Video-CalculatePsnr";
}

PsnrExperiment::PsnrExperiment(const FieldTrialsView& field_trials)
    : enabled_(false), sampling_interval_(TimeDelta::Millis(1000)) {
  if (!field_trials.IsEnabled(kFieldTrialName)) {
    return;
  }

  enabled_ = true;

  FieldTrialParameter<TimeDelta> sampling_interval("sampling_interval",
                                                   TimeDelta::Millis(1000));
  ParseFieldTrial({&sampling_interval}, field_trials.Lookup(kFieldTrialName));
  sampling_interval_ = sampling_interval.Get();
  if (sampling_interval_ <= TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Invalid sampling interval "
                        << sampling_interval_.ms()
                        << " ms, defaulting to 1000 ms.";
    sampling_interval_ = TimeDelta::Millis(1000);
  }
}

}  // namespace webrtc
