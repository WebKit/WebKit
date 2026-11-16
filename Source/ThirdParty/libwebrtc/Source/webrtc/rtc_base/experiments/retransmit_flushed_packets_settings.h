/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_RETRANSMIT_FLUSHED_PACKETS_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_RETRANSMIT_FLUSHED_PACKETS_SETTINGS_H_

#include "api/field_trials_view.h"

namespace webrtc {

// TODO(bugs.webrtc.org/564720400): Remove when experiment is concluded.
class RetransmitFlushedPacketsSettings {
 public:
  explicit RetransmitFlushedPacketsSettings(
      const FieldTrialsView& field_trials);

  bool is_enabled() const { return enabled_; }
  double rtt_multiplier() const { return rtt_multiplier_; }

 private:
  bool enabled_ = false;
  double rtt_multiplier_ = 2.0;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_RETRANSMIT_FLUSHED_PACKETS_SETTINGS_H_
