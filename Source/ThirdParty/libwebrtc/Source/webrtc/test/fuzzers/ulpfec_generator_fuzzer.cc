/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/ulpfec_generator.h"

namespace webrtc {

namespace {
constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  UlpfecGenerator generator;
  size_t i = 0;
  if (size < 4)
    return;
  FecProtectionParams params = {
      data[i++] % 128, static_cast<int>(data[i++] % 10), kFecMaskBursty};
  generator.SetFecParameters(params);
  uint16_t seq_num = data[i++];

  while (i + 3 < size) {
    size_t rtp_header_length = data[i++] % 10 + 12;
    size_t payload_size = data[i++] % 10;
    if (i + payload_size + rtp_header_length + 2 > size)
      break;
    std::unique_ptr<uint8_t[]> packet(
        new uint8_t[payload_size + rtp_header_length]);
    memcpy(packet.get(), &data[i], payload_size + rtp_header_length);
    ByteWriter<uint16_t>::WriteBigEndian(&packet[2], seq_num++);
    i += payload_size + rtp_header_length;
    // Make sure sequence numbers are increasing.
    std::unique_ptr<RedPacket> red_packet = UlpfecGenerator::BuildRedPacket(
        packet.get(), payload_size, rtp_header_length, kRedPayloadType);
    const bool protect = data[i++] % 2 == 1;
    if (protect) {
      generator.AddRtpPacketAndGenerateFec(packet.get(), payload_size,
                                           rtp_header_length);
    }
    const size_t num_fec_packets = generator.NumAvailableFecPackets();
    if (num_fec_packets > 0) {
      std::vector<std::unique_ptr<RedPacket>> fec_packets =
          generator.GetUlpfecPacketsAsRed(kRedPayloadType, kFecPayloadType, 100,
                                          rtp_header_length);
      RTC_CHECK_EQ(num_fec_packets, fec_packets.size());
    }
  }
}
}  // namespace webrtc
