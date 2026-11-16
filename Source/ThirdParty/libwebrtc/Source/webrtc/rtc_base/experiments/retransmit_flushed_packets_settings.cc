/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/retransmit_flushed_packets_settings.h"

#include <algorithm>

#include "api/field_trials_view.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

namespace {
constexpr char kFieldTrialName[] = "WebRTC-RetransmitFlushedPackets";
constexpr double kDefaultRttMultiplier = 2.0;
}  // namespace

RetransmitFlushedPacketsSettings::RetransmitFlushedPacketsSettings(
    const FieldTrialsView& field_trials) {
  FieldTrialFlag enabled("enabled");
  FieldTrialParameter<double> rtt_multiplier("rtt_multiplier",
                                             kDefaultRttMultiplier);
  ParseFieldTrial({&enabled, &rtt_multiplier},
                  field_trials.Lookup(kFieldTrialName));

  if (field_trials.IsDisabled(kFieldTrialName)) {
    enabled_ = false;
  } else if (field_trials.IsEnabled(kFieldTrialName)) {
    enabled_ = true;
  } else {
    enabled_ = enabled.Get();
  }

  rtt_multiplier_ = std::max(0.0, rtt_multiplier.Get());
}

}  // namespace webrtc
