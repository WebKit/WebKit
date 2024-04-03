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

static inline void fillRTCStats(RTCStatsReport::Stats& stats, const webrtc::RTCStats& rtcStats)
{
    stats.timestamp = Performance::reduceTimeResolution(Seconds::fromMicroseconds(rtcStats.timestamp().us_or(0))).milliseconds();
    stats.id = fromStdString(rtcStats.id());
}

static inline void fillRtpStreamStats(RTCStatsReport::RtpStreamStats& stats, const webrtc::RTCRtpStreamStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.ssrc.is_defined())
        stats.ssrc = *rtcStats.ssrc;
    if (rtcStats.kind.is_defined())
        stats.kind = fromStdString(*rtcStats.kind);
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        stats.codecId = fromStdString(*rtcStats.codec_id);
}

static inline void fillReceivedRtpStreamStats(RTCStatsReport::ReceivedRtpStreamStats& stats, const webrtc::RTCReceivedRtpStreamStats& rtcStats)
{
    fillRtpStreamStats(stats, rtcStats);

    // packetsReceived should be in the base class, but somehow isn't, it's only define for RTCInboundRtpStreamStats
    // if (rtcStats.packets_received.is_defined())
    //     stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.packets_lost.is_defined())
        stats.packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter.is_defined())
        stats.jitter = *rtcStats.jitter;
}

