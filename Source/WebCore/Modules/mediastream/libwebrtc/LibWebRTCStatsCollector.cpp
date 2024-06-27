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

#include "JSDOMMapLike.h"
#include "JSRTCIceTcpCandidateType.h"
#include "JSRTCStatsReport.h"
#include "LibWebRTCUtils.h"
#include "Performance.h"
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

namespace WebCore {

LibWebRTCStatsCollector::LibWebRTCStatsCollector(CollectorCallback&& callback)
    : m_callback(WTFMove(callback))
{
}

LibWebRTCStatsCollector::~LibWebRTCStatsCollector()
{
    if (!m_callback)
        return;

    callOnMainThread([callback = WTFMove(m_callback)]() mutable {
        callback(nullptr);
    });
}

RTCStatsReport::Stats::Stats(Type type, const webrtc::RTCStats& rtcStats)
    : timestamp(Performance::reduceTimeResolution(Seconds::fromMicroseconds(rtcStats.timestamp().us_or(0))).milliseconds())
    , type(type)
    , id(fromStdString(rtcStats.id()))
{
}

RTCStatsReport::RtpStreamStats::RtpStreamStats(Type type, const webrtc::RTCRtpStreamStats& rtcStats)
    : Stats(type, rtcStats)
{
    if (rtcStats.ssrc)
        ssrc = *rtcStats.ssrc;
    if (rtcStats.kind)
        kind = fromStdString(*rtcStats.kind);
    if (rtcStats.transport_id)
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id)
        codecId = fromStdString(*rtcStats.codec_id);
}

RTCStatsReport::ReceivedRtpStreamStats::ReceivedRtpStreamStats(Type type, const webrtc::RTCReceivedRtpStreamStats& rtcStats)
    : RtpStreamStats(type, rtcStats)
{
    // packetsReceived should be in the base class, but somehow isn't, it's only define for RTCInboundRtpStreamStats
    // if (rtcStats.packets_received)
    //     stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.packets_lost)
        packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter)
        jitter = *rtcStats.jitter;
}

