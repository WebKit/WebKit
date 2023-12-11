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

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/stats/rtc_stats.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
class RTC_EXPORT RTCCertificateStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCCertificateStats(std::string id, Timestamp timestamp);
  RTCCertificateStats(const RTCCertificateStats& other);
  ~RTCCertificateStats() override;

  RTCStatsMember<std::string> fingerprint;
  RTCStatsMember<std::string> fingerprint_algorithm;
  RTCStatsMember<std::string> base64_certificate;
  RTCStatsMember<std::string> issuer_certificate_id;
};

// https://w3c.github.io/webrtc-stats/#codec-dict*
class RTC_EXPORT RTCCodecStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCCodecStats(std::string id, Timestamp timestamp);
  RTCCodecStats(const RTCCodecStats& other);
  ~RTCCodecStats() override;

  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<uint32_t> payload_type;
  RTCStatsMember<std::string> mime_type;
  RTCStatsMember<uint32_t> clock_rate;
  RTCStatsMember<uint32_t> channels;
  RTCStatsMember<std::string> sdp_fmtp_line;
};

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
class RTC_EXPORT RTCDataChannelStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCDataChannelStats(std::string id, Timestamp timestamp);
  RTCDataChannelStats(const RTCDataChannelStats& other);
  ~RTCDataChannelStats() override;

  RTCStatsMember<std::string> label;
  RTCStatsMember<std::string> protocol;
  RTCStatsMember<int32_t> data_channel_identifier;
  RTCStatsMember<std::string> state;
  RTCStatsMember<uint32_t> messages_sent;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint32_t> messages_received;
  RTCStatsMember<uint64_t> bytes_received;
};

// https://w3c.github.io/webrtc-stats/#candidatepair-dict*
class RTC_EXPORT RTCIceCandidatePairStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidatePairStats(std::string id, Timestamp timestamp);
  RTCIceCandidatePairStats(const RTCIceCandidatePairStats& other);
  ~RTCIceCandidatePairStats() override;

  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<std::string> local_candidate_id;
  RTCStatsMember<std::string> remote_candidate_id;
  RTCStatsMember<std::string> state;
  // Obsolete: priority
  RTCStatsMember<uint64_t> priority;
  RTCStatsMember<bool> nominated;
  // `writable` does not exist in the spec and old comments suggest it used to
  // exist but was incorrectly implemented.
  // TODO(https://crbug.com/webrtc/14171): Standardize and/or modify
  // implementation.
  RTCStatsMember<bool> writable;
  RTCStatsMember<uint64_t> packets_sent;
  RTCStatsMember<uint64_t> packets_received;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<double> total_round_trip_time;
  RTCStatsMember<double> current_round_trip_time;
  RTCStatsMember<double> available_outgoing_bitrate;
  RTCStatsMember<double> available_incoming_bitrate;
  RTCStatsMember<uint64_t> requests_received;
  RTCStatsMember<uint64_t> requests_sent;
  RTCStatsMember<uint64_t> responses_received;
  RTCStatsMember<uint64_t> responses_sent;
  RTCStatsMember<uint64_t> consent_requests_sent;
  RTCStatsMember<uint64_t> packets_discarded_on_send;
  RTCStatsMember<uint64_t> bytes_discarded_on_send;
  RTCStatsMember<double> last_packet_received_timestamp;
  RTCStatsMember<double> last_packet_sent_timestamp;
};

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
class RTC_EXPORT RTCIceCandidateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidateStats(const RTCIceCandidateStats& other);
  ~RTCIceCandidateStats() override;

  RTCStatsMember<std::string> transport_id;
  // Obsolete: is_remote
  RTCStatsMember<bool> is_remote;
  RTCStatsMember<std::string> network_type;
  RTCStatsMember<std::string> ip;
  RTCStatsMember<std::string> address;
  RTCStatsMember<int32_t> port;
  RTCStatsMember<std::string> protocol;
  RTCStatsMember<std::string> relay_protocol;
  RTCStatsMember<std::string> candidate_type;
  RTCStatsMember<int32_t> priority;
  RTCStatsMember<std::string> url;
  RTCStatsMember<std::string> foundation;
  RTCStatsMember<std::string> related_address;
  RTCStatsMember<int32_t> related_port;
  RTCStatsMember<std::string> username_fragment;
  RTCStatsMember<std::string> tcp_type;

  // The following metrics are NOT exposed to JavaScript. We should consider
  // standardizing or removing them.
  RTCStatsMember<bool> vpn;
  RTCStatsMember<std::string> network_adapter_type;

 protected:
  RTCIceCandidateStats(std::string id, Timestamp timestamp, bool is_remote);
};

