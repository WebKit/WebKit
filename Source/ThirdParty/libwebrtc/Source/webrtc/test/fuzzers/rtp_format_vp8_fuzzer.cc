/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stddef.h>
#include <stdint.h>

#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_input(rtc::MakeArrayView(data, size));

  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  // Read uint8_t to be sure reduction_lens are much smaller than
  // max_payload_len and thus limits structure is valid.
  limits.first_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.last_packet_reduction_len = fuzz_input.ReadOrDefaultValue<uint8_t>(0);
  limits.single_packet_reduction_len =
      fuzz_input.ReadOrDefaultValue<uint8_t>(0);

  RTPVideoHeaderVP8 hdr_info;
  hdr_info.InitRTPVideoHeaderVP8();
  uint16_t picture_id = fuzz_input.ReadOrDefaultValue<uint16_t>(0);
  hdr_info.pictureId =
      picture_id >= 0x8000 ? kNoPictureId : picture_id & 0x7fff;

  // Main function under test: RtpPacketizerVp8's constructor.
  RtpPacketizerVp8 packetizer(fuzz_input.ReadByteArray(fuzz_input.BytesLeft()),
                              limits, hdr_info);

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
    RTC_CHECK(packetizer.NextPacket(&rtp_packet))
        << "Failed to get packet#" << i;
    RTC_CHECK_LE(rtp_packet.payload_size(), limits.max_payload_len)
        << "Packet #" << i << " exceeds it's limit";
  }
  // Last packet.
  RTC_CHECK(packetizer.NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.last_packet_reduction_len);
}
}  // namespace webrtc
