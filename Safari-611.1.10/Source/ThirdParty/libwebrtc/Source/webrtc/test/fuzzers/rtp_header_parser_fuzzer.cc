/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "test/rtp_header_parser.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  RtpHeaderParser::IsRtcp(data, size);
  RtpHeaderParser::GetSsrc(data, size);
  RTPHeader rtp_header;

  std::unique_ptr<RtpHeaderParser> rtp_header_parser(
      RtpHeaderParser::CreateForTest());

  rtp_header_parser->Parse(data, size, &rtp_header);
  for (int i = 1; i < kRtpExtensionNumberOfExtensions; ++i) {
    if (size > 0 && i >= data[size - 1]) {
      RTPExtensionType add_extension = static_cast<RTPExtensionType>(i);
      rtp_header_parser->RegisterRtpHeaderExtension(add_extension, i);
    }
  }
  rtp_header_parser->Parse(data, size, &rtp_header);

  for (int i = 1; i < kRtpExtensionNumberOfExtensions; ++i) {
    if (size > 1 && i >= data[size - 2]) {
      RTPExtensionType remove_extension = static_cast<RTPExtensionType>(i);
      rtp_header_parser->DeregisterRtpHeaderExtension(remove_extension);
    }
  }
  rtp_header_parser->Parse(data, size, &rtp_header);
}

}  // namespace webrtc
