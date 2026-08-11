/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "modules/rtp_rtcp/include/flexfec_receiver.h"
#include "modules/rtp_rtcp/include/recovered_packet_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

namespace {
class DummyCallback : public RecoveredPacketReceiver {
  void OnRecoveredPacket(const RtpPacketReceived& packet) override {}
};
}  // namespace

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  constexpr size_t kMinDataNeeded = 12;
  if (fuzz_data.size() < kMinDataNeeded || fuzz_data.size() > 2'000) {
    return;
  }

  uint32_t flexfec_ssrc = fuzz_data.Read<uint32_t>();
  uint16_t flexfec_seq_num = fuzz_data.Read<uint16_t>();
  uint32_t media_ssrc = fuzz_data.Read<uint32_t>();
  uint16_t media_seq_num = fuzz_data.Read<uint16_t>();

  DummyCallback callback;
  FlexfecReceiver receiver(flexfec_ssrc, media_ssrc, &callback);

  size_t packet_length;
  while (fuzz_data.BytesLeft() > 0) {
    packet_length = kRtpHeaderSize + fuzz_data.Read<uint8_t>();
    if (fuzz_data.BytesLeft() < packet_length + 1) {
      break;
    }
    RtpPacketReceived parsed_packet;
    if (!parsed_packet.Parse(fuzz_data.ReadByteArray(packet_length))) {
      continue;
    }
    if (fuzz_data.Read<uint8_t>() % 2 == 0) {
      // Simulate FlexFEC packet.
      parsed_packet.SetSequenceNumber(flexfec_seq_num++);
      parsed_packet.SetSsrc(flexfec_ssrc);
    } else {
      // Simulate media packet.
      parsed_packet.SetSequenceNumber(media_seq_num++);
      parsed_packet.SetSsrc(media_ssrc);
    }
    receiver.OnRtpPacket(parsed_packet);
  }
}

}  // namespace webrtc
