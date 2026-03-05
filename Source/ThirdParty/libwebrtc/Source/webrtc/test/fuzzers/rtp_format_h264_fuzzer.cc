/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *  Copyright (c) 2023-2025 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stddef.h>
#include <stdint.h>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#if WEBRTC_WEBKIT_BUILD
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h264.h"
#include "rtc_base/checks.h"
#endif
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/fuzzers/utils/validate_rtp_packetizer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_input(MakeArrayView(data, size));

  RtpPacketizer::PayloadSizeLimits limits = ReadPayloadSizeLimits(fuzz_input);

  const H264PacketizationMode kPacketizationModes[] = {
      H264PacketizationMode::NonInterleaved,
      H264PacketizationMode::SingleNalUnit};

  H264PacketizationMode packetization_mode =
      fuzz_input.SelectOneOf(kPacketizationModes);

  // Main function under test: RtpPacketizerH264's constructor.
  RtpPacketizerH264 packetizer(fuzz_input.ReadByteArray(fuzz_input.BytesLeft()),
                               limits, packetization_mode);

  size_t num_packets = packetizer.NumPackets();
  if (num_packets == 0) {
    return;
  }
  // When packetization was successful, validate NextPacket function too.
  // While at it, check that packets respect the payload size limits.
#if WEBRTC_WEBKIT_BUILD
  // While at it, also depacketize the generated payloads.
  VideoRtpDepacketizerH264 depacketizer;
#endif
  RtpPacketToSend rtp_packet(nullptr);
  // Single packet.
  if (num_packets == 1) {
    RTC_CHECK(packetizer.NextPacket(&rtp_packet));
    RTC_CHECK_LE(rtp_packet.payload_size(),
                 limits.max_payload_len - limits.single_packet_reduction_len);
#if WEBRTC_WEBKIT_BUILD
    depacketizer.Parse(rtp_packet.PayloadBuffer());
#endif
    return;
  }
  // First packet.
  RTC_CHECK(packetizer.NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.first_packet_reduction_len);
#if WEBRTC_WEBKIT_BUILD
  depacketizer.Parse(rtp_packet.PayloadBuffer());
#endif
  // Middle packets.
  for (size_t i = 1; i < num_packets - 1; ++i) {
    rtp_packet.Clear();
    RTC_CHECK(packetizer.NextPacket(&rtp_packet))
        << "Failed to get packet#" << i;
    RTC_CHECK_LE(rtp_packet.payload_size(), limits.max_payload_len)
        << "Packet #" << i << " exceeds it's limit";
#if WEBRTC_WEBKIT_BUILD
    depacketizer.Parse(rtp_packet.PayloadBuffer());
#endif
  }
  // Last packet.
  rtp_packet.Clear();
  RTC_CHECK(packetizer.NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.last_packet_reduction_len);
#if WEBRTC_WEBKIT_BUILD
  depacketizer.Parse(rtp_packet.PayloadBuffer());
#endif
}
}  // namespace webrtc
