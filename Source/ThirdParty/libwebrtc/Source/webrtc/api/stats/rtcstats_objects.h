/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTCSTATS_OBJECTS_H_
#define API_STATS_RTCSTATS_OBJECTS_H_

#include <memory>
#include <string>
#include <vector>

#include "api/stats/rtcstats.h"

namespace webrtc {

// https://w3c.github.io/webrtc-pc/#idl-def-rtcdatachannelstate
struct RTCDataChannelState {
  static const char* const kConnecting;
  static const char* const kOpen;
  static const char* const kClosing;
  static const char* const kClosed;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcstatsicecandidatepairstate
struct RTCStatsIceCandidatePairState {
  static const char* const kFrozen;
  static const char* const kWaiting;
  static const char* const kInProgress;
  static const char* const kFailed;
  static const char* const kSucceeded;
};

// https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
struct RTCIceCandidateType {
  static const char* const kHost;
  static const char* const kSrflx;
  static const char* const kPrflx;
  static const char* const kRelay;
};

// https://w3c.github.io/webrtc-pc/#idl-def-rtcdtlstransportstate
struct RTCDtlsTransportState {
  static const char* const kNew;
  static const char* const kConnecting;
  static const char* const kConnected;
  static const char* const kClosed;
  static const char* const kFailed;
};

// |RTCMediaStreamTrackStats::kind| is not an enum in the spec but the only
// valid values are "audio" and "video".
// https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats-kind
struct RTCMediaStreamTrackKind {
  static const char* const kAudio;
  static const char* const kVideo;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcnetworktype
struct RTCNetworkType {
  static const char* const kBluetooth;
  static const char* const kCellular;
  static const char* const kEthernet;
  static const char* const kWifi;
  static const char* const kWimax;
  static const char* const kVpn;
  static const char* const kUnknown;
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

// https://w3c.github.io/webrtc-stats/#codec-dict*
class RTCCodecStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCCodecStats(const std::string& id, int64_t timestamp_us);
  RTCCodecStats(std::string&& id, int64_t timestamp_us);
  RTCCodecStats(const RTCCodecStats& other);
  ~RTCCodecStats() override;