static inline void fillInboundRtpStreamStats(RTCStatsReport::InboundRtpStreamStats& stats, const webrtc::RTCInboundRtpStreamStats& rtcStats)
{
    fillReceivedRtpStreamStats(stats, rtcStats);

    // should be in the base class.
    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;

    if (rtcStats.track_identifier.is_defined())
        stats.trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.mid.is_defined())
        stats.mid = fromStdString(*rtcStats.mid);
    if (rtcStats.remote_id.is_defined())
        stats.remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.key_frames_decoded.is_defined())
        stats.keyFramesDecoded = *rtcStats.key_frames_decoded;
    if (rtcStats.frames_dropped.is_defined())
        stats.framesDropped = *rtcStats.frames_dropped;
    if (rtcStats.frame_width.is_defined())
        stats.frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        stats.frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        stats.framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_decode_time.is_defined())
        stats.totalDecodeTime = *rtcStats.total_decode_time;
    if (rtcStats.total_inter_frame_delay.is_defined())
        stats.totalInterFrameDelay = *rtcStats.total_inter_frame_delay;
    if (rtcStats.total_squared_inter_frame_delay.is_defined())
        stats.totalSquaredInterFrameDelay = *rtcStats.total_squared_inter_frame_delay;
    if (rtcStats.pause_count.is_defined())
        stats.pauseCount = *rtcStats.pause_count;
    if (rtcStats.total_pauses_duration.is_defined())
        stats.totalPausesDuration = *rtcStats.total_pauses_duration;
    if (rtcStats.freeze_count.is_defined())
        stats.freezeCount = *rtcStats.freeze_count;
    if (rtcStats.total_freezes_duration.is_defined())
        stats.totalFreezesDuration = *rtcStats.total_freezes_duration;
    if (rtcStats.last_packet_received_timestamp.is_defined())
        stats.lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;

    if (rtcStats.fec_packets_received.is_defined())
        stats.fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_bytes_received.is_defined())
        stats.fecBytesReceived = *rtcStats.fec_bytes_received;
    if (rtcStats.fec_packets_discarded.is_defined())
        stats.fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.fec_ssrc.is_defined())
        stats.fecSsrc = *rtcStats.fec_ssrc;
    if (rtcStats.header_bytes_received.is_defined())
        stats.headerBytesReceived = *rtcStats.header_bytes_received;
    if (rtcStats.rtx_ssrc.is_defined())
        stats.rtxSsrc = *rtcStats.rtx_ssrc;
    if (rtcStats.packets_discarded.is_defined())
        stats.packetsDiscarded = *rtcStats.packets_discarded;
    if (rtcStats.fec_packets_received.is_defined())
        stats.fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_packets_discarded.is_defined())
        stats.fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.total_processing_delay.is_defined())
        stats.totalProcessingDelay = *rtcStats.total_processing_delay;
    if (rtcStats.estimated_playout_timestamp.is_defined())
        stats.estimatedPlayoutTimestamp = *rtcStats.estimated_playout_timestamp;
    if (rtcStats.jitter_buffer_delay.is_defined())
        stats.jitterBufferDelay = *rtcStats.jitter_buffer_delay;
    if (rtcStats.jitter_buffer_target_delay.is_defined())
        stats.jitterBufferTargetDelay = *rtcStats.jitter_buffer_target_delay;
    if (rtcStats.jitter_buffer_emitted_count.is_defined())
        stats.jitterBufferEmittedCount = *rtcStats.jitter_buffer_emitted_count;
    if (rtcStats.jitter_buffer_minimum_delay.is_defined())
        stats.jitterBufferMinimumDelay = *rtcStats.jitter_buffer_minimum_delay;
    if (rtcStats.total_samples_received.is_defined())
        stats.totalSamplesReceived = *rtcStats.total_samples_received;
    if (rtcStats.concealed_samples.is_defined())
        stats.concealedSamples = *rtcStats.concealed_samples;
    if (rtcStats.silent_concealed_samples.is_defined())
        stats.silentConcealedSamples = *rtcStats.silent_concealed_samples;
    if (rtcStats.concealment_events.is_defined())
        stats.concealmentEvents = *rtcStats.concealment_events;
    if (rtcStats.inserted_samples_for_deceleration.is_defined())
        stats.insertedSamplesForDeceleration = *rtcStats.inserted_samples_for_deceleration;
    if (rtcStats.removed_samples_for_acceleration.is_defined())
        stats.removedSamplesForAcceleration = *rtcStats.removed_samples_for_acceleration;
    if (rtcStats.audio_level.is_defined())
        stats.audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy.is_defined())
        stats.totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration.is_defined())
        stats.totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.frames_received.is_defined())
        stats.framesReceived = *rtcStats.frames_received;
    // TODO: Restrict Access
    // if (rtcStats.decoder_implementation.is_defined())
    //     stats.decoderImplementation = fromStdString(*rtcStats.decoder_implementation);
    if (rtcStats.playout_id.is_defined())
        stats.playoutId = fromStdString(*rtcStats.playout_id);
    // TODO: Restrict Access
    // if (rtcStats.power_efficient_decoder.is_defined())
    //     stats.powerEfficientDecoder = *rtcStats.power_efficient_decoder;
    if (rtcStats.frames_assembled_from_multiple_packets.is_defined())
        stats.framesAssembledFromMultiplePackets = *rtcStats.frames_assembled_from_multiple_packets;
    if (rtcStats.total_assembly_time.is_defined())
        stats.totalAssemblyTime = *rtcStats.total_assembly_time;
    if (rtcStats.retransmitted_packets_received.is_defined())
        stats.retransmittedPacketsReceived = *rtcStats.retransmitted_packets_received;
    if (rtcStats.retransmitted_bytes_received.is_defined())
        stats.retransmittedBytesReceived = *rtcStats.retransmitted_bytes_received;
}

static inline void fillRemoteInboundRtpStreamStats(RTCStatsReport::RemoteInboundRtpStreamStats& stats, const webrtc::RTCRemoteInboundRtpStreamStats& rtcStats)
{
    fillReceivedRtpStreamStats(stats, rtcStats);

    if (rtcStats.local_id.is_defined())
        stats.localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.round_trip_time.is_defined())
        stats.roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time.is_defined())
        stats.totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.fraction_lost.is_defined())
        stats.fractionLost = *rtcStats.fraction_lost;
    if (rtcStats.round_trip_time_measurements.is_defined())
        stats.roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

static inline void fillSentRtpStreamStats(RTCStatsReport::SentRtpStreamStats& stats, const webrtc::RTCSentRtpStreamStats& rtcStats)
{
    fillRtpStreamStats(stats, rtcStats);

    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
}

