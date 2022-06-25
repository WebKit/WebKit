/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void IgnoreResult(video_coding::PacketBuffer::InsertResult result) {}

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 200000) {
    return;
  }
  video_coding::PacketBuffer packet_buffer(8, 1024);
  test::FuzzDataHelper helper(rtc::ArrayView<const uint8_t>(data, size));

  while (helper.BytesLeft()) {
    auto packet = std::make_unique<video_coding::PacketBuffer::Packet>();
    // Fuzz POD members of the packet.
    helper.CopyTo(&packet->marker_bit);
    helper.CopyTo(&packet->payload_type);
    helper.CopyTo(&packet->seq_num);
    helper.CopyTo(&packet->timestamp);
    helper.CopyTo(&packet->times_nacked);

    // Fuzz non-POD member of the packet.
    packet->video_payload.SetSize(helper.ReadOrDefaultValue<uint8_t>(0));
    // TODO(danilchap): Fuzz other non-POD members of the |packet|.

    IgnoreResult(packet_buffer.InsertPacket(std::move(packet)));
  }
}

}  // namespace webrtc
