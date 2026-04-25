/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/receiver.h"

#include <cstdint>
#include <map>

#include "absl/base/nullability.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/timing/simulator/rtp_packet_simulator.h"

namespace webrtc::video_timing_simulator {

namespace {

// The RtcEventLog currently does not encode the RTX pt<->apt mapping. That is
// a problem for RTX->RTP decapsulation, but it is not a real problem for our
// "decoding", since the latter is just fake anyways. So in order to satisfy the
// `RtxReceiveStream` runtime requirements, we provide a noop pt<->apt mapping.
std::map<int, int> BuildNoopPtToAptMap() {
  // Build a noop mapping from the allowed range of PTs (see pc/g3doc/rtp.md).
  std::map<int, int> pt_to_apt;
  for (int pt = 35; pt <= 63; ++pt) {
    pt_to_apt[pt] = pt;
  }
  for (int pt = 96; pt <= 127; ++pt) {
    pt_to_apt[pt] = pt;
  }
  return pt_to_apt;
}

}  // namespace

Receiver::Receiver(const Environment& env,
                   uint32_t ssrc,
                   uint32_t rtx_ssrc,
                   ReceivedRtpPacketCallback* absl_nonnull
                       received_rtp_packet_cb)
    : env_(env),
      ssrc_(ssrc),
      rtx_ssrc_(rtx_ssrc),
      adapter_(received_rtp_packet_cb),
      rtx_receive_stream_(env_, &adapter_, BuildNoopPtToAptMap(), ssrc) {
  RTC_DCHECK_NE(ssrc_, rtx_ssrc_);
}

Receiver::~Receiver() = default;

void Receiver::InsertSimulatedPacket(
    const RtpPacketSimulator::SimulatedPacket& simulated_packet) {
  uint32_t packet_ssrc = simulated_packet.rtp_packet.Ssrc();
  bool is_video = (packet_ssrc == ssrc_);
  bool is_rtx = (packet_ssrc == rtx_ssrc_);
  if (!is_video && !is_rtx) {
    RTC_LOG(LS_WARNING) << "Received packet with ssrc=" << packet_ssrc
                        << " that was neither video [ssrc=" << ssrc_
                        << "] nor RTX [rtx_ssrc=" << rtx_ssrc_ << "]. "
                        << "Discarding it. (simulated_ts="
                        << env_.clock().CurrentTime() << ")";
    return;
  }
  if (is_rtx) {
    if (!simulated_packet.has_rtx_osn) {
      RTC_LOG(LS_INFO) << "RTX packet without RTX OSN received. Discarding it.";
      return;
    }
    InsertRtxPacket(simulated_packet.rtp_packet);
    return;
  }
  InsertVideoPacket(simulated_packet.rtp_packet);
}

void Receiver::InsertVideoPacket(const RtpPacketReceived& rtp_packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK_EQ(rtp_packet.Ssrc(), ssrc_);
  adapter_.OnRtpPacket(rtp_packet);
}

void Receiver::InsertRtxPacket(const RtpPacketReceived& rtp_packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK_EQ(rtp_packet.Ssrc(), rtx_ssrc_);
  rtx_receive_stream_.OnRtpPacket(rtp_packet);
}

}  // namespace webrtc::video_timing_simulator
