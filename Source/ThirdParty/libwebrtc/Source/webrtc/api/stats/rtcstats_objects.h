/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
#define WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_

#include <string>

#include "webrtc/api/stats/rtcstats.h"
#include "webrtc/base/export.h"

namespace webrtc {

// https://w3c.github.io/webrtc-pc/#idl-def-rtcdatachannelstate
struct RTCDataChannelState {
  static const char* kConnecting;
  static const char* kOpen;
  static const char* kClosing;
  static const char* kClosed;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcstatsicecandidatepairstate
struct RTCStatsIceCandidatePairState {
  static const char* kFrozen;
  static const char* kWaiting;
  static const char* kInProgress;
  static const char* kFailed;
  static const char* kSucceeded;
  static const char* kCancelled;
};

// https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
struct RTCIceCandidateType {
  static const char* kHost;
  static const char* kSrflx;
  static const char* kPrflx;
  static const char* kRelay;
};

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
class RTCCertificateStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCCertificateStats(const std::string& id, int64_t timestamp_us);
  RTCCertificateStats(std::string&& id, int64_t timestamp_us);
  RTCCertificateStats(const RTCCertificateStats& other);
  ~RTCCertificateStats() override;

  RTCStatsMember<std::string> fingerprint;
  RTCStatsMember<std::string> fingerprint_algorithm;
  RTCStatsMember<std::string> base64_certificate;
  RTCStatsMember<std::string> issuer_certificate_id;
};

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
class RTCDataChannelStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCDataChannelStats(const std::string& id, int64_t timestamp_us);
  RTCDataChannelStats(std::string&& id, int64_t timestamp_us);
  RTCDataChannelStats(const RTCDataChannelStats& other);
  ~RTCDataChannelStats() override;

  RTCStatsMember<std::string> label;
  RTCStatsMember<std::string> protocol;
  RTCStatsMember<int32_t> datachannelid;
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCDataChannelState>"?
  RTCStatsMember<std::string> state;
  RTCStatsMember<uint32_t> messages_sent;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint32_t> messages_received;
  RTCStatsMember<uint64_t> bytes_received;
};

// https://w3c.github.io/webrtc-stats/#candidatepair-dict*
// TODO(hbos): Finish implementation. Tracking bug crbug.com/633550
class RTCIceCandidatePairStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidatePairStats(const std::string& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(std::string&& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(const RTCIceCandidatePairStats& other);
  ~RTCIceCandidatePairStats() override;

  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550, 653873
  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<std::string> local_candidate_id;
  RTCStatsMember<std::string> remote_candidate_id;
  // TODO(hbos): Support enum types?
  // "RTCStatsMember<RTCStatsIceCandidatePairState>"?
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<std::string> state;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> priority;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<bool> nominated;
  // TODO(hbos): Collected by |RTCStatsCollector| but different than the spec.
  // crbug.com/633550
  RTCStatsMember<bool> writable;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<bool> readable;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> bytes_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<double> total_rtt;
  // TODO(hbos): Collected by |RTCStatsCollector| but different than the spec.
  // crbug.com/633550
  RTCStatsMember<double> current_rtt;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<double> available_outgoing_bitrate;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<double> available_incoming_bitrate;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> requests_received;
  RTCStatsMember<uint64_t> requests_sent;
  RTCStatsMember<uint64_t> responses_received;
  RTCStatsMember<uint64_t> responses_sent;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> retransmissions_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> retransmissions_sent;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> consent_requests_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> consent_requests_sent;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> consent_responses_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/633550
  RTCStatsMember<uint64_t> consent_responses_sent;
};

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
// TODO(hbos): |RTCStatsCollector| only collects candidates that are part of
// ice candidate pairs, but there could be candidates not paired with anything.
// crbug.com/632723
class RTCIceCandidateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidateStats(const RTCIceCandidateStats& other);
  ~RTCIceCandidateStats() override;

  RTCStatsMember<std::string> ip;
  RTCStatsMember<int32_t> port;
  RTCStatsMember<std::string> protocol;
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCIceCandidateType>"?
  RTCStatsMember<std::string> candidate_type;
  RTCStatsMember<int32_t> priority;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/632723
  RTCStatsMember<std::string> url;

 protected:
  RTCIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCIceCandidateStats(std::string&& id, int64_t timestamp_us);
};