RTCStatsReport::InboundRtpStreamStats::InboundRtpStreamStats(const webrtc::RTCInboundRtpStreamStats& rtcStats)
    : ReceivedRtpStreamStats(RTCStatsReport::Type::InboundRtp, rtcStats)
{
    // should be in the base class.
    if (rtcStats.packets_received)
        packetsReceived = *rtcStats.packets_received;

    if (rtcStats.track_identifier)
        trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.mid)
        mid = fromStdString(*rtcStats.mid);
    if (rtcStats.remote_id)
        remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.frames_decoded)
        framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.key_frames_decoded)
        keyFramesDecoded = *rtcStats.key_frames_decoded;
    if (rtcStats.frames_dropped)
        framesDropped = *rtcStats.frames_dropped;
    if (rtcStats.frame_width)
        frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height)
        frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second)
        framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.qp_sum)
        qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_decode_time)
        totalDecodeTime = *rtcStats.total_decode_time;
    if (rtcStats.total_inter_frame_delay)
        totalInterFrameDelay = *rtcStats.total_inter_frame_delay;
    if (rtcStats.total_squared_inter_frame_delay)
        totalSquaredInterFrameDelay = *rtcStats.total_squared_inter_frame_delay;
    if (rtcStats.pause_count)
        pauseCount = *rtcStats.pause_count;
    if (rtcStats.total_pauses_duration)
        totalPausesDuration = *rtcStats.total_pauses_duration;
    if (rtcStats.freeze_count)
        freezeCount = *rtcStats.freeze_count;
    if (rtcStats.total_freezes_duration)
        totalFreezesDuration = *rtcStats.total_freezes_duration;
    if (rtcStats.last_packet_received_timestamp)
        lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;

    if (rtcStats.fec_packets_received)
        fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_bytes_received)
        fecBytesReceived = *rtcStats.fec_bytes_received;
    if (rtcStats.fec_packets_discarded)
        fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.fec_ssrc)
        fecSsrc = *rtcStats.fec_ssrc;
    if (rtcStats.header_bytes_received)
        headerBytesReceived = *rtcStats.header_bytes_received;
    if (rtcStats.rtx_ssrc)
        rtxSsrc = *rtcStats.rtx_ssrc;
    if (rtcStats.packets_discarded)
        packetsDiscarded = *rtcStats.packets_discarded;
    if (rtcStats.fec_packets_received)
        fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_packets_discarded)
        fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.bytes_received)
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.fir_count)
        firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count)
        pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count)
        nackCount = *rtcStats.nack_count;
    if (rtcStats.total_processing_delay)
        totalProcessingDelay = *rtcStats.total_processing_delay;
    if (rtcStats.estimated_playout_timestamp)
        estimatedPlayoutTimestamp = *rtcStats.estimated_playout_timestamp;
    if (rtcStats.jitter_buffer_delay)
        jitterBufferDelay = *rtcStats.jitter_buffer_delay;
    if (rtcStats.jitter_buffer_target_delay)
        jitterBufferTargetDelay = *rtcStats.jitter_buffer_target_delay;
    if (rtcStats.jitter_buffer_emitted_count)
        jitterBufferEmittedCount = *rtcStats.jitter_buffer_emitted_count;
    if (rtcStats.jitter_buffer_minimum_delay)
        jitterBufferMinimumDelay = *rtcStats.jitter_buffer_minimum_delay;
    if (rtcStats.total_samples_received)
        totalSamplesReceived = *rtcStats.total_samples_received;
    if (rtcStats.concealed_samples)
        concealedSamples = *rtcStats.concealed_samples;
    if (rtcStats.silent_concealed_samples)
        silentConcealedSamples = *rtcStats.silent_concealed_samples;
    if (rtcStats.concealment_events)
        concealmentEvents = *rtcStats.concealment_events;
    if (rtcStats.inserted_samples_for_deceleration)
        insertedSamplesForDeceleration = *rtcStats.inserted_samples_for_deceleration;
    if (rtcStats.removed_samples_for_acceleration)
        removedSamplesForAcceleration = *rtcStats.removed_samples_for_acceleration;
    if (rtcStats.audio_level)
        audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy)
        totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration)
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.frames_received)
        framesReceived = *rtcStats.frames_received;
    // TODO: Restrict Access
    // if (rtcStats.decoder_implementation)
    //     stats.decoderImplementation = fromStdString(*rtcStats.decoder_implementation);
    if (rtcStats.playout_id)
        playoutId = fromStdString(*rtcStats.playout_id);
    // TODO: Restrict Access
    // if (rtcStats.power_efficient_decoder)
    //     stats.powerEfficientDecoder = *rtcStats.power_efficient_decoder;
    if (rtcStats.frames_assembled_from_multiple_packets)
        framesAssembledFromMultiplePackets = *rtcStats.frames_assembled_from_multiple_packets;
    if (rtcStats.total_assembly_time)
        totalAssemblyTime = *rtcStats.total_assembly_time;
    if (rtcStats.retransmitted_packets_received)
        retransmittedPacketsReceived = *rtcStats.retransmitted_packets_received;
    if (rtcStats.retransmitted_bytes_received)
        retransmittedBytesReceived = *rtcStats.retransmitted_bytes_received;
}

RTCStatsReport::RemoteInboundRtpStreamStats::RemoteInboundRtpStreamStats(const webrtc::RTCRemoteInboundRtpStreamStats& rtcStats)
    : ReceivedRtpStreamStats(RTCStatsReport::Type::RemoteInboundRtp, rtcStats)
{
    if (rtcStats.local_id)
        localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.round_trip_time)
        roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time)
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.fraction_lost)
        fractionLost = *rtcStats.fraction_lost;
    if (rtcStats.round_trip_time_measurements)
        roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

