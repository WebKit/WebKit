/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtcp_demuxer.h"

#include "api/rtp_headers.h"
#include "call/rtcp_packet_sink_interface.h"
#include "call/rtp_rtcp_demuxer_helper.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/checks.h"

namespace webrtc {

RtcpDemuxer::RtcpDemuxer() = default;

RtcpDemuxer::~RtcpDemuxer() {
  RTC_DCHECK(ssrc_sinks_.empty());
  RTC_DCHECK(rsid_sinks_.empty());
  RTC_DCHECK(broadcast_sinks_.empty());
}

void RtcpDemuxer::AddSink(uint32_t sender_ssrc, RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  RTC_DCHECK(!ContainerHasKey(broadcast_sinks_, sink));
  RTC_DCHECK(!MultimapAssociationExists(ssrc_sinks_, sender_ssrc, sink));
  ssrc_sinks_.emplace(sender_ssrc, sink);
}

void RtcpDemuxer::AddSink(const std::string& rsid,
                          RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  RTC_DCHECK(sink);
  RTC_DCHECK(!ContainerHasKey(broadcast_sinks_, sink));
  RTC_DCHECK(!MultimapAssociationExists(rsid_sinks_, rsid, sink));
  rsid_sinks_.emplace(rsid, sink);
}

void RtcpDemuxer::AddBroadcastSink(RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  RTC_DCHECK(!MultimapHasValue(ssrc_sinks_, sink));
  RTC_DCHECK(!MultimapHasValue(rsid_sinks_, sink));
  RTC_DCHECK(!ContainerHasKey(broadcast_sinks_, sink));
  broadcast_sinks_.push_back(sink);
}

void RtcpDemuxer::RemoveSink(const RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  size_t removal_count = RemoveFromMultimapByValue(&ssrc_sinks_, sink) +
                         RemoveFromMultimapByValue(&rsid_sinks_, sink);
  RTC_DCHECK_GT(removal_count, 0);
}

void RtcpDemuxer::RemoveBroadcastSink(const RtcpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  auto it = std::find(broadcast_sinks_.begin(), broadcast_sinks_.end(), sink);
  RTC_DCHECK(it != broadcast_sinks_.end());
  broadcast_sinks_.erase(it);
}

void RtcpDemuxer::OnRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
  // Perform sender-SSRC-based demuxing for packets with a sender-SSRC.
  rtc::Optional<uint32_t> sender_ssrc = ParseRtcpPacketSenderSsrc(packet);
  if (sender_ssrc) {
    auto it_range = ssrc_sinks_.equal_range(*sender_ssrc);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      it->second->OnRtcpPacket(packet);
    }
  }

  // All packets, even those without a sender-SSRC, are broadcast to sinks
  // which listen to broadcasts.
  for (RtcpPacketSinkInterface* sink : broadcast_sinks_) {
    sink->OnRtcpPacket(packet);
  }
}

void RtcpDemuxer::OnSsrcBoundToRsid(const std::string& rsid, uint32_t ssrc) {
  // Record the new SSRC association for all of the sinks that were associated
  // with the RSID.
  auto it_range = rsid_sinks_.equal_range(rsid);
  for (auto it = it_range.first; it != it_range.second; ++it) {
    RtcpPacketSinkInterface* sink = it->second;
    // Watch out for pre-existing SSRC-based associations.
    if (!MultimapAssociationExists(ssrc_sinks_, ssrc, sink)) {
      AddSink(ssrc, sink);
    }
  }

  // RSIDs are uniquely associated with SSRCs; no need to keep in memory
  // the RSID-to-sink association of resolved RSIDs.
  rsid_sinks_.erase(it_range.first, it_range.second);
}

}  // namespace webrtc
