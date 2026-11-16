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
#include "modules/congestion_controller/scream/scream_v2_parameters.h"
#include "rtc_base/checks.h"

namespace webrtc {

ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg) {
  return ParseScreamFeedback(msg, ScreamV2Parameters());
}

ScreamFeedback ParseScreamFeedback(const TransportPacketsFeedback& msg,
                                   const ScreamV2Parameters& params) {
  RTC_DCHECK_LE(params.burst_window_min.Get(), params.burst_window_max.Get());

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
      parsed.received += packet.sent_packet.size;
      parsed.num_received_packets++;
      if (packet.ecn == EcnMarking::kCe) {
        parsed.num_ce_marked_packets++;
      }

      // Replicate exact ReceiveTimeOrder tie-breaking logic to find first &
      // last packets among packets with unambiguous receive times.
      if (!packet.ambiguous_receive_time) {
        if (!first_packet || order(packet, *first_packet)) {
          first_packet = &packet;
        }
        if (!last_packet || order(*last_packet, packet)) {
          last_packet = &packet;
        }
      }
    }
  }

  // Calculate min and max delay for packets sent within a window of the
  // latest received packet. Feedback is typically received every ~25ms, but
  // can span significantly longer (e.g. 100-250ms) at low packet rates,
  // such as audio-only or application-limited periods. Earlier packets in
  // such sparse feedback are excluded to prevent natural route jitter over
  // long intervals from being falsely interpreted as burst queueing delay,
  // and to avoid lag in queue delay estimation.
  Timestamp latest_send_time = Timestamp::MinusInfinity();
  Timestamp newer_send_time = Timestamp::MinusInfinity();
  TimeDelta min_one_way_delay = TimeDelta::PlusInfinity();
  TimeDelta max_one_way_delay = TimeDelta::MinusInfinity();

  for (auto it = msg.packet_feedbacks.rbegin();
       it != msg.packet_feedbacks.rend(); ++it) {
    const PacketResult& packet = *it;
    if (!packet.IsReceived() || packet.ambiguous_receive_time) {
      continue;
    }

    if (!latest_send_time.IsFinite()) {
      latest_send_time = packet.sent_packet.send_time;
      newer_send_time = packet.sent_packet.send_time;
    }

    TimeDelta delta_from_latest =
        latest_send_time - packet.sent_packet.send_time;
    TimeDelta gap = newer_send_time - packet.sent_packet.send_time;

    if (delta_from_latest <= params.burst_window_min.Get() ||
        (delta_from_latest <= params.burst_window_max.Get() &&
         gap <= params.burst_window_max_gap.Get())) {
      TimeDelta one_way_delay =
          packet.receive_time - packet.sent_packet.send_time;
      min_one_way_delay = std::min(min_one_way_delay, one_way_delay);
      max_one_way_delay = std::max(max_one_way_delay, one_way_delay);
      newer_send_time = packet.sent_packet.send_time;
    } else {
      break;
    }
  }

  if (last_packet != nullptr) {
    RTC_DCHECK(first_packet != nullptr);
    RTC_DCHECK(min_one_way_delay.IsFinite());
    RTC_DCHECK(max_one_way_delay.IsFinite());
    TimeDelta feedback_hold_time =
        last_packet->receive_time +
        last_packet->arrival_time_offset.value_or(TimeDelta::Zero()) -
        first_packet->receive_time;
    TimeDelta rtt_sample = std::max(
        msg.feedback_time - last_packet->sent_packet.send_time -
            last_packet->arrival_time_offset.value_or(TimeDelta::Zero()),
        TimeDelta::Zero());
    parsed.delay_metrics = ScreamFeedback::DelayMetrics{
        .min_one_way_delay = min_one_way_delay,
        .max_one_way_delay = max_one_way_delay,
        .feedback_hold_time = feedback_hold_time,
        .rtt_sample = rtt_sample,
        .last_packet_receive_time = last_packet->receive_time,
    };
  }

  return parsed;
}

}  // namespace webrtc
