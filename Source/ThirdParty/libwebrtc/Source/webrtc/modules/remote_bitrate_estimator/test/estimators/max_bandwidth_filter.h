/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_

#include <climits>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/remote_bitrate_estimator/include/send_time_history.h"
#include "modules/remote_bitrate_estimator/test/bwe.h"

namespace webrtc {
namespace testing {
namespace bwe {
class MaxBandwidthFilter {
 public:
  // Number of rounds for bandwidth estimate to expire.
  static const size_t kBandwidthWindowFilterSize = 10;

  MaxBandwidthFilter();
  ~MaxBandwidthFilter();
  int64_t max_bandwidth_estimate_bps() { return max_bandwidth_estimate_bps_; }

  // Adds bandwidth sample to the bandwidth filter.
  void AddBandwidthSample(int64_t sample, int64_t round);

  // Checks if bandwidth has grown by certain multiplier for past x rounds,
  // to decide whether or full bandwidth was reached.
  bool FullBandwidthReached(float growth_target, int max_rounds_without_growth);

 private:
  int64_t bandwidth_last_round_bytes_per_ms_;
  int64_t max_bandwidth_estimate_bps_;
  int64_t rounds_without_growth_;
  std::pair<int64_t, size_t> bandwidth_samples_[3];
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MAX_BANDWIDTH_FILTER_H_
