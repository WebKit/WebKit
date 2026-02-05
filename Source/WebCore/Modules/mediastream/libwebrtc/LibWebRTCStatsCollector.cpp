/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCStatsCollector.h"

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "ContextDestructionObserverInlines.h"
#include "JSDOMMapLike.h"
#include "JSRTCIceTcpCandidateType.h"
#include "JSRTCStatsReport.h"
#include "LibWebRTCUtils.h"
#include "Performance.h"
#include "ScriptExecutionContextInlines.h"
#include <webrtc/api/stats/rtcstats_objects.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

namespace WebCore {

LibWebRTCStatsCollector::LibWebRTCStatsCollector(CollectorCallback&& callback)
    : m_callback(WTF::move(callback))
{
}

LibWebRTCStatsCollector::~LibWebRTCStatsCollector()
{
    if (!m_callback)
        return;

    callOnMainThread([callback = WTF::move(m_callback)]() mutable {
        callback(nullptr);
    });
}

RTCStatsReport::Stats RTCStatsReport::Stats::convert(Type type, const webrtc::RTCStats& rtcStats)
{
    return Stats {
        Performance::reduceTimeResolution(Seconds::fromMicroseconds(rtcStats.timestamp().us_or(0))).milliseconds(),
        type,
        fromStdString(rtcStats.id()),
    };
}

RTCStatsReport::RtpStreamStats RTCStatsReport::RtpStreamStats::convert(Type type, const webrtc::RTCRtpStreamStats& rtcStats)
{
    return RtpStreamStats {
        Stats::convert(type, rtcStats),
        rtcStats.ssrc ? *rtcStats.ssrc : 0,
        rtcStats.kind ? fromStdString(*rtcStats.kind) : String(),
        rtcStats.transport_id ? fromStdString(*rtcStats.transport_id) : String(),
        rtcStats.codec_id ? fromStdString(*rtcStats.codec_id) : String(),
    };
}

RTCStatsReport::ReceivedRtpStreamStats RTCStatsReport::ReceivedRtpStreamStats::convert(Type type, const webrtc::RTCReceivedRtpStreamStats& rtcStats, std::optional<uint64_t> packetsReceived)
{
    return ReceivedRtpStreamStats {
        RtpStreamStats::convert(type, rtcStats),
        // packetsReceived should be in the base class, but somehow isn't, it's only define for RTCInboundRtpStreamStats
        packetsReceived,
        rtcStats.packets_lost ? std::optional { *rtcStats.packets_lost } : std::nullopt,
        rtcStats.jitter ? std::optional { *rtcStats.jitter } : std::nullopt,
    };
}

RTCStatsReport::InboundRtpStreamStats RTCStatsReport::InboundRtpStreamStats::convert(const webrtc::RTCInboundRtpStreamStats& rtcStats)
{
    return InboundRtpStreamStats {
        ReceivedRtpStreamStats::convert(RTCStatsReport::Type::InboundRtp, rtcStats, rtcStats.packets_received ? std::optional { *rtcStats.packets_received } : std::nullopt),
        rtcStats.track_identifier ? fromStdString(*rtcStats.track_identifier) : String(),
        rtcStats.mid ? fromStdString(*rtcStats.mid) : String(),
        rtcStats.remote_id ? fromStdString(*rtcStats.remote_id) : String(),
        rtcStats.frames_decoded ? std::optional { *rtcStats.frames_decoded } : std::nullopt,
        rtcStats.key_frames_decoded ? std::optional { *rtcStats.key_frames_decoded } : std::nullopt,
        { }, // FIXME: Support `framesRendered`
        rtcStats.frames_dropped ? std::optional { *rtcStats.frames_dropped } : std::nullopt,
        rtcStats.frame_width ? std::optional { *rtcStats.frame_width } : std::nullopt,
        rtcStats.frame_height ? std::optional { *rtcStats.frame_height } : std::nullopt,
        rtcStats.frames_per_second ? std::optional { *rtcStats.frames_per_second } : std::nullopt,
        rtcStats.qp_sum ? std::optional { *rtcStats.qp_sum } : std::nullopt,
        rtcStats.total_decode_time ? std::optional { *rtcStats.total_decode_time } : std::nullopt,
        rtcStats.total_inter_frame_delay ? std::optional { *rtcStats.total_inter_frame_delay } : std::nullopt,
        rtcStats.total_squared_inter_frame_delay ? std::optional { *rtcStats.total_squared_inter_frame_delay } : std::nullopt,
        rtcStats.pause_count ? std::optional { *rtcStats.pause_count } : std::nullopt,
        rtcStats.total_pauses_duration ? std::optional { *rtcStats.total_pauses_duration } : std::nullopt,
        rtcStats.freeze_count ? std::optional { *rtcStats.freeze_count } : std::nullopt,
        rtcStats.total_freezes_duration ? std::optional { *rtcStats.total_freezes_duration } : std::nullopt,
        rtcStats.last_packet_received_timestamp ? std::optional { *rtcStats.last_packet_received_timestamp } : std::nullopt,
        rtcStats.header_bytes_received ? std::optional { *rtcStats.header_bytes_received } : std::nullopt,
        rtcStats.packets_discarded ? std::optional { *rtcStats.packets_discarded } : std::nullopt,
        rtcStats.fec_bytes_received ? std::optional { *rtcStats.fec_bytes_received } : std::nullopt,
        rtcStats.fec_packets_received ? std::optional { *rtcStats.fec_packets_received } : std::nullopt,
        rtcStats.fec_packets_discarded ? std::optional { *rtcStats.fec_packets_discarded } : std::nullopt,
        rtcStats.bytes_received ? std::optional { *rtcStats.bytes_received } : std::nullopt,
        rtcStats.nack_count ? std::optional { *rtcStats.nack_count } : std::nullopt,
        rtcStats.fir_count ? std::optional { *rtcStats.fir_count } : std::nullopt,
        rtcStats.pli_count ? std::optional { *rtcStats.pli_count } : std::nullopt,
        rtcStats.total_processing_delay ? std::optional { *rtcStats.total_processing_delay } : std::nullopt,
        rtcStats.estimated_playout_timestamp ? std::optional { *rtcStats.estimated_playout_timestamp } : std::nullopt,
        rtcStats.jitter_buffer_delay ? std::optional { *rtcStats.jitter_buffer_delay } : std::nullopt,
        rtcStats.jitter_buffer_target_delay ? std::optional { *rtcStats.jitter_buffer_target_delay } : std::nullopt,
        rtcStats.jitter_buffer_emitted_count ? std::optional { *rtcStats.jitter_buffer_emitted_count } : std::nullopt,
        rtcStats.jitter_buffer_minimum_delay ? std::optional { *rtcStats.jitter_buffer_minimum_delay } : std::nullopt,
        rtcStats.total_samples_received ? std::optional { *rtcStats.total_samples_received } : std::nullopt,
        rtcStats.concealed_samples ? std::optional { *rtcStats.concealed_samples } : std::nullopt,
        rtcStats.silent_concealed_samples ? std::optional { *rtcStats.silent_concealed_samples } : std::nullopt,
        rtcStats.concealment_events ? std::optional { *rtcStats.concealment_events } : std::nullopt,
        rtcStats.inserted_samples_for_deceleration ? std::optional { *rtcStats.inserted_samples_for_deceleration } : std::nullopt,
        rtcStats.removed_samples_for_acceleration ? std::optional { *rtcStats.removed_samples_for_acceleration } : std::nullopt,
        rtcStats.audio_level ? std::optional { *rtcStats.audio_level } : std::nullopt,
        rtcStats.total_audio_energy ? std::optional { *rtcStats.total_audio_energy } : std::nullopt,
        rtcStats.total_samples_duration ? std::optional { *rtcStats.total_samples_duration } : std::nullopt,
        rtcStats.frames_received ? std::optional { *rtcStats.frames_received } : std::nullopt,
        // TODO: Restrict Access
        { }, // rtcStats.decoder_implementation ? fromStdString(*rtcStats.decoder_implementation) : String(),
        rtcStats.playout_id ? fromStdString(*rtcStats.playout_id) : String(),
        // TODO: Restrict Access
        { }, // rtcStats.power_efficient_decoder ? std::optional { *rtcStats.power_efficient_decoder } : std::nullopt,
        rtcStats.frames_assembled_from_multiple_packets ? std::optional { *rtcStats.frames_assembled_from_multiple_packets } : std::nullopt,
        rtcStats.total_assembly_time ? std::optional { *rtcStats.total_assembly_time } : std::nullopt,
        rtcStats.retransmitted_packets_received ? std::optional { *rtcStats.retransmitted_packets_received } : std::nullopt,
        rtcStats.retransmitted_bytes_received ? std::optional { *rtcStats.retransmitted_bytes_received } : std::nullopt,
        rtcStats.rtx_ssrc ? std::optional { *rtcStats.rtx_ssrc } : std::nullopt,
        rtcStats.fec_ssrc ? std::optional { *rtcStats.fec_ssrc } : std::nullopt,
    };
}

RTCStatsReport::RemoteInboundRtpStreamStats RTCStatsReport::RemoteInboundRtpStreamStats::convert(const webrtc::RTCRemoteInboundRtpStreamStats& rtcStats)
{
    return RemoteInboundRtpStreamStats {
        ReceivedRtpStreamStats::convert(RTCStatsReport::Type::RemoteInboundRtp, rtcStats, std::nullopt),
        rtcStats.local_id ? fromStdString(*rtcStats.local_id) : String(),
        rtcStats.round_trip_time ? std::optional { *rtcStats.round_trip_time } : std::nullopt,
        rtcStats.total_round_trip_time ? std::optional { *rtcStats.total_round_trip_time } : std::nullopt,
        rtcStats.fraction_lost ? std::optional { *rtcStats.fraction_lost } : std::nullopt,
        rtcStats.round_trip_time_measurements ? std::optional { *rtcStats.round_trip_time_measurements } : std::nullopt,
    };
}

RTCStatsReport::SentRtpStreamStats RTCStatsReport::SentRtpStreamStats::convert(Type type, const webrtc::RTCSentRtpStreamStats& rtcStats)
{
    return SentRtpStreamStats {
        RtpStreamStats::convert(type, rtcStats),
        rtcStats.packets_sent ? std::optional { *rtcStats.packets_sent } : std::nullopt,
        rtcStats.bytes_sent ? std::optional { *rtcStats.bytes_sent } : std::nullopt,
    };
}

static inline std::optional<RTCStatsReport::QualityLimitationReason> convertQualityLimitationReason(const std::string& reason)
{
    if (reason == "none")
        return RTCStatsReport::QualityLimitationReason::None;
    if (reason == "cpu")
        return RTCStatsReport::QualityLimitationReason::Cpu;
    if (reason == "bandwidth")
        return RTCStatsReport::QualityLimitationReason::Bandwidth;
    return RTCStatsReport::QualityLimitationReason::Other;
}

static inline std::optional<Vector<KeyValuePair<String, double>>> convertQualityLimitationDurations(const std::map<std::string, double>& durations)
{
    auto it = durations.begin();
    return Vector<KeyValuePair<String, double>>(durations.size(), [&](size_t) {
        ASSERT(it != durations.end());
        auto element = KeyValuePair<String, double> { fromStdString(it->first), it->second };
        ++it;
        return element;
    });
}

RTCStatsReport::OutboundRtpStreamStats RTCStatsReport::OutboundRtpStreamStats::convert(const webrtc::RTCOutboundRtpStreamStats& rtcStats)
{
    return OutboundRtpStreamStats {
        SentRtpStreamStats::convert(RTCStatsReport::Type::OutboundRtp, rtcStats),
        rtcStats.mid ? fromStdString(*rtcStats.mid) : String(),
        rtcStats.media_source_id ? fromStdString(*rtcStats.media_source_id) : String(),
        rtcStats.remote_id ? fromStdString(*rtcStats.remote_id) : String(),
        rtcStats.rid ? fromStdString(*rtcStats.rid) : String(),
        rtcStats.header_bytes_sent ? std::optional { *rtcStats.header_bytes_sent } : std::nullopt,
        rtcStats.retransmitted_packets_sent ? std::optional { *rtcStats.retransmitted_packets_sent } : std::nullopt,
        rtcStats.retransmitted_bytes_sent ? std::optional { *rtcStats.retransmitted_bytes_sent } : std::nullopt,
        rtcStats.rtx_ssrc ? std::optional { *rtcStats.rtx_ssrc } : std::nullopt,
        rtcStats.target_bitrate ? std::optional { *rtcStats.target_bitrate } : std::nullopt,
        rtcStats.total_encoded_bytes_target ? std::optional { *rtcStats.total_encoded_bytes_target } : std::nullopt,
        rtcStats.frame_width ? std::optional { *rtcStats.frame_width } : std::nullopt,
        rtcStats.frame_height ? std::optional { *rtcStats.frame_height } : std::nullopt,
        rtcStats.frames_per_second ? std::optional { *rtcStats.frames_per_second } : std::nullopt,
        rtcStats.frames_sent ? std::optional { *rtcStats.frames_sent } : std::nullopt,
        rtcStats.huge_frames_sent ? std::optional { *rtcStats.huge_frames_sent } : std::nullopt,
        rtcStats.frames_encoded ? std::optional { *rtcStats.frames_encoded } : std::nullopt,
        rtcStats.key_frames_encoded ? std::optional { *rtcStats.key_frames_encoded } : std::nullopt,
        rtcStats.qp_sum ? std::optional { *rtcStats.qp_sum } : std::nullopt,
        rtcStats.total_encode_time ? std::optional { *rtcStats.total_encode_time } : std::nullopt,
        rtcStats.total_packet_send_delay ? std::optional { *rtcStats.total_packet_send_delay } : std::nullopt,
        rtcStats.quality_limitation_reason ? convertQualityLimitationReason(*rtcStats.quality_limitation_reason) : std::nullopt,
        rtcStats.quality_limitation_durations ? convertQualityLimitationDurations(*rtcStats.quality_limitation_durations) : std::nullopt,
        rtcStats.quality_limitation_resolution_changes ? std::optional { *rtcStats.quality_limitation_resolution_changes } : std::nullopt,
        rtcStats.nack_count ? std::optional { *rtcStats.nack_count } : std::nullopt,
        rtcStats.fir_count ? std::optional { *rtcStats.fir_count } : std::nullopt,
        rtcStats.pli_count ? std::optional { *rtcStats.pli_count } : std::nullopt,
        rtcStats.active ? std::optional { *rtcStats.active } : std::nullopt,
        rtcStats.scalability_mode ? fromStdString(*rtcStats.scalability_mode) : String(),
    };
}

RTCStatsReport::RemoteOutboundRtpStreamStats RTCStatsReport::RemoteOutboundRtpStreamStats::convert(const webrtc::RTCRemoteOutboundRtpStreamStats& rtcStats)
{
    return RemoteOutboundRtpStreamStats {
        SentRtpStreamStats::convert(RTCStatsReport::Type::RemoteOutboundRtp, rtcStats),
        rtcStats.local_id ? fromStdString(*rtcStats.local_id) : String(),
        rtcStats.remote_timestamp ? std::optional { *rtcStats.remote_timestamp } : std::nullopt,
        rtcStats.reports_sent ? std::optional { *rtcStats.reports_sent } : std::nullopt,
        rtcStats.round_trip_time ? std::optional { *rtcStats.round_trip_time } : std::nullopt,
        rtcStats.total_round_trip_time ? std::optional { *rtcStats.total_round_trip_time } : std::nullopt,
        rtcStats.round_trip_time_measurements ? std::optional { *rtcStats.round_trip_time_measurements } : std::nullopt,
    };
}

RTCStatsReport::DataChannelStats RTCStatsReport::DataChannelStats::convert(const webrtc::RTCDataChannelStats& rtcStats)
{
    return DataChannelStats {
        Stats::convert(RTCStatsReport::Type::DataChannel, rtcStats),
        rtcStats.label ? fromStdString(*rtcStats.label) : String(),
        rtcStats.protocol ? fromStdString(*rtcStats.protocol) : String(),
        rtcStats.data_channel_identifier ? std::optional { *rtcStats.data_channel_identifier } : std::nullopt,
        rtcStats.state ? fromStdString(*rtcStats.state) : String(),
        rtcStats.messages_sent ? std::optional { *rtcStats.messages_sent } : std::nullopt,
        rtcStats.bytes_sent ? std::optional { *rtcStats.bytes_sent } : std::nullopt,
        rtcStats.messages_received ? std::optional { *rtcStats.messages_received } : std::nullopt,
        rtcStats.bytes_received ? std::optional { *rtcStats.bytes_received } : std::nullopt,
    };
}

static inline RTCStatsReport::IceCandidatePairState iceCandidatePairState(const std::string& state)
{
    if (state == "frozen")
        return RTCStatsReport::IceCandidatePairState::Frozen;
    if (state == "waiting")
        return RTCStatsReport::IceCandidatePairState::Waiting;
    if (state == "in-progress")
        return RTCStatsReport::IceCandidatePairState::InProgress;
    if (state == "failed")
        return RTCStatsReport::IceCandidatePairState::Failed;
    if (state == "succeeded")
        return RTCStatsReport::IceCandidatePairState::Succeeded;
    ASSERT_NOT_REACHED();
    return RTCStatsReport::IceCandidatePairState::Frozen;
}

RTCStatsReport::IceCandidatePairStats RTCStatsReport::IceCandidatePairStats::convert(const webrtc::RTCIceCandidatePairStats& rtcStats)
{
    return IceCandidatePairStats {
        Stats::convert(RTCStatsReport::Type::CandidatePair, rtcStats),
        rtcStats.transport_id ? fromStdString(*rtcStats.transport_id) : String(),
        rtcStats.local_candidate_id ? fromStdString(*rtcStats.local_candidate_id) : String(),
        rtcStats.remote_candidate_id ? fromStdString(*rtcStats.remote_candidate_id) : String(),
        rtcStats.state ? iceCandidatePairState(*rtcStats.state) : RTCStatsReport::IceCandidatePairState::Frozen,
        rtcStats.nominated ? std::optional { *rtcStats.nominated } : std::nullopt,
        rtcStats.packets_sent ? std::optional { *rtcStats.packets_sent } : std::nullopt,
        rtcStats.packets_received ? std::optional { *rtcStats.packets_received } : std::nullopt,
        rtcStats.bytes_sent ? std::optional { *rtcStats.bytes_sent } : std::nullopt,
        rtcStats.bytes_received ? std::optional { *rtcStats.bytes_received } : std::nullopt,
        rtcStats.last_packet_sent_timestamp ? std::optional { *rtcStats.last_packet_sent_timestamp } : std::nullopt,
        rtcStats.last_packet_received_timestamp ? std::optional { *rtcStats.last_packet_received_timestamp } : std::nullopt,
        rtcStats.total_round_trip_time ? std::optional { *rtcStats.total_round_trip_time } : std::nullopt,
        rtcStats.current_round_trip_time ? std::optional { *rtcStats.current_round_trip_time } : std::nullopt,
        rtcStats.available_outgoing_bitrate ? std::optional { *rtcStats.available_outgoing_bitrate } : std::nullopt,
        rtcStats.available_incoming_bitrate ? std::optional { *rtcStats.available_incoming_bitrate } : std::nullopt,
        rtcStats.requests_received ? std::optional { *rtcStats.requests_received } : std::nullopt,
        rtcStats.requests_sent ? std::optional { *rtcStats.requests_sent } : std::nullopt,
        rtcStats.responses_received ? std::optional { *rtcStats.responses_received } : std::nullopt,
        rtcStats.responses_sent ? std::optional { *rtcStats.responses_sent } : std::nullopt,
        rtcStats.consent_requests_sent ? std::optional { *rtcStats.consent_requests_sent } : std::nullopt,
        rtcStats.packets_discarded_on_send ? std::optional { *rtcStats.packets_discarded_on_send } : std::nullopt,
        rtcStats.bytes_discarded_on_send ? std::optional { *rtcStats.bytes_discarded_on_send } : std::nullopt,
    };
}

static inline RTCIceCandidateType iceCandidateState(const std::string& state)
{
    if (state == "host")
        return RTCIceCandidateType::Host;
    if (state == "srflx")
        return RTCIceCandidateType::Srflx;
    if (state == "prflx")
        return RTCIceCandidateType::Prflx;
    if (state == "relay")
        return RTCIceCandidateType::Relay;

    ASSERT_NOT_REACHED();
    return RTCIceCandidateType::Host;
}

RTCStatsReport::IceCandidateStats RTCStatsReport::IceCandidateStats::convert(const webrtc::RTCIceCandidateStats& rtcStats)
{
    auto result = IceCandidateStats {
        Stats::convert(rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType ? RTCStatsReport::Type::RemoteCandidate : RTCStatsReport::Type::LocalCandidate, rtcStats),
        rtcStats.transport_id ? fromStdString(*rtcStats.transport_id) : String(),
        rtcStats.ip ? std::optional { fromStdString(*rtcStats.ip) } : std::nullopt,
        rtcStats.port ? std::optional { *rtcStats.port } : std::nullopt,
        rtcStats.protocol ? fromStdString(*rtcStats.protocol) : String(),
        rtcStats.candidate_type ? iceCandidateState(*rtcStats.candidate_type) : RTCIceCandidateType::Host,
        rtcStats.priority ? std::optional { *rtcStats.priority } : std::nullopt,
        rtcStats.url ? fromStdString(*rtcStats.url) : String(),
        { }, // FIXME: Support `relayProtocol`
        rtcStats.foundation ? fromStdString(*rtcStats.foundation) : String(),
        { }, // FIXME: Support `relatedAddress`,
        { }, // FIXME: Support `relatedPort`,
        rtcStats.username_fragment ? fromStdString(*rtcStats.username_fragment) : String(),
        rtcStats.tcp_type ? parseEnumerationFromString<RTCIceTcpCandidateType>(fromStdString(*rtcStats.tcp_type)) : std::nullopt,
    };

    if (result.candidateType == RTCIceCandidateType::Prflx || result.candidateType == RTCIceCandidateType::Host)
        result.address = { };

    return result;
}

RTCStatsReport::CertificateStats RTCStatsReport::CertificateStats::convert(const webrtc::RTCCertificateStats& rtcStats)
{
    return CertificateStats {
        Stats::convert(RTCStatsReport::Type::Certificate, rtcStats),
        rtcStats.fingerprint ? fromStdString(*rtcStats.fingerprint) : String(),
        rtcStats.fingerprint_algorithm ? fromStdString(*rtcStats.fingerprint_algorithm) : String(),
        rtcStats.base64_certificate ? fromStdString(*rtcStats.base64_certificate) : String(),
        rtcStats.issuer_certificate_id ? fromStdString(*rtcStats.issuer_certificate_id) : String(),
    };
}

RTCStatsReport::CodecStats RTCStatsReport::CodecStats::convert(const webrtc::RTCCodecStats& rtcStats)
{
    return CodecStats {
        Stats::convert(RTCStatsReport::Type::Codec, rtcStats),
        rtcStats.payload_type ? *rtcStats.payload_type : 0,
        rtcStats.transport_id ? fromStdString(*rtcStats.transport_id) : String(),
        rtcStats.mime_type ? fromStdString(*rtcStats.mime_type) : String(),
        rtcStats.clock_rate ? std::optional { *rtcStats.clock_rate } : std::nullopt,
        rtcStats.channels ? std::optional { *rtcStats.channels } : std::nullopt,
        rtcStats.sdp_fmtp_line ? fromStdString(*rtcStats.sdp_fmtp_line) : String(),
    };
}

static inline std::optional<RTCIceRole> convertIceRole(const std::string& state)
{
    if (state == "unkown")
        return RTCIceRole::Unknown;
    if (state == "controlling")
        return RTCIceRole::Controlling;
    if (state == "controlled")
        return RTCIceRole::Controlled;

    return { };
}

static inline std::optional<RTCDtlsTransportState> dtlsTransportState(const std::string& state)
{
    if (state == "new")
        return RTCDtlsTransportState::New;
    if (state == "connecting")
        return RTCDtlsTransportState::Connecting;
    if (state == "connected")
        return RTCDtlsTransportState::Connected;
    if (state == "closed")
        return RTCDtlsTransportState::Closed;
    if (state == "failed")
        return RTCDtlsTransportState::Failed;

    return { };
}

static inline std::optional<RTCIceTransportState> iceTransportState(const std::string& state)
{
    if (state == "new")
        return RTCIceTransportState::New;
    if (state == "checking")
        return RTCIceTransportState::Checking;
    if (state == "connected")
        return RTCIceTransportState::Connected;
    if (state == "completed")
        return RTCIceTransportState::Completed;
    if (state == "failed")
        return RTCIceTransportState::Failed;
    if (state == "disconnected")
        return RTCIceTransportState::Disconnected;
    if (state == "closed")
        return RTCIceTransportState::Closed;

    ASSERT_NOT_REACHED();
    return { };
}

static inline std::optional<RTCStatsReport::DtlsRole> convertDtlsRole(const std::string& state)
{
    if (state == "client")
        return RTCStatsReport::DtlsRole::Client;
    if (state == "server")
        return RTCStatsReport::DtlsRole::Server;
    if (state == "unknown")
        return RTCStatsReport::DtlsRole::Unknown;

    return { };
}

RTCStatsReport::TransportStats RTCStatsReport::TransportStats::convert(const webrtc::RTCTransportStats& rtcStats)
{
    return TransportStats {
        Stats::convert(RTCStatsReport::Type::Transport, rtcStats),
        rtcStats.packets_sent ? std::optional { *rtcStats.packets_sent } : std::nullopt,
        rtcStats.packets_received ? std::optional { *rtcStats.packets_received } : std::nullopt,
        rtcStats.bytes_sent ? std::optional { *rtcStats.bytes_sent } : std::nullopt,
        rtcStats.bytes_received ? std::optional { *rtcStats.bytes_received } : std::nullopt,
        rtcStats.ice_role ? convertIceRole(*rtcStats.ice_role) : std::nullopt,
        rtcStats.ice_local_username_fragment ? fromStdString(*rtcStats.ice_local_username_fragment) : String(),
        rtcStats.dtls_state ? *dtlsTransportState(*rtcStats.dtls_state) : RTCDtlsTransportState::New,
        rtcStats.ice_state ? iceTransportState(*rtcStats.ice_state) : std::nullopt,
        rtcStats.selected_candidate_pair_id ? fromStdString(*rtcStats.selected_candidate_pair_id) : String(),
        rtcStats.local_certificate_id ? fromStdString(*rtcStats.local_certificate_id) : String(),
        rtcStats.remote_certificate_id ? fromStdString(*rtcStats.remote_certificate_id) : String(),
        rtcStats.tls_version ? fromStdString(*rtcStats.tls_version) : String(),
        rtcStats.dtls_cipher ? fromStdString(*rtcStats.dtls_cipher) : String(),
        rtcStats.dtls_role ? convertDtlsRole(*rtcStats.dtls_role) : std::nullopt,
        rtcStats.srtp_cipher ? fromStdString(*rtcStats.srtp_cipher) : String(),
        rtcStats.selected_candidate_pair_changes ? std::optional { *rtcStats.selected_candidate_pair_changes } : std::nullopt,
    };
}

RTCStatsReport::PeerConnectionStats RTCStatsReport::PeerConnectionStats::convert(const webrtc::RTCPeerConnectionStats& rtcStats)
{
    return PeerConnectionStats {
        Stats::convert(RTCStatsReport::Type::PeerConnection, rtcStats),
        rtcStats.data_channels_opened ? std::optional { *rtcStats.data_channels_opened } : std::nullopt,
        rtcStats.data_channels_closed ? std::optional { *rtcStats.data_channels_closed } : std::nullopt,
    };
}

RTCStatsReport::MediaSourceStats RTCStatsReport::MediaSourceStats::convert(Type type, const webrtc::RTCMediaSourceStats& rtcStats)
{
    return MediaSourceStats {
        Stats::convert(type, rtcStats),
        rtcStats.track_identifier ? fromStdString(*rtcStats.track_identifier) : String(),
        rtcStats.kind ? fromStdString(*rtcStats.kind) : String(),
    };
}

RTCStatsReport::AudioSourceStats RTCStatsReport::AudioSourceStats::convert(const webrtc::RTCAudioSourceStats& rtcStats)
{
    return AudioSourceStats {
        MediaSourceStats::convert(RTCStatsReport::Type::MediaSource, rtcStats),
        rtcStats.audio_level ? std::optional { *rtcStats.audio_level } : std::nullopt,
        rtcStats.total_audio_energy ? std::optional { *rtcStats.total_audio_energy } : std::nullopt,
        rtcStats.total_samples_duration ? std::optional { *rtcStats.total_samples_duration } : std::nullopt,
        rtcStats.echo_return_loss ? std::optional { *rtcStats.echo_return_loss } : std::nullopt,
        rtcStats.echo_return_loss_enhancement ? std::optional { *rtcStats.echo_return_loss_enhancement } : std::nullopt,

        // Not Implemented
        // rtcStats.dropped_samples_duration ? std::optional { *rtcStats.dropped_samples_duration } : std::nullopt,
        // Not Implemented
        // rtcStats.dropped_samples_events ? std::optional { *rtcStats.dropped_samples_events } : std::nullopt,
        // Not Implemented
        // rtcStats.total_capture_delay ? std::optional { *rtcStats.total_capture_delay } : std::nullopt,
        // Not Implemented
        // rtcStats.total_samples_captured ? std::optional { *rtcStats.total_samples_captured } : std::nullopt,
    };
}

RTCStatsReport::AudioPlayoutStats RTCStatsReport::AudioPlayoutStats::convert(const webrtc::RTCAudioPlayoutStats& rtcStats)
{
    return AudioPlayoutStats {
        Stats::convert(RTCStatsReport::Type::MediaPlayout, rtcStats),
        rtcStats.kind ? fromStdString(*rtcStats.kind) : String(),
        rtcStats.synthesized_samples_duration ? std::optional { *rtcStats.synthesized_samples_duration } : std::nullopt,
        rtcStats.synthesized_samples_events ? std::optional { *rtcStats.synthesized_samples_events } : std::nullopt,
        rtcStats.total_samples_duration ? std::optional { *rtcStats.total_samples_duration } : std::nullopt,
        rtcStats.total_playout_delay ? std::optional { *rtcStats.total_playout_delay } : std::nullopt,
        rtcStats.total_samples_count ? std::optional { *rtcStats.total_samples_count } : std::nullopt,
    };
}

RTCStatsReport::VideoSourceStats RTCStatsReport::VideoSourceStats::convert(const webrtc::RTCVideoSourceStats& rtcStats)
{
    return VideoSourceStats {
        MediaSourceStats::convert(RTCStatsReport::Type::MediaSource, rtcStats),
        rtcStats.width ? std::optional { *rtcStats.width } : std::nullopt,
        rtcStats.height ? std::optional { *rtcStats.height } : std::nullopt,
        rtcStats.frames ? std::optional { *rtcStats.frames } : std::nullopt,
        rtcStats.frames_per_second ? std::optional { *rtcStats.frames_per_second } : std::nullopt,
    };
}

template<typename T, typename PreciseType>
void addToStatsMap(DOMMapAdapter& report, const webrtc::RTCStats& rtcStats)
{
    // This is a cast from a webrtc type, not much we can do to make it safe.
    SUPPRESS_MEMORY_UNSAFE_CAST auto stats = T::convert(static_cast<const PreciseType&>(rtcStats));
    auto statsId = stats.id;
    report.set<IDLDOMString, IDLDictionary<T>>(WTF::move(statsId), WTF::move(stats));
}

static inline void initializeRTCStatsReportBackingMap(DOMMapAdapter& report, const webrtc::RTCStatsReport& rtcReport)
{
    for (const auto& rtcStats : rtcReport) {
        if (rtcStats.type() == webrtc::RTCInboundRtpStreamStats::kType)
            addToStatsMap<RTCStatsReport::InboundRtpStreamStats, webrtc::RTCInboundRtpStreamStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCOutboundRtpStreamStats::kType)
            addToStatsMap<RTCStatsReport::OutboundRtpStreamStats, webrtc::RTCOutboundRtpStreamStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCDataChannelStats::kType)
            addToStatsMap<RTCStatsReport::DataChannelStats, webrtc::RTCDataChannelStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCIceCandidatePairStats::kType)
            addToStatsMap<RTCStatsReport::IceCandidatePairStats, webrtc::RTCIceCandidatePairStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType || rtcStats.type() == webrtc::RTCLocalIceCandidateStats::kType)
            addToStatsMap<RTCStatsReport::IceCandidateStats, webrtc::RTCIceCandidateStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCCertificateStats::kType)
            addToStatsMap<RTCStatsReport::CertificateStats, webrtc::RTCCertificateStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCCodecStats::kType)
            addToStatsMap<RTCStatsReport::CodecStats, webrtc::RTCCodecStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCTransportStats::kType)
            addToStatsMap<RTCStatsReport::TransportStats, webrtc::RTCTransportStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCPeerConnectionStats::kType)
            addToStatsMap<RTCStatsReport::PeerConnectionStats, webrtc::RTCPeerConnectionStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCAudioSourceStats::kType)
            addToStatsMap<RTCStatsReport::AudioSourceStats, webrtc::RTCAudioSourceStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCVideoSourceStats::kType)
            addToStatsMap<RTCStatsReport::VideoSourceStats, webrtc::RTCVideoSourceStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCRemoteInboundRtpStreamStats::kType)
            addToStatsMap<RTCStatsReport::RemoteInboundRtpStreamStats, webrtc::RTCRemoteInboundRtpStreamStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCRemoteOutboundRtpStreamStats::kType)
            addToStatsMap<RTCStatsReport::RemoteOutboundRtpStreamStats, webrtc::RTCRemoteOutboundRtpStreamStats>(report, rtcStats);
        else if (rtcStats.type() == webrtc::RTCAudioPlayoutStats::kType)
            addToStatsMap<RTCStatsReport::AudioPlayoutStats, webrtc::RTCAudioPlayoutStats>(report, rtcStats);
    }
}

void LibWebRTCStatsCollector::OnStatsDelivered(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    callOnMainThread([this, protectedThis = webrtc::scoped_refptr<LibWebRTCStatsCollector>(this), rtcReport]() {
        m_callback(rtcReport);
    });
}

Ref<RTCStatsReport> LibWebRTCStatsCollector::createReport(const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    return RTCStatsReport::create([rtcReport](auto& mapAdapter) {
        if (rtcReport)
            initializeRTCStatsReportBackingMap(mapAdapter, *rtcReport);
    });
}

}; // namespace WTF


#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
