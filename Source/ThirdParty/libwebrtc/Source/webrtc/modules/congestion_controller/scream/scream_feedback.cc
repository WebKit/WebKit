/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_feedback.h"

#include <algorithm>

#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg) {
  ScreamFeedback parsed;
  parsed.feedback_time = msg.feedback_time;
  parsed.data_in_flight = msg.data_in_flight;

  const PacketResult* first_packet = nullptr;
  const PacketResult* last_packet = nullptr;
  PacketResult::ReceiveTimeOrder order;

  for (const PacketResult& packet : msg.packet_feedbacks) {
    // Sum size of all packets in feedback that are not CE marked (includes
    // lost).
    if (packet.ecn != EcnMarking::kCe) {
      parsed.acked_not_marked_size += packet.sent_packet.size;
    }

    // Record loss and recovery events.
    bool is_lost =
        !packet.IsReceived() && packet.reported_lost_for_the_first_time;
    bool is_recovered = packet.reported_recovered_for_the_first_time;
    if (is_lost) {
      parsed.num_lost_packets++;
    } else if (is_recovered) {
      parsed.num_recovered_packets++;
    }

    // Update received-packet metrics.
    if (packet.IsReceived()) {
      parsed.num_received_packets++;
      if (packet.ecn == EcnMarking::kCe) {
        parsed.num_ce_marked_packets++;
      }

      TimeDelta one_way_delay =
          packet.receive_time - packet.sent_packet.send_time;
      parsed.min_one_way_delay =
          std::min(parsed.min_one_way_delay, one_way_delay);
      parsed.max_one_way_delay =
          std::max(parsed.max_one_way_delay, one_way_delay);

      // Replicate exact ReceiveTimeOrder tie-breaking logic to find first &
      // last packets.
      if (!first_packet || order(packet, *first_packet)) {
        first_packet = &packet;
      }
      if (!last_packet || order(*last_packet, packet)) {
        last_packet = &packet;
      }
    }
  }

  // Directly calculate feedback hold time of this feedback.
  if (first_packet && last_packet) {
    parsed.feedback_hold_time =
        last_packet->receive_time +
        last_packet->arrival_time_offset.value_or(TimeDelta::Zero()) -
        first_packet->receive_time;
  }

  // Directly calculate the RTT sample of this feedback.
  if (last_packet) {
    parsed.rtt_sample = std::max(
        msg.feedback_time - last_packet->sent_packet.send_time -
            last_packet->arrival_time_offset.value_or(TimeDelta::Zero()),
        TimeDelta::Zero());
  }

  return parsed;
}

}  // namespace webrtc
