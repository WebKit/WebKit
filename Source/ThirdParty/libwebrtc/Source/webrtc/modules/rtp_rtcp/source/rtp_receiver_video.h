/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/onetimeevent.h"

namespace webrtc {

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  explicit RTPReceiverVideo(RtpData* data_callback);

  ~RTPReceiverVideo() override;

  int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                         const PayloadUnion& specific_payload,
                         const uint8_t* packet,
                         size_t packet_length,
                         int64_t timestamp) override;

  void SetPacketOverHead(uint16_t packet_over_head);

 private:
  OneTimeEvent first_packet_received_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
