/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/call/rtp_transport_controller_send_interface.h"
#include "webrtc/modules/congestion_controller/include/send_side_congestion_controller.h"

namespace webrtc {
class Clock;
class RtcEventLog;

// TODO(nisse): When we get the underlying transports here, we should
// have one object implementing RtpTransportControllerSendInterface
// per transport, sharing the same congestion controller.
class RtpTransportControllerSend : public RtpTransportControllerSendInterface {
 public:
  RtpTransportControllerSend(Clock* clock, webrtc::RtcEventLog* event_log);

  // Implements RtpTransportControllerSendInterface
  PacketRouter* packet_router() override;
  SendSideCongestionController* send_side_cc() override;
  TransportFeedbackObserver* transport_feedback_observer() override;
  RtpPacketSender* packet_sender() override;

 private:
  PacketRouter packet_router_;
  SendSideCongestionController send_side_cc_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpTransportControllerSend);
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
