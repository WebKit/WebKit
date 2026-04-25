/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/utils/validate_rtp_packetizer.h"

#include <cstddef>

#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

RtpPacketizer::PayloadSizeLimits ReadPayloadSizeLimits(
    test::FuzzDataHelper& fuzz_input) {
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;

  // Read uint8_t to be sure reduction_lens are much smaller than
  // max_payload_len and thus limits structure is valid.
  limits.first_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.last_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.single_packet_reduction_len =
      fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  return limits;
}

void ValidateRtpPacketizer(const RtpPacketizer::PayloadSizeLimits& limits,
                           RtpPacketizer& packetizer) {
  size_t num_packets = packetizer.NumPackets();
  if (num_packets == 0) {
    return;
  }

  // When packetization was successful, validate NextPacket function too.
  // While at it, check that packets respect the payload size limits.
  RtpPacketToSend rtp_packet(nullptr);

  // Single packet.
  if (num_packets == 1) {
    RTC_CHECK(packetizer.NextPacket(&rtp_packet));
    RTC_CHECK_LE(rtp_packet.payload_size(),
                 limits.max_payload_len - limits.single_packet_reduction_len);
    return;
  }

  // First packet.
  RTC_CHECK(packetizer.NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.first_packet_reduction_len);

  // Middle packets.
  for (size_t i = 1; i < num_packets - 1; ++i) {
    rtp_packet.Clear();
    RTC_CHECK(packetizer.NextPacket(&rtp_packet))
        << "Failed to get packet#" << i;
    RTC_CHECK_LE(rtp_packet.payload_size(), limits.max_payload_len)
        << "Packet #" << i << " exceeds it's limit";
  }

  // Last packet.
  rtp_packet.Clear();
  RTC_CHECK(packetizer.NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.last_packet_reduction_len);
}

}  // namespace webrtc
