/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "RTCDtlsTransportState.h"
#include "RTCIceCandidateType.h"
#include "RTCIceRole.h"
#include "RTCIceServerTransportProtocol.h"
#include "RTCIceTcpCandidateType.h"
#include "RTCIceTransportState.h"
#include <wtf/KeyValuePair.h>

namespace WebCore {

class DOMMapAdapter;

class RTCStatsReport : public RefCounted<RTCStatsReport> {
public:
    using MapInitializer = Function<void(DOMMapAdapter&)>;
    static Ref<RTCStatsReport> create(MapInitializer&& mapInitializer) { return adoptRef(*new RTCStatsReport(WTFMove(mapInitializer))); }

    void initializeMapLike(DOMMapAdapter& adapter) { m_mapInitializer(adapter); }

    enum class Type {
        Codec,
        InboundRtp,
        OutboundRtp,
        RemoteInboundRtp,
        RemoteOutboundRtp,
        MediaSource,
        MediaPlayout,
        PeerConnection,
        DataChannel,
        Transport,
        CandidatePair,
        LocalCandidate,
        RemoteCandidate,
        Certificate,
    };
    struct Stats {
        double timestamp;
        Type type;
        String id;
    };

    struct RtpStreamStats : Stats {
        uint32_t ssrc { 0 };
        String kind;
        String transportId;
        String codecId;
    };

    struct ReceivedRtpStreamStats : RtpStreamStats {
        std::optional<uint64_t> packetsReceived;
        std::optional<int64_t> packetsLost;
        std::optional<double> jitter;
    };

    struct InboundRtpStreamStats : ReceivedRtpStreamStats {
        InboundRtpStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        String trackIdentifier;
        String mid;
        String remoteId;
        std::optional<uint32_t> framesDecoded;
        std::optional<uint32_t> keyFramesDecoded;
        std::optional<uint32_t> framesRendered;
        std::optional<uint32_t> framesDropped;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<double> framesPerSecond;
        std::optional<uint64_t> qpSum;
        std::optional<double> totalDecodeTime;
        std::optional<double> totalInterFrameDelay;
        std::optional<double> totalSquaredInterFrameDelay;
        std::optional<uint32_t> pauseCount;
        std::optional<double> totalPausesDuration;
        std::optional<uint32_t> freezeCount;
        std::optional<double> totalFreezesDuration;
        std::optional<double> lastPacketReceivedTimestamp;
        std::optional<uint64_t> headerBytesReceived;
        std::optional<uint64_t> packetsDiscarded;
        std::optional<uint64_t> fecBytesReceived;
        std::optional<uint64_t> fecPacketsReceived;
        std::optional<uint64_t> fecPacketsDiscarded;
        std::optional<uint64_t> bytesReceived;
        std::optional<uint64_t> packetsFailedDecryption;
        std::optional<uint64_t> packetsDuplicated;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<double> totalProcessingDelay;
        std::optional<double> estimatedPlayoutTimestamp;
        std::optional<double> jitterBufferDelay;
        std::optional<double> jitterBufferTargetDelay;
        std::optional<uint64_t> jitterBufferEmittedCount;
        std::optional<double> jitterBufferMinimumDelay;
        std::optional<uint64_t> totalSamplesReceived;
        std::optional<uint64_t> samplesDecodedWithSilk;
        std::optional<uint64_t> samplesDecodedWithCelt;
        std::optional<uint64_t> concealedSamples;
        std::optional<uint64_t> silentConcealedSamples;
        std::optional<uint64_t> concealmentEvents;
        std::optional<uint64_t> insertedSamplesForDeceleration;
        std::optional<uint64_t> removedSamplesForAcceleration;
        std::optional<double> audioLevel;
        std::optional<double> totalAudioEnergy;
        std::optional<double> totalSamplesDuration;
        std::optional<uint32_t> framesReceived;
        String decoderImplementation;
        String playoutId;
        std::optional<bool> powerEfficientDecoder;
        std::optional<uint32_t> framesAssembledFromMultiplePackets;
        std::optional<double> totalAssemblyTime;
        std::optional<uint64_t> retransmittedPacketsReceived;
        std::optional<uint64_t> retransmittedBytesReceived;
        std::optional<uint32_t> rtxSsrc;
        std::optional<uint32_t> fecSsrc;
    };

