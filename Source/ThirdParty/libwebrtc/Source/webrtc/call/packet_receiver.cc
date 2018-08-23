/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/packet_receiver.h"

namespace webrtc {

PacketReceiver::DeliveryStatus PacketReceiver::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    int64_t packet_time_us) {
  return DeliverPacket(media_type, packet, PacketTime(packet_time_us, -1));
}

// TODO(bugs.webrtc.org/9584): Deprecated. Over the transition, default
// implementations are used, and subclasses must override one or the other.
PacketReceiver::DeliveryStatus PacketReceiver::DeliverPacket(
    MediaType media_type,
    rtc::CopyOnWriteBuffer packet,
    const PacketTime& packet_time) {
  return DeliverPacket(media_type, packet, packet_time.timestamp);
}

}  // namespace webrtc
