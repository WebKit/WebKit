/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtcstats_objects.h"

#include <utility>

#include "api/stats/rtc_stats.h"
#include "rtc_base/checks.h"

namespace webrtc {

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCCertificateStats, RTCStats, "certificate",
    &fingerprint,
    &fingerprint_algorithm,
    &base64_certificate,
    &issuer_certificate_id)
// clang-format on

RTCCertificateStats::RTCCertificateStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      fingerprint("fingerprint"),
      fingerprint_algorithm("fingerprintAlgorithm"),
      base64_certificate("base64Certificate"),
      issuer_certificate_id("issuerCertificateId") {}

RTCCertificateStats::RTCCertificateStats(const RTCCertificateStats& other) =
    default;
RTCCertificateStats::~RTCCertificateStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCCodecStats, RTCStats, "codec",
    &transport_id,
    &payload_type,
    &mime_type,
    &clock_rate,
    &channels,
    &sdp_fmtp_line)
// clang-format on

RTCCodecStats::RTCCodecStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      transport_id("transportId"),
      payload_type("payloadType"),
      mime_type("mimeType"),
      clock_rate("clockRate"),
      channels("channels"),
      sdp_fmtp_line("sdpFmtpLine") {}

RTCCodecStats::RTCCodecStats(const RTCCodecStats& other) = default;

RTCCodecStats::~RTCCodecStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCDataChannelStats, RTCStats, "data-channel",
    &label,
    &protocol,
    &data_channel_identifier,
    &state,
    &messages_sent,
    &bytes_sent,
    &messages_received,
    &bytes_received)
// clang-format on

RTCDataChannelStats::RTCDataChannelStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      label("label"),
      protocol("protocol"),
      data_channel_identifier("dataChannelIdentifier"),
      state("state"),
      messages_sent("messagesSent"),
      bytes_sent("bytesSent"),
      messages_received("messagesReceived"),
      bytes_received("bytesReceived") {}

RTCDataChannelStats::RTCDataChannelStats(const RTCDataChannelStats& other) =
    default;

RTCDataChannelStats::~RTCDataChannelStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCIceCandidatePairStats, RTCStats, "candidate-pair",
    &transport_id,
    &local_candidate_id,
    &remote_candidate_id,
    &state,
    &priority,
    &nominated,
    &writable,
    &packets_sent,
    &packets_received,
    &bytes_sent,
    &bytes_received,
    &total_round_trip_time,
    &current_round_trip_time,
    &available_outgoing_bitrate,
    &available_incoming_bitrate,
    &requests_received,
    &requests_sent,
    &responses_received,
    &responses_sent,
    &consent_requests_sent,
    &packets_discarded_on_send,
    &bytes_discarded_on_send,
    &last_packet_received_timestamp,
    &last_packet_sent_timestamp)
// clang-format on

RTCIceCandidatePairStats::RTCIceCandidatePairStats(std::string id,
                                                   Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      transport_id("transportId"),
      local_candidate_id("localCandidateId"),
      remote_candidate_id("remoteCandidateId"),
      state("state"),
      priority("priority"),
      nominated("nominated"),
      writable("writable"),
      packets_sent("packetsSent"),
      packets_received("packetsReceived"),
      bytes_sent("bytesSent"),
      bytes_received("bytesReceived"),
      total_round_trip_time("totalRoundTripTime"),
      current_round_trip_time("currentRoundTripTime"),
      available_outgoing_bitrate("availableOutgoingBitrate"),
      available_incoming_bitrate("availableIncomingBitrate"),
      requests_received("requestsReceived"),
      requests_sent("requestsSent"),
      responses_received("responsesReceived"),
      responses_sent("responsesSent"),
      consent_requests_sent("consentRequestsSent"),
      packets_discarded_on_send("packetsDiscardedOnSend"),
      bytes_discarded_on_send("bytesDiscardedOnSend"),
      last_packet_received_timestamp("lastPacketReceivedTimestamp"),
      last_packet_sent_timestamp("lastPacketSentTimestamp") {}

RTCIceCandidatePairStats::RTCIceCandidatePairStats(
    const RTCIceCandidatePairStats& other) = default;

RTCIceCandidatePairStats::~RTCIceCandidatePairStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCIceCandidateStats, RTCStats, "abstract-ice-candidate",
    &transport_id,
    &is_remote,
    &network_type,
    &ip,
    &address,
    &port,
    &protocol,
    &relay_protocol,
    &candidate_type,
    &priority,
    &url,
    &foundation,
    &related_address,
    &related_port,
    &username_fragment,
    &tcp_type,
    &vpn,
    &network_adapter_type)