// In the spec both local and remote varieties are of type RTCIceCandidateStats.
// But here we define them as subclasses of |RTCIceCandidateStats| because the
// |kType| need to be different ("RTCStatsType type") in the local/remote case.
// https://w3c.github.io/webrtc-stats/#rtcstatstype-str*
class RTCLocalIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCLocalIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCLocalIceCandidateStats(std::string&& id, int64_t timestamp_us);
  const char* type() const override;
};

class RTCRemoteIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCRemoteIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCRemoteIceCandidateStats(std::string&& id, int64_t timestamp_us);
  const char* type() const override;
};

// https://w3c.github.io/webrtc-stats/#msstats-dict*
// TODO(hbos): Finish implementation. Tracking bug crbug.com/660827
class RTCMediaStreamStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCMediaStreamStats(const std::string& id, int64_t timestamp_us);
  RTCMediaStreamStats(std::string&& id, int64_t timestamp_us);
  RTCMediaStreamStats(const RTCMediaStreamStats& other);
  ~RTCMediaStreamStats() override;

  RTCStatsMember<std::string> stream_identifier;
  RTCStatsMember<std::vector<std::string>> track_ids;
};

// https://w3c.github.io/webrtc-stats/#mststats-dict*
// TODO(hbos): Finish implementation. Tracking bug crbug.com/659137
class RTCMediaStreamTrackStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCMediaStreamTrackStats(const std::string& id, int64_t timestamp_us);
  RTCMediaStreamTrackStats(std::string&& id, int64_t timestamp_us);
  RTCMediaStreamTrackStats(const RTCMediaStreamTrackStats& other);
  ~RTCMediaStreamTrackStats() override;

  RTCStatsMember<std::string> track_identifier;
  RTCStatsMember<bool> remote_source;
  RTCStatsMember<bool> ended;
  // TODO(hbos): |RTCStatsCollector| does not return stats for detached tracks.
  // crbug.com/659137
  RTCStatsMember<bool> detached;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<std::vector<std::string>> ssrc_ids;
  // Video-only members
  RTCStatsMember<uint32_t> frame_width;
  RTCStatsMember<uint32_t> frame_height;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<double> frames_per_second;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_sent;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_decoded;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_dropped;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_corrupted;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> partial_frames_lost;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> full_frames_lost;
  // Audio-only members
  RTCStatsMember<double> audio_level;
  RTCStatsMember<double> echo_return_loss;
  RTCStatsMember<double> echo_return_loss_enhancement;
};

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
// TODO(hbos): Finish implementation. Tracking bug crbug.com/636818
class RTCPeerConnectionStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCPeerConnectionStats(const std::string& id, int64_t timestamp_us);
  RTCPeerConnectionStats(std::string&& id, int64_t timestamp_us);
  RTCPeerConnectionStats(const RTCPeerConnectionStats& other);
  ~RTCPeerConnectionStats() override;

  // TODO(hbos): Collected by |RTCStatsCollector| but different than the spec.
  // crbug.com/636818
  RTCStatsMember<uint32_t> data_channels_opened;
  // TODO(hbos): Collected by |RTCStatsCollector| but different than the spec.
  // crbug.com/636818
  RTCStatsMember<uint32_t> data_channels_closed;
};

