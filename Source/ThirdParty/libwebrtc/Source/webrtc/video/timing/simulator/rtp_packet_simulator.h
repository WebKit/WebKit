/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RTP_PACKET_SIMULATOR_H_
#define VIDEO_TIMING_SIMULATOR_RTP_PACKET_SIMULATOR_H_

#include "api/environment/environment.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc::video_timing_simulator {

// The `RtpPacketSimulator` takes a `LoggedRtpPacket` and produces a simulated
// `RtpPacketReceived`.
class RtpPacketSimulator {
 public:
  struct SimulatedPacket {
    RtpPacketReceived rtp_packet;
    // The presence of RTX OSN is provided here, out-of-band, since not all
    // RTX packets were historically logged with OSN.
    // See https://webrtc-review.googlesource.com/c/src/+/442320.
    // The value of the RTX OSN is passed in-band (as the RTX payload header),
    // when it is present.
    bool has_rtx_osn = false;
  };

  explicit RtpPacketSimulator(const Environment& env);
  ~RtpPacketSimulator() = default;

  RtpPacketSimulator(const RtpPacketSimulator&) = delete;
  RtpPacketSimulator& operator=(const RtpPacketSimulator&) = delete;

  // Builds a simulated `RtpPacketReceived` from a `LoggedRtpPacket`.
  // Notably, the simulated arrival time is taken from `env_.clock()` and not
  // from `logged_packet.log_time()`. This allows the caller to provide its own
  // clock offset, that might be different from the logged time base.
  SimulatedPacket SimulateRtpPacketReceived(
      const LoggedRtpPacket& logged_packet) const;

 private:
  const Environment env_;
  const RtpHeaderExtensionMap rtp_header_extension_map_;
};

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RTP_PACKET_SIMULATOR_H_