// clang-format on

RTCIceCandidateStats::RTCIceCandidateStats(std::string id,
                                           Timestamp timestamp,
                                           bool is_remote)
    : RTCStats(std::move(id), timestamp),
      transport_id("transportId"),
      is_remote("isRemote", is_remote),
      network_type("networkType"),
      ip("ip"),
      address("address"),
      port("port"),
      protocol("protocol"),
      relay_protocol("relayProtocol"),
      candidate_type("candidateType"),
      priority("priority"),
      url("url"),
      foundation("foundation"),
      related_address("relatedAddress"),
      related_port("relatedPort"),
      username_fragment("usernameFragment"),
      tcp_type("tcpType"),
      vpn("vpn"),
      network_adapter_type("networkAdapterType") {}

RTCIceCandidateStats::RTCIceCandidateStats(const RTCIceCandidateStats& other) =
    default;

RTCIceCandidateStats::~RTCIceCandidateStats() {}

const char RTCLocalIceCandidateStats::kType[] = "local-candidate";

RTCLocalIceCandidateStats::RTCLocalIceCandidateStats(std::string id,
                                                     Timestamp timestamp)
    : RTCIceCandidateStats(std::move(id), timestamp, false) {}

std::unique_ptr<RTCStats> RTCLocalIceCandidateStats::copy() const {
  return std::make_unique<RTCLocalIceCandidateStats>(*this);
}

const char* RTCLocalIceCandidateStats::type() const {
  return kType;
}

const char RTCRemoteIceCandidateStats::kType[] = "remote-candidate";

RTCRemoteIceCandidateStats::RTCRemoteIceCandidateStats(std::string id,
                                                       Timestamp timestamp)
    : RTCIceCandidateStats(std::move(id), timestamp, true) {}

std::unique_ptr<RTCStats> RTCRemoteIceCandidateStats::copy() const {
  return std::make_unique<RTCRemoteIceCandidateStats>(*this);
}

const char* RTCRemoteIceCandidateStats::type() const {
  return kType;
}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCPeerConnectionStats, RTCStats, "peer-connection",
    &data_channels_opened,
    &data_channels_closed)
// clang-format on

RTCPeerConnectionStats::RTCPeerConnectionStats(std::string id,
                                               Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      data_channels_opened("dataChannelsOpened"),
      data_channels_closed("dataChannelsClosed") {}

RTCPeerConnectionStats::RTCPeerConnectionStats(
    const RTCPeerConnectionStats& other) = default;

RTCPeerConnectionStats::~RTCPeerConnectionStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCRtpStreamStats, RTCStats, "rtp",
    &ssrc,
    &kind,
    &transport_id,
    &codec_id)
// clang-format on

RTCRtpStreamStats::RTCRtpStreamStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      ssrc("ssrc"),
      kind("kind"),
      transport_id("transportId"),
      codec_id("codecId") {}

RTCRtpStreamStats::RTCRtpStreamStats(const RTCRtpStreamStats& other) = default;

RTCRtpStreamStats::~RTCRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCReceivedRtpStreamStats, RTCRtpStreamStats, "received-rtp",
    &jitter,
    &packets_lost)
// clang-format on