static inline std::optional<RTCStatsReport::QualityLimitationReason> qualityLimitationReason(const std::string& reason)
{
    if (reason == "none")
        return RTCStatsReport::QualityLimitationReason::None;
    if (reason == "cpu")
        return RTCStatsReport::QualityLimitationReason::Cpu;
    if (reason == "bandwidth")
        return RTCStatsReport::QualityLimitationReason::Bandwidth;
    return RTCStatsReport::QualityLimitationReason::Other;
}

static inline void fillOutboundRtpStreamStats(RTCStatsReport::OutboundRtpStreamStats& stats, const webrtc::RTCOutboundRtpStreamStats& rtcStats)
{
    fillSentRtpStreamStats(stats, rtcStats);

    if (rtcStats.mid.is_defined())
        stats.mid = fromStdString(*rtcStats.mid);
    if (rtcStats.media_source_id.is_defined())
        stats.mediaSourceId = fromStdString(*rtcStats.media_source_id);
    if (rtcStats.remote_id.is_defined())
        stats.remoteId = fromStdString(*rtcStats.remote_id);
    if (rtcStats.rid.is_defined())
        stats.rid = fromStdString(*rtcStats.rid);
    if (rtcStats.header_bytes_sent.is_defined())
        stats.headerBytesSent = *rtcStats.header_bytes_sent;
    if (rtcStats.retransmitted_packets_sent.is_defined())
        stats.retransmittedPacketsSent = *rtcStats.retransmitted_packets_sent;
    if (rtcStats.retransmitted_bytes_sent.is_defined())
        stats.retransmittedBytesSent = *rtcStats.retransmitted_bytes_sent;
    if (rtcStats.target_bitrate.is_defined())
        stats.targetBitrate = *rtcStats.target_bitrate;
    if (rtcStats.total_encoded_bytes_target.is_defined())
        stats.totalEncodedBytesTarget = *rtcStats.total_encoded_bytes_target;
    if (rtcStats.frame_width.is_defined())
        stats.frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        stats.frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        stats.framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.frames_sent.is_defined())
        stats.framesSent = *rtcStats.frames_sent;
    if (rtcStats.huge_frames_sent.is_defined())
        stats.hugeFramesSent = *rtcStats.huge_frames_sent;
    if (rtcStats.frames_encoded.is_defined())
        stats.framesEncoded = *rtcStats.frames_encoded;
    if (rtcStats.key_frames_encoded.is_defined())
        stats.keyFramesEncoded = *rtcStats.key_frames_encoded;
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_encode_time.is_defined())
        stats.totalEncodeTime = *rtcStats.total_encode_time;
    if (rtcStats.total_packet_send_delay.is_defined())
        stats.totalPacketSendDelay = *rtcStats.total_packet_send_delay;
    if (rtcStats.quality_limitation_reason.is_defined())
        stats.qualityLimitationReason = qualityLimitationReason(*rtcStats.quality_limitation_reason);
    if (rtcStats.quality_limitation_durations.is_defined()) {
        auto& durations = *rtcStats.quality_limitation_durations;
        auto it = durations.begin();
        stats.qualityLimitationDurations = Vector<KeyValuePair<String, double>>(durations.size(), [&] (size_t) {
            ASSERT(it != durations.end());
            KeyValuePair<String, double> element = { fromStdString(it->first), it->second };
            ++it;
            return element;
        });
    }
    if (rtcStats.quality_limitation_resolution_changes.is_defined())
        stats.qualityLimitationResolutionChanges = *rtcStats.quality_limitation_resolution_changes;
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;

    if (rtcStats.active.is_defined())
        stats.active = *rtcStats.active;
    if (rtcStats.scalability_mode.is_defined())
        stats.scalabilityMode = fromStdString(*rtcStats.scalability_mode);
    if (rtcStats.rtx_ssrc.is_defined())
        stats.rtxSsrc = *rtcStats.rtx_ssrc;
}


