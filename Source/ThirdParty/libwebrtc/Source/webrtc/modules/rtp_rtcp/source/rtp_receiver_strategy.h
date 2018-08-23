/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_

#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

struct CodecInst;

// This strategy deals with media-specific RTP packet processing.
// This class is not thread-safe and must be protected by its caller.
class RTPReceiverStrategy {
 public:
  static RTPReceiverStrategy* CreateVideoStrategy(RtpData* data_callback);
  static RTPReceiverStrategy* CreateAudioStrategy(RtpData* data_callback);

  virtual ~RTPReceiverStrategy();

  // Parses the RTP packet and calls the data callback with the payload data.
  // Implementations are encouraged to use the provided packet buffer and RTP
  // header as arguments to the callback; implementations are also allowed to
  // make changes in the data as necessary. The specific_payload argument
  // provides audio or video-specific data.
  virtual int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                 const PayloadUnion& specific_payload,
                                 const uint8_t* payload,
                                 size_t payload_length,
                                 int64_t timestamp_ms) = 0;

 protected:
  // The data callback is where we should send received payload data.
  // See ParseRtpPacket. This class does not claim ownership of the callback.
  // Implementations must NOT hold any critical sections while calling the
  // callback.
  //
  // Note: Implementations may call the callback for other reasons than calls
  // to ParseRtpPacket, for instance if the implementation somehow recovers a
  // packet.
  explicit RTPReceiverStrategy(RtpData* data_callback);

  rtc::CriticalSection crit_sect_;
  RtpData* data_callback_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