RTCReceivedRtpStreamStats::RTCReceivedRtpStreamStats(std::string id,
                                                     Timestamp timestamp)
    : RTCRtpStreamStats(std::move(id), timestamp),
      jitter("jitter"),
      packets_lost("packetsLost") {}

RTCReceivedRtpStreamStats::RTCReceivedRtpStreamStats(
    const RTCReceivedRtpStreamStats& other) = default;

RTCReceivedRtpStreamStats::~RTCReceivedRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCSentRtpStreamStats, RTCRtpStreamStats, "sent-rtp",
    &packets_sent,
    &bytes_sent)
// clang-format on

RTCSentRtpStreamStats::RTCSentRtpStreamStats(std::string id,
                                             Timestamp timestamp)
    : RTCRtpStreamStats(std::move(id), timestamp),
      packets_sent("packetsSent"),
      bytes_sent("bytesSent") {}

RTCSentRtpStreamStats::RTCSentRtpStreamStats(
    const RTCSentRtpStreamStats& other) = default;

RTCSentRtpStreamStats::~RTCSentRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCInboundRtpStreamStats, RTCReceivedRtpStreamStats, "inbound-rtp",
    &track_identifier,
    &mid,
    &remote_id,
    &packets_received,
    &packets_discarded,
    &fec_packets_received,
    &fec_bytes_received,
    &fec_packets_discarded,
    &fec_ssrc,
    &bytes_received,
    &header_bytes_received,
    &retransmitted_packets_received,
    &retransmitted_bytes_received,
    &rtx_ssrc,
    &last_packet_received_timestamp,
    &jitter_buffer_delay,
    &jitter_buffer_target_delay,
    &jitter_buffer_minimum_delay,
    &jitter_buffer_emitted_count,
    &total_samples_received,
    &concealed_samples,
    &silent_concealed_samples,
    &concealment_events,
    &inserted_samples_for_deceleration,
    &removed_samples_for_acceleration,
    &audio_level,
    &total_audio_energy,
    &total_samples_duration,
    &playout_id,
    &frames_received,
    &frame_width,
    &frame_height,
    &frames_per_second,
    &frames_decoded,
    &key_frames_decoded,
    &frames_dropped,
    &total_decode_time,
    &total_processing_delay,
    &total_assembly_time,
    &frames_assembled_from_multiple_packets,
    &total_inter_frame_delay,
    &total_squared_inter_frame_delay,
    &pause_count,
    &total_pauses_duration,
    &freeze_count,
    &total_freezes_duration,
    &content_type,
    &estimated_playout_timestamp,
    &decoder_implementation,
    &fir_count,
    &pli_count,
    &nack_count,
    &qp_sum,
    &goog_timing_frame_info,
    &power_efficient_decoder,
    &jitter_buffer_flushes,
    &delayed_packet_outage_samples,
    &relative_packet_arrival_delay,
    &interruption_count,
    &total_interruption_duration,
    &min_playout_delay)
// clang-format on