static inline void fillRemoteOutboundRtpStreamStats(RTCStatsReport::RemoteOutboundRtpStreamStats& stats, const webrtc::RTCRemoteOutboundRtpStreamStats& rtcStats)
{
    fillSentRtpStreamStats(stats, rtcStats);

    if (rtcStats.local_id.is_defined())
        stats.localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.remote_timestamp.is_defined())
        stats.remoteTimestamp = *rtcStats.remote_timestamp;
    if (rtcStats.reports_sent.is_defined())
        stats.reportsSent = *rtcStats.reports_sent;
    if (rtcStats.round_trip_time.is_defined())
        stats.roundTripTime = *rtcStats.round_trip_time;
    if (rtcStats.total_round_trip_time.is_defined())
        stats.totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.round_trip_time_measurements.is_defined())
        stats.roundTripTimeMeasurements = *rtcStats.round_trip_time_measurements;
}

static inline void fillRTCDataChannelStats(RTCStatsReport::DataChannelStats& stats, const webrtc::RTCDataChannelStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.label.is_defined())
        stats.label = fromStdString(*rtcStats.label);
    if (rtcStats.protocol.is_defined())
        stats.protocol = fromStdString(*rtcStats.protocol);
    if (rtcStats.data_channel_identifier.is_defined())
        stats.dataChannelIdentifier = *rtcStats.data_channel_identifier;
    if (rtcStats.state.is_defined())
        stats.state = fromStdString(*rtcStats.state);
    if (rtcStats.messages_sent.is_defined())
        stats.messagesSent = *rtcStats.messages_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.messages_received.is_defined())
        stats.messagesReceived = *rtcStats.messages_received;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
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

static inline void fillRTCIceCandidatePairStats(RTCStatsReport::IceCandidatePairStats& stats, const webrtc::RTCIceCandidatePairStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.local_candidate_id.is_defined())
        stats.localCandidateId = fromStdString(*rtcStats.local_candidate_id);
    if (rtcStats.remote_candidate_id.is_defined())
        stats.remoteCandidateId = fromStdString(*rtcStats.remote_candidate_id);
    if (rtcStats.state.is_defined())
        stats.state = iceCandidatePairState(*rtcStats.state);
    if (rtcStats.nominated.is_defined())
        stats.nominated = *rtcStats.nominated;
    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.last_packet_sent_timestamp.is_defined())
        stats.lastPacketSentTimestamp = *rtcStats.last_packet_sent_timestamp;
    if (rtcStats.last_packet_received_timestamp.is_defined())
        stats.lastPacketReceivedTimestamp = *rtcStats.last_packet_received_timestamp;
    if (rtcStats.total_round_trip_time.is_defined())
        stats.totalRoundTripTime = *rtcStats.total_round_trip_time;
    if (rtcStats.current_round_trip_time.is_defined())
        stats.currentRoundTripTime = *rtcStats.current_round_trip_time;
    if (rtcStats.available_outgoing_bitrate.is_defined())
        stats.availableOutgoingBitrate = *rtcStats.available_outgoing_bitrate;
    if (rtcStats.available_incoming_bitrate.is_defined())
        stats.availableIncomingBitrate = *rtcStats.available_incoming_bitrate;
    if (rtcStats.requests_received.is_defined())
        stats.requestsReceived = *rtcStats.requests_received;
    if (rtcStats.requests_sent.is_defined())
        stats.requestsSent = *rtcStats.requests_sent;
    if (rtcStats.responses_received.is_defined())
        stats.responsesReceived = *rtcStats.responses_received;
    if (rtcStats.responses_sent.is_defined())
        stats.responsesSent = *rtcStats.responses_sent;

    if (rtcStats.consent_requests_sent.is_defined())
        stats.consentRequestsSent = *rtcStats.consent_requests_sent;
    if (rtcStats.packets_discarded_on_send.is_defined())
        stats.packetsDiscardedOnSend = *rtcStats.packets_discarded_on_send;
    if (rtcStats.bytes_discarded_on_send.is_defined())
        stats.bytesDiscardedOnSend = *rtcStats.bytes_discarded_on_send;
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

