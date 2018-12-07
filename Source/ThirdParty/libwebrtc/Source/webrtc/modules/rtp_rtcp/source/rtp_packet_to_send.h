/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_TO_SEND_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_TO_SEND_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "api/array_view.h"
#include "api/video/video_timing.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {
// Class to hold rtp packet with metadata for sender side.
class RtpPacketToSend : public RtpPacket {
 public:
  explicit RtpPacketToSend(const ExtensionManager* extensions);
  RtpPacketToSend(const ExtensionManager* extensions, size_t capacity);
  RtpPacketToSend(const RtpPacketToSend& packet);
  RtpPacketToSend(RtpPacketToSend&& packet);

  RtpPacketToSend& operator=(const RtpPacketToSend& packet);
  RtpPacketToSend& operator=(RtpPacketToSend&& packet);

  ~RtpPacketToSend();

  // Time in local time base as close as it can to frame capture time.
  int64_t capture_time_ms() const { return capture_time_ms_; }

  void set_capture_time_ms(int64_t time) { capture_time_ms_ = time; }

  // Additional data bound to the RTP packet for use in application code,
  // outside of WebRTC.
  rtc::ArrayView<const uint8_t> application_data() const {
    return application_data_;
  }

  void set_application_data(rtc::ArrayView<const uint8_t> data) {
    application_data_.assign(data.begin(), data.end());
  }

  void set_packetization_finish_time_ms(int64_t time) {
    SetExtension<VideoTimingExtension>(
        VideoSendTiming::GetDeltaCappedMs(capture_time_ms_, time),
        VideoSendTiming::kPacketizationFinishDeltaOffset);
  }

  void set_pacer_exit_time_ms(int64_t time) {
    SetExtension<VideoTimingExtension>(
        VideoSendTiming::GetDeltaCappedMs(capture_time_ms_, time),
        VideoSendTiming::kPacerExitDeltaOffset);
  }

  void set_network_time_ms(int64_t time) {
    SetExtension<VideoTimingExtension>(
        VideoSendTiming::GetDeltaCappedMs(capture_time_ms_, time),
        VideoSendTiming::kNetworkTimestampDeltaOffset);
  }

  void set_network2_time_ms(int64_t time) {
    SetExtension<VideoTimingExtension>(
        VideoSendTiming::GetDeltaCappedMs(capture_time_ms_, time),
        VideoSendTiming::kNetwork2TimestampDeltaOffset);
  }

 private:
  int64_t capture_time_ms_ = 0;
  std::vector<uint8_t> application_data_;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_PACKET_TO_SEND_H_
