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

#if USE(LIBWEBRTC)

#include "JSDOMMapLike.h"
#include "JSRTCStatsReport.h"
#include "Performance.h"
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

static inline String fromStdString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

static inline void fillRTCStats(RTCStatsReport::Stats& stats, const webrtc::RTCStats& rtcStats)
{
    stats.timestamp = Performance::reduceTimeResolution(Seconds::fromMicroseconds(rtcStats.timestamp_us())).milliseconds();
    stats.id = fromStdString(rtcStats.id());
}

static inline void fillRtpStreamStats(RTCStatsReport::RtpStreamStats& stats, const webrtc::RTCRTPStreamStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.ssrc.is_defined())
        stats.ssrc = *rtcStats.ssrc;
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        stats.codecId = fromStdString(*rtcStats.codec_id);
    if (rtcStats.media_type.is_defined()) {
        stats.mediaType = fromStdString(*rtcStats.media_type);
        stats.kind = stats.mediaType;
    }
}

static inline void fillReceivedRtpStreamStats(RTCStatsReport::ReceivedRtpStreamStats& stats, const webrtc::RTCInboundRTPStreamStats& rtcStats)
{
    fillRtpStreamStats(stats, rtcStats);

    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.packets_lost.is_defined())
        stats.packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter.is_defined())
        stats.jitter = *rtcStats.jitter;
    if (rtcStats.packets_discarded.is_defined())
        stats.packetsDiscarded = *rtcStats.packets_discarded;
    if (rtcStats.packets_repaired.is_defined())
        stats.packetsRepaired = *rtcStats.packets_repaired;
    if (rtcStats.burst_packets_lost.is_defined())
        stats.burstPacketsLost = *rtcStats.burst_packets_lost;
    if (rtcStats.burst_packets_discarded.is_defined())
        stats.burstPacketsDiscarded = *rtcStats.burst_packets_discarded;
    if (rtcStats.burst_loss_count.is_defined())
        stats.burstLossCount = *rtcStats.burst_loss_count;
    if (rtcStats.burst_discard_count.is_defined())
        stats.burstDiscardCount = *rtcStats.burst_discard_count;
    if (rtcStats.burst_loss_rate.is_defined())
        stats.burstLossRate = *rtcStats.burst_loss_rate;
    if (rtcStats.burst_discard_rate.is_defined())
        stats.burstDiscardRate = *rtcStats.burst_discard_rate;
    if (rtcStats.gap_loss_rate.is_defined())
        stats.gapLossRate = *rtcStats.gap_loss_rate;
    if (rtcStats.gap_discard_rate.is_defined())
        stats.gapDiscardRate = *rtcStats.gap_discard_rate;
    // Add frames_dropped and full_frames_lost.
}

static inline void fillInboundRtpStreamStats(RTCStatsReport::InboundRtpStreamStats& stats, const webrtc::RTCInboundRTPStreamStats& rtcStats)
{
    fillReceivedRtpStreamStats(stats, rtcStats);

    // receiverId
    // remoteId
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.key_frames_decoded.is_defined())
        stats.keyFramesDecoded = *rtcStats.key_frames_decoded;
    // frameWidth, frameHeight, frameBitDepth, framesPerSecond
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
    if (rtcStats.total_decode_time.is_defined())
        stats.totalDecodeTime = *rtcStats.total_decode_time;
    if (rtcStats.total_inter_frame_delay.is_defined())
        stats.totalInterFrameDelay = *rtcStats.total_inter_frame_delay;
    if (rtcStats.total_squared_inter_frame_delay.is_defined())
        stats.totalSquaredInterFrameDelay = *rtcStats.total_squared_inter_frame_delay;
    // voiceActivityFlag, lastPacketReceivedTimestamp, averageRtcpInterval
    if (rtcStats.header_bytes_received.is_defined())
        stats.headerBytesReceived = *rtcStats.header_bytes_received;
    if (rtcStats.fec_packets_received.is_defined())
        stats.fecPacketsReceived = *rtcStats.fec_packets_received;
    if (rtcStats.fec_packets_discarded.is_defined())
        stats.fecPacketsDiscarded = *rtcStats.fec_packets_discarded;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    // packetsFailedDecryption, packetsDuplicated
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.sli_count.is_defined())
        stats.sliCount = *rtcStats.sli_count;
    // estimatedPlayoutTimestamp, jitterBufferDelay, jitterBufferEmittedCount, totalSamplesReceived, samplesDecodedWithSilk, samplesDecodedWithCelt
    // concealedSamples, silentConcealedSamples, concealmentEvents, insertedSamplesForDeceleration, removedSamplesForAcceleration
    // audioLevel, totalAudioEnergy, totalSamplesDuration, framesReceived

    if (rtcStats.track_id.is_defined())
        stats.trackId = fromStdString(*rtcStats.track_id);
}