static inline void fillRTCIceCandidateStats(RTCStatsReport::IceCandidateStats& stats, const webrtc::RTCIceCandidateStats& rtcStats)
{
    stats.type = rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType ? RTCStatsReport::Type::RemoteCandidate : RTCStatsReport::Type::LocalCandidate;

    fillRTCStats(stats, rtcStats);

    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.ip.is_defined())
        stats.address = fromStdString(*rtcStats.ip);
    if (rtcStats.port.is_defined())
        stats.port = *rtcStats.port;
    if (rtcStats.protocol.is_defined())
        stats.protocol = fromStdString(*rtcStats.protocol);
    ASSERT(rtcStats.candidate_type.is_defined());
    if (rtcStats.candidate_type.is_defined())
        stats.candidateType = iceCandidateState(*rtcStats.candidate_type);
    if (stats.candidateType == RTCIceCandidateType::Prflx || stats.candidateType == RTCIceCandidateType::Host)
        stats.address = { };

    if (rtcStats.priority.is_defined())
        stats.priority = *rtcStats.priority;
    if (rtcStats.url.is_defined())
        stats.url = fromStdString(*rtcStats.url);
    if (rtcStats.foundation.is_defined())
        stats.foundation = fromStdString(*rtcStats.foundation);
    if (rtcStats.username_fragment.is_defined())
        stats.usernameFragment = fromStdString(*rtcStats.username_fragment);
    if (rtcStats.tcp_type.is_defined()) {
        if (auto tcpType = parseEnumerationFromString<RTCIceTcpCandidateType>(fromStdString(*rtcStats.tcp_type)))
            stats.tcpType = *tcpType;
    }
}

static inline void fillRTCCertificateStats(RTCStatsReport::CertificateStats& stats, const webrtc::RTCCertificateStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.fingerprint.is_defined())
        stats.fingerprint = fromStdString(*rtcStats.fingerprint);
    if (rtcStats.fingerprint_algorithm.is_defined())
        stats.fingerprintAlgorithm = fromStdString(*rtcStats.fingerprint_algorithm);
    if (rtcStats.base64_certificate.is_defined())
        stats.base64Certificate = fromStdString(*rtcStats.base64_certificate);
    if (rtcStats.issuer_certificate_id.is_defined())
        stats.issuerCertificateId = fromStdString(*rtcStats.issuer_certificate_id);
}

static inline void fillRTCCodecStats(RTCStatsReport::CodecStats& stats, const webrtc::RTCCodecStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.payload_type.is_defined())
        stats.payloadType = *rtcStats.payload_type;
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.mime_type.is_defined())
        stats.mimeType = fromStdString(*rtcStats.mime_type);
    if (rtcStats.clock_rate.is_defined())
        stats.clockRate = *rtcStats.clock_rate;
    if (rtcStats.channels.is_defined())
        stats.channels = *rtcStats.channels;
    if (rtcStats.sdp_fmtp_line.is_defined())
        stats.sdpFmtpLine = fromStdString(*rtcStats.sdp_fmtp_line);
}

static inline std::optional<RTCIceRole> iceRole(const std::string& state)
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

static inline std::optional<RTCStatsReport::DtlsRole> dtlsRole(const std::string& state)
{
    if (state == "client")
        return RTCStatsReport::DtlsRole::Client;
    if (state == "server")
        return RTCStatsReport::DtlsRole::Server;
    if (state == "unknown")
        return RTCStatsReport::DtlsRole::Unknown;

    return { };
}

