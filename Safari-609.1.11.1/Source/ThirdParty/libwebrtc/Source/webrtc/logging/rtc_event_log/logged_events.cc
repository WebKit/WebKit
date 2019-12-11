/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "logging/rtc_event_log/logged_events.h"

namespace webrtc {

LoggedPacketInfo::LoggedPacketInfo(const LoggedRtpPacket& rtp,
                                   LoggedMediaType media_type,
                                   bool rtx,
                                   Timestamp capture_time)
    : ssrc(rtp.header.ssrc),
      stream_seq_no(rtp.header.sequenceNumber),
      size(static_cast<uint16_t>(rtp.total_length)),
      payload_type(rtp.header.payloadType),
      media_type(media_type),
      rtx(rtx),
      marker_bit(rtp.header.markerBit),
      has_transport_seq_no(rtp.header.extension.hasTransportSequenceNumber),
      transport_seq_no(static_cast<uint16_t>(
          has_transport_seq_no ? rtp.header.extension.transportSequenceNumber
                               : 0)),
      capture_time(capture_time),
      log_packet_time(Timestamp::us(rtp.log_time_us())) {}

LoggedPacketInfo::LoggedPacketInfo(const LoggedPacketInfo&) = default;

LoggedPacketInfo::~LoggedPacketInfo() {}
}  // namespace webrtc
