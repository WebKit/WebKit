/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define WEBRTC_CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include "webrtc/call/rtp_transport_controller_send_interface.h"
#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"
#include "webrtc/modules/pacing/packet_router.h"

namespace webrtc {

class FakeRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
 public:
  explicit FakeRtpTransportControllerSend(
      PacketRouter* packet_router,
      SendSideCongestionController* send_side_cc)
      : packet_router_(packet_router), send_side_cc_(send_side_cc) {
    RTC_DCHECK(send_side_cc);
  }

  PacketRouter* packet_router() override { return packet_router_; }

  SendSideCongestionController* send_side_cc() override {
    return send_side_cc_;
  }

  TransportFeedbackObserver* transport_feedback_observer() override {
    return send_side_cc_;
  }

  RtpPacketSender* packet_sender() override { return send_side_cc_->pacer(); }

 private:
  PacketRouter* packet_router_;
  SendSideCongestionController* send_side_cc_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