static inline void fillRemoteInboundRtpStreamStats(RTCStatsReport::RemoteInboundRtpStreamStats& stats, const webrtc::RTCRemoteInboundRtpStreamStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    // FIXME: this should be filled in fillRtpStreamStats.
    if (rtcStats.ssrc.is_defined())
        stats.ssrc = *rtcStats.ssrc;
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        stats.codecId = fromStdString(*rtcStats.codec_id);
    if (rtcStats.kind.is_defined())
        stats.kind = stats.kind;

    if (rtcStats.local_id.is_defined())
        stats.localId = fromStdString(*rtcStats.local_id);
    if (rtcStats.round_trip_time.is_defined())
        stats.roundTripTime = *rtcStats.round_trip_time;

    // totalRoundTripTime, fractionLost, reportsReceived, roundTripTimeMeasurements
}

static inline void fillSentRtpStreamStats(RTCStatsReport::SentRtpStreamStats& stats, const webrtc::RTCOutboundRTPStreamStats& rtcStats)
{
    fillRtpStreamStats(stats, rtcStats);

    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
}

static inline void fillOutboundRtpStreamStats(RTCStatsReport::OutboundRtpStreamStats& stats, const webrtc::RTCOutboundRTPStreamStats& rtcStats)
{
    fillSentRtpStreamStats(stats, rtcStats);

    // rtxSsrc
    if (rtcStats.media_source_id.is_defined())
        stats.mediaSourceId = fromStdString(*rtcStats.media_source_id);
    // senderId
    if (rtcStats.remote_id.is_defined())
        stats.remoteId = fromStdString(*rtcStats.remote_id);
    // rid, packetsDiscardedOnSend
    if (rtcStats.header_bytes_sent.is_defined())
        stats.headerBytesSent = *rtcStats.header_bytes_sent;
    // packetsDiscardedOnSend, bytesDiscardedOnSend, fecPacketsSent;
    if (rtcStats.retransmitted_packets_sent.is_defined())
        stats.retransmittedPacketsSent = *rtcStats.retransmitted_packets_sent;
    if (rtcStats.retransmitted_bytes_sent.is_defined())
        stats.retransmittedBytesSent = *rtcStats.retransmitted_bytes_sent;
    if (rtcStats.target_bitrate.is_defined())
        stats.targetBitrate = *rtcStats.target_bitrate;
    // totalEncodedBytesTarget, frameWidth, frameHeight, frameBitDepth, framesPerSecond, framesSent, hugeFramesSent
    if (rtcStats.frames_encoded.is_defined())
        stats.framesEncoded = *rtcStats.frames_encoded;
    if (rtcStats.key_frames_encoded.is_defined())
        stats.keyFramesEncoded = *rtcStats.key_frames_encoded;
    // framesDiscardedOnSend;
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
    // totalEncodedBytesTarget, frameWidth, frameHeight, frameBitDepth, framesPerSecond, framesSent, hugeFramesSent
    // totalSamplesSent, samplesEncodedWithSilk, samplesEncodedWithCelt, voiceActivityFlag, totalEncodeTime
    if (rtcStats.total_packet_send_delay.is_defined())
        stats.totalPacketSendDelay = *rtcStats.total_packet_send_delay;
    // averageRtcpInterval, qualityLimitationResolutionChanges
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;
    if (rtcStats.sli_count.is_defined())
        stats.sliCount = *rtcStats.sli_count;

    if (rtcStats.track_id.is_defined())
        stats.trackId = fromStdString(*rtcStats.track_id);
}

