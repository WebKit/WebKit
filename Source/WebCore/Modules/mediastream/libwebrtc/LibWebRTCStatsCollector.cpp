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
        callback();
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

static inline void fillRTCRTPStreamStats(RTCStatsReport::RTCRTPStreamStats& stats, const webrtc::RTCRTPStreamStats& rtcStats)
{
    fillRTCStats(stats, rtcStats);

    if (rtcStats.ssrc.is_defined())
        stats.ssrc = *rtcStats.ssrc;
    if (rtcStats.associate_stats_id.is_defined())
        stats.associateStatsId = fromStdString(*rtcStats.associate_stats_id);
    if (rtcStats.is_remote.is_defined())
        stats.isRemote = *rtcStats.is_remote;
    if (rtcStats.media_type.is_defined())
        stats.mediaType = fromStdString(*rtcStats.media_type);
    if (rtcStats.track_id.is_defined())
        stats.trackId = fromStdString(*rtcStats.track_id);
    if (rtcStats.transport_id.is_defined())
        stats.transportId = fromStdString(*rtcStats.transport_id);
    if (rtcStats.codec_id.is_defined())
        stats.codecId = fromStdString(*rtcStats.codec_id);
    if (rtcStats.fir_count.is_defined())
        stats.firCount = *rtcStats.fir_count;
    if (rtcStats.pli_count.is_defined())
        stats.pliCount = *rtcStats.pli_count;
    if (rtcStats.nack_count.is_defined())
        stats.nackCount = *rtcStats.nack_count;
    if (rtcStats.sli_count.is_defined())
        stats.sliCount = *rtcStats.sli_count;
    if (rtcStats.qp_sum.is_defined())
        stats.qpSum = *rtcStats.qp_sum;
    stats.qpSum = 0;
}

static inline void fillInboundRTPStreamStats(RTCStatsReport::InboundRTPStreamStats& stats, const webrtc::RTCInboundRTPStreamStats& rtcStats)
{
    fillRTCRTPStreamStats(stats, rtcStats);

    if (rtcStats.packets_received.is_defined())
        stats.packetsReceived = *rtcStats.packets_received;
    if (rtcStats.bytes_received.is_defined())
        stats.bytesReceived = *rtcStats.bytes_received;
    if (rtcStats.packets_lost.is_defined())
        stats.packetsLost = *rtcStats.packets_lost;
    if (rtcStats.jitter.is_defined())
        stats.jitter = *rtcStats.jitter;
    if (rtcStats.fraction_lost.is_defined())
        stats.fractionLost = *rtcStats.fraction_lost;
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
    if (rtcStats.frames_decoded.is_defined())
        stats.framesDecoded = *rtcStats.frames_decoded;
}

static inline void fillOutboundRTPStreamStats(RTCStatsReport::OutboundRTPStreamStats& stats, const webrtc::RTCOutboundRTPStreamStats& rtcStats)
{
    fillRTCRTPStreamStats(stats, rtcStats);

    if (rtcStats.packets_sent.is_defined())
        stats.packetsSent = *rtcStats.packets_sent;
    if (rtcStats.bytes_sent.is_defined())
        stats.bytesSent = *rtcStats.bytes_sent;
    if (rtcStats.target_bitrate.is_defined())
        stats.targetBitrate = *rtcStats.target_bitrate;
    if (rtcStats.frames_encoded.is_defined())
        stats.framesEncoded = *rtcStats.frames_encoded;
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
    if (rtcStats.implementation.is_defined())
        stats.implementation = fromStdString(*rtcStats.implementation);
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

void LibWebRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& rtcReport)
{
    callOnMainThread([protectedThis = rtc::scoped_refptr<LibWebRTCStatsCollector>(this), rtcReport] {
        auto report = protectedThis->m_callback();
        if (!report)
            return;

        ASSERT(report->backingMap());

        for (const auto& rtcStats : *rtcReport) {
            if (rtcStats.type() == webrtc::RTCInboundRTPStreamStats::kType) {
                RTCStatsReport::InboundRTPStreamStats stats;
                fillInboundRTPStreamStats(stats, static_cast<const webrtc::RTCInboundRTPStreamStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::InboundRTPStreamStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCOutboundRTPStreamStats::kType) {
                RTCStatsReport::OutboundRTPStreamStats stats;
                fillOutboundRTPStreamStats(stats, static_cast<const webrtc::RTCOutboundRTPStreamStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::OutboundRTPStreamStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCMediaStreamTrackStats::kType) {
                RTCStatsReport::MediaStreamTrackStats stats;
                fillRTCMediaStreamTrackStats(stats, static_cast<const webrtc::RTCMediaStreamTrackStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::MediaStreamTrackStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCDataChannelStats::kType) {
                RTCStatsReport::DataChannelStats stats;
                fillRTCDataChannelStats(stats, static_cast<const webrtc::RTCDataChannelStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::DataChannelStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCIceCandidatePairStats::kType) {
                RTCStatsReport::IceCandidatePairStats stats;
                fillRTCIceCandidatePairStats(stats, static_cast<const webrtc::RTCIceCandidatePairStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::IceCandidatePairStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCRemoteIceCandidateStats::kType || rtcStats.type() == webrtc::RTCLocalIceCandidateStats::kType) {
                RTCStatsReport::IceCandidateStats stats;
                fillRTCIceCandidateStats(stats, static_cast<const webrtc::RTCIceCandidateStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::IceCandidateStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCCertificateStats::kType) {
                RTCStatsReport::CertificateStats stats;
                fillRTCCertificateStats(stats, static_cast<const webrtc::RTCCertificateStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::CertificateStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCCodecStats::kType) {
                RTCStatsReport::CodecStats stats;
                fillRTCCodecStats(stats, static_cast<const webrtc::RTCCodecStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::CodecStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCTransportStats::kType) {
                RTCStatsReport::TransportStats stats;
                fillRTCTransportStats(stats, static_cast<const webrtc::RTCTransportStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::TransportStats>>(WTFMove(stats));
            } else if (rtcStats.type() == webrtc::RTCPeerConnectionStats::kType) {
                RTCStatsReport::PeerConnectionStats stats;
                fillRTCPeerConnectionStats(stats, static_cast<const webrtc::RTCPeerConnectionStats&>(rtcStats));
                report->addStats<IDLDictionary<RTCStatsReport::PeerConnectionStats>>(WTFMove(stats));
            }
        }
    });
}

}; // namespace WTF


#endif // USE(LIBWEBRTC)