RTCStatsReport::SentRtpStreamStats::SentRtpStreamStats(Type type, const webrtc::RTCSentRtpStreamStats& rtcStats)
    : RtpStreamStats(type, rtcStats)
{
    if (rtcStats.packets_sent)
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent)
        bytesSent = *rtcStats.bytes_sent;
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

RTCStatsReport::OutboundRtpStreamStats::OutboundRtpStreamStats(const webrtc::RTCOutboundRtpStreamStats& rtcStats)
    : SentRtpStreamStats(RTCStatsReport::Type::OutboundRtp, rtcStats)
{
    if (rtcStats.mid)
        mid = fromStdString(*rtcStats.mid);
    if (rtcStats.media_source_id)
        mediaSourceId = fromStdString(*rtcStats.media_source_id);
    if (rtcStats.remote_id)
        remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.rid)
        rid = fromStdString(*rtcStats.rid);
    if (rtcStats.header_bytes_sent)
        headerBytesSent = *rtcStats.header_bytes_sent;
    if (rtcStats.retransmitted_packets_sent)
        retransmittedPacketsSent = *rtcStats.retransmitted_packets_sent;
    if (rtcStats.retransmitted_bytes_sent)
        retransmittedBytesSent = *rtcStats.retransmitted_bytes_sent;
    if (rtcStats.target_bitrate)
        targetBitrate = *rtcStats.target_bitrate;
    if (rtcStats.total_encoded_bytes_target)
        totalEncodedBytesTarget = *rtcStats.total_encoded_bytes_target;
    if (rtcStats.frame_width)
        frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height)
        frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second)
        framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.frames_sent)
        framesSent = *rtcStats.frames_sent;
    if (rtcStats.huge_frames_sent)
        hugeFramesSent = *rtcStats.huge_frames_sent;
    if (rtcStats.frames_encoded)
        framesEncoded = *rtcStats.frames_encoded;
    if (rtcStats.key_frames_encoded)
        keyFramesEncoded = *rtcStats.key_frames_encoded;
    if (rtcStats.qp_sum)
        qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_encode_time)
        totalEncodeTime = *rtcStats.total_encode_time;
    if (rtcStats.total_packet_send_delay)
        totalPacketSendDelay = *rtcStats.total_packet_send_delay;
    if (rtcStats.quality_limitation_reason)
        qualityLimitationReason = convertQualityLimitationReason(*rtcStats.quality_limitation_reason);
    if (rtcStats.quality_limitation_durations) {
        auto& durations = *rtcStats.quality_limitation_durations;
        auto it = durations.begin();
        qualityLimitationDurations = Vector<KeyValuePair<String, double>>(durations.size(), [&] (size_t) {
            ASSERT(it != durations.end());
            KeyValuePair<String, double> element = { fromStdString(it->first), it->second };
            ++it;
            return element;
        });
    }
    if (rtcStats.quality_limitation_resolution_changes)
        qualityLimitationResolutionChanges = *rtcStats.quality_limitation_resolution_changes;
    if (rtcStats.nack_count)
        nackCount = *rtcStats.nack_count;
    if (rtcStats.fir_count)
        firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count)
        pliCount = *rtcStats.pli_count;

    if (rtcStats.active)
        active = *rtcStats.active;
    if (rtcStats.scalability_mode)
        scalabilityMode = fromStdString(*rtcStats.scalability_mode);
    if (rtcStats.rtx_ssrc)
        rtxSsrc = *rtcStats.rtx_ssrc;
}

RTCStatsReport::RemoteOutboundRtpStreamStats::RemoteOutboundRtpStreamStats(const webrtc::RTCRemoteOutboundRtpStreamStats& rtcStats)
    : SentRtpStreamStats(RTCStatsReport::Type::RemoteOutboundRtp, rtcStats)
{
    if (rtcStats.local_id)
        localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.remote_timestamp)
        remoteTimestamp = *rtcStats.remote_timestamp;
    if (rtcStats.reports_sent)
        reportsSent = *rtcStats.reports_sent;
    if (rtcStats.round_trip_time)
        roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time)
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.round_trip_time_measurements)
        roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

