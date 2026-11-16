/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_FEEDBACK_H_
#define MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_FEEDBACK_H_

#include <stddef.h>

#include <optional>

#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"

namespace webrtc {

struct ScreamFeedback {
  Timestamp feedback_time = Timestamp::MinusInfinity();
  DataSize data_in_flight = DataSize::Zero();

  // General feedback metrics.
  int num_received_packets = 0;
  int num_ce_marked_packets = 0;
  int num_lost_packets = 0;
  int num_recovered_packets = 0;
  DataSize received = DataSize::Zero();

  // Sum of the sizes of all packets in the feedback that are NOT ECN CE-marked.
  // This explicitly includes packets reported as lost (since lost packets do
  // not have ECN marking).
  DataSize acked_not_marked_size = DataSize::Zero();

  // Metrics that depend on at least one packet with an unambiguous receive
  // time. All fields are guaranteed to be valid and finite when present.
  struct DelayMetrics {
    // Aggregated delay & RTT metrics calculated over the recent burst / tail
    // window.
    TimeDelta min_one_way_delay;
    TimeDelta max_one_way_delay;

    // The duration between the receive times of the first and last packets in
    // this feedback, including the last packet's receiver delay
    // (arrival_time_offset).
    TimeDelta feedback_hold_time;

    // The calculated RTT sample of this feedback.
    TimeDelta rtt_sample;

    // Receive timestamp of the latest packet received in this feedback.
    Timestamp last_packet_receive_time;
  };

  std::optional<DelayMetrics> delay_metrics;
};

// Free function helper to convert original feedback into the flat struct.
ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg,
                                   const ScreamV2Parameters& params);
ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg);

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_FEEDBACK_H_
