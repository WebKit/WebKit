/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "api/video/rtp_video_frame_assembler.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return;
  }
  RtpHeaderExtensionMap extensions;
  extensions.Register<RtpDependencyDescriptorExtension>(1);
  extensions.Register<RtpGenericFrameDescriptorExtension00>(2);
  RtpPacketReceived rtp_packet(&extensions);

  RtpVideoFrameAssembler assembler(
      static_cast<RtpVideoFrameAssembler::PayloadFormat>(data[0] % 6));

  for (size_t i = 1; i < size;) {
    size_t packet_size = std::min<size_t>(size - i, 300);
    if (rtp_packet.Parse(data + i, packet_size)) {
      assembler.InsertPacket(rtp_packet);
    }
    i += packet_size;
  }
}

}  // namespace webrtc
