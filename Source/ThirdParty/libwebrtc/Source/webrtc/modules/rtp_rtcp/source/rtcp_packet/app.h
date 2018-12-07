/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "rtc_base/buffer.h"

namespace webrtc {
namespace rtcp {
class CommonHeader;

class App : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 204;
  App();
  ~App() override;

  // Parse assumes header is already parsed and validated.
  bool Parse(const CommonHeader& packet);

  void SetSsrc(uint32_t ssrc) { ssrc_ = ssrc; }
  void SetSubType(uint8_t subtype);
  void SetName(uint32_t name) { name_ = name; }
  void SetData(const uint8_t* data, size_t data_length);

  uint8_t sub_type() const { return sub_type_; }
  uint32_t ssrc() const { return ssrc_; }
  uint32_t name() const { return name_; }
  size_t data_size() const { return data_.size(); }
  const uint8_t* data() const { return data_.data(); }

  size_t BlockLength() const override;

  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;

 private:
  static constexpr size_t kAppBaseLength = 8;  // Ssrc and Name.
  static constexpr size_t kMaxDataSize = 0xffff * 4 - kAppBaseLength;

  uint8_t sub_type_;
  uint32_t ssrc_;
  uint32_t name_;
  rtc::Buffer data_;
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_