static inline void fillRTCTransportStats(RTCStatsReport::TransportStats& stats, const webrtc::RTCTransportStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.ice_role.is_defined())
        stats.iceRole = iceRole(*rtcStats.ice_role);
    if (rtcStats.ice_local_username_fragment.is_defined())
        stats.iceLocalUsernameFragment = fromStdString(*rtcStats.ice_local_username_fragment);
    if (rtcStats.dtls_state.is_defined())
        stats.dtlsState = *dtlsTransportState(*rtcStats.dtls_state);
    if (rtcStats.ice_state.is_defined())
        stats.iceState = iceTransportState(*rtcStats.ice_state);
    if (rtcStats.selected_candidate_pair_id.is_defined())
        stats.selectedCandidatePairId = fromStdString(*rtcStats.selected_candidate_pair_id);
    if (rtcStats.local_certificate_id.is_defined())
        stats.localCertificateId = fromStdString(*rtcStats.local_certificate_id);
    if (rtcStats.remote_certificate_id.is_defined())
        stats.remoteCertificateId = fromStdString(*rtcStats.remote_certificate_id);
    if (rtcStats.tls_version.is_defined())
        stats.tlsVersion = fromStdString(*rtcStats.tls_version);
    if (rtcStats.dtls_cipher.is_defined())
        stats.dtlsCipher = fromStdString(*rtcStats.dtls_cipher);
    if (rtcStats.dtls_role.is_defined())
        stats.dtlsRole = dtlsRole(*rtcStats.dtls_role);
    if (rtcStats.srtp_cipher.is_defined())
        stats.srtpCipher = fromStdString(*rtcStats.srtp_cipher);
    if (rtcStats.selected_candidate_pair_changes.is_defined())
        stats.selectedCandidatePairChanges = *rtcStats.selected_candidate_pair_changes;
}

static inline void fillRTCPeerConnectionStats(RTCStatsReport::PeerConnectionStats& stats, const webrtc::RTCPeerConnectionStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.data_channels_opened.is_defined())
        stats.dataChannelsOpened = *rtcStats.data_channels_opened;
    if (rtcStats.data_channels_closed.is_defined())
        stats.dataChannelsClosed = *rtcStats.data_channels_closed;
}

static inline void fillRTCMediaSourceStats(RTCStatsReport::MediaSourceStats& stats, const webrtc::RTCMediaSourceStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);
    if (rtcStats.track_identifier.is_defined())
        stats.trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.kind.is_defined())
        stats.kind = fromStdString(*rtcStats.kind);
}

static inline void fillRTCAudioSourceStats(RTCStatsReport::AudioSourceStats& stats, const webrtc::RTCAudioSourceStats& rtcStats)
{
    fillRTCMediaSourceStats(stats, rtcStats);
    if (rtcStats.audio_level.is_defined())
        stats.audioLevel = *rtcStats.audio_level;
    if (rtcStats.total_audio_energy.is_defined())
        stats.totalAudioEnergy = *rtcStats.total_audio_energy;
    if (rtcStats.total_samples_duration.is_defined())
        stats.totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.echo_return_loss.is_defined())
        stats.echoReturnLoss = *rtcStats.echo_return_loss;
    if (rtcStats.echo_return_loss_enhancement.is_defined())
        stats.echoReturnLossEnhancement = *rtcStats.echo_return_loss_enhancement;
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

static inline void fillRTCAudioPlayoutStats(RTCStatsReport::AudioPlayoutStats& stats, const webrtc::RTCAudioPlayoutStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.kind.is_defined())
        stats.kind = fromStdString(*rtcStats.kind);
    if (rtcStats.synthesized_samples_duration.is_defined())
        stats.synthesizedSamplesDuration = *rtcStats.synthesized_samples_duration;
    if (rtcStats.synthesized_samples_events.is_defined())
        stats.synthesizedSamplesEvents = *rtcStats.synthesized_samples_events;
    if (rtcStats.total_samples_duration.is_defined())
        stats.totalSamplesDuration = *rtcStats.total_samples_duration;
    if (rtcStats.total_playout_delay.is_defined())
        stats.totalPlayoutDelay = *rtcStats.total_playout_delay;
    if (rtcStats.total_samples_count.is_defined())
        stats.totalSamplesCount = *rtcStats.total_samples_count;
}

