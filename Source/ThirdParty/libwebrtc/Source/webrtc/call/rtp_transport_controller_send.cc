/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_transport_controller_send.h"

namespace webrtc {

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log)
    : pacer_(clock, &packet_router_, event_log),
      send_side_cc_(clock, nullptr /* observer */, event_log, &pacer_) {}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

PacedSender* RtpTransportControllerSend::pacer() {
  return &pacer_;
}

SendSideCongestionController* RtpTransportControllerSend::send_side_cc() {
  return &send_side_cc_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return &send_side_cc_;
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpTransportControllerSend::keepalive_config() const {
  return keepalive_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps) {
  pacer_.SetSendBitrateLimits(min_send_bitrate_bps, max_padding_bitrate_bps);
}

void RtpTransportControllerSend::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}

}  // namespace webrtc
