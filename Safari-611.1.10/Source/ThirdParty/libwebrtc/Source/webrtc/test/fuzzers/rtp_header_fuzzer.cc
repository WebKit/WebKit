/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {
// We decide which header extensions to register by reading four bytes
// from the beginning of |data| and interpreting it as a bitmask over
// the RTPExtensionType enum. This assert ensures four bytes are enough.
static_assert(kRtpExtensionNumberOfExtensions <= 32,
              "Insufficient bits read to configure all header extensions. Add "
              "an extra byte and update the switches.");

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size <= 4)
    return;

  // Don't use the configuration byte as part of the packet.
  std::bitset<32> extensionMask(*reinterpret_cast<const uint32_t*>(data));
  data += 4;
  size -= 4;

  RtpPacketReceived::ExtensionManager extensions(/*extmap_allow_mixed=*/true);
  // Start at local_id = 1 since 0 is an invalid extension id.
  int local_id = 1;
  // Skip i = 0 since it maps to kRtpExtensionNone.
  for (int i = 1; i < kRtpExtensionNumberOfExtensions; i++) {
    RTPExtensionType extension_type = static_cast<RTPExtensionType>(i);
    if (extensionMask[i]) {
      // Extensions are registered with an ID, which you signal to the
      // peer so they know what to expect. This code only cares about
      // parsing so the value of the ID isn't relevant.
      extensions.RegisterByType(local_id++, extension_type);
    }
  }

  RTPHeader rtp_header;
  RtpUtility::RtpHeaderParser rtp_parser(data, size);
  rtp_parser.Parse(&rtp_header, &extensions);
}
}  // namespace webrtc