static inline void fillRTCVideoSourceStats(RTCStatsReport::VideoSourceStats& stats, const webrtc::RTCVideoSourceStats& rtcStats)
{
    fillRTCMediaSourceStats(stats, rtcStats);

    if (rtcStats.width.is_defined())
        stats.width = *rtcStats.width;
    if (rtcStats.height.is_defined())
        stats.height = *rtcStats.height;
    if (rtcStats.frames.is_defined())
        stats.frames = *rtcStats.frames;
    if (rtcStats.frames_per_second.is_defined())
        stats.framesPerSecond = *rtcStats.frames_per_second;
}

static inline void initializeRTCStatsReportBackingMap(DOMMapAdapter& report, const webrtc::RTCStatsReport& rtcReport)
{
    for (const auto& rtcStats : rtcReport) {
        if (rtcStats.type() == webrtc::RTCInboundRtpStreamStats::kType) {
            RTCStatsReport::InboundRtpStreamStats stats;
            fillInboundRtpStreamStats(stats, static_cast<const webrtc::RTCInboundRtpStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCOutboundRtpStreamStats::kType) {
            RTCStatsReport::OutboundRtpStreamStats stats;
            fillOutboundRtpStreamStats(stats, static_cast<const webrtc::RTCOutboundRtpStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCDataChannelStats::kType) {
            RTCStatsReport::DataChannelStats stats;
            fillRTCDataChannelStats(stats, static_cast<const webrtc::RTCDataChannelStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::DataChannelStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCIceCandidatePairStats::kType) {
            RTCStatsReport::IceCandidatePairStats stats;
            fillRTCIceCandidatePairStats(stats, static_cast<const webrtc::RTCIceCandidatePairStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidatePairStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType || rtcStats.type() == webrtc::RTCLocalIceCandidateStats::kType) {
            RTCStatsReport::IceCandidateStats stats;
            fillRTCIceCandidateStats(stats, static_cast<const webrtc::RTCIceCandidateStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::IceCandidateStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCCertificateStats::kType) {
            RTCStatsReport::CertificateStats stats;
            fillRTCCertificateStats(stats, static_cast<const webrtc::RTCCertificateStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CertificateStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCCodecStats::kType) {
            RTCStatsReport::CodecStats stats;
            fillRTCCodecStats(stats, static_cast<const webrtc::RTCCodecStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::CodecStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCTransportStats::kType) {
            RTCStatsReport::TransportStats stats;
            fillRTCTransportStats(stats, static_cast<const webrtc::RTCTransportStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::TransportStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCPeerConnectionStats::kType) {
            RTCStatsReport::PeerConnectionStats stats;
            fillRTCPeerConnectionStats(stats, static_cast<const webrtc::RTCPeerConnectionStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::PeerConnectionStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCAudioSourceStats::kType) {
            RTCStatsReport::AudioSourceStats stats;
            fillRTCAudioSourceStats(stats, static_cast<const webrtc::RTCAudioSourceStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::AudioSourceStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCVideoSourceStats::kType) {
            RTCStatsReport::VideoSourceStats stats;
            fillRTCVideoSourceStats(stats, static_cast<const webrtc::RTCVideoSourceStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::VideoSourceStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCRemoteInboundRtpStreamStats::kType) {
            RTCStatsReport::RemoteInboundRtpStreamStats stats;
            fillRemoteInboundRtpStreamStats(stats, static_cast<const webrtc::RTCRemoteInboundRtpStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteInboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCRemoteOutboundRtpStreamStats::kType) {
            RTCStatsReport::RemoteOutboundRtpStreamStats stats;
            fillRemoteOutboundRtpStreamStats(stats, static_cast<const webrtc::RTCRemoteOutboundRtpStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::RemoteOutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCAudioPlayoutStats::kType) {
            RTCStatsReport::AudioPlayoutStats stats;
            fillRTCAudioPlayoutStats(stats, static_cast<const webrtc::RTCAudioPlayoutStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::AudioPlayoutStats>>(stats.id, WTFMove(stats));
        }
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
