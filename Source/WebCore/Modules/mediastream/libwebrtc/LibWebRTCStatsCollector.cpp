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
    if (rtcStats.ssrc.is_defined())
        ssrc = *rtcStats.ssrc;
    if (rtcStats.kind.is_defined())
        kind = fromStdString(*rtcStats.kind);
    if (rtcStats.transport_id.is_defined())
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        codecId = fromStdString(*rtcStats.codec_id);
}

RTCStatsReport::ReceivedRtpStreamStats::ReceivedRtpStreamStats(Type type, const webrtc::RTCReceivedRtpStreamStats& rtcStats)
    : RtpStreamStats(type, rtcStats)
{
    // packetsReceived should be in the base class, but somehow isn't, it's only define for RTCInboundRtpStreamStats
    // if (rtcStats.packets_received.is_defined())
    //     stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.packets_lost.is_defined())
        packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter.is_defined())
        jitter = *rtcStats.jitter;
}

RTCStatsReport::InboundRtpStreamStats::InboundRtpStreamStats(const webrtc::RTCInboundRtpStreamStats& rtcStats)
    : ReceivedRtpStreamStats(RTCStatsReport::Type::InboundRtp, rtcStats)
{
    // should be in the base class.
    if (rtcStats.packets_received.is_defined())
        packetsReceived = *rtcStats.packets_received;

    if (rtcStats.track_identifier.is_defined())
        trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.mid.is_defined())
        mid = fromStdString(*rtcStats.mid);
    if (rtcStats.remote_id.is_defined())
        remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.frames_decoded.is_defined())
        framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.key_frames_decoded.is_defined())
        keyFramesDecoded = *rtcStats.key_frames_decoded;
    if (rtcStats.frames_dropped.is_defined())
        framesDropped = *rtcStats.frames_dropped;
    if (rtcStats.frame_width.is_defined())
        frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.qp_sum.is_defined())
        qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_decode_time.is_defined())
        totalDecodeTime = *rtcStats.total_decode_time;
    if (rtcStats.total_inter_frame_delay.is_defined())
        totalInterFrameDelay = *rtcStats.total_inter_frame_delay;
    if (rtcStats.total_squared_inter_frame_delay.is_defined())
        totalSquaredInterFrameDelay = *rtcStats.total_squared_inter_frame_delay;
    if (rtcStats.pause_count.is_defined())
        pauseCount = *rtcStats.pause_count;
    if (rtcStats.total_pauses_duration.is_defined())
        totalPausesDuration = *rtcStats.total_pauses_duration;
    if (rtcStats.freeze_count.is_defined())
        freezeCount = *rtcStats.freeze_count;
    if (rtcStats.total_freezes_duration.is_defined())
        totalFreezesDuration = *rtcStats.total_freezes_duration;
    if (rtcStats.last_packet_received_timestamp.is_defined())
        lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;

    if (rtcStats.fec_packets_received.is_defined())
        fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_bytes_received.is_defined())
        fecBytesReceived = *rtcStats.fec_bytes_received;
    if (rtcStats.fec_packets_discarded.is_defined())
        fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.fec_ssrc.is_defined())
        fecSsrc = *rtcStats.fec_ssrc;
    if (rtcStats.header_bytes_received.is_defined())
        headerBytesReceived = *rtcStats.header_bytes_received;
    if (rtcStats.rtx_ssrc.is_defined())
        rtxSsrc = *rtcStats.rtx_ssrc;
    if (rtcStats.packets_discarded.is_defined())
        packetsDiscarded = *rtcStats.packets_discarded;
    if (rtcStats.fec_packets_received.is_defined())
        fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_packets_discarded.is_defined())
        fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.bytes_received.is_defined())
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.fir_count.is_defined())
        firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count.is_defined())
        nackCount = *rtcStats.nack_count;
    if (rtcStats.total_processing_delay.is_defined())
        totalProcessingDelay = *rtcStats.total_processing_delay;
    if (rtcStats.estimated_playout_timestamp.is_defined())
        estimatedPlayoutTimestamp = *rtcStats.estimated_playout_timestamp;
    if (rtcStats.jitter_buffer_delay.is_defined())
        jitterBufferDelay = *rtcStats.jitter_buffer_delay;
    if (rtcStats.jitter_buffer_target_delay.is_defined())
        jitterBufferTargetDelay = *rtcStats.jitter_buffer_target_delay;
    if (rtcStats.jitter_buffer_emitted_count.is_defined())
        jitterBufferEmittedCount = *rtcStats.jitter_buffer_emitted_count;
    if (rtcStats.jitter_buffer_minimum_delay.is_defined())
        jitterBufferMinimumDelay = *rtcStats.jitter_buffer_minimum_delay;
    if (rtcStats.total_samples_received.is_defined())
        totalSamplesReceived = *rtcStats.total_samples_received;
    if (rtcStats.concealed_samples.is_defined())
        concealedSamples = *rtcStats.concealed_samples;
    if (rtcStats.silent_concealed_samples.is_defined())
        silentConcealedSamples = *rtcStats.silent_concealed_samples;
    if (rtcStats.concealment_events.is_defined())
        concealmentEvents = *rtcStats.concealment_events;
    if (rtcStats.inserted_samples_for_deceleration.is_defined())
        insertedSamplesForDeceleration = *rtcStats.inserted_samples_for_deceleration;
    if (rtcStats.removed_samples_for_acceleration.is_defined())
        removedSamplesForAcceleration = *rtcStats.removed_samples_for_acceleration;
    if (rtcStats.audio_level.is_defined())
        audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy.is_defined())
        totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration.is_defined())
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.frames_received.is_defined())
        framesReceived = *rtcStats.frames_received;
    // TODO: Restrict Access
    // if (rtcStats.decoder_implementation.is_defined())
    //     stats.decoderImplementation = fromStdString(*rtcStats.decoder_implementation);
    if (rtcStats.playout_id.is_defined())
        playoutId = fromStdString(*rtcStats.playout_id);
    // TODO: Restrict Access
    // if (rtcStats.power_efficient_decoder.is_defined())
    //     stats.powerEfficientDecoder = *rtcStats.power_efficient_decoder;
    if (rtcStats.frames_assembled_from_multiple_packets.is_defined())
        framesAssembledFromMultiplePackets = *rtcStats.frames_assembled_from_multiple_packets;
    if (rtcStats.total_assembly_time.is_defined())
        totalAssemblyTime = *rtcStats.total_assembly_time;
    if (rtcStats.retransmitted_packets_received.is_defined())
        retransmittedPacketsReceived = *rtcStats.retransmitted_packets_received;
    if (rtcStats.retransmitted_bytes_received.is_defined())
        retransmittedBytesReceived = *rtcStats.retransmitted_bytes_received;
}

