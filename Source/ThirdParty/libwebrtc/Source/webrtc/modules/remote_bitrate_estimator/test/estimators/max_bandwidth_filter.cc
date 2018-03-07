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

#include "modules/remote_bitrate_estimator/test/estimators/max_bandwidth_filter.h"

namespace webrtc {
namespace testing {
namespace bwe {

const size_t MaxBandwidthFilter::kBandwidthWindowFilterSize;

MaxBandwidthFilter::MaxBandwidthFilter()
    : bandwidth_last_round_bytes_per_ms_(0),
      max_bandwidth_estimate_bps_(0),
      rounds_without_growth_(0) {}

MaxBandwidthFilter::~MaxBandwidthFilter() {}

// For detailed explanation about implementing bandwidth filter this way visit
// "Bbr design" doc. |sample_bps| was measured during |round|.
void MaxBandwidthFilter::AddBandwidthSample(int64_t sample_bps, int64_t round) {
  if (bandwidth_samples_[0].first == 0 ||
      sample_bps >= bandwidth_samples_[0].first ||
      round - bandwidth_samples_[2].second >=
          MaxBandwidthFilter::kBandwidthWindowFilterSize)
    bandwidth_samples_[0] = bandwidth_samples_[1] =
        bandwidth_samples_[2] = {sample_bps, round};
  if (sample_bps >= bandwidth_samples_[1].first) {
    bandwidth_samples_[1] = {sample_bps, round};
    bandwidth_samples_[2] = bandwidth_samples_[1];
  } else {
    if (sample_bps >= bandwidth_samples_[2].first)
      bandwidth_samples_[2] = {sample_bps, round};
  }
  if (round - bandwidth_samples_[0].second >=
      MaxBandwidthFilter::kBandwidthWindowFilterSize) {
    bandwidth_samples_[0] = bandwidth_samples_[1];
    bandwidth_samples_[1] = bandwidth_samples_[2];
    bandwidth_samples_[2] = {sample_bps, round};
    if (round - bandwidth_samples_[0].second >=
        MaxBandwidthFilter::kBandwidthWindowFilterSize) {
      bandwidth_samples_[0] = bandwidth_samples_[1];
      bandwidth_samples_[1] = bandwidth_samples_[2];
    }
    max_bandwidth_estimate_bps_ = bandwidth_samples_[0].first;
    return;
  }
  if (bandwidth_samples_[1].first == bandwidth_samples_[0].first &&
      round - bandwidth_samples_[1].second >
          MaxBandwidthFilter::kBandwidthWindowFilterSize / 4) {
    bandwidth_samples_[2] = bandwidth_samples_[1] = {sample_bps, round};
    max_bandwidth_estimate_bps_ = bandwidth_samples_[0].first;
    return;
  }
  if (bandwidth_samples_[2].first == bandwidth_samples_[1].first &&
      round - bandwidth_samples_[2].second >
          MaxBandwidthFilter::kBandwidthWindowFilterSize / 2)
    bandwidth_samples_[2] = {sample_bps, round};
  max_bandwidth_estimate_bps_ = bandwidth_samples_[0].first;
}

bool MaxBandwidthFilter::FullBandwidthReached(float growth_target,
                                              int max_rounds_without_growth) {
  // Minimal bandwidth necessary to assume that better bandwidth can still be
  // found and full bandwidth is not reached.
  int64_t minimal_bandwidth =
      bandwidth_last_round_bytes_per_ms_ * growth_target;
  if (max_bandwidth_estimate_bps_ >= minimal_bandwidth) {
    bandwidth_last_round_bytes_per_ms_ = max_bandwidth_estimate_bps_;
    rounds_without_growth_ = 0;
    return false;
  }
  rounds_without_growth_++;
  return rounds_without_growth_ >= max_rounds_without_growth;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
