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

#if WEBRTC_WEBKIT_BUILD

#include "modules/rtp_rtcp/source/rtp_packetizer_h265.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer_h265.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_input(webrtc::MakeArrayView(data, size));

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  // Read uint8_t to be sure reduction_lens are much smaller than
  // max_payload_len and thus limits structure is valid.
  limits.first_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.last_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.single_packet_reduction_len =
      fuzz_input.ReadOrDefaultValue<uint8_t>(0);

  // Main function under test: RtpPacketizerH265's constructor.
  RtpPacketizerH265 packetizer(fuzz_input.ReadByteArray(fuzz_input.BytesLeft()),
                               limits);

  size_t num_packets = packetizer.NumPackets();
  if (num_packets == 0) {
    return;
  }
  // When packetization was successful, validate NextPacket function too.
  // While at it, check that packets respect the payload size limits.
  // While at it, also depacketize the generated payloads.
  VideoRtpDepacketizerH265 depacketizer;
  RtpPacketToSend rtp_packet(nullptr);
  // Single packet.
  if (num_packets == 1) {
    bool result = packetizer.NextPacket(&rtp_packet);
    if (!result)
      return;
    RTC_CHECK_LE(rtp_packet.payload_size(),
                 limits.max_payload_len - std::min(limits.single_packet_reduction_len, limits.first_packet_reduction_len));
    depacketizer.Parse(rtp_packet.PayloadBuffer());
    return;
  }
  // First packet.
  bool result = packetizer.NextPacket(&rtp_packet);
  if (!result)
    return;
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.first_packet_reduction_len);
  depacketizer.Parse(rtp_packet.PayloadBuffer());
  // Middle packets.
  for (size_t i = 1; i < num_packets - 1; ++i) {
    rtp_packet.Clear();
    bool result = packetizer.NextPacket(&rtp_packet);
    if (!result)
      return;

    RTC_CHECK_LE(rtp_packet.payload_size(), limits.max_payload_len)
        << "Packet #" << i << " exceeds it's limit";
    depacketizer.Parse(rtp_packet.PayloadBuffer());
  }
  // Last packet.
  rtp_packet.Clear();
  result = packetizer.NextPacket(&rtp_packet);
  if (!result)
    return;

  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.last_packet_reduction_len);
  depacketizer.Parse(rtp_packet.PayloadBuffer());
}
}  // namespace webrtc

#endif // WEBRTC_WEBKIT_BUILD
