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
// We decide which header extensions to register by reading two bytes
// from the beginning of |data| and interpreting it as a bitmask over
// the RTPExtensionType enum. This assert ensures two bytes are enough.
static_assert(kRtpExtensionNumberOfExtensions <= 16,
              "Insufficient bits read to configure all header extensions. Add "
              "an extra byte and update the switches.");

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size <= 2)
    return;

  // Don't use the configuration byte as part of the packet.
  std::bitset<16> extensionMask(*reinterpret_cast<const uint16_t*>(data));
  data += 2;
  size -= 2;

  RtpPacketReceived::ExtensionManager extensions;
  // Skip i = 0 since it maps to ExtensionNone and extension id = 0 is invalid.
  for (int i = 0; i < kRtpExtensionNumberOfExtensions; i++) {
    RTPExtensionType extension_type = static_cast<RTPExtensionType>(i);
    if (extensionMask[i] && extension_type != kRtpExtensionNone) {
      // Extensions are registered with an ID, which you signal to the
      // peer so they know what to expect. This code only cares about
      // parsing so the value of the ID isn't relevant; we use i.
      extensions.RegisterByType(i, extension_type);
    }
  }

  RTPHeader rtp_header;
  RtpUtility::RtpHeaderParser rtp_parser(data, size);
  rtp_parser.Parse(&rtp_header, &extensions);
}
}  // namespace webrtc