RTCStatsReport::DataChannelStats::DataChannelStats(const webrtc::RTCDataChannelStats& rtcStats)
    : Stats(RTCStatsReport::Type::DataChannel, rtcStats)
{
    if (rtcStats.label)
        label = fromStdString(*rtcStats.label);
    if (rtcStats.protocol)
        protocol = fromStdString(*rtcStats.protocol);
    if (rtcStats.data_channel_identifier)
        dataChannelIdentifier = *rtcStats.data_channel_identifier;
    if (rtcStats.state)
        state = fromStdString(*rtcStats.state);
    if (rtcStats.messages_sent)
        messagesSent = *rtcStats.messages_sent;
    if (rtcStats.bytes_sent)
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.messages_received)
        messagesReceived = *rtcStats.messages_received;
    if (rtcStats.bytes_received)
        bytesReceived = *rtcStats.bytes_received;
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

RTCStatsReport::IceCandidatePairStats::IceCandidatePairStats(const webrtc::RTCIceCandidatePairStats& rtcStats)
    : Stats(RTCStatsReport::Type::CandidatePair, rtcStats)
{
    if (rtcStats.transport_id)
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.local_candidate_id)
        localCandidateId = fromStdString(*rtcStats.local_candidate_id);
    if (rtcStats.remote_candidate_id)
        remoteCandidateId = fromStdString(*rtcStats.remote_candidate_id);
    if (rtcStats.state)
        state = iceCandidatePairState(*rtcStats.state);
    if (rtcStats.nominated)
        nominated = *rtcStats.nominated;
    if (rtcStats.packets_sent)
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received)
        packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent)
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received)
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.last_packet_sent_timestamp)
        lastPacketSentTimestamp = *rtcStats.last_packet_sent_timestamp;
    if (rtcStats.last_packet_received_timestamp)
        lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;
    if (rtcStats.total_round_trip_time)
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.current_round_trip_time)
        currentRoundTripTime = *rtcStats.current_round_trip_time;
    if (rtcStats.available_outgoing_bitrate)
        availableOutgoingBitrate = *rtcStats.available_outgoing_bitrate;
    if (rtcStats.available_incoming_bitrate)
        availableIncomingBitrate = *rtcStats.available_incoming_bitrate;
    if (rtcStats.requests_received)
        requestsReceived = *rtcStats.requests_received;
    if (rtcStats.requests_sent)
        requestsSent = *rtcStats.requests_sent;
    if (rtcStats.responses_received)
        responsesReceived = *rtcStats.responses_received;
    if (rtcStats.responses_sent)
        responsesSent = *rtcStats.responses_sent;

    if (rtcStats.consent_requests_sent)
        consentRequestsSent = *rtcStats.consent_requests_sent;
    if (rtcStats.packets_discarded_on_send)
        packetsDiscardedOnSend = *rtcStats.packets_discarded_on_send;
    if (rtcStats.bytes_discarded_on_send)
        bytesDiscardedOnSend = *rtcStats.bytes_discarded_on_send;
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

RTCStatsReport::IceCandidateStats::IceCandidateStats(const webrtc::RTCIceCandidateStats& rtcStats)
    : Stats(rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType ? RTCStatsReport::Type::RemoteCandidate : RTCStatsReport::Type::LocalCandidate, rtcStats)
{
    if (rtcStats.transport_id)
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.ip)
        address = fromStdString(*rtcStats.ip);
    if (rtcStats.port)
        port = *rtcStats.port;
    if (rtcStats.protocol)
        protocol = fromStdString(*rtcStats.protocol);
    ASSERT(rtcStats.candidate_type);
    if (rtcStats.candidate_type)
        candidateType = iceCandidateState(*rtcStats.candidate_type);
    if (candidateType == RTCIceCandidateType::Prflx || candidateType == RTCIceCandidateType::Host)
        address = { };

    if (rtcStats.priority)
        priority = *rtcStats.priority;
    if (rtcStats.url)
        url = fromStdString(*rtcStats.url);
    if (rtcStats.foundation)
        foundation = fromStdString(*rtcStats.foundation);
    if (rtcStats.username_fragment)
        usernameFragment = fromStdString(*rtcStats.username_fragment);
    if (rtcStats.tcp_type) {
        if (auto tcpType = parseEnumerationFromString<RTCIceTcpCandidateType>(fromStdString(*rtcStats.tcp_type)))
            tcpType = *tcpType;
    }
}

