/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_

#include <cstring>
#include <map>

#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

const uint8_t kRtpMarkerBitMask = 0x80;

namespace RtpUtility {

struct Payload {
  Payload(const char* name, const PayloadUnion& pu) : typeSpecific(pu) {
    std::strncpy(this->name, name, sizeof(this->name) - 1);
    this->name[sizeof(this->name) - 1] = '\0';
  }
  char name[RTP_PAYLOAD_NAME_SIZE];
  PayloadUnion typeSpecific;
};

bool StringCompare(const char* str1, const char* str2, const uint32_t length);

// Round up to the nearest size that is a multiple of 4.
size_t Word32Align(size_t size);

class RtpHeaderParser {
 public:
  RtpHeaderParser(const uint8_t* rtpData, size_t rtpDataLength);
  ~RtpHeaderParser();

  bool RTCP() const;
  bool ParseRtcp(RTPHeader* header) const;
  bool Parse(RTPHeader* parsedPacket,
             const RtpHeaderExtensionMap* ptrExtensionMap = nullptr) const;

 private:
  void ParseOneByteExtensionHeader(RTPHeader* parsedPacket,
                                   const RtpHeaderExtensionMap* ptrExtensionMap,
                                   const uint8_t* ptrRTPDataExtensionEnd,
                                   const uint8_t* ptr) const;

  const uint8_t* const _ptrRTPDataBegin;
  const uint8_t* const _ptrRTPDataEnd;
};
}  // namespace RtpUtility
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
