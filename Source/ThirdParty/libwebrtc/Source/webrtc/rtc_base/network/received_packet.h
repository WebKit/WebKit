/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_NETWORK_RECEIVED_PACKET_H_
#define RTC_BASE_NETWORK_RECEIVED_PACKET_H_

#include <cstdint>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"

namespace rtc {

// ReceivedPacket repressent a received IP packet.
// It contains a payload and metadata.
// ReceivedPacket itself does not put constraints on what payload contains. For
// example it may contains STUN, SCTP, SRTP, RTP, RTCP.... etc.
class ReceivedPacket {
 public:
  // Caller must keep memory pointed to by payload valid for the lifetime of
  // this ReceivedPacket.
  ReceivedPacket(
      rtc::ArrayView<const uint8_t> payload,
      absl::optional<webrtc::Timestamp> arrival_time = absl::nullopt);

  rtc::ArrayView<const uint8_t> payload() const { return payload_; }

  // Timestamp when this packet was received. Not available on all socket
  // implementations.
  absl::optional<webrtc::Timestamp> arrival_time() const {
    return arrival_time_;
  }

 private:
  rtc::ArrayView<const uint8_t> payload_;
  absl::optional<webrtc::Timestamp> arrival_time_;
};

}  // namespace rtc
#endif  // RTC_BASE_NETWORK_RECEIVED_PACKET_H_
