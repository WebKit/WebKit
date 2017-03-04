/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

namespace {
class DummyCallback : public RecoveredPacketReceiver {
  bool OnRecoveredPacket(const uint8_t* packet, size_t length) { return true; }
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMinDataNeeded = 12;
  if (size < kMinDataNeeded) {
    return;
  }

  uint32_t flexfec_ssrc;
  memcpy(&flexfec_ssrc, data + 0, 4);
  uint16_t flexfec_seq_num;
  memcpy(&flexfec_seq_num, data + 4, 2);
  uint32_t media_ssrc;
  memcpy(&media_ssrc, data + 6, 4);
  uint16_t media_seq_num;
  memcpy(&media_seq_num, data + 10, 2);

  DummyCallback callback;
  FlexfecReceiver receiver(flexfec_ssrc, media_ssrc, &callback);

  std::unique_ptr<uint8_t[]> packet;
  size_t packet_length;
  size_t i = kMinDataNeeded;
  while (i < size) {
    packet_length = kRtpHeaderSize + data[i++];
    packet = std::unique_ptr<uint8_t[]>(new uint8_t[packet_length]);
    if (i + packet_length >= size) {
      break;
    }
    memcpy(packet.get(), data + i, packet_length);
    i += packet_length;
    if (i < size && data[i++] % 2 == 0) {
      // Simulate FlexFEC packet.
      ByteWriter<uint16_t>::WriteBigEndian(packet.get() + 2, flexfec_seq_num++);
      ByteWriter<uint32_t>::WriteBigEndian(packet.get() + 8, flexfec_ssrc);
    } else {
      // Simulate media packet.
      ByteWriter<uint16_t>::WriteBigEndian(packet.get() + 2, media_seq_num++);
      ByteWriter<uint32_t>::WriteBigEndian(packet.get() + 8, media_ssrc);
    }
    RtpPacketReceived parsed_packet;
    if (parsed_packet.Parse(packet.get(), packet_length)) {
      receiver.OnRtpPacket(parsed_packet);
    }
  }
}

}  // namespace webrtc