// In the spec both local and remote varieties are of type RTCIceCandidateStats.
// But here we define them as subclasses of `RTCIceCandidateStats` because the
// `kType` need to be different ("RTCStatsType type") in the local/remote case.
// https://w3c.github.io/webrtc-stats/#rtcstatstype-str*
// This forces us to have to override copy() and type().
class RTC_EXPORT RTCLocalIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCLocalIceCandidateStats(std::string id, Timestamp timestamp);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

class RTC_EXPORT RTCRemoteIceCandidateStats final
    : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCRemoteIceCandidateStats(std::string id, Timestamp timestamp);
  std::unique_ptr<RTCStats> copy() const override;
  const char* type() const override;
};

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
class RTC_EXPORT RTCPeerConnectionStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCPeerConnectionStats(std::string id, Timestamp timestamp);
  RTCPeerConnectionStats(const RTCPeerConnectionStats& other);
  ~RTCPeerConnectionStats() override;

  RTCStatsMember<uint32_t> data_channels_opened;
  RTCStatsMember<uint32_t> data_channels_closed;
};

// https://w3c.github.io/webrtc-stats/#streamstats-dict*
class RTC_EXPORT RTCRtpStreamStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCRtpStreamStats(const RTCRtpStreamStats& other);
  ~RTCRtpStreamStats() override;

  RTCStatsMember<uint32_t> ssrc;
  RTCStatsMember<std::string> kind;
  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<std::string> codec_id;

 protected:
  RTCRtpStreamStats(std::string id, Timestamp timestamp);
};

// https://www.w3.org/TR/webrtc-stats/#receivedrtpstats-dict*
class RTC_EXPORT RTCReceivedRtpStreamStats : public RTCRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCReceivedRtpStreamStats(const RTCReceivedRtpStreamStats& other);
  ~RTCReceivedRtpStreamStats() override;

  RTCStatsMember<double> jitter;
  RTCStatsMember<int32_t> packets_lost;  // Signed per RFC 3550

 protected:
  RTCReceivedRtpStreamStats(std::string id, Timestamp timestamp);
};

// https://www.w3.org/TR/webrtc-stats/#sentrtpstats-dict*
class RTC_EXPORT RTCSentRtpStreamStats : public RTCRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCSentRtpStreamStats(const RTCSentRtpStreamStats& other);
  ~RTCSentRtpStreamStats() override;

  RTCStatsMember<uint64_t> packets_sent;
  RTCStatsMember<uint64_t> bytes_sent;

 protected:
  RTCSentRtpStreamStats(std::string id, Timestamp timestamp);
};

