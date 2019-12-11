/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTCP_DEMUXER_H_
#define CALL_RTCP_DEMUXER_H_

#include <map>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "call/ssrc_binding_observer.h"

namespace webrtc {

class RtcpPacketSinkInterface;

// This class represents the RTCP demuxing, for a single RTP session (i.e., one
// SSRC space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
class RtcpDemuxer : public SsrcBindingObserver {
 public:
  RtcpDemuxer();
  ~RtcpDemuxer() override;

  // Registers a sink. The sink will be notified of incoming RTCP packets with
  // that sender-SSRC. The same sink can be registered for multiple SSRCs, and
  // the same SSRC can have multiple sinks. Null pointer is not allowed.
  // Sinks may be associated with both an SSRC and an RSID.
  // Sinks may be registered as SSRC/RSID-specific or broadcast, but not both.
  void AddSink(uint32_t sender_ssrc, RtcpPacketSinkInterface* sink);

  // Registers a sink. Once the RSID is resolved to an SSRC, the sink will be
  // notified of all RTCP packets with that sender-SSRC.
  // The same sink can be registered for multiple RSIDs, and
  // the same RSID can have multiple sinks. Null pointer is not allowed.
  // Sinks may be associated with both an SSRC and an RSID.
  // Sinks may be registered as SSRC/RSID-specific or broadcast, but not both.
  void AddSink(const std::string& rsid, RtcpPacketSinkInterface* sink);

  // Registers a sink. The sink will be notified of any incoming RTCP packet.
  // Null pointer is not allowed.
  // Sinks may be registered as SSRC/RSID-specific or broadcast, but not both.
  void AddBroadcastSink(RtcpPacketSinkInterface* sink);

  // Undo previous AddSink() calls with the given sink.
  void RemoveSink(const RtcpPacketSinkInterface* sink);

  // Undo AddBroadcastSink().
  void RemoveBroadcastSink(const RtcpPacketSinkInterface* sink);

  // Process a new RTCP packet and forward it to the appropriate sinks.
  void OnRtcpPacket(rtc::ArrayView<const uint8_t> packet);

  // Implement SsrcBindingObserver - become notified whenever RSIDs resolve to
  // an SSRC.
  void OnSsrcBoundToRsid(const std::string& rsid, uint32_t ssrc) override;

  // TODO(eladalon): Add the ability to resolve RSIDs and inform observers,
  // like in the RtpDemuxer case, once the relevant standard is finalized.

 private:
  // Records the association SSRCs to sinks.
  std::multimap<uint32_t, RtcpPacketSinkInterface*> ssrc_sinks_;

  // Records the association RSIDs to sinks.
  std::multimap<std::string, RtcpPacketSinkInterface*> rsid_sinks_;

  // Sinks which will receive notifications of all incoming RTCP packets.
  // Additional/removal of sinks is expected to be significantly less frequent
  // than RTCP message reception; container chosen for iteration performance.
  std::vector<RtcpPacketSinkInterface*> broadcast_sinks_;
};

}  // namespace webrtc

#endif  // CALL_RTCP_DEMUXER_H_
