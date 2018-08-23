/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/packet_queue_interface.h"

namespace webrtc {

PacketQueueInterface::Packet::Packet(RtpPacketSender::Priority priority,
                                     uint32_t ssrc,
                                     uint16_t seq_number,
                                     int64_t capture_time_ms,
                                     int64_t enqueue_time_ms,
                                     size_t length_in_bytes,
                                     bool retransmission,
                                     uint64_t enqueue_order)
    : priority(priority),
      ssrc(ssrc),
      sequence_number(seq_number),
      capture_time_ms(capture_time_ms),
      enqueue_time_ms(enqueue_time_ms),
      sum_paused_ms(0),
      bytes(length_in_bytes),
      retransmission(retransmission),
      enqueue_order(enqueue_order) {}

PacketQueueInterface::Packet::Packet(const Packet& other) = default;

PacketQueueInterface::Packet::~Packet() {}

bool PacketQueueInterface::Packet::operator<(
    const PacketQueueInterface::Packet& other) const {
  if (priority != other.priority)
    return priority > other.priority;
  if (retransmission != other.retransmission)
    return other.retransmission;

  return enqueue_order > other.enqueue_order;
}
}  // namespace webrtc
