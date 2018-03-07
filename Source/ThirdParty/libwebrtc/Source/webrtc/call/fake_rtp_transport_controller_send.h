/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include "call/rtp_transport_controller_send_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"

namespace webrtc {

class FakeRtpTransportControllerSend
    : public RtpTransportControllerSendInterface {
 public:
  explicit FakeRtpTransportControllerSend(
      PacketRouter* packet_router,
      PacedSender* paced_sender,
      SendSideCongestionController* send_side_cc)
      : packet_router_(packet_router),
        paced_sender_(paced_sender),
        send_side_cc_(send_side_cc) {
    RTC_DCHECK(send_side_cc);
  }

  PacketRouter* packet_router() override { return packet_router_; }

  SendSideCongestionController* send_side_cc() override {
    return send_side_cc_;
  }

  TransportFeedbackObserver* transport_feedback_observer() override {
    return send_side_cc_;
  }

  PacedSender* pacer() override { return paced_sender_; }

  RtpPacketSender* packet_sender() override { return paced_sender_; }

  const RtpKeepAliveConfig& keepalive_config() const override {
    return keepalive_;
  }

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps) override {}

  void set_keepalive_config(const RtpKeepAliveConfig& keepalive_config) {
    keepalive_ = keepalive_config;
  }

 private:
  PacketRouter* packet_router_;
  PacedSender* paced_sender_;
  SendSideCongestionController* send_side_cc_;
  RtpKeepAliveConfig keepalive_;
};

}  // namespace webrtc

#endif  // CALL_FAKE_RTP_TRANSPORT_CONTROLLER_SEND_H_
