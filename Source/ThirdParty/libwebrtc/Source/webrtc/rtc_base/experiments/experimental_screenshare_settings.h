/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_EXPERIMENTAL_SCREENSHARE_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_EXPERIMENTAL_SCREENSHARE_SETTINGS_H_

#include "absl/types/optional.h"
#include "api/transport/webrtc_key_value_config.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class ExperimentalScreenshareSettings {
 public:
  static ExperimentalScreenshareSettings ParseFromFieldTrials();
  explicit ExperimentalScreenshareSettings(
      const WebRtcKeyValueConfig* key_value_config);

  absl::optional<int> MaxQp() const;
  absl::optional<bool> DefaultTlInBaseLayer() const;
  absl::optional<int> BaseLayerMaxBitrate() const;
  absl::optional<int> TopLayerMaxBitrate() const;

 private:
  FieldTrialOptional<int> max_qp_;
  FieldTrialOptional<bool> default_tl_in_base_layer_;
  FieldTrialOptional<int> base_layer_max_bitrate_;
  FieldTrialOptional<int> top_layer_max_bitrate;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_EXPERIMENTAL_SCREENSHARE_SETTINGS_H_