RTCStatsReport::RemoteInboundRtpStreamStats::RemoteInboundRtpStreamStats(const webrtc::RTCRemoteInboundRtpStreamStats& rtcStats)
    : ReceivedRtpStreamStats(RTCStatsReport::Type::RemoteInboundRtp, rtcStats)
{
    if (rtcStats.local_id.is_defined())
        localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.round_trip_time.is_defined())
        roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time.is_defined())
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.fraction_lost.is_defined())
        fractionLost = *rtcStats.fraction_lost;
    if (rtcStats.round_trip_time_measurements.is_defined())
        roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

RTCStatsReport::SentRtpStreamStats::SentRtpStreamStats(Type type, const webrtc::RTCSentRtpStreamStats& rtcStats)
    : RtpStreamStats(type, rtcStats)
{
    if (rtcStats.packets_sent.is_defined())
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent.is_defined())
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
    if (rtcStats.mid.is_defined())
        mid = fromStdString(*rtcStats.mid);
    if (rtcStats.media_source_id.is_defined())
        mediaSourceId = fromStdString(*rtcStats.media_source_id);
    if (rtcStats.remote_id.is_defined())
        remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.rid.is_defined())
        rid = fromStdString(*rtcStats.rid);
    if (rtcStats.header_bytes_sent.is_defined())
        headerBytesSent = *rtcStats.header_bytes_sent;
    if (rtcStats.retransmitted_packets_sent.is_defined())
        retransmittedPacketsSent = *rtcStats.retransmitted_packets_sent;
    if (rtcStats.retransmitted_bytes_sent.is_defined())
        retransmittedBytesSent = *rtcStats.retransmitted_bytes_sent;
    if (rtcStats.target_bitrate.is_defined())
        targetBitrate = *rtcStats.target_bitrate;
    if (rtcStats.total_encoded_bytes_target.is_defined())
        totalEncodedBytesTarget = *rtcStats.total_encoded_bytes_target;
    if (rtcStats.frame_width.is_defined())
        frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.frames_sent.is_defined())
        framesSent = *rtcStats.frames_sent;
    if (rtcStats.huge_frames_sent.is_defined())
        hugeFramesSent = *rtcStats.huge_frames_sent;
    if (rtcStats.frames_encoded.is_defined())
        framesEncoded = *rtcStats.frames_encoded;
    if (rtcStats.key_frames_encoded.is_defined())
        keyFramesEncoded = *rtcStats.key_frames_encoded;
    if (rtcStats.qp_sum.is_defined())
        qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_encode_time.is_defined())
        totalEncodeTime = *rtcStats.total_encode_time;
    if (rtcStats.total_packet_send_delay.is_defined())
        totalPacketSendDelay = *rtcStats.total_packet_send_delay;
    if (rtcStats.quality_limitation_reason.is_defined())
        qualityLimitationReason = convertQualityLimitationReason(*rtcStats.quality_limitation_reason);
    if (rtcStats.quality_limitation_durations.is_defined()) {
        auto& durations = *rtcStats.quality_limitation_durations;
        auto it = durations.begin();
        qualityLimitationDurations = Vector<KeyValuePair<String, double>>(durations.size(), [&] (size_t) {
            ASSERT(it != durations.end());
            KeyValuePair<String, double> element = { fromStdString(it->first), it->second };
            ++it;
            return element;
        });
    }
    if (rtcStats.quality_limitation_resolution_changes.is_defined())
        qualityLimitationResolutionChanges = *rtcStats.quality_limitation_resolution_changes;
    if (rtcStats.nack_count.is_defined())
        nackCount = *rtcStats.nack_count;
    if (rtcStats.fir_count.is_defined())
        firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        pliCount = *rtcStats.pli_count;

    if (rtcStats.active.is_defined())
        active = *rtcStats.active;
    if (rtcStats.scalability_mode.is_defined())
        scalabilityMode = fromStdString(*rtcStats.scalability_mode);
    if (rtcStats.rtx_ssrc.is_defined())
        rtxSsrc = *rtcStats.rtx_ssrc;
}

