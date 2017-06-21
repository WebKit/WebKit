/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"

namespace webrtc {
namespace rtcp {

class CompoundPacket : public RtcpPacket {
 public:
  CompoundPacket();
  ~CompoundPacket() override;

  void Append(RtcpPacket* packet);

  // Size of this packet in bytes (i.e. total size of nested packets).
  size_t BlockLength() const override;
  // Returns true if all calls to Create succeeded.
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 protected:
  std::vector<RtcpPacket*> appended_packets_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(CompoundPacket);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_COMPOUND_PACKET_H_