// https://w3c.github.io/webrtc-stats/#streamstats-dict*
// TODO(hbos): Finish implementation. Tracking bug crbug.com/657854
class RTCRTPStreamStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCRTPStreamStats(const RTCRTPStreamStats& other);
  ~RTCRTPStreamStats() override;

  RTCStatsMember<std::string> ssrc;
  // TODO(hbos): When the remote case is supported |RTCStatsCollector| needs to
  // set this. crbug.com/657855, 657856
  RTCStatsMember<std::string> associate_stats_id;
  // TODO(hbos): Remote case not supported by |RTCStatsCollector|.
  // crbug.com/657855, 657856
  RTCStatsMember<bool> is_remote;  // = false
  RTCStatsMember<std::string> media_type;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657854, 659137
  RTCStatsMember<std::string> media_track_id;
  RTCStatsMember<std::string> transport_id;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657854, 659117
  RTCStatsMember<std::string> codec_id;
  // FIR and PLI counts are only defined for |media_type == "video"|.
  RTCStatsMember<uint32_t> fir_count;
  RTCStatsMember<uint32_t> pli_count;
  // TODO(hbos): NACK count should be collected by |RTCStatsCollector| for both
  // audio and video but is only defined in the "video" case. crbug.com/657856
  RTCStatsMember<uint32_t> nack_count;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657854
  // SLI count is only defined for |media_type == "video"|.
  RTCStatsMember<uint32_t> sli_count;

 protected:
  RTCRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCRTPStreamStats(std::string&& id, int64_t timestamp_us);
};

// https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
// TODO(hbos): Finish implementation and support the remote case
// |is_remote = true|. Tracking bug crbug.com/657855
class WEBRTC_EXPORT RTCInboundRTPStreamStats final : public RTCRTPStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCInboundRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCInboundRTPStreamStats(std::string&& id, int64_t timestamp_us);
  RTCInboundRTPStreamStats(const RTCInboundRTPStreamStats& other);
  ~RTCInboundRTPStreamStats() override;

  RTCStatsMember<uint32_t> packets_received;
  RTCStatsMember<uint64_t> bytes_received;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> packets_lost;
  // TODO(hbos): Not collected in the "video" case by |RTCStatsCollector|.
  // crbug.com/657855
  RTCStatsMember<double> jitter;
  RTCStatsMember<double> fraction_lost;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> packets_discarded;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> packets_repaired;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> burst_packets_lost;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> burst_packets_discarded;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> burst_loss_count;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<uint32_t> burst_discard_count;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<double> burst_loss_rate;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<double> burst_discard_rate;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<double> gap_loss_rate;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657855
  RTCStatsMember<double> gap_discard_rate;
};

// https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*
// TODO(hbos): Finish implementation and support the remote case
// |is_remote = true|. Tracking bug crbug.com/657856
class WEBRTC_EXPORT RTCOutboundRTPStreamStats final : public RTCRTPStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCOutboundRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCOutboundRTPStreamStats(std::string&& id, int64_t timestamp_us);
  RTCOutboundRTPStreamStats(const RTCOutboundRTPStreamStats& other);
  ~RTCOutboundRTPStreamStats() override;

  RTCStatsMember<uint32_t> packets_sent;
  RTCStatsMember<uint64_t> bytes_sent;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/657856
  RTCStatsMember<double> target_bitrate;
  RTCStatsMember<double> round_trip_time;
};

// https://w3c.github.io/webrtc-stats/#transportstats-dict*
class RTCTransportStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTransportStats(const std::string& id, int64_t timestamp_us);
  RTCTransportStats(std::string&& id, int64_t timestamp_us);
  RTCTransportStats(const RTCTransportStats& other);
  ~RTCTransportStats() override;

  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<std::string> rtcp_transport_stats_id;
  RTCStatsMember<bool> active_connection;
  RTCStatsMember<std::string> selected_candidate_pair_id;
  RTCStatsMember<std::string> local_certificate_id;
  RTCStatsMember<std::string> remote_certificate_id;
};

}  // namespace webrtc

#endif  // WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
