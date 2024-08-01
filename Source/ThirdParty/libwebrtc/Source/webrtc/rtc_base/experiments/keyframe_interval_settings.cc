/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/keyframe_interval_settings.h"

#include "api/field_trials_view.h"

namespace webrtc {

namespace {

constexpr char kFieldTrialName[] = "WebRTC-KeyframeInterval";

}  // namespace

KeyframeIntervalSettings::KeyframeIntervalSettings(
    const FieldTrialsView& key_value_config)
    : min_keyframe_send_interval_ms_("min_keyframe_send_interval_ms") {
  ParseFieldTrial({&min_keyframe_send_interval_ms_},
                  key_value_config.Lookup(kFieldTrialName));
}

absl::optional<int> KeyframeIntervalSettings::MinKeyframeSendIntervalMs()
    const {
  return min_keyframe_send_interval_ms_.GetOptional();
}
}  // namespace webrtc
