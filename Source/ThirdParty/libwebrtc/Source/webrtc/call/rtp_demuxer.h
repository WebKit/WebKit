/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTP_DEMUXER_H_
#define WEBRTC_CALL_RTP_DEMUXER_H_

#include <map>
#include <set>
#include <string>

namespace webrtc {

class RtpPacketReceived;
class RtpPacketSinkInterface;

// This class represents the RTP demuxing, for a single RTP session (i.e., one
// ssrc space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
// TODO(nisse): Should be extended to also do MID-based demux and payload-type
// demux.
class RtpDemuxer {
 public:
  RtpDemuxer();
  ~RtpDemuxer();

  // Registers a sink. The same sink can be registered for multiple ssrcs, and
  // the same ssrc can have multiple sinks. Null pointer is not allowed.
  void AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // Registers a sink's association to an RSID. Null pointer is not allowed.
  void AddSink(const std::string& rsid, RtpPacketSinkInterface* sink);

  // Removes a sink. Return value reports if anything was actually removed.
  // Null pointer is not allowed.
  bool RemoveSink(const RtpPacketSinkInterface* sink);

  // Returns true if at least one matching sink was found, otherwise false.
  bool OnRtpPacket(const RtpPacketReceived& packet);

 private:
  // Records a sink<->SSRC association. This can happen by explicit
  // configuration by AddSink(ssrc...), or by inferred configuration from an
  // RSID-based configuration which is resolved to an SSRC upon
  // packet reception.
  void RecordSsrcToSinkAssociation(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // When a new packet arrives, we attempt to resolve extra associations,
  // such as which RSIDs are associated with which SSRCs.
  void FindSsrcAssociations(const RtpPacketReceived& packet);

  // This records the association SSRCs to sinks. Other associations, such
  // as by RSID, also end up here once the RSID, etc., is resolved to an SSRC.
  std::multimap<uint32_t, RtpPacketSinkInterface*> sinks_;

  // A sink may be associated with an RSID - RTP Stream ID. This tag has a
  // one-to-one association with an SSRC, but that SSRC is not yet known.
  // When it becomes known, the association of the sink to the RSID is deleted
  // from this container, and moved into |sinks_|.
  std::multimap<std::string, RtpPacketSinkInterface*> rsid_sinks_;

  // Iterating over |rsid_sinks_| for each incoming and performing multiple
  // string comparisons is of non-trivial cost. To avoid this cost, we only
  // check RSIDs for the first packet on each incoming SSRC stream.
  // (If RSID associations are added later, we check again.)
  std::set<uint32_t> processed_ssrcs_;

  // Avoid an attack that would create excessive logging.
  bool logged_max_processed_ssrcs_exceeded_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
