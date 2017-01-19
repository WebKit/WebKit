/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_PARSER_H_
#define WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_PARSER_H_

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct RTPHeader;

class RtpHeaderParser {
 public:
  static RtpHeaderParser* Create();
  virtual ~RtpHeaderParser() {}

  // Returns true if the packet is an RTCP packet, false otherwise.
  static bool IsRtcp(const uint8_t* packet, size_t length);

  // Parses the packet and stores the parsed packet in |header|. Returns true on
  // success, false otherwise.
  // This method is thread-safe in the sense that it can parse multiple packets
  // at once.
  virtual bool Parse(const uint8_t* packet,
                     size_t length,
                     RTPHeader* header) const = 0;

  // Registers an RTP header extension and binds it to |id|.
  virtual bool RegisterRtpHeaderExtension(RTPExtensionType type,
                                          uint8_t id) = 0;

  // De-registers an RTP header extension.
  virtual bool DeregisterRtpHeaderExtension(RTPExtensionType type) = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INCLUDE_RTP_HEADER_PARSER_H_
