/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_

#include "logging/rtc_event_log/events/rtc_event.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

class RtpPacketToSend;

class RtcEventRtpPacketOutgoing final : public RtcEvent {
 public:
  RtcEventRtpPacketOutgoing(const RtpPacketToSend& packet,
                            int probe_cluster_id);
  ~RtcEventRtpPacketOutgoing() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  RtpPacket header_;            // Only the packet's header will be stored here.
  const size_t packet_length_;  // Length before stripping away all but header.
  const int probe_cluster_id_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_RTP_PACKET_OUTGOING_H_