RTCStatsReport::CertificateStats::CertificateStats(const webrtc::RTCCertificateStats& rtcStats)
    : Stats(RTCStatsReport::Type::Certificate, rtcStats)
{
    if (rtcStats.fingerprint)
        fingerprint = fromStdString(*rtcStats.fingerprint);
    if (rtcStats.fingerprint_algorithm)
        fingerprintAlgorithm = fromStdString(*rtcStats.fingerprint_algorithm);
    if (rtcStats.base64_certificate)
        base64Certificate = fromStdString(*rtcStats.base64_certificate);
    if (rtcStats.issuer_certificate_id)
        issuerCertificateId = fromStdString(*rtcStats.issuer_certificate_id);
}

RTCStatsReport::CodecStats::CodecStats(const webrtc::RTCCodecStats& rtcStats)
    : Stats(RTCStatsReport::Type::Codec, rtcStats)
{
    if (rtcStats.payload_type)
        payloadType = *rtcStats.payload_type;
    if (rtcStats.transport_id)
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.mime_type)
        mimeType = fromStdString(*rtcStats.mime_type);
    if (rtcStats.clock_rate)
        clockRate = *rtcStats.clock_rate;
    if (rtcStats.channels)
        channels = *rtcStats.channels;
    if (rtcStats.sdp_fmtp_line)
        sdpFmtpLine = fromStdString(*rtcStats.sdp_fmtp_line);
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

RTCStatsReport::TransportStats::TransportStats(const webrtc::RTCTransportStats& rtcStats)
    : Stats(RTCStatsReport::Type::Transport, rtcStats)
{
    if (rtcStats.packets_sent)
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received)
        packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent)
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received)
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.ice_role)
        iceRole = convertIceRole(*rtcStats.ice_role);
    if (rtcStats.ice_local_username_fragment)
        iceLocalUsernameFragment = fromStdString(*rtcStats.ice_local_username_fragment);
    if (rtcStats.dtls_state)
        dtlsState = *dtlsTransportState(*rtcStats.dtls_state);
    if (rtcStats.ice_state)
        iceState = iceTransportState(*rtcStats.ice_state);
    if (rtcStats.selected_candidate_pair_id)
        selectedCandidatePairId = fromStdString(*rtcStats.selected_candidate_pair_id);
    if (rtcStats.local_certificate_id)
        localCertificateId = fromStdString(*rtcStats.local_certificate_id);
    if (rtcStats.remote_certificate_id)
        remoteCertificateId = fromStdString(*rtcStats.remote_certificate_id);
    if (rtcStats.tls_version)
        tlsVersion = fromStdString(*rtcStats.tls_version);
    if (rtcStats.dtls_cipher)
        dtlsCipher = fromStdString(*rtcStats.dtls_cipher);
    if (rtcStats.dtls_role)
        dtlsRole = convertDtlsRole(*rtcStats.dtls_role);
    if (rtcStats.srtp_cipher)
        srtpCipher = fromStdString(*rtcStats.srtp_cipher);
    if (rtcStats.selected_candidate_pair_changes)
        selectedCandidatePairChanges = *rtcStats.selected_candidate_pair_changes;
}