RTCInboundRtpStreamStats::RTCInboundRtpStreamStats(std::string id,
                                                   Timestamp timestamp)
    : RTCReceivedRtpStreamStats(std::move(id), timestamp),
      playout_id("playoutId"),
      track_identifier("trackIdentifier"),
      mid("mid"),
      remote_id("remoteId"),
      packets_received("packetsReceived"),
      packets_discarded("packetsDiscarded"),
      fec_packets_received("fecPacketsReceived"),
      fec_bytes_received("fecBytesReceived"),
      fec_packets_discarded("fecPacketsDiscarded"),
      fec_ssrc("fecSsrc"),
      bytes_received("bytesReceived"),
      header_bytes_received("headerBytesReceived"),
      retransmitted_packets_received("retransmittedPacketsReceived"),
      retransmitted_bytes_received("retransmittedBytesReceived"),
      rtx_ssrc("rtxSsrc"),
      last_packet_received_timestamp("lastPacketReceivedTimestamp"),
      jitter_buffer_delay("jitterBufferDelay"),
      jitter_buffer_target_delay("jitterBufferTargetDelay"),
      jitter_buffer_minimum_delay("jitterBufferMinimumDelay"),
      jitter_buffer_emitted_count("jitterBufferEmittedCount"),
      total_samples_received("totalSamplesReceived"),
      concealed_samples("concealedSamples"),
      silent_concealed_samples("silentConcealedSamples"),
      concealment_events("concealmentEvents"),
      inserted_samples_for_deceleration("insertedSamplesForDeceleration"),
      removed_samples_for_acceleration("removedSamplesForAcceleration"),
      audio_level("audioLevel"),
      total_audio_energy("totalAudioEnergy"),
      total_samples_duration("totalSamplesDuration"),
      frames_received("framesReceived"),
      frame_width("frameWidth"),
      frame_height("frameHeight"),
      frames_per_second("framesPerSecond"),
      frames_decoded("framesDecoded"),
      key_frames_decoded("keyFramesDecoded"),
      frames_dropped("framesDropped"),
      total_decode_time("totalDecodeTime"),
      total_processing_delay("totalProcessingDelay"),
      total_assembly_time("totalAssemblyTime"),
      frames_assembled_from_multiple_packets(
          "framesAssembledFromMultiplePackets"),
      total_inter_frame_delay("totalInterFrameDelay"),
      total_squared_inter_frame_delay("totalSquaredInterFrameDelay"),
      pause_count("pauseCount"),
      total_pauses_duration("totalPausesDuration"),
      freeze_count("freezeCount"),
      total_freezes_duration("totalFreezesDuration"),
      content_type("contentType"),
      estimated_playout_timestamp("estimatedPlayoutTimestamp"),
      decoder_implementation("decoderImplementation"),
      fir_count("firCount"),
      pli_count("pliCount"),
      nack_count("nackCount"),
      qp_sum("qpSum"),
      goog_timing_frame_info("googTimingFrameInfo"),
      power_efficient_decoder("powerEfficientDecoder"),
      jitter_buffer_flushes("jitterBufferFlushes"),
      delayed_packet_outage_samples("delayedPacketOutageSamples"),
      relative_packet_arrival_delay("relativePacketArrivalDelay"),
      interruption_count("interruptionCount"),
      total_interruption_duration("totalInterruptionDuration"),
      min_playout_delay("minPlayoutDelay") {}

RTCInboundRtpStreamStats::RTCInboundRtpStreamStats(
    const RTCInboundRtpStreamStats& other) = default;
RTCInboundRtpStreamStats::~RTCInboundRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCOutboundRtpStreamStats, RTCSentRtpStreamStats, "outbound-rtp",
    &media_source_id,
    &remote_id,
    &mid,
    &rid,
    &retransmitted_packets_sent,
    &header_bytes_sent,
    &retransmitted_bytes_sent,
    &target_bitrate,
    &frames_encoded,
    &key_frames_encoded,
    &total_encode_time,
    &total_encoded_bytes_target,
    &frame_width,
    &frame_height,
    &frames_per_second,
    &frames_sent,
    &huge_frames_sent,
    &total_packet_send_delay,
    &quality_limitation_reason,
    &quality_limitation_durations,
    &quality_limitation_resolution_changes,
    &content_type,
    &encoder_implementation,
    &fir_count,
    &pli_count,
    &nack_count,
    &qp_sum,
    &active,
    &power_efficient_encoder,
    &scalability_mode,
    &rtx_ssrc)
// clang-format on

