/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_
#define MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_

#include <stdint.h>

namespace webrtc {

// Statistics for an RTCP channel
struct RtcpStatistics {
  uint8_t fraction_lost = 0;
  int32_t packets_lost = 0;  // Defined as a 24 bit signed integer in RTCP
  uint32_t extended_highest_sequence_number = 0;
  uint32_t jitter = 0;
};

class RtcpStatisticsCallback {
 public:
  virtual ~RtcpStatisticsCallback() {}

  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) = 0;
  virtual void CNameChanged(const char* cname, uint32_t ssrc) = 0;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_INCLUDE_RTCP_STATISTICS_H_
