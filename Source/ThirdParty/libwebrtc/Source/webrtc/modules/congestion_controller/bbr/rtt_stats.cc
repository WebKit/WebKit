/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/rtt_stats.h"

#include <algorithm>
#include <string>
#include <type_traits>

#include "rtc_base/logging.h"

namespace webrtc {
namespace bbr {
namespace {

// Default initial rtt used before any samples are received.
const int kInitialRttMs = 100;
const double kAlpha = 0.125;
const double kOneMinusAlpha = (1 - kAlpha);
const double kBeta = 0.25;
const double kOneMinusBeta = (1 - kBeta);
const int64_t kNumMicrosPerMilli = 1000;
}  // namespace

RttStats::RttStats()
    : latest_rtt_(TimeDelta::Zero()),
      min_rtt_(TimeDelta::Zero()),
      smoothed_rtt_(TimeDelta::Zero()),
      previous_srtt_(TimeDelta::Zero()),
      mean_deviation_(TimeDelta::Zero()),
      initial_rtt_us_(kInitialRttMs * kNumMicrosPerMilli) {}

void RttStats::ExpireSmoothedMetrics() {
  mean_deviation_ =
      std::max(mean_deviation_, (smoothed_rtt_ - latest_rtt_).Abs());
  smoothed_rtt_ = std::max(smoothed_rtt_, latest_rtt_);
}

// Updates the RTT based on a new sample.
void RttStats::UpdateRtt(TimeDelta send_delta,
                         TimeDelta ack_delay,
                         Timestamp now) {
  if (send_delta.IsInfinite() || send_delta <= TimeDelta::Zero()) {
    RTC_LOG(LS_WARNING) << "Ignoring measured send_delta, because it's is "
                           "either infinite, zero, or negative.  send_delta = "
                        << ToString(send_delta);
    return;
  }

  // Update min_rtt_ first. min_rtt_ does not use an rtt_sample corrected for
  // ack_delay but the raw observed send_delta, since poor clock granularity at
  // the client may cause a high ack_delay to result in underestimation of the
  // min_rtt_.
  if (min_rtt_.IsZero() || min_rtt_ > send_delta) {
    min_rtt_ = send_delta;
  }

  // Correct for ack_delay if information received from the peer results in a
  // positive RTT sample. Otherwise, we use the send_delta as a reasonable
  // measure for smoothed_rtt.
  TimeDelta rtt_sample = send_delta;
  previous_srtt_ = smoothed_rtt_;

  if (rtt_sample > ack_delay) {
    rtt_sample = rtt_sample - ack_delay;
  }
  latest_rtt_ = rtt_sample;
  // First time call.
  if (smoothed_rtt_.IsZero()) {
    smoothed_rtt_ = rtt_sample;
    mean_deviation_ = rtt_sample / 2;
  } else {
    mean_deviation_ = kOneMinusBeta * mean_deviation_ +
                      kBeta * (smoothed_rtt_ - rtt_sample).Abs();
    smoothed_rtt_ = kOneMinusAlpha * smoothed_rtt_ + kAlpha * rtt_sample;
    RTC_LOG(LS_VERBOSE) << " smoothed_rtt(us):" << smoothed_rtt_.us()
                        << " mean_deviation(us):" << mean_deviation_.us();
  }
}

void RttStats::OnConnectionMigration() {
  latest_rtt_ = TimeDelta::Zero();
  min_rtt_ = TimeDelta::Zero();
  smoothed_rtt_ = TimeDelta::Zero();
  mean_deviation_ = TimeDelta::Zero();
  initial_rtt_us_ = kInitialRttMs * kNumMicrosPerMilli;
}

}  // namespace bbr
}  // namespace webrtc
