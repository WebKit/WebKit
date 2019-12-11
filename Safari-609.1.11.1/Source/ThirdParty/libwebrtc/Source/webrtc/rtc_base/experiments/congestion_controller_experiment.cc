/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/congestion_controller_experiment.h"

#include <string>

#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

const char kControllerExperiment[] = "WebRTC-BweCongestionController";
}  // namespace

bool CongestionControllerExperiment::BbrControllerEnabled() {
  std::string trial_string =
      webrtc::field_trial::FindFullName(kControllerExperiment);
  return trial_string.find("Enabled,BBR") == 0;
}

bool CongestionControllerExperiment::InjectedControllerEnabled() {
  std::string trial_string =
      webrtc::field_trial::FindFullName(kControllerExperiment);
  return trial_string.find("Enabled,Injected") == 0;
}
}  // namespace webrtc
