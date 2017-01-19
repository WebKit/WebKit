/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;
// Picture loss indication (PLI) (RFC 4585).
class Pli : public Psfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 1;

  Pli() {}
  ~Pli() override {}

  bool Parse(const CommonHeader& packet);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override {
    return kHeaderLength + kCommonFeedbackLength;
  }

  RTC_DISALLOW_COPY_AND_ASSIGN(Pli);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_PLI_H_
