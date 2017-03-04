/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/packet_buffer.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

namespace {
class NullCallback : public video_coding::OnReceivedFrameCallback {
  void OnReceivedFrame(std::unique_ptr<video_coding::RtpFrameObject> frame) {}
};
}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  // Two bytes for the sequence number,
  // one byte for |is_first_packet_in_frame| and |markerBit|.
  constexpr size_t kMinDataNeeded = 3;
  if (size < kMinDataNeeded) {
    return;
  }

  VCMPacket packet;
  NullCallback callback;
  SimulatedClock clock(0);
  rtc::scoped_refptr<video_coding::PacketBuffer> packet_buffer(
      video_coding::PacketBuffer::Create(&clock, 8, 1024, &callback));

  size_t i = kMinDataNeeded;
  while (i < size) {
    memcpy(&packet.seqNum, &data[i - kMinDataNeeded], 2);
    packet.is_first_packet_in_frame = data[i] & 1;
    packet.markerBit = data[i] & 2;
    packet_buffer->InsertPacket(&packet);
    i += kMinDataNeeded;
  }
}

}  // namespace webrtc
