/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/ulpfec_receiver.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

namespace {
class DummyCallback : public RecoveredPacketReceiver {
  void OnRecoveredPacket(const uint8_t* packet, size_t length) override {}
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMinDataNeeded = 12;
  if (size < kMinDataNeeded || size > 2000) {
    return;
  }

  uint32_t ulpfec_ssrc = ByteReader<uint32_t>::ReadLittleEndian(data + 0);
  uint16_t ulpfec_seq_num = ByteReader<uint16_t>::ReadLittleEndian(data + 4);
  uint32_t media_ssrc = ByteReader<uint32_t>::ReadLittleEndian(data + 6);
  uint16_t media_seq_num = ByteReader<uint16_t>::ReadLittleEndian(data + 10);

  DummyCallback callback;
  UlpfecReceiver receiver(ulpfec_ssrc, 0, &callback, {},
                          Clock::GetRealTimeClock());

  test::FuzzDataHelper fuzz_data(rtc::MakeArrayView(data, size));
  while (fuzz_data.CanReadBytes(kMinDataNeeded)) {
    size_t packet_length = kRtpHeaderSize + fuzz_data.Read<uint8_t>();
    auto raw_packet = fuzz_data.ReadByteArray(packet_length);

    RtpPacketReceived parsed_packet;
    if (!parsed_packet.Parse(raw_packet))
      continue;

    // Overwrite the fields for the sequence number and SSRC with
    // consistent values for either a received UlpFEC packet or received media
    // packet. (We're still relying on libfuzzer to manage to generate packet
    // headers that interact together; this just ensures that we have two
    // consistent streams).
    if (fuzz_data.ReadOrDefaultValue<uint8_t>(0) % 2 == 0) {
      // Simulate UlpFEC packet.
      parsed_packet.SetSequenceNumber(ulpfec_seq_num++);
      parsed_packet.SetSsrc(ulpfec_ssrc);
    } else {
      // Simulate media packet.
      parsed_packet.SetSequenceNumber(media_seq_num++);
      parsed_packet.SetSsrc(media_ssrc);
    }

    receiver.AddReceivedRedPacket(parsed_packet);
  }

  receiver.ProcessReceivedFec();
}

}  // namespace webrtc
