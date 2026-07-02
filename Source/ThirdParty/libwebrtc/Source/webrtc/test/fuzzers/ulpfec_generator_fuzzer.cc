/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "modules/include/module_common_types_public.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/ulpfec_generator.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

namespace {
constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
}  // namespace

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  // Create Environment once because creating it for each input noticably
  // reduces the speed of the fuzzer.
  static const Environment* const env =
      new Environment(CreateEnvironment(std::make_unique<SimulatedClock>(1)));

  UlpfecGenerator generator(*env, kRedPayloadType, kFecPayloadType);
  if (fuzz_data.size() < 4)
    return;
  FecProtectionParams params = {
      .fec_rate = fuzz_data.Read<uint8_t>() % 128,
      .max_fec_frames = static_cast<int>(fuzz_data.Read<uint8_t>() % 10),
      .fec_mask_type = kFecMaskBursty};
  generator.SetProtectionParameters(params, params);
  uint16_t seq_num = fuzz_data.Read<uint8_t>();
  uint16_t prev_seq_num = 0;
  while (fuzz_data.BytesLeft() > 3) {
    size_t rtp_header_length = fuzz_data.Read<uint8_t>() % 10 + 12;
    size_t payload_size = fuzz_data.Read<uint8_t>() % 10;
    if (fuzz_data.BytesLeft() < payload_size + rtp_header_length + 2)
      break;
    CopyOnWriteBuffer packet(
        fuzz_data.ReadByteArray(payload_size + rtp_header_length),
        IP_PACKET_SIZE);
    // Write a valid parsable header (version = 2, no padding, no extensions,
    // no CSRCs).
    ByteWriter<uint8_t>::WriteBigEndian(packet.MutableData(), 2 << 6);
    // Make sure sequence numbers are increasing.
    ByteWriter<uint16_t>::WriteBigEndian(packet.MutableData() + 2, seq_num++);
    const bool protect = fuzz_data.Read<uint8_t>() % 2 == 1;

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
