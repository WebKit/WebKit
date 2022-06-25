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

#include "modules/include/module_common_types_public.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/ulpfec_generator.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

namespace {
constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  SimulatedClock clock(1);
  UlpfecGenerator generator(kRedPayloadType, kFecPayloadType, &clock);
  size_t i = 0;
  if (size < 4)
    return;
  FecProtectionParams params = {
      data[i++] % 128, static_cast<int>(data[i++] % 10), kFecMaskBursty};
  generator.SetProtectionParameters(params, params);
  uint16_t seq_num = data[i++];
  uint16_t prev_seq_num = 0;
  while (i + 3 < size) {
    size_t rtp_header_length = data[i++] % 10 + 12;
    size_t payload_size = data[i++] % 10;
    if (i + payload_size + rtp_header_length + 2 > size)
      break;
    rtc::CopyOnWriteBuffer packet(&data[i], payload_size + rtp_header_length);
    packet.EnsureCapacity(IP_PACKET_SIZE);
    // Write a valid parsable header (version = 2, no padding, no extensions,
    // no CSRCs).
    ByteWriter<uint8_t>::WriteBigEndian(packet.MutableData(), 2 << 6);
    // Make sure sequence numbers are increasing.
    ByteWriter<uint16_t>::WriteBigEndian(packet.MutableData() + 2, seq_num++);
    i += payload_size + rtp_header_length;
    const bool protect = data[i++] % 2 == 1;

    // Check the sequence numbers are monotonic. In rare case the packets number
    // may loop around and in the same FEC-protected group the packet sequence
    // number became out of order.
    if (protect && IsNewerSequenceNumber(seq_num, prev_seq_num) &&
        seq_num < prev_seq_num + kUlpfecMaxMediaPackets) {
      RtpPacketToSend rtp_packet(nullptr);
      // Check that we actually have a parsable packet, we want to fuzz FEC
      // logic, not RTP header parsing.
      RTC_CHECK(rtp_packet.Parse(packet));
      generator.AddPacketAndGenerateFec(rtp_packet);
      prev_seq_num = seq_num;
    }

    generator.GetFecPackets();
  }
}
}  // namespace webrtc