RTCStatsReport::PeerConnectionStats::PeerConnectionStats(const webrtc::RTCPeerConnectionStats& rtcStats)
    : Stats(RTCStatsReport::Type::PeerConnection, rtcStats)
{
    if (rtcStats.data_channels_opened)
        dataChannelsOpened = *rtcStats.data_channels_opened;
    if (rtcStats.data_channels_closed)
        dataChannelsClosed = *rtcStats.data_channels_closed;
}

RTCStatsReport::MediaSourceStats::MediaSourceStats(Type type, const webrtc::RTCMediaSourceStats& rtcStats)
    : Stats(type, rtcStats)
{
    if (rtcStats.track_identifier)
        trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.kind)
        kind = fromStdString(*rtcStats.kind);
}

RTCStatsReport::AudioSourceStats::AudioSourceStats(const webrtc::RTCAudioSourceStats& rtcStats)
    : MediaSourceStats(RTCStatsReport::Type::MediaSource, rtcStats)
{
    if (rtcStats.audio_level)
        audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy)
        totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration)
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.echo_return_loss)
        echoReturnLoss = *rtcStats.echo_return_loss;
    if (rtcStats.echo_return_loss_enhancement)
        echoReturnLossEnhancement = *rtcStats.echo_return_loss_enhancement;
    // Not Implemented
    // if (rtcStats.dropped_samples_duration)
    //     stats.droppedSamplesDuration = *rtcStats.dropped_samples_duration;
    // Not Implemented
    // if (rtcStats.dropped_samples_events)
    //     stats.droppedSamplesEvents = *rtcStats.dropped_samples_events;
    // Not Implemented
    // if (rtcStats.total_capture_delay)
    //     stats.totalCaptureDelay = *rtcStats.total_capture_delay;
    // Not Implemented
    // if (rtcStats.total_samples_captured)
    //     stats.totalSamplesCaptured = *rtcStats.total_samples_captured;
}

RTCStatsReport::AudioPlayoutStats::AudioPlayoutStats(const webrtc::RTCAudioPlayoutStats& rtcStats)
    : Stats(RTCStatsReport::Type::MediaPlayout, rtcStats)
{
    if (rtcStats.kind)
        kind = fromStdString(*rtcStats.kind);
    if (rtcStats.synthesized_samples_duration)
        synthesizedSamplesDuration = *rtcStats.synthesized_samples_duration;
    if (rtcStats.synthesized_samples_events)
        synthesizedSamplesEvents = *rtcStats.synthesized_samples_events;
    if (rtcStats.total_samples_duration)
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.total_playout_delay)
        totalPlayoutDelay = *rtcStats.total_playout_delay;
    if (rtcStats.total_samples_count)
        totalSamplesCount = *rtcStats.total_samples_count;
}

RTCStatsReport::VideoSourceStats::VideoSourceStats(const webrtc::RTCVideoSourceStats& rtcStats)
    : MediaSourceStats(RTCStatsReport::Type::MediaSource, rtcStats)
{
    if (rtcStats.width)
        width = *rtcStats.width;
    if (rtcStats.height)
        height = *rtcStats.height;
    if (rtcStats.frames)
        frames = *rtcStats.frames;
    if (rtcStats.frames_per_second)
        framesPerSecond = *rtcStats.frames_per_second;
}

template <typename T, typename PreciseType>
void addToStatsMap(DOMMapAdapter& report, const webrtc::RTCStats& rtcStats)
{
    T stats { static_cast<const PreciseType&>(rtcStats) };
    String statsId = stats.id;
    report.set<IDLDOMString, IDLDictionary<T>>(WTFMove(statsId), WTFMove(stats));
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

void LibWebRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    callOnMainThread([this, protectedThis = rtc::scoped_refptr<LibWebRTCStatsCollector>(this), rtcReport]() {
        m_callback(rtcReport);
    });
}

Ref<RTCStatsReport> LibWebRTCStatsCollector::createReport(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    return RTCStatsReport::create([rtcReport](auto& mapAdapter) {
        if (rtcReport)
            initializeRTCStatsReportBackingMap(mapAdapter, *rtcReport);
    });
}

}; // namespace WTF


#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
