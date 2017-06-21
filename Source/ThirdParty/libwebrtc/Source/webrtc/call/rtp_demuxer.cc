/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/call/rtp_demuxer.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/call/rtp_packet_sink_interface.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

namespace {

constexpr size_t kMaxProcessedSsrcs = 1000;  // Prevent memory overuse.

template <typename Key, typename Value>
bool MultimapAssociationExists(const std::multimap<Key, Value>& multimap,
                               Key key,
                               Value val) {
  auto it_range = multimap.equal_range(key);
  using Reference = typename std::multimap<Key, Value>::const_reference;
  return std::any_of(it_range.first, it_range.second,
                     [val](Reference elem) { return elem.second == val; });
}

template <typename Key, typename Value>
size_t RemoveFromMultimapByValue(std::multimap<Key, Value*>* multimap,
                                 const Value* value) {
  size_t count = 0;
  for (auto it = multimap->begin(); it != multimap->end();) {
    if (it->second == value) {
      it = multimap->erase(it);
      ++count;
    } else {
      ++it;
    }
  }
  return count;
}

}  // namespace

RtpDemuxer::RtpDemuxer() {}

RtpDemuxer::~RtpDemuxer() {
  RTC_DCHECK(sinks_.empty());
}

void RtpDemuxer::AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink) {
  RecordSsrcToSinkAssociation(ssrc, sink);
}

void RtpDemuxer::AddSink(const std::string& rsid,
                         RtpPacketSinkInterface* sink) {
  RTC_DCHECK(StreamId::IsLegalName(rsid));
  RTC_DCHECK(sink);
  RTC_DCHECK(!MultimapAssociationExists(rsid_sinks_, rsid, sink));

  rsid_sinks_.emplace(rsid, sink);

  // This RSID might now map to an SSRC which we saw earlier.
  processed_ssrcs_.clear();
}

bool RtpDemuxer::RemoveSink(const RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  return (RemoveFromMultimapByValue(&sinks_, sink) +
          RemoveFromMultimapByValue(&rsid_sinks_, sink)) > 0;
}

void RtpDemuxer::RecordSsrcToSinkAssociation(uint32_t ssrc,
                                             RtpPacketSinkInterface* sink) {
  RTC_DCHECK(sink);
  // The association might already have been set by a different
  // configuration source.
  if (!MultimapAssociationExists(sinks_, ssrc, sink)) {
    sinks_.emplace(ssrc, sink);
  }
}

bool RtpDemuxer::OnRtpPacket(const RtpPacketReceived& packet) {
  FindSsrcAssociations(packet);
  auto it_range = sinks_.equal_range(packet.Ssrc());
  for (auto it = it_range.first; it != it_range.second; ++it) {
    it->second->OnRtpPacket(packet);
  }
  return it_range.first != it_range.second;
}

void RtpDemuxer::FindSsrcAssociations(const RtpPacketReceived& packet) {
  // Avoid expensive string comparisons for RSID by looking the sinks up only
  // by SSRC whenever possible.
  if (processed_ssrcs_.find(packet.Ssrc()) != processed_ssrcs_.cend()) {
    return;
  }

  // RSID-based associations:
  std::string rsid;
  if (packet.GetExtension<RtpStreamId>(&rsid)) {
    // All streams associated with this RSID need to be marked as associated
    // with this SSRC (if they aren't already).
    auto it_range = rsid_sinks_.equal_range(rsid);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      RecordSsrcToSinkAssociation(packet.Ssrc(), it->second);
    }

    // To prevent memory-overuse attacks, forget this RSID. Future packets
    // with this RSID, but a different SSRC, will not spawn new associations.
    rsid_sinks_.erase(it_range.first, it_range.second);
  }

  if (processed_ssrcs_.size() < kMaxProcessedSsrcs) {  // Prevent memory overuse
    processed_ssrcs_.insert(packet.Ssrc());  // Avoid re-examining in-depth.
  } else if (!logged_max_processed_ssrcs_exceeded_) {
    LOG(LS_WARNING) << "More than " << kMaxProcessedSsrcs
                    << " different SSRCs seen.";
    logged_max_processed_ssrcs_exceeded_ = true;
  }
}

}  // namespace webrtc
