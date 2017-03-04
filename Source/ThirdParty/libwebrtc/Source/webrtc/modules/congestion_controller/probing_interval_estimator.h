/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBING_INTERVAL_ESTIMATOR_H_
#define WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBING_INTERVAL_ESTIMATOR_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"

namespace webrtc {

class ProbingIntervalEstimator {
 public:
  explicit ProbingIntervalEstimator(const AimdRateControl* aimd_rate_control);

  ProbingIntervalEstimator(int64_t min_interval_ms,
                           int64_t max_interval_ms,
                           int64_t default_interval_ms,
                           const AimdRateControl* aimd_rate_control);
  int64_t GetIntervalMs() const;

 private:
  const int64_t min_interval_ms_;
  const int64_t max_interval_ms_;
  const int64_t default_interval_ms_;
  const AimdRateControl* const aimd_rate_control_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ProbingIntervalEstimator);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_CONGESTION_CONTROLLER_PROBING_INTERVAL_ESTIMATOR_H_