// https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
class RTC_EXPORT RTCInboundRtpStreamStats final
    : public RTCReceivedRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCInboundRtpStreamStats(std::string id, Timestamp timestamp);
  RTCInboundRtpStreamStats(const RTCInboundRtpStreamStats& other);
  ~RTCInboundRtpStreamStats() override;

  RTCStatsMember<std::string> playout_id;
  RTCStatsMember<std::string> track_identifier;
  RTCStatsMember<std::string> mid;
  RTCStatsMember<std::string> remote_id;
  RTCStatsMember<uint32_t> packets_received;
  RTCStatsMember<uint64_t> packets_discarded;
  RTCStatsMember<uint64_t> fec_packets_received;
  RTCStatsMember<uint64_t> fec_bytes_received;
  RTCStatsMember<uint64_t> fec_packets_discarded;
  // Inbound FEC SSRC. Only present if a mechanism like FlexFEC is negotiated.
  RTCStatsMember<uint32_t> fec_ssrc;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<uint64_t> header_bytes_received;
  // Inbound RTX stats. Only defined when RTX is used and it is therefore
  // possible to distinguish retransmissions.
  RTCStatsMember<uint64_t> retransmitted_packets_received;
  RTCStatsMember<uint64_t> retransmitted_bytes_received;
  RTCStatsMember<uint32_t> rtx_ssrc;

  RTCStatsMember<double> last_packet_received_timestamp;
  RTCStatsMember<double> jitter_buffer_delay;
  RTCStatsMember<double> jitter_buffer_target_delay;
  RTCStatsMember<double> jitter_buffer_minimum_delay;
  RTCStatsMember<uint64_t> jitter_buffer_emitted_count;
  RTCStatsMember<uint64_t> total_samples_received;
  RTCStatsMember<uint64_t> concealed_samples;
  RTCStatsMember<uint64_t> silent_concealed_samples;
  RTCStatsMember<uint64_t> concealment_events;
  RTCStatsMember<uint64_t> inserted_samples_for_deceleration;
  RTCStatsMember<uint64_t> removed_samples_for_acceleration;
  RTCStatsMember<double> audio_level;
  RTCStatsMember<double> total_audio_energy;
  RTCStatsMember<double> total_samples_duration;
  // Stats below are only implemented or defined for video.
  RTCStatsMember<uint32_t> frames_received;
  RTCStatsMember<uint32_t> frame_width;
  RTCStatsMember<uint32_t> frame_height;
  RTCStatsMember<double> frames_per_second;
  RTCStatsMember<uint32_t> frames_decoded;
  RTCStatsMember<uint32_t> key_frames_decoded;
  RTCStatsMember<uint32_t> frames_dropped;
  RTCStatsMember<double> total_decode_time;
  RTCStatsMember<double> total_processing_delay;
  RTCStatsMember<double> total_assembly_time;
  RTCStatsMember<uint32_t> frames_assembled_from_multiple_packets;
  // TODO(https://crbug.com/webrtc/15600): Implement framesRendered, which is
  // incremented at the same time that totalInterFrameDelay and
  // totalSquaredInterFrameDelay is incremented. (Dividing inter-frame delay by
  // framesDecoded is slightly wrong.)
  // https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-framesrendered
  //
  // TODO(https://crbug.com/webrtc/15601): Inter-frame, pause and freeze metrics
  // all related to when the frame is rendered, but our implementation measures
  // at delivery to sink, not at actual render time. When we have an actual
  // frame rendered callback, move the calculating of these metrics to there in
  // order to make them more accurate.
  RTCStatsMember<double> total_inter_frame_delay;
  RTCStatsMember<double> total_squared_inter_frame_delay;
  RTCStatsMember<uint32_t> pause_count;
  RTCStatsMember<double> total_pauses_duration;
  RTCStatsMember<uint32_t> freeze_count;
  RTCStatsMember<double> total_freezes_duration;
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcinboundrtpstreamstats-contenttype
  RTCStatsMember<std::string> content_type;
  // Only populated if audio/video sync is enabled.
  // TODO(https://crbug.com/webrtc/14177): Expose even if A/V sync is off?
  RTCStatsMember<double> estimated_playout_timestamp;
  // Only defined for video.
  // In JavaScript, this is only exposed if HW exposure is allowed.
  RTCStatsMember<std::string> decoder_implementation;
  // FIR and PLI counts are only defined for |kind == "video"|.
  RTCStatsMember<uint32_t> fir_count;
  RTCStatsMember<uint32_t> pli_count;
  RTCStatsMember<uint32_t> nack_count;
  RTCStatsMember<uint64_t> qp_sum;
  // This is a remnant of the legacy getStats() API. When the "video-timing"
  // header extension is used,
  // https://webrtc.github.io/webrtc-org/experiments/rtp-hdrext/video-timing/,
  // `googTimingFrameInfo` is exposed with the value of
  // TimingFrameInfo::ToString().
  // TODO(https://crbug.com/webrtc/14586): Unship or standardize this metric.
  RTCStatsMember<std::string> goog_timing_frame_info;
  // In JavaScript, this is only exposed if HW exposure is allowed.
  RTCStatsMember<bool> power_efficient_decoder;

  // The following metrics are NOT exposed to JavaScript. We should consider
  // standardizing or removing them.
  RTCStatsMember<uint64_t> jitter_buffer_flushes;
  RTCStatsMember<uint64_t> delayed_packet_outage_samples;
  RTCStatsMember<double> relative_packet_arrival_delay;
  RTCStatsMember<uint32_t> interruption_count;
  RTCStatsMember<double> total_interruption_duration;
  RTCStatsMember<double> min_playout_delay;
};