static inline void fillRTCMediaStreamTrackStats(RTCStatsReport::MediaStreamTrackStats& stats, const webrtc::RTCMediaStreamTrackStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.track_identifier.is_defined())
        stats.trackIdentifier = fromStdString(*rtcStats.track_identifier);
    if (rtcStats.remote_source.is_defined())
        stats.remoteSource = *rtcStats.remote_source;
    if (rtcStats.ended.is_defined())
        stats.ended = *rtcStats.ended;
    if (rtcStats.detached.is_defined())
        stats.detached = *rtcStats.detached;
    if (rtcStats.frame_width.is_defined())
        stats.frameWidth = *rtcStats.frame_width;
    if (rtcStats.frame_height.is_defined())
        stats.frameHeight = *rtcStats.frame_height;
    if (rtcStats.frames_per_second.is_defined())
        stats.framesPerSecond = *rtcStats.frames_per_second;
    if (rtcStats.frames_sent.is_defined())
        stats.framesSent = *rtcStats.frames_sent;
    if (rtcStats.frames_received.is_defined())
        stats.framesReceived = *rtcStats.frames_received;
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
    if (rtcStats.frames_dropped.is_defined())
        stats.framesDropped = *rtcStats.frames_dropped;
    if (rtcStats.partial_frames_lost.is_defined())
        stats.partialFramesLost = *rtcStats.partial_frames_lost;
    if (rtcStats.full_frames_lost.is_defined())
        stats.fullFramesLost = *rtcStats.full_frames_lost;
    if (rtcStats.audio_level.is_defined())
        stats.audioLevel = *rtcStats.audio_level;
    if (rtcStats.echo_return_loss.is_defined())
        stats.echoReturnLoss = *rtcStats.echo_return_loss;
    if (rtcStats.echo_return_loss_enhancement.is_defined())
        stats.echoReturnLossEnhancement = *rtcStats.echo_return_loss_enhancement;
}

static inline void fillRTCDataChannelStats(RTCStatsReport::DataChannelStats& stats, const webrtc::RTCDataChannelStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.label.is_defined())
        stats.label = fromStdString(*rtcStats.label);
    if (rtcStats.protocol.is_defined())
        stats.protocol = fromStdString(*rtcStats.protocol);
    if (rtcStats.datachannelid.is_defined())
        stats.datachannelid = *rtcStats.datachannelid;
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
        return RTCStatsReport::IceCandidatePairState::Inprogress;
    if (state == "failed")
        return RTCStatsReport::IceCandidatePairState::Failed;
    if (state == "succeeded")
        return RTCStatsReport::IceCandidatePairState::Succeeded;
    if (state == "cancelled")
        return RTCStatsReport::IceCandidatePairState::Cancelled;
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

    if (rtcStats.priority.is_defined())
        stats.priority = *rtcStats.priority;
    if (rtcStats.nominated.is_defined())
        stats.nominated = *rtcStats.nominated;
    if (rtcStats.writable.is_defined())
        stats.writable = *rtcStats.writable;
    if (rtcStats.readable.is_defined())
        stats.readable = *rtcStats.readable;

    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
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

    if (rtcStats.requests_received.is_defined())
        stats.retransmissionsReceived = *rtcStats.requests_received;
    if (rtcStats.requests_sent.is_defined())
        stats.retransmissionsSent = *rtcStats.requests_sent;
    if (rtcStats.responses_received.is_defined())
        stats.consentRequestsReceived = *rtcStats.responses_received;
    if (rtcStats.responses_sent.is_defined())
        stats.consentRequestsSent = *rtcStats.responses_sent;
    if (rtcStats.responses_received.is_defined())
        stats.consentResponsesReceived = *rtcStats.responses_received;
    if (rtcStats.responses_sent.is_defined())
        stats.consentResponsesSent = *rtcStats.responses_sent;
}

