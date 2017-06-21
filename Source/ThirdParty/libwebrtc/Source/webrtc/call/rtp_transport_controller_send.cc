/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_transport_controller_send.h"

namespace webrtc {

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log)
    : send_side_cc_(clock, nullptr /* observer */, event_log, &packet_router_) {
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

SendSideCongestionController* RtpTransportControllerSend::send_side_cc() {
  return &send_side_cc_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return &send_side_cc_;
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return send_side_cc_.pacer();
}

}  // namespace webrtc
