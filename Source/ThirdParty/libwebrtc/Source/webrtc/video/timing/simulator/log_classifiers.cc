/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/log_classifiers.h"

#include <optional>

#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "rtc_base/checks.h"

namespace webrtc::video_timing_simulator {

std::optional<RtxOsnLoggingStatus> GetRtxOsnLoggingStatus(
    const ParsedRtcEventLog& parsed_log) {
  // Logs without video RTX packets are undeterminable.
  bool log_contained_video_rtx_packets = false;

  // Boolean accumulators for the presence of `rtx_original_sequence_number`
  // for the video RTX packets across the log.
  bool all_presence = true;
  bool any_presence = false;

  // Loop over all video RTX streams, and then over packets within the streams.
  for (const ParsedRtcEventLog::LoggedRtpStreamIncoming& stream :
       parsed_log.incoming_rtp_packets_by_ssrc()) {
    // Check for video.
    if (!parsed_log.incoming_video_ssrcs().contains(stream.ssrc)) {
      continue;
    }
    // Check for RTX.
    if (!parsed_log.incoming_rtx_ssrcs().contains(stream.ssrc)) {
      continue;
    }
    // Check all packets for this video RTX stream.
    for (const LoggedRtpPacketIncoming& incoming_packet :
         stream.incoming_packets) {
      log_contained_video_rtx_packets = true;
      bool presence =
          incoming_packet.rtp.rtx_original_sequence_number.has_value();
      all_presence = all_presence && presence;
      any_presence = any_presence || presence;
    }
  }

  if (!log_contained_video_rtx_packets) {
    return std::nullopt;
  }
  if (all_presence) {
    RTC_DCHECK(any_presence);
    return RtxOsnLoggingStatus::kAllRtxOsnLogged;
  }
  if (any_presence) {
    return RtxOsnLoggingStatus::kSomeRtxOsnLogged;
  }
  return RtxOsnLoggingStatus::kNoRtxOsnLogged;
}

}  // namespace webrtc::video_timing_simulator