// https://w3c.github.io/webrtc-stats/#outboundrtpstats-dict*
class RTC_EXPORT RTCOutboundRtpStreamStats final
    : public RTCSentRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCOutboundRtpStreamStats(std::string id, Timestamp timestamp);
  RTCOutboundRtpStreamStats(const RTCOutboundRtpStreamStats& other);
  ~RTCOutboundRtpStreamStats() override;

  RTCStatsMember<std::string> media_source_id;
  RTCStatsMember<std::string> remote_id;
  RTCStatsMember<std::string> mid;
  RTCStatsMember<std::string> rid;
  RTCStatsMember<uint64_t> retransmitted_packets_sent;
  RTCStatsMember<uint64_t> header_bytes_sent;
  RTCStatsMember<uint64_t> retransmitted_bytes_sent;
  RTCStatsMember<double> target_bitrate;
  RTCStatsMember<uint32_t> frames_encoded;
  RTCStatsMember<uint32_t> key_frames_encoded;
  RTCStatsMember<double> total_encode_time;
  RTCStatsMember<uint64_t> total_encoded_bytes_target;
  RTCStatsMember<uint32_t> frame_width;
  RTCStatsMember<uint32_t> frame_height;
  RTCStatsMember<double> frames_per_second;
  RTCStatsMember<uint32_t> frames_sent;
  RTCStatsMember<uint32_t> huge_frames_sent;
  RTCStatsMember<double> total_packet_send_delay;
  RTCStatsMember<std::string> quality_limitation_reason;
  RTCStatsMember<std::map<std::string, double>> quality_limitation_durations;
  // https://w3c.github.io/webrtc-stats/#dom-rtcoutboundrtpstreamstats-qualitylimitationresolutionchanges
  RTCStatsMember<uint32_t> quality_limitation_resolution_changes;
  // https://w3c.github.io/webrtc-provisional-stats/#dom-rtcoutboundrtpstreamstats-contenttype
  RTCStatsMember<std::string> content_type;
  // In JavaScript, this is only exposed if HW exposure is allowed.
  // Only implemented for video.
  // TODO(https://crbug.com/webrtc/14178): Implement for audio as well.
  RTCStatsMember<std::string> encoder_implementation;
  // FIR and PLI counts are only defined for |kind == "video"|.
  RTCStatsMember<uint32_t> fir_count;
  RTCStatsMember<uint32_t> pli_count;
  RTCStatsMember<uint32_t> nack_count;
  RTCStatsMember<uint64_t> qp_sum;
  RTCStatsMember<bool> active;
  // In JavaScript, this is only exposed if HW exposure is allowed.
  RTCStatsMember<bool> power_efficient_encoder;
  RTCStatsMember<std::string> scalability_mode;

  // RTX ssrc. Only present if RTX is negotiated.
  RTCStatsMember<uint32_t> rtx_ssrc;
};

// https://w3c.github.io/webrtc-stats/#remoteinboundrtpstats-dict*
class RTC_EXPORT RTCRemoteInboundRtpStreamStats final
    : public RTCReceivedRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCRemoteInboundRtpStreamStats(std::string id, Timestamp timestamp);
  RTCRemoteInboundRtpStreamStats(const RTCRemoteInboundRtpStreamStats& other);
  ~RTCRemoteInboundRtpStreamStats() override;

  RTCStatsMember<std::string> local_id;
  RTCStatsMember<double> round_trip_time;
  RTCStatsMember<double> fraction_lost;
  RTCStatsMember<double> total_round_trip_time;
  RTCStatsMember<int32_t> round_trip_time_measurements;
};

