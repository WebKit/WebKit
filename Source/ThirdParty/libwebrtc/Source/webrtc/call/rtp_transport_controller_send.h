/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include "call/rtp_transport_controller_send_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"
#include "rtc_base/constructormagic.h"

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
  // TODO(holmer): Temporarily exposed, should be removed and the
  // appropriate methods should be added to this class instead.
  // In addition the PacedSender should be driven by this class, either
  // by owning the process thread, or later by using a task queue.
  PacedSender* pacer() override;
  SendSideCongestionController* send_side_cc() override;
  TransportFeedbackObserver* transport_feedback_observer() override;
  RtpPacketSender* packet_sender() override;
  const RtpKeepAliveConfig& keepalive_config() const override;

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps) override;

  void SetKeepAliveConfig(const RtpKeepAliveConfig& config);

 private:
  PacketRouter packet_router_;
  PacedSender pacer_;
  SendSideCongestionController send_side_cc_;
  RtpKeepAliveConfig keepalive_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpTransportControllerSend);
};

}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