  RTCStatsMember<uint32_t> payload_type;
  RTCStatsMember<std::string> mime_type;
  RTCStatsMember<uint32_t> clock_rate;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7061
  RTCStatsMember<uint32_t> channels;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7061
  RTCStatsMember<std::string> sdp_fmtp_line;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7061
  RTCStatsMember<std::string> implementation;
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
// TODO(hbos): Tracking bug https://bugs.webrtc.org/7062
class RTCIceCandidatePairStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidatePairStats(const std::string& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(std::string&& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(const RTCIceCandidatePairStats& other);
  ~RTCIceCandidatePairStats() override;

  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<std::string> local_candidate_id;
  RTCStatsMember<std::string> remote_candidate_id;
  // TODO(hbos): Support enum types?
  // "RTCStatsMember<RTCStatsIceCandidatePairState>"?
  RTCStatsMember<std::string> state;
  RTCStatsMember<uint64_t> priority;
  RTCStatsMember<bool> nominated;
  // TODO(hbos): Collect this the way the spec describes it. We have a value for
  // it but it is not spec-compliant. https://bugs.webrtc.org/7062
  RTCStatsMember<bool> writable;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<bool> readable;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<double> total_round_trip_time;
  RTCStatsMember<double> current_round_trip_time;
  RTCStatsMember<double> available_outgoing_bitrate;
  // TODO(hbos): Populate this value. It is wired up and collected the same way
  // "VideoBwe.googAvailableReceiveBandwidth" is, but that value is always
  // undefined. https://bugs.webrtc.org/7062
  RTCStatsMember<double> available_incoming_bitrate;
  RTCStatsMember<uint64_t> requests_received;
  RTCStatsMember<uint64_t> requests_sent;
  RTCStatsMember<uint64_t> responses_received;
  RTCStatsMember<uint64_t> responses_sent;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<uint64_t> retransmissions_received;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<uint64_t> retransmissions_sent;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<uint64_t> consent_requests_received;
  RTCStatsMember<uint64_t> consent_requests_sent;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<uint64_t> consent_responses_received;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7062
  RTCStatsMember<uint64_t> consent_responses_sent;
};

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
// TODO(hbos): |RTCStatsCollector| only collects candidates that are part of
// ice candidate pairs, but there could be candidates not paired with anything.
// crbug.com/632723
// TODO(qingsi): Add the stats of STUN binding requests (keepalives) and collect
// them in the new PeerConnection::GetStats.
class RTCIceCandidateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidateStats(const RTCIceCandidateStats& other);
  ~RTCIceCandidateStats() override;

  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<bool> is_remote;
  RTCStatsMember<std::string> network_type;
  RTCStatsMember<std::string> ip;
  RTCStatsMember<int32_t> port;
  RTCStatsMember<std::string> protocol;
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCIceCandidateType>"?
  RTCStatsMember<std::string> candidate_type;
  RTCStatsMember<int32_t> priority;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/632723
  RTCStatsMember<std::string> url;
  // TODO(hbos): |deleted = true| case is not supported by |RTCStatsCollector|.
  // crbug.com/632723
  RTCStatsMember<bool> deleted;  // = false

 protected:
  RTCIceCandidateStats(const std::string& id,
                       int64_t timestamp_us,
                       bool is_remote);
  RTCIceCandidateStats(std::string&& id, int64_t timestamp_us, bool is_remote);
};

// In the spec both local and remote varieties are of type RTCIceCandidateStats.
// But here we define them as subclasses of |RTCIceCandidateStats| because the
// |kType| need to be different ("RTCStatsType type") in the local/remote case.
// https://w3c.github.io/webrtc-stats/#rtcstatstype-str*
// This forces us to have to override copy() and type().
class RTCLocalIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCLocalIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCLocalIceCandidateStats(std::string&& id, int64_t timestamp_us);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

class RTCRemoteIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCRemoteIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCRemoteIceCandidateStats(std::string&& id, int64_t timestamp_us);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

// https://w3c.github.io/webrtc-stats/#msstats-dict*
// TODO(hbos): Tracking bug crbug.com/660827
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
// TODO(hbos): Tracking bug crbug.com/659137
class RTCMediaStreamTrackStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCMediaStreamTrackStats(const std::string& id,
                           int64_t timestamp_us,
                           const char* kind);
  RTCMediaStreamTrackStats(std::string&& id,
                           int64_t timestamp_us,
                           const char* kind);
  RTCMediaStreamTrackStats(const RTCMediaStreamTrackStats& other);
  ~RTCMediaStreamTrackStats() override;

  RTCStatsMember<std::string> track_identifier;
  RTCStatsMember<bool> remote_source;
  RTCStatsMember<bool> ended;
  // TODO(hbos): |RTCStatsCollector| does not return stats for detached tracks.
  // crbug.com/659137
  RTCStatsMember<bool> detached;
  // See |RTCMediaStreamTrackKind| for valid values.
  RTCStatsMember<std::string> kind;
  // TODO(gustaf): Implement jitter_buffer_delay for video (currently
  // implemented for audio only).
  // https://crbug.com/webrtc/8318
  RTCStatsMember<double> jitter_buffer_delay;
  // Video-only members
  RTCStatsMember<uint32_t> frame_width;
  RTCStatsMember<uint32_t> frame_height;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<double> frames_per_second;
  RTCStatsMember<uint32_t> frames_sent;
  RTCStatsMember<uint32_t> huge_frames_sent;
  RTCStatsMember<uint32_t> frames_received;
  RTCStatsMember<uint32_t> frames_decoded;
  RTCStatsMember<uint32_t> frames_dropped;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> frames_corrupted;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> partial_frames_lost;
  // TODO(hbos): Not collected by |RTCStatsCollector|. crbug.com/659137
  RTCStatsMember<uint32_t> full_frames_lost;
  // Audio-only members
  RTCStatsMember<double> audio_level;
  RTCStatsMember<double> total_audio_energy;
  RTCStatsMember<double> echo_return_loss;
  RTCStatsMember<double> echo_return_loss_enhancement;
  RTCStatsMember<uint64_t> total_samples_received;
  RTCStatsMember<double> total_samples_duration;
  RTCStatsMember<uint64_t> concealed_samples;
  RTCStatsMember<uint64_t> concealment_events;
};

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
class RTCPeerConnectionStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCPeerConnectionStats(const std::string& id, int64_t timestamp_us);
  RTCPeerConnectionStats(std::string&& id, int64_t timestamp_us);
  RTCPeerConnectionStats(const RTCPeerConnectionStats& other);
  ~RTCPeerConnectionStats() override;