// https://w3c.github.io/webrtc-stats/#remoteoutboundrtpstats-dict*
class RTC_EXPORT RTCRemoteOutboundRtpStreamStats final
    : public RTCSentRtpStreamStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCRemoteOutboundRtpStreamStats(std::string id, Timestamp timestamp);
  RTCRemoteOutboundRtpStreamStats(const RTCRemoteOutboundRtpStreamStats& other);
  ~RTCRemoteOutboundRtpStreamStats() override;

  RTCStatsMember<std::string> local_id;
  RTCStatsMember<double> remote_timestamp;
  RTCStatsMember<uint64_t> reports_sent;
  RTCStatsMember<double> round_trip_time;
  RTCStatsMember<uint64_t> round_trip_time_measurements;
  RTCStatsMember<double> total_round_trip_time;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcmediasourcestats
class RTC_EXPORT RTCMediaSourceStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCMediaSourceStats(const RTCMediaSourceStats& other);
  ~RTCMediaSourceStats() override;

  RTCStatsMember<std::string> track_identifier;
  RTCStatsMember<std::string> kind;

 protected:
  RTCMediaSourceStats(std::string id, Timestamp timestamp);
};

// https://w3c.github.io/webrtc-stats/#dom-rtcaudiosourcestats
class RTC_EXPORT RTCAudioSourceStats final : public RTCMediaSourceStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCAudioSourceStats(std::string id, Timestamp timestamp);
  RTCAudioSourceStats(const RTCAudioSourceStats& other);
  ~RTCAudioSourceStats() override;

  RTCStatsMember<double> audio_level;
  RTCStatsMember<double> total_audio_energy;
  RTCStatsMember<double> total_samples_duration;
  RTCStatsMember<double> echo_return_loss;
  RTCStatsMember<double> echo_return_loss_enhancement;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcvideosourcestats
class RTC_EXPORT RTCVideoSourceStats final : public RTCMediaSourceStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCVideoSourceStats(std::string id, Timestamp timestamp);
  RTCVideoSourceStats(const RTCVideoSourceStats& other);
  ~RTCVideoSourceStats() override;

  RTCStatsMember<uint32_t> width;
  RTCStatsMember<uint32_t> height;
  RTCStatsMember<uint32_t> frames;
  RTCStatsMember<double> frames_per_second;
};

// https://w3c.github.io/webrtc-stats/#transportstats-dict*
class RTC_EXPORT RTCTransportStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCTransportStats(std::string id, Timestamp timestamp);
  RTCTransportStats(const RTCTransportStats& other);
  ~RTCTransportStats() override;

  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> packets_sent;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<uint64_t> packets_received;
  RTCStatsMember<std::string> rtcp_transport_stats_id;
  RTCStatsMember<std::string> dtls_state;
  RTCStatsMember<std::string> selected_candidate_pair_id;
  RTCStatsMember<std::string> local_certificate_id;
  RTCStatsMember<std::string> remote_certificate_id;
  RTCStatsMember<std::string> tls_version;
  RTCStatsMember<std::string> dtls_cipher;
  RTCStatsMember<std::string> dtls_role;
  RTCStatsMember<std::string> srtp_cipher;
  RTCStatsMember<uint32_t> selected_candidate_pair_changes;
  RTCStatsMember<std::string> ice_role;
  RTCStatsMember<std::string> ice_local_username_fragment;
  RTCStatsMember<std::string> ice_state;
};

// https://w3c.github.io/webrtc-stats/#playoutstats-dict*
class RTC_EXPORT RTCAudioPlayoutStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCAudioPlayoutStats(const std::string& id, Timestamp timestamp);
  RTCAudioPlayoutStats(const RTCAudioPlayoutStats& other);
  ~RTCAudioPlayoutStats() override;

  RTCStatsMember<std::string> kind;
  RTCStatsMember<double> synthesized_samples_duration;
  RTCStatsMember<uint64_t> synthesized_samples_events;
  RTCStatsMember<double> total_samples_duration;
  RTCStatsMember<double> total_playout_delay;
  RTCStatsMember<uint64_t> total_samples_count;
};

}  // namespace webrtc

#endif  // API_STATS_RTCSTATS_OBJECTS_H_
