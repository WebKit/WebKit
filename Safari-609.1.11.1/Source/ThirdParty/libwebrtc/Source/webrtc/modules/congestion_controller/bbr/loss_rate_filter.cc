/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/bbr/loss_rate_filter.h"

namespace webrtc {
namespace bbr {
namespace {
// From SendSideBandwidthEstimation.
const int kLimitNumPackets = 20;
// From RTCPSender video report interval.
const int64_t kUpdateIntervalMs = 1000;
}  // namespace

LossRateFilter::LossRateFilter()
    : lost_packets_since_last_loss_update_(0),
      expected_packets_since_last_loss_update_(0),
      loss_rate_estimate_(0.0),
      next_loss_update_ms_(0) {}

void LossRateFilter::UpdateWithLossStatus(int64_t feedback_time,
                                          int packets_sent,
                                          int packets_lost) {
  lost_packets_since_last_loss_update_ += packets_lost;
  expected_packets_since_last_loss_update_ += packets_sent;

  if (feedback_time >= next_loss_update_ms_ &&
      expected_packets_since_last_loss_update_ >= kLimitNumPackets) {
    int64_t lost = lost_packets_since_last_loss_update_;
    int64_t expected = expected_packets_since_last_loss_update_;
    loss_rate_estimate_ = static_cast<double>(lost) / expected;
    next_loss_update_ms_ = feedback_time + kUpdateIntervalMs;
    lost_packets_since_last_loss_update_ = 0;
    expected_packets_since_last_loss_update_ = 0;
  }
}

double LossRateFilter::GetLossRate() const {
  return loss_rate_estimate_;
}
}  // namespace bbr
}  // namespace webrtc
