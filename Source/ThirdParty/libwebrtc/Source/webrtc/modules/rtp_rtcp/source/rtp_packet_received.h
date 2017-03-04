/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet.h"
#include "webrtc/system_wrappers/include/ntp_time.h"

namespace webrtc {
// Class to hold rtp packet with metadata for receiver side.
class RtpPacketReceived : public rtp::Packet {
 public:
  RtpPacketReceived() = default;
  explicit RtpPacketReceived(const ExtensionManager* extensions)
      : Packet(extensions) {}

  void GetHeader(RTPHeader* header) const {
    Packet::GetHeader(header);
    header->payload_type_frequency = payload_type_frequency();
  }

  // Time in local time base as close as it can to packet arrived on the
  // network.
  int64_t arrival_time_ms() const { return arrival_time_ms_; }
  void set_arrival_time_ms(int64_t time) { arrival_time_ms_ = time; }

  // Estimated from Timestamp() using rtcp Sender Reports.
  NtpTime capture_ntp_time() const { return capture_time_; }
  void set_capture_ntp_time(NtpTime time) { capture_time_ = time; }

  // Flag if packet arrived via rtx.
  bool retransmit() const { return retransmit_; }
  void set_retransmit(bool value) { retransmit_ = value; }

  int payload_type_frequency() const { return payload_type_frequency_; }
  void set_payload_type_frequency(int value) {
    payload_type_frequency_ = value;
  }

 private:
  NtpTime capture_time_;
  int64_t arrival_time_ms_ = 0;
  int payload_type_frequency_ = 0;
  bool retransmit_ = false;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_
