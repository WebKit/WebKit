/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_TEXT2PCAP_H_
#define RTC_BASE_TEXT2PCAP_H_

#include <cstdint>
#include <span>
#include <string>

namespace webrtc {

class Text2Pcap {
 public:
  // Dumps the packet in text2pcap format, returning the formatted string.
  // The format is described on
  // https://www.wireshark.org/docs/man-pages/text2pcap.html
  // and resulting logs can be turned into a PCAP file that can be opened
  // with the Wireshark tool using a command line along the lines oﬀ
  //   text2pcap -D -u 1000,2000 -t %H:%M:%S.%f log.txt out.pcap
  // Returns the text2pcap formatted log which is typically prefixed with a
  // newline and has a grep-able suffix (e.g. ` # SCTP_PACKET` or ` # RTP_DUMP`)
  // for easy extraction from logs.
  static std::string DumpPacket(bool outbound,
                                std::span<const uint8_t> payload,
                                int64_t timestamp_ms);
};

}  // namespace webrtc

#endif  // RTC_BASE_TEXT2PCAP_H_