RTCOutboundRtpStreamStats::RTCOutboundRtpStreamStats(std::string id,
                                                     Timestamp timestamp)
    : RTCSentRtpStreamStats(std::move(id), timestamp),
      media_source_id("mediaSourceId"),
      remote_id("remoteId"),
      mid("mid"),
      rid("rid"),
      retransmitted_packets_sent("retransmittedPacketsSent"),
      header_bytes_sent("headerBytesSent"),
      retransmitted_bytes_sent("retransmittedBytesSent"),
      target_bitrate("targetBitrate"),
      frames_encoded("framesEncoded"),
      key_frames_encoded("keyFramesEncoded"),
      total_encode_time("totalEncodeTime"),
      total_encoded_bytes_target("totalEncodedBytesTarget"),
      frame_width("frameWidth"),
      frame_height("frameHeight"),
      frames_per_second("framesPerSecond"),
      frames_sent("framesSent"),
      huge_frames_sent("hugeFramesSent"),
      total_packet_send_delay("totalPacketSendDelay"),
      quality_limitation_reason("qualityLimitationReason"),
      quality_limitation_durations("qualityLimitationDurations"),
      quality_limitation_resolution_changes(
          "qualityLimitationResolutionChanges"),
      content_type("contentType"),
      encoder_implementation("encoderImplementation"),
      fir_count("firCount"),
      pli_count("pliCount"),
      nack_count("nackCount"),
      qp_sum("qpSum"),
      active("active"),
      power_efficient_encoder("powerEfficientEncoder"),
      scalability_mode("scalabilityMode"),
      rtx_ssrc("rtxSsrc") {}

RTCOutboundRtpStreamStats::RTCOutboundRtpStreamStats(
    const RTCOutboundRtpStreamStats& other) = default;

RTCOutboundRtpStreamStats::~RTCOutboundRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCRemoteInboundRtpStreamStats, RTCReceivedRtpStreamStats,
        "remote-inbound-rtp",
    &local_id,
    &round_trip_time,
    &fraction_lost,
    &total_round_trip_time,
    &round_trip_time_measurements)
// clang-format on

RTCRemoteInboundRtpStreamStats::RTCRemoteInboundRtpStreamStats(
    std::string id,
    Timestamp timestamp)
    : RTCReceivedRtpStreamStats(std::move(id), timestamp),
      local_id("localId"),
      round_trip_time("roundTripTime"),
      fraction_lost("fractionLost"),
      total_round_trip_time("totalRoundTripTime"),
      round_trip_time_measurements("roundTripTimeMeasurements") {}

RTCRemoteInboundRtpStreamStats::RTCRemoteInboundRtpStreamStats(
    const RTCRemoteInboundRtpStreamStats& other) = default;

RTCRemoteInboundRtpStreamStats::~RTCRemoteInboundRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(
    RTCRemoteOutboundRtpStreamStats, RTCSentRtpStreamStats,
    "remote-outbound-rtp",
    &local_id,
    &remote_timestamp,
    &reports_sent,
    &round_trip_time,
    &round_trip_time_measurements,
    &total_round_trip_time)
// clang-format on

RTCRemoteOutboundRtpStreamStats::RTCRemoteOutboundRtpStreamStats(
    std::string id,
    Timestamp timestamp)
    : RTCSentRtpStreamStats(std::move(id), timestamp),
      local_id("localId"),
      remote_timestamp("remoteTimestamp"),
      reports_sent("reportsSent"),
      round_trip_time("roundTripTime"),
      round_trip_time_measurements("roundTripTimeMeasurements"),
      total_round_trip_time("totalRoundTripTime") {}

RTCRemoteOutboundRtpStreamStats::RTCRemoteOutboundRtpStreamStats(
    const RTCRemoteOutboundRtpStreamStats& other) = default;

RTCRemoteOutboundRtpStreamStats::~RTCRemoteOutboundRtpStreamStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCMediaSourceStats, RTCStats, "parent-media-source",
    &track_identifier,
    &kind)
// clang-format on

RTCMediaSourceStats::RTCMediaSourceStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      track_identifier("trackIdentifier"),
      kind("kind") {}

RTCMediaSourceStats::RTCMediaSourceStats(const RTCMediaSourceStats& other) =
    default;

RTCMediaSourceStats::~RTCMediaSourceStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCAudioSourceStats, RTCMediaSourceStats, "media-source",
    &audio_level,
    &total_audio_energy,
    &total_samples_duration,
    &echo_return_loss,
    &echo_return_loss_enhancement)
