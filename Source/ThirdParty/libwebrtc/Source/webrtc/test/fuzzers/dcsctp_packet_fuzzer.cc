/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/packet/sctp_packet.h"

namespace webrtc {
using dcsctp::SctpPacket;

void FuzzOneInput(const uint8_t* data, size_t size) {
  absl::optional<SctpPacket> c =
      SctpPacket::Parse(rtc::ArrayView<const uint8_t>(data, size),
                        /*disable_checksum_verification=*/true);

  if (!c.has_value()) {
    return;
  }

  for (const SctpPacket::ChunkDescriptor& desc : c->descriptors()) {
    dcsctp::DebugConvertChunkToString(desc.data);
  }
}
}  // namespace webrtc