static inline Optional<RTCStatsReport::IceCandidateType> iceCandidateState(const std::string& state)
{
    if (state == "host")
        return RTCStatsReport::IceCandidateType::Host;
    if (state == "srflx")
        return RTCStatsReport::IceCandidateType::Srflx;
    if (state == "prflx")
        return RTCStatsReport::IceCandidateType::Prflx;
    if (state == "relay")
        return RTCStatsReport::IceCandidateType::Relay;

    return { };
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

    if (rtcStats.candidate_type.is_defined())
        stats.candidateType = iceCandidateState(*rtcStats.candidate_type);

    if (!stats.candidateType || stats.candidateType == RTCStatsReport::IceCandidateType::Prflx || stats.candidateType == RTCStatsReport::IceCandidateType::Host)
        stats.address = { };

    if (rtcStats.priority.is_defined())
        stats.priority = *rtcStats.priority;
    if (rtcStats.url.is_defined())
        stats.url = fromStdString(*rtcStats.url);
    if (rtcStats.deleted.is_defined())
        stats.deleted = *rtcStats.deleted;
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
    if (rtcStats.mime_type.is_defined())
        stats.mimeType = fromStdString(*rtcStats.mime_type);
    if (rtcStats.clock_rate.is_defined())
        stats.clockRate = *rtcStats.clock_rate;
    if (rtcStats.channels.is_defined())
        stats.channels = *rtcStats.channels;
    if (rtcStats.sdp_fmtp_line.is_defined())
        stats.sdpFmtpLine = fromStdString(*rtcStats.sdp_fmtp_line);
}

static inline void fillRTCTransportStats(RTCStatsReport::TransportStats& stats, const webrtc::RTCTransportStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.rtcp_transport_stats_id.is_defined())
        stats.rtcpTransportStatsId = fromStdString(*rtcStats.rtcp_transport_stats_id);
    if (rtcStats.selected_candidate_pair_id.is_defined())
        stats.selectedCandidatePairId = fromStdString(*rtcStats.selected_candidate_pair_id);
    if (rtcStats.local_certificate_id.is_defined())
        stats.localCertificateId = fromStdString(*rtcStats.local_certificate_id);
    if (rtcStats.remote_certificate_id.is_defined())
        stats.remoteCertificateId = fromStdString(*rtcStats.remote_certificate_id);
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
        if (rtcStats.type() == webrtc::RTCInboundRTPStreamStats::kType) {
            RTCStatsReport::InboundRtpStreamStats stats;
            fillInboundRtpStreamStats(stats, static_cast<const webrtc::RTCInboundRTPStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::InboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCOutboundRTPStreamStats::kType) {
            RTCStatsReport::OutboundRtpStreamStats stats;
            fillOutboundRtpStreamStats(stats, static_cast<const webrtc::RTCOutboundRTPStreamStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::OutboundRtpStreamStats>>(stats.id, WTFMove(stats));
        } else if (rtcStats.type() == webrtc::RTCMediaStreamTrackStats::kType) {
            RTCStatsReport::MediaStreamTrackStats stats;
            fillRTCMediaStreamTrackStats(stats, static_cast<const webrtc::RTCMediaStreamTrackStats&>(rtcStats));
            report.set<IDLDOMString, IDLDictionary<RTCStatsReport::MediaStreamTrackStats>>(stats.id, WTFMove(stats));
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
        }
    }
}

void LibWebRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    callOnMainThread([this, protectedThis = rtc::scoped_refptr<LibWebRTCStatsCollector>(this), rtcReport]() {
        m_callback(RTCStatsReport::create([rtcReport](auto& mapAdapter) {
            if (rtcReport)
                initializeRTCStatsReportBackingMap(mapAdapter, *rtcReport);
        }));
    });
}

}; // namespace WTF


#endif // USE(LIBWEBRTC)
