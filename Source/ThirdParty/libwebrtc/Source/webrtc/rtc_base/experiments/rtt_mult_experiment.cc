/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/rtt_mult_experiment.h"

#include <algorithm>
#include <string>

#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
const char kRttMultExperiment[] = "WebRTC-RttMult";
const float max_rtt_mult_setting = 1.0;
const float min_rtt_mult_setting = 0.0;
}  // namespace

bool RttMultExperiment::RttMultEnabled() {
  return field_trial::IsEnabled(kRttMultExperiment);
}

float RttMultExperiment::GetRttMultValue() {
  const std::string group =
      webrtc::field_trial::FindFullName(kRttMultExperiment);
  if (group.empty()) {
    RTC_LOG(LS_WARNING) << "Could not find rtt_mult_experiment.";
    return 0.0;
  }

  float rtt_mult_setting;
  if (sscanf(group.c_str(), "Enabled-%f", &rtt_mult_setting) != 1) {
    RTC_LOG(LS_WARNING) << "Invalid number of parameters provided.";
    return 0.0;
  }
  // Bounds check rtt_mult_setting value.
  rtt_mult_setting = std::min(rtt_mult_setting, max_rtt_mult_setting);
  rtt_mult_setting = std::max(rtt_mult_setting, min_rtt_mult_setting);
  return rtt_mult_setting;
}

}  // namespace webrtc
