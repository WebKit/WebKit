/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// A convenience class to store RTT samples and calculate smoothed RTT.
// From the Quic BBR implementation in Chromium.

#ifndef MODULES_CONGESTION_CONTROLLER_BBR_RTT_STATS_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_RTT_STATS_H_

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace bbr {

class RttStats {
 public:
  RttStats();

  // Updates the RTT from an incoming ack which is received |send_delta| after
  // the packet is sent and the peer reports the ack being delayed |ack_delay|.
  void UpdateRtt(TimeDelta send_delta, TimeDelta ack_delay, Timestamp now);

  // Causes the smoothed_rtt to be increased to the latest_rtt if the latest_rtt
  // is larger. The mean deviation is increased to the most recent deviation if
  // it's larger.
  void ExpireSmoothedMetrics();

  // Called when connection migrates and RTT measurement needs to be reset.
  void OnConnectionMigration();

  // Returns the EWMA smoothed RTT for the connection.
  // May return Zero if no valid updates have occurred.
  TimeDelta smoothed_rtt() const { return smoothed_rtt_; }

  // Returns the EWMA smoothed RTT prior to the most recent RTT sample.
  TimeDelta previous_srtt() const { return previous_srtt_; }

  int64_t initial_rtt_us() const { return initial_rtt_us_; }

  // Sets an initial RTT to be used for SmoothedRtt before any RTT updates.
  void set_initial_rtt_us(int64_t initial_rtt_us) {
    RTC_DCHECK_GE(initial_rtt_us, 0);
    if (initial_rtt_us <= 0) {
      RTC_LOG(LS_ERROR) << "Attempt to set initial rtt to <= 0.";
      return;
    }
    initial_rtt_us_ = initial_rtt_us;
  }

  // The most recent RTT measurement.
  // May return Zero if no valid updates have occurred.
  TimeDelta latest_rtt() const { return latest_rtt_; }

  // Returns the min_rtt for the entire connection.
  // May return Zero if no valid updates have occurred.
  TimeDelta min_rtt() const { return min_rtt_; }

  TimeDelta mean_deviation() const { return mean_deviation_; }

 private:
  TimeDelta latest_rtt_;
  TimeDelta min_rtt_;
  TimeDelta smoothed_rtt_;
  TimeDelta previous_srtt_;
  // Mean RTT deviation during this session.
  // Approximation of standard deviation, the error is roughly 1.25 times
  // larger than the standard deviation, for a normally distributed signal.
  TimeDelta mean_deviation_;
  int64_t initial_rtt_us_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RttStats);
};

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_RTT_STATS_H_