    struct RemoteInboundRtpStreamStats : ReceivedRtpStreamStats {
        RemoteInboundRtpStreamStats() { type = RTCStatsReport::Type::RemoteInboundRtp; }

        String localId;
        std::optional<double> roundTripTime;
        std::optional<double> totalRoundTripTime;
        std::optional<double> fractionLost;
        std::optional<uint64_t> roundTripTimeMeasurements;
    };

    struct SentRtpStreamStats : RtpStreamStats {
        std::optional<uint32_t> packetsSent;
        std::optional<uint64_t> bytesSent;
    };

    enum class QualityLimitationReason {
        None,
        Cpu,
        Bandwidth,
        Other
    };

    struct OutboundRtpStreamStats : SentRtpStreamStats {
        OutboundRtpStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        String mid;
        String mediaSourceId;
        String remoteId;
        String rid;
        std::optional<uint64_t> headerBytesSent;
        std::optional<uint64_t> retransmittedPacketsSent;
        std::optional<uint64_t> retransmittedBytesSent;
        std::optional<uint32_t> rtxSsrc;
        std::optional<double> targetBitrate;
        std::optional<uint64_t> totalEncodedBytesTarget;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<double> framesPerSecond;
        std::optional<uint32_t> framesSent;
        std::optional<uint32_t> hugeFramesSent;
        std::optional<uint32_t> framesEncoded;
        std::optional<uint32_t> keyFramesEncoded;
        std::optional<uint64_t> qpSum;
        std::optional<double> totalEncodeTime;
        std::optional<double> totalPacketSendDelay;
        std::optional<QualityLimitationReason> qualityLimitationReason;
        std::optional<Vector<KeyValuePair<String, double>>> qualityLimitationDurations;
        std::optional<uint32_t> qualityLimitationResolutionChanges;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<bool> active;
        String scalabilityMode;
    };

    struct RemoteOutboundRtpStreamStats : SentRtpStreamStats {
        RemoteOutboundRtpStreamStats() { type = RTCStatsReport::Type::RemoteOutboundRtp; }

        String localId;
        std::optional<double> remoteTimestamp;
        std::optional<uint64_t> reportsSent;
        std::optional<double> roundTripTime;
        std::optional<double> totalRoundTripTime;
        std::optional<uint64_t> roundTripTimeMeasurements;
    };

    struct DataChannelStats : Stats {
        DataChannelStats() { type = RTCStatsReport::Type::DataChannel; }

        String label;
        String protocol;
        std::optional<int> dataChannelIdentifier;
        String state;
        std::optional<uint32_t> messagesSent;
        std::optional<uint64_t> bytesSent;
        std::optional<uint32_t> messagesReceived;
        std::optional<uint64_t> bytesReceived;
    };

    enum class IceCandidatePairState {
        Frozen,
        Waiting,
        InProgress,
        Failed,
        Succeeded
    };

    struct IceCandidatePairStats : Stats {
        IceCandidatePairStats() { type = RTCStatsReport::Type::CandidatePair; }

        String transportId;
        String localCandidateId;
        String remoteCandidateId;
        IceCandidatePairState state;
        std::optional<bool> nominated;
        std::optional<uint64_t> packetsSent;
        std::optional<uint64_t> packetsReceived;
        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        std::optional<double> lastPacketSentTimestamp;
        std::optional<double> lastPacketReceivedTimestamp;
        std::optional<double> totalRoundTripTime;
        std::optional<double> currentRoundTripTime;
        std::optional<double> availableOutgoingBitrate;
        std::optional<double> availableIncomingBitrate;
        std::optional<uint64_t> requestsReceived;
        std::optional<uint64_t> requestsSent;
        std::optional<uint64_t> responsesReceived;
        std::optional<uint64_t> responsesSent;
        std::optional<uint64_t> consentRequestsSent;
        std::optional<uint32_t> packetsDiscardedOnSend;
        std::optional<uint64_t> bytesDiscardedOnSend;
    };