RTCStatsReport::RemoteOutboundRtpStreamStats::RemoteOutboundRtpStreamStats(const webrtc::RTCRemoteOutboundRtpStreamStats& rtcStats)
    : SentRtpStreamStats(RTCStatsReport::Type::RemoteOutboundRtp, rtcStats)
{
    if (rtcStats.local_id.is_defined())
        localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.remote_timestamp.is_defined())
        remoteTimestamp = *rtcStats.remote_timestamp;
    if (rtcStats.reports_sent.is_defined())
        reportsSent = *rtcStats.reports_sent;
    if (rtcStats.round_trip_time.is_defined())
        roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time.is_defined())
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.round_trip_time_measurements.is_defined())
        roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

RTCStatsReport::DataChannelStats::DataChannelStats(const webrtc::RTCDataChannelStats& rtcStats)
    : Stats(RTCStatsReport::Type::DataChannel, rtcStats)
{
    if (rtcStats.label.is_defined())
        label = fromStdString(*rtcStats.label);
    if (rtcStats.protocol.is_defined())
        protocol = fromStdString(*rtcStats.protocol);
    if (rtcStats.data_channel_identifier.is_defined())
        dataChannelIdentifier = *rtcStats.data_channel_identifier;
    if (rtcStats.state.is_defined())
        state = fromStdString(*rtcStats.state);
    if (rtcStats.messages_sent.is_defined())
        messagesSent = *rtcStats.messages_sent;
    if (rtcStats.bytes_sent.is_defined())
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.messages_received.is_defined())
        messagesReceived = *rtcStats.messages_received;
    if (rtcStats.bytes_received.is_defined())
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
    if (rtcStats.transport_id.is_defined())
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.local_candidate_id.is_defined())
        localCandidateId = fromStdString(*rtcStats.local_candidate_id);
    if (rtcStats.remote_candidate_id.is_defined())
        remoteCandidateId = fromStdString(*rtcStats.remote_candidate_id);
    if (rtcStats.state.is_defined())
        state = iceCandidatePairState(*rtcStats.state);
    if (rtcStats.nominated.is_defined())
        nominated = *rtcStats.nominated;
    if (rtcStats.packets_sent.is_defined())
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received.is_defined())
        packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent.is_defined())
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.last_packet_sent_timestamp.is_defined())
        lastPacketSentTimestamp = *rtcStats.last_packet_sent_timestamp;
    if (rtcStats.last_packet_received_timestamp.is_defined())
        lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;
    if (rtcStats.total_round_trip_time.is_defined())
        totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.current_round_trip_time.is_defined())
        currentRoundTripTime = *rtcStats.current_round_trip_time;
    if (rtcStats.available_outgoing_bitrate.is_defined())
        availableOutgoingBitrate = *rtcStats.available_outgoing_bitrate;
    if (rtcStats.available_incoming_bitrate.is_defined())
        availableIncomingBitrate = *rtcStats.available_incoming_bitrate;
    if (rtcStats.requests_received.is_defined())
        requestsReceived = *rtcStats.requests_received;
    if (rtcStats.requests_sent.is_defined())
        requestsSent = *rtcStats.requests_sent;
    if (rtcStats.responses_received.is_defined())
        responsesReceived = *rtcStats.responses_received;
    if (rtcStats.responses_sent.is_defined())
        responsesSent = *rtcStats.responses_sent;

    if (rtcStats.consent_requests_sent.is_defined())
        consentRequestsSent = *rtcStats.consent_requests_sent;
    if (rtcStats.packets_discarded_on_send.is_defined())
        packetsDiscardedOnSend = *rtcStats.packets_discarded_on_send;
    if (rtcStats.bytes_discarded_on_send.is_defined())
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
    if (rtcStats.transport_id.is_defined())
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.ip.is_defined())
        address = fromStdString(*rtcStats.ip);
    if (rtcStats.port.is_defined())
        port = *rtcStats.port;
    if (rtcStats.protocol.is_defined())
        protocol = fromStdString(*rtcStats.protocol);
    ASSERT(rtcStats.candidate_type.is_defined());
    if (rtcStats.candidate_type.is_defined())
        candidateType = iceCandidateState(*rtcStats.candidate_type);
    if (candidateType == RTCIceCandidateType::Prflx || candidateType == RTCIceCandidateType::Host)
        address = { };

    if (rtcStats.priority.is_defined())
        priority = *rtcStats.priority;
    if (rtcStats.url.is_defined())
        url = fromStdString(*rtcStats.url);
    if (rtcStats.foundation.is_defined())
        foundation = fromStdString(*rtcStats.foundation);
    if (rtcStats.username_fragment.is_defined())
        usernameFragment = fromStdString(*rtcStats.username_fragment);
    if (rtcStats.tcp_type.is_defined()) {
        if (auto tcpType = parseEnumerationFromString<RTCIceTcpCandidateType>(fromStdString(*rtcStats.tcp_type)))
            tcpType = *tcpType;
    }
}

