/*
 * Copyright 2026 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_EXPERIMENTS_CORRUPTION_DETECTION_FRAME_SELECTOR_SETTINGS_H_
#define RTC_BASE_EXPERIMENTS_CORRUPTION_DETECTION_FRAME_SELECTOR_SETTINGS_H_

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"

namespace webrtc {

class CorruptionDetectionFrameSelectorSettings {
 public:
  explicit CorruptionDetectionFrameSelectorSettings(
      const FieldTrialsView& field_trials);

  bool is_enabled() const { return enabled_; }
  TimeDelta low_overhead_lower_bound() const;
  TimeDelta low_overhead_upper_bound() const;
  TimeDelta high_overhead_lower_bound() const;
  TimeDelta high_overhead_upper_bound() const;

 private:
  bool enabled_ = false;
  TimeDelta low_overhead_lower_bound_;
  TimeDelta low_overhead_upper_bound_;
  TimeDelta high_overhead_lower_bound_;
  TimeDelta high_overhead_upper_bound_;
};

}  // namespace webrtc
#endif  // RTC_BASE_EXPERIMENTS_CORRUPTION_DETECTION_FRAME_SELECTOR_SETTINGS_H_