// clang-format on

RTCAudioSourceStats::RTCAudioSourceStats(std::string id, Timestamp timestamp)
    : RTCMediaSourceStats(std::move(id), timestamp),
      audio_level("audioLevel"),
      total_audio_energy("totalAudioEnergy"),
      total_samples_duration("totalSamplesDuration"),
      echo_return_loss("echoReturnLoss"),
      echo_return_loss_enhancement("echoReturnLossEnhancement") {}

RTCAudioSourceStats::RTCAudioSourceStats(const RTCAudioSourceStats& other) =
    default;

RTCAudioSourceStats::~RTCAudioSourceStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCVideoSourceStats, RTCMediaSourceStats, "media-source",
    &width,
    &height,
    &frames,
    &frames_per_second)
// clang-format on

RTCVideoSourceStats::RTCVideoSourceStats(std::string id, Timestamp timestamp)
    : RTCMediaSourceStats(std::move(id), timestamp),
      width("width"),
      height("height"),
      frames("frames"),
      frames_per_second("framesPerSecond") {}

RTCVideoSourceStats::RTCVideoSourceStats(const RTCVideoSourceStats& other) =
    default;

RTCVideoSourceStats::~RTCVideoSourceStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCTransportStats, RTCStats, "transport",
    &bytes_sent,
    &packets_sent,
    &bytes_received,
    &packets_received,
    &rtcp_transport_stats_id,
    &dtls_state,
    &selected_candidate_pair_id,
    &local_certificate_id,
    &remote_certificate_id,
    &tls_version,
    &dtls_cipher,
    &dtls_role,
    &srtp_cipher,
    &selected_candidate_pair_changes,
    &ice_role,
    &ice_local_username_fragment,
    &ice_state)
// clang-format on

RTCTransportStats::RTCTransportStats(std::string id, Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      bytes_sent("bytesSent"),
      packets_sent("packetsSent"),
      bytes_received("bytesReceived"),
      packets_received("packetsReceived"),
      rtcp_transport_stats_id("rtcpTransportStatsId"),
      dtls_state("dtlsState"),
      selected_candidate_pair_id("selectedCandidatePairId"),
      local_certificate_id("localCertificateId"),
      remote_certificate_id("remoteCertificateId"),
      tls_version("tlsVersion"),
      dtls_cipher("dtlsCipher"),
      dtls_role("dtlsRole"),
      srtp_cipher("srtpCipher"),
      selected_candidate_pair_changes("selectedCandidatePairChanges"),
      ice_role("iceRole"),
      ice_local_username_fragment("iceLocalUsernameFragment"),
      ice_state("iceState") {}

RTCTransportStats::RTCTransportStats(const RTCTransportStats& other) = default;

RTCTransportStats::~RTCTransportStats() {}

RTCAudioPlayoutStats::RTCAudioPlayoutStats(const std::string& id,
                                           Timestamp timestamp)
    : RTCStats(std::move(id), timestamp),
      kind("kind", "audio"),
      synthesized_samples_duration("synthesizedSamplesDuration"),
      synthesized_samples_events("synthesizedSamplesEvents"),
      total_samples_duration("totalSamplesDuration"),
      total_playout_delay("totalPlayoutDelay"),
      total_samples_count("totalSamplesCount") {}

RTCAudioPlayoutStats::RTCAudioPlayoutStats(const RTCAudioPlayoutStats& other) =
    default;

RTCAudioPlayoutStats::~RTCAudioPlayoutStats() {}

// clang-format off
WEBRTC_RTCSTATS_IMPL(RTCAudioPlayoutStats, RTCStats, "media-playout",
    &kind,
    &synthesized_samples_duration,
    &synthesized_samples_events,
    &total_samples_duration,
    &total_playout_delay,
    &total_samples_count)
// clang-format on

}  // namespace webrtc
