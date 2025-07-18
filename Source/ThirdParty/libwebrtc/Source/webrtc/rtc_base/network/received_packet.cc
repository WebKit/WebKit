/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/network/received_packet.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "api/array_view.h"
#include "api/transport/ecn_marking.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

ReceivedIpPacket::ReceivedIpPacket(ArrayView<const uint8_t> payload,
                                   const SocketAddress& source_address,
                                   std::optional<Timestamp> arrival_time,
                                   EcnMarking ecn,
                                   DecryptionInfo decryption)
    : payload_(payload),
      arrival_time_(std::move(arrival_time)),
      source_address_(source_address),
      ecn_(ecn),
      decryption_info_(decryption) {}

ReceivedIpPacket ReceivedIpPacket::CopyAndSet(
    DecryptionInfo decryption_info) const {
  return ReceivedIpPacket(payload_, source_address_, arrival_time_, ecn_,
                          decryption_info);
}

// static
ReceivedIpPacket ReceivedIpPacket::CreateFromLegacy(
    const uint8_t* data,
    size_t size,
    int64_t packet_time_us,
    const SocketAddress& source_address) {
  RTC_DCHECK(packet_time_us == -1 || packet_time_us >= 0);
  return ReceivedIpPacket(
      MakeArrayView(data, size), source_address,
      (packet_time_us >= 0)
          ? std::optional<Timestamp>(Timestamp::Micros(packet_time_us))
          : std::nullopt);
}

}  // namespace webrtc
