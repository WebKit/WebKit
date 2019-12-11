/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

class RtpPacketSender {
 public:
  virtual ~RtpPacketSender() = default;

  // Insert packet into queue, for eventual transmission. Based on the type of
  // the packet, it will be prioritized and scheduled relative to other packets
  // and the current target send rate.
  virtual void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_PACKET_SENDER_H_
