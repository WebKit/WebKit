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

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_

#include <cstdint>
#include <limits>
#include <list>

#include "absl/types/optional.h"

namespace webrtc {
namespace testing {
namespace bwe {

// Average rtt for past |kRttFilterSize| packets should grow by
// |kRttIncreaseThresholdForExpiry| in order to enter PROBE_RTT mode and expire.
// old min_rtt estimate.
const float kRttIncreaseThresholdForExpiry = 2.3f;
const size_t kRttFilterSize = 25;

class MinRttFilter {
 public:
  // This class implements a simple filter to ensure that PROBE_RTT is only
  // entered when RTTs start to increase, instead of fixed 10 second window as
  // in orginal BBR design doc, to avoid unnecessary freezes in stream.
  MinRttFilter();
  ~MinRttFilter();

  absl::optional<int64_t> min_rtt_ms() { return min_rtt_ms_; }
  void AddRttSample(int64_t rtt_ms, int64_t now_ms) {
    if (!min_rtt_ms_ || rtt_ms <= *min_rtt_ms_ || MinRttExpired(now_ms)) {
      min_rtt_ms_.emplace(rtt_ms);
    }
    rtt_samples_.push_back(rtt_ms);
    if (rtt_samples_.size() > kRttFilterSize)
      rtt_samples_.pop_front();
  }

  // Checks whether or not last RTT values for past |kRttFilterSize| packets
  // started to increase, meaning we have to update min_rtt estimate.
  bool MinRttExpired(int64_t now_ms) {
    if (rtt_samples_.size() < kRttFilterSize || !min_rtt_ms_)
      return false;
    int64_t sum_of_rtts_ms = 0;
    for (int64_t i : rtt_samples_)
      sum_of_rtts_ms += i;
    if (sum_of_rtts_ms >=
        *min_rtt_ms_ * kRttIncreaseThresholdForExpiry * kRttFilterSize) {
      rtt_samples_.clear();
      return true;
    }
    return false;
  }

 private:
  absl::optional<int64_t> min_rtt_ms_;
  std::list<int64_t> rtt_samples_;
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_MIN_RTT_FILTER_H_