    struct IceCandidateStats : Stats {
        String transportId;
        String address;
        std::optional<int32_t> port;
        String protocol;
        RTCIceCandidateType candidateType;
        std::optional<int32_t> priority;
        String url;
        std::optional<RTCIceServerTransportProtocol> relayProtocol;
        String foundation;
        String relatedAddress;
        std::optional<int32_t> relatedPort;
        String usernameFragment;
        std::optional<RTCIceTcpCandidateType> tcpType;
    };

    struct CertificateStats : Stats {
        CertificateStats() { type = RTCStatsReport::Type::Certificate; }

        String fingerprint;
        String fingerprintAlgorithm;
        String base64Certificate;
        String issuerCertificateId;
    };

    enum class CodecType {
        Encode,
        Decode
    };

    struct CodecStats : Stats {
        CodecStats() { type = RTCStatsReport::Type::Codec; }

        std::optional<uint32_t> payloadType;
        String transportId;
        String mimeType;
        std::optional<uint32_t> clockRate;
        std::optional<uint32_t> channels;
        String sdpFmtpLine;
        String implementation;
    };

    enum DtlsRole {
        Client,
        Server,
        Unknown
    };

    struct TransportStats : Stats {
        TransportStats() { type = RTCStatsReport::Type::Transport; }

        std::optional<uint64_t> packetsSent;
        std::optional<uint64_t> packetsReceived;
        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        std::optional<RTCIceRole> iceRole;
        String iceLocalUsernameFragment;
        RTCDtlsTransportState dtlsState;
        std::optional<RTCIceTransportState> iceState;
        String selectedCandidatePairId;
        String localCertificateId;
        String remoteCertificateId;
        String tlsVersion;
        String dtlsCipher;
        std::optional<DtlsRole> dtlsRole;
        String srtpCipher;
        std::optional<uint32_t> selectedCandidatePairChanges;
    };

    struct AudioPlayoutStats : Stats {
        AudioPlayoutStats() { type = RTCStatsReport::Type::MediaPlayout; }

        String kind;
        std::optional<double> synthesizedSamplesDuration;
        std::optional<uint32_t> synthesizedSamplesEvents;
        std::optional<double> totalSamplesDuration;
        std::optional<double> totalPlayoutDelay;
        std::optional<uint64_t> totalSamplesCount;
    };

    struct PeerConnectionStats : Stats {
        PeerConnectionStats() { type = RTCStatsReport::Type::PeerConnection; }

        std::optional<uint32_t> dataChannelsOpened;
        std::optional<uint32_t> dataChannelsClosed;
    };

    struct MediaSourceStats : Stats {
        String trackIdentifier;
        String kind;
        std::optional<bool> relayedSource;
    };

    struct AudioSourceStats : MediaSourceStats {
        AudioSourceStats() { type = RTCStatsReport::Type::MediaSource; }

        std::optional<double> audioLevel;
        std::optional<double> totalAudioEnergy;
        std::optional<double> totalSamplesDuration;
        std::optional<double> echoReturnLoss;
        std::optional<double> echoReturnLossEnhancement;
        std::optional<double> droppedSamplesDuration;
        std::optional<uint64_t> droppedSamplesEvents;
        std::optional<double> totalCaptureDelay;
        std::optional<uint64_t> totalSamplesCaptured;
    };

    struct VideoSourceStats : MediaSourceStats {
        VideoSourceStats() { type = RTCStatsReport::Type::MediaSource; }

        std::optional<unsigned long> width;
        std::optional<unsigned long> height;
        std::optional<unsigned long> frames;
        std::optional<double> framesPerSecond;
    };

private:
    explicit RTCStatsReport(MapInitializer&&);

    MapInitializer m_mapInitializer;
};

inline RTCStatsReport::RTCStatsReport(MapInitializer&& mapInitializer)
    : m_mapInitializer(WTFMove(mapInitializer))
{
}

} // namespace WebCore
