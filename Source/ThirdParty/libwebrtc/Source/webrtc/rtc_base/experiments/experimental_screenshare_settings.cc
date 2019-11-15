/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/experimental_screenshare_settings.h"

#include "api/transport/field_trial_based_config.h"

namespace webrtc {

namespace {
constexpr char kFieldTrialName[] = "WebRTC-ExperimentalScreenshareSettings";
}  // namespace

ExperimentalScreenshareSettings::ExperimentalScreenshareSettings(
    const WebRtcKeyValueConfig* key_value_config)
    : max_qp_("max_qp"),
      default_tl_in_base_layer_("default_tl_in_base_layer"),
      base_layer_max_bitrate_("base_layer_max_bitrate"),
      top_layer_max_bitrate("top_layer_max_bitrate") {
  ParseFieldTrial({&max_qp_, &default_tl_in_base_layer_,
                   &base_layer_max_bitrate_, &top_layer_max_bitrate},
                  key_value_config->Lookup(kFieldTrialName));
}

ExperimentalScreenshareSettings
ExperimentalScreenshareSettings::ParseFromFieldTrials() {
  FieldTrialBasedConfig field_trial_config;
  return ExperimentalScreenshareSettings(&field_trial_config);
}

absl::optional<int> ExperimentalScreenshareSettings::MaxQp() const {
  return max_qp_.GetOptional();
}

absl::optional<bool> ExperimentalScreenshareSettings::DefaultTlInBaseLayer()
    const {
  return default_tl_in_base_layer_.GetOptional();
}

absl::optional<int> ExperimentalScreenshareSettings::BaseLayerMaxBitrate()
    const {
  return base_layer_max_bitrate_.GetOptional();
}

absl::optional<int> ExperimentalScreenshareSettings::TopLayerMaxBitrate()
    const {
  return top_layer_max_bitrate.GetOptional();
}

}  // namespace webrtc
