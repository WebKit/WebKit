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

#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

struct ScreamFeedback {
  Timestamp feedback_time = Timestamp::MinusInfinity();
  DataSize data_in_flight = DataSize::Zero();

  // General feedback metrics.
  int num_received_packets = 0;
  int num_ce_marked_packets = 0;
  int num_lost_packets = 0;
  int num_recovered_packets = 0;

  // Sum of the sizes of all packets in the feedback that are NOT ECN CE-marked.
  // This explicitly includes packets reported as lost (since lost packets do
  // not have ECN marking).
  DataSize acked_not_marked_size = DataSize::Zero();

  // Aggregated delay & RTT metrics.
  TimeDelta min_one_way_delay = TimeDelta::PlusInfinity();
  TimeDelta max_one_way_delay = TimeDelta::Zero();

  // The duration between the receive times of the first and last packets in
  // this feedback, including the last packet's receiver delay
  // (arrival_time_offset).
  TimeDelta feedback_hold_time = TimeDelta::Zero();

  // The calculated RTT sample of this feedback.
  TimeDelta rtt_sample = TimeDelta::Zero();
};

// Free function helper to convert original feedback into the flat struct.
ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg);

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_SCREAM_SCREAM_FEEDBACK_H_