  RTCStatsMember<uint32_t> data_channels_opened;
  RTCStatsMember<uint32_t> data_channels_closed;
};

// https://w3c.github.io/webrtc-stats/#streamstats-dict*
// TODO(hbos): Tracking bug crbug.com/657854
class RTCRTPStreamStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCRTPStreamStats(const RTCRTPStreamStats& other);
  ~RTCRTPStreamStats() override;

  RTCStatsMember<uint32_t> ssrc;
  // TODO(hbos): When the remote case is supported |RTCStatsCollector| needs to
  // set this. crbug.com/657855, 657856
  RTCStatsMember<std::string> associate_stats_id;
  // TODO(hbos): Remote case not supported by |RTCStatsCollector|.
  // crbug.com/657855, 657856
  RTCStatsMember<bool> is_remote;  // = false
  RTCStatsMember<std::string> media_type;
  RTCStatsMember<std::string> track_id;
  RTCStatsMember<std::string> transport_id;
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
  RTCStatsMember<uint64_t> qp_sum;

 protected:
  RTCRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCRTPStreamStats(std::string&& id, int64_t timestamp_us);
};

// https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
// TODO(hbos): Support the remote case |is_remote = true|.
// https://bugs.webrtc.org/7065
class RTCInboundRTPStreamStats final : public RTCRTPStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCInboundRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCInboundRTPStreamStats(std::string&& id, int64_t timestamp_us);
  RTCInboundRTPStreamStats(const RTCInboundRTPStreamStats& other);
  ~RTCInboundRTPStreamStats() override;

  RTCStatsMember<uint32_t> packets_received;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<int32_t> packets_lost;  // Signed per RFC 3550
  // TODO(hbos): Collect and populate this value for both "audio" and "video",
  // currently not collected for "video". https://bugs.webrtc.org/7065
  RTCStatsMember<double> jitter;
  RTCStatsMember<double> fraction_lost;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<double> round_trip_time;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> packets_discarded;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> packets_repaired;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> burst_packets_lost;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> burst_packets_discarded;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> burst_loss_count;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<uint32_t> burst_discard_count;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<double> burst_loss_rate;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<double> burst_discard_rate;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<double> gap_loss_rate;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7065
  RTCStatsMember<double> gap_discard_rate;
  RTCStatsMember<uint32_t> frames_decoded;
};

// https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*
// TODO(hbos): Support the remote case |is_remote = true|.
// https://bugs.webrtc.org/7066
class RTCOutboundRTPStreamStats final : public RTCRTPStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCOutboundRTPStreamStats(const std::string& id, int64_t timestamp_us);
  RTCOutboundRTPStreamStats(std::string&& id, int64_t timestamp_us);
  RTCOutboundRTPStreamStats(const RTCOutboundRTPStreamStats& other);
  ~RTCOutboundRTPStreamStats() override;

  RTCStatsMember<uint32_t> packets_sent;
  RTCStatsMember<uint64_t> bytes_sent;
  // TODO(hbos): Collect and populate this value. https://bugs.webrtc.org/7066
  RTCStatsMember<double> target_bitrate;
  RTCStatsMember<uint32_t> frames_encoded;
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
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCDtlsTransportState>"?
  RTCStatsMember<std::string> dtls_state;
  RTCStatsMember<std::string> selected_candidate_pair_id;
  RTCStatsMember<std::string> local_certificate_id;
  RTCStatsMember<std::string> remote_certificate_id;
};

}  // namespace webrtc

#endif  // API_STATS_RTCSTATS_OBJECTS_H_