RTCStatsReport::CertificateStats::CertificateStats(const webrtc::RTCCertificateStats& rtcStats)
    : Stats(RTCStatsReport::Type::Certificate, rtcStats)
{
    if (rtcStats.fingerprint.is_defined())
        fingerprint = fromStdString(*rtcStats.fingerprint);
    if (rtcStats.fingerprint_algorithm.is_defined())
        fingerprintAlgorithm = fromStdString(*rtcStats.fingerprint_algorithm);
    if (rtcStats.base64_certificate.is_defined())
        base64Certificate = fromStdString(*rtcStats.base64_certificate);
    if (rtcStats.issuer_certificate_id.is_defined())
        issuerCertificateId = fromStdString(*rtcStats.issuer_certificate_id);
}

RTCStatsReport::CodecStats::CodecStats(const webrtc::RTCCodecStats& rtcStats)
    : Stats(RTCStatsReport::Type::Codec, rtcStats)
{
    if (rtcStats.payload_type.is_defined())
        payloadType = *rtcStats.payload_type;
    if (rtcStats.transport_id.is_defined())
        transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.mime_type.is_defined())
        mimeType = fromStdString(*rtcStats.mime_type);
    if (rtcStats.clock_rate.is_defined())
        clockRate = *rtcStats.clock_rate;
    if (rtcStats.channels.is_defined())
        channels = *rtcStats.channels;
    if (rtcStats.sdp_fmtp_line.is_defined())
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
    if (rtcStats.packets_sent.is_defined())
        packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received.is_defined())
        packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent.is_defined())
        bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.ice_role.is_defined())
        iceRole = convertIceRole(*rtcStats.ice_role);
    if (rtcStats.ice_local_username_fragment.is_defined())
        iceLocalUsernameFragment = fromStdString(*rtcStats.ice_local_username_fragment);
    if (rtcStats.dtls_state.is_defined())
        dtlsState = *dtlsTransportState(*rtcStats.dtls_state);
    if (rtcStats.ice_state.is_defined())
        iceState = iceTransportState(*rtcStats.ice_state);
    if (rtcStats.selected_candidate_pair_id.is_defined())
        selectedCandidatePairId = fromStdString(*rtcStats.selected_candidate_pair_id);
    if (rtcStats.local_certificate_id.is_defined())
        localCertificateId = fromStdString(*rtcStats.local_certificate_id);
    if (rtcStats.remote_certificate_id.is_defined())
        remoteCertificateId = fromStdString(*rtcStats.remote_certificate_id);
    if (rtcStats.tls_version.is_defined())
        tlsVersion = fromStdString(*rtcStats.tls_version);
    if (rtcStats.dtls_cipher.is_defined())
        dtlsCipher = fromStdString(*rtcStats.dtls_cipher);
    if (rtcStats.dtls_role.is_defined())
        dtlsRole = convertDtlsRole(*rtcStats.dtls_role);
    if (rtcStats.srtp_cipher.is_defined())
        srtpCipher = fromStdString(*rtcStats.srtp_cipher);
    if (rtcStats.selected_candidate_pair_changes.is_defined())
        selectedCandidatePairChanges = *rtcStats.selected_candidate_pair_changes;
}

