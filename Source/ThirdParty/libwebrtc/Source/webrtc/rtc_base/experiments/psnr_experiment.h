// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef RTC_BASE_EXPERIMENTS_PSNR_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_PSNR_EXPERIMENT_H_

#include "api/field_trials_view.h"
#include "api/units/time_delta.h"

namespace webrtc {

class PsnrExperiment {
 public:
  explicit PsnrExperiment(const FieldTrialsView& field_trials);

  bool IsEnabled() const { return enabled_; }
  TimeDelta SamplingInterval() const { return sampling_interval_; }

 private:
  bool enabled_;
  TimeDelta sampling_interval_;
};

}  // namespace webrtc

#endif  // RTC_BASE_EXPERIMENTS_PSNR_EXPERIMENT_H_