RTCStatsReport::PeerConnectionStats::PeerConnectionStats(const webrtc::RTCPeerConnectionStats& rtcStats)
    : Stats(RTCStatsReport::Type::PeerConnection, rtcStats)
{
    if (rtcStats.data_channels_opened.is_defined())
        dataChannelsOpened = *rtcStats.data_channels_opened;
    if (rtcStats.data_channels_closed.is_defined())
        dataChannelsClosed = *rtcStats.data_channels_closed;
}

RTCStatsReport::MediaSourceStats::MediaSourceStats(Type type, const webrtc::RTCMediaSourceStats& rtcStats)
    : Stats(type, rtcStats)
{
    if (rtcStats.track_identifier.is_defined())
        trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.kind.is_defined())
        kind = fromStdString(*rtcStats.kind);
}

RTCStatsReport::AudioSourceStats::AudioSourceStats(const webrtc::RTCAudioSourceStats& rtcStats)
    : MediaSourceStats(RTCStatsReport::Type::MediaSource, rtcStats)
{
    if (rtcStats.audio_level.is_defined())
        audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy.is_defined())
        totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration.is_defined())
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.echo_return_loss.is_defined())
        echoReturnLoss = *rtcStats.echo_return_loss;
    if (rtcStats.echo_return_loss_enhancement.is_defined())
        echoReturnLossEnhancement = *rtcStats.echo_return_loss_enhancement;
    // Not Implemented
    // if (rtcStats.dropped_samples_duration.is_defined())
    //     stats.droppedSamplesDuration = *rtcStats.dropped_samples_duration;
    // Not Implemented
    // if (rtcStats.dropped_samples_events.is_defined())
    //     stats.droppedSamplesEvents = *rtcStats.dropped_samples_events;
    // Not Implemented
    // if (rtcStats.total_capture_delay.is_defined())
    //     stats.totalCaptureDelay = *rtcStats.total_capture_delay;
    // Not Implemented
    // if (rtcStats.total_samples_captured.is_defined())
    //     stats.totalSamplesCaptured = *rtcStats.total_samples_captured;
}

RTCStatsReport::AudioPlayoutStats::AudioPlayoutStats(const webrtc::RTCAudioPlayoutStats& rtcStats)
    : Stats(RTCStatsReport::Type::MediaPlayout, rtcStats)
{
    if (rtcStats.kind.is_defined())
        kind = fromStdString(*rtcStats.kind);
    if (rtcStats.synthesized_samples_duration.is_defined())
        synthesizedSamplesDuration = *rtcStats.synthesized_samples_duration;
    if (rtcStats.synthesized_samples_events.is_defined())
        synthesizedSamplesEvents = *rtcStats.synthesized_samples_events;
    if (rtcStats.total_samples_duration.is_defined())
        totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.total_playout_delay.is_defined())
        totalPlayoutDelay = *rtcStats.total_playout_delay;
    if (rtcStats.total_samples_count.is_defined())
        totalSamplesCount = *rtcStats.total_samples_count;
}

RTCStatsReport::VideoSourceStats::VideoSourceStats(const webrtc::RTCVideoSourceStats& rtcStats)
    : MediaSourceStats(RTCStatsReport::Type::MediaSource, rtcStats)
{
    if (rtcStats.width.is_defined())
        width = *rtcStats.width;
    if (rtcStats.height.is_defined())
        height = *rtcStats.height;
    if (rtcStats.frames.is_defined())
        frames = *rtcStats.frames;
    if (rtcStats.frames_per_second.is_defined())
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
