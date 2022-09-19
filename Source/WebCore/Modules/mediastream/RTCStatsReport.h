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

#include "LibWebRTCStatsCollector.h"
#include "RTCIceCandidateType.h"

namespace WebCore {

class DOMMapAdapter;

class RTCStatsReport : public RefCounted<RTCStatsReport> {
public:
    using MapInitializer = Function<void(DOMMapAdapter&)>;
    static Ref<RTCStatsReport> create(MapInitializer&& mapInitializer) { return adoptRef(*new RTCStatsReport(WTFMove(mapInitializer))); }

    void initializeMapLike(DOMMapAdapter& adapter) { m_mapInitializer(adapter); }

    enum class Type {
        CandidatePair,
        Certificate,
        Codec,
        DataChannel,
        InboundRtp,
        LocalCandidate,
        MediaSource,
        OutboundRtp,
        PeerConnection,
        RemoteCandidate,
        RemoteInboundRtp,
        Track,
        Transport
    };
    struct Stats {
        double timestamp;
        Type type;
        String id;
    };

    struct RtpStreamStats : Stats {
        uint32_t ssrc { 0 };
        String kind;
        String mediaType;
        String transportId;
        String codecId;
    };

    struct ReceivedRtpStreamStats : RtpStreamStats {
        std::optional<uint64_t> packetsReceived;
        std::optional<int64_t> packetsLost;
        std::optional<double> jitter;
        std::optional<uint64_t> packetsDiscarded;
        std::optional<uint64_t> packetsRepaired;
        std::optional<uint64_t> burstPacketsLost;
        std::optional<uint64_t> burstPacketsDiscarded;
        std::optional<uint32_t> burstLossCount;
        std::optional<uint32_t> burstDiscardCount;
        std::optional<double> burstLossRate;
        std::optional<double> burstDiscardRate;
        std::optional<double> gapLossRate;
        std::optional<double> gapDiscardRate;
        std::optional<uint32_t> framesDropped;
        std::optional<uint32_t> partialFramesLost;
        std::optional<uint32_t> fullFramesLost;
    };

    struct InboundRtpStreamStats : ReceivedRtpStreamStats {
        InboundRtpStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        String receiverId;
        String remoteId;
        std::optional<uint32_t> framesDecoded;
        std::optional<uint32_t> keyFramesDecoded;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<uint32_t> frameBitDepth;
        std::optional<double> framesPerSecond;
        std::optional<uint64_t> qpSum;
        std::optional<double> totalDecodeTime;
        std::optional<double> totalInterFrameDelay;
        std::optional<double> totalSquaredInterFrameDelay;
        std::optional<bool>  voiceActivityFlag;
        std::optional<double> lastPacketReceivedTimestamp;
        std::optional<double> averageRtcpInterval;
        std::optional<uint64_t> headerBytesReceived;
        std::optional<uint64_t> fecPacketsReceived;
        std::optional<uint64_t> fecPacketsDiscarded;
        std::optional<uint64_t> bytesReceived;
        std::optional<uint64_t> packetsFailedDecryption;
        std::optional<uint64_t> packetsDuplicated;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<uint32_t> sliCount;
        std::optional<double> estimatedPlayoutTimestamp;
        std::optional<double> jitterBufferDelay;
        std::optional<uint64_t> jitterBufferEmittedCount;
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

        String trackId;
    };

    struct RemoteInboundRtpStreamStats : ReceivedRtpStreamStats {
        RemoteInboundRtpStreamStats() { type = RTCStatsReport::Type::RemoteInboundRtp; }

        String localId;
        std::optional<double> roundTripTime;
        std::optional<double> totalRoundTripTime;
        std::optional<double> fractionLost;
        std::optional<uint64_t> reportsReceived;
        std::optional<uint64_t> roundTripTimeMeasurements;
    };

    struct SentRtpStreamStats : RtpStreamStats {
        std::optional<uint32_t> packetsSent;
        std::optional<uint64_t> bytesSent;
    };

    struct OutboundRtpStreamStats : SentRtpStreamStats {
        OutboundRtpStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        std::optional<uint32_t> rtxSsrc;
        String mediaSourceId;
        String senderId;
        String remoteId;
        String rid;
        std::optional<double> lastPacketSentTimestamp;
        std::optional<uint64_t> headerBytesSent;
        std::optional<uint32_t> packetsDiscardedOnSend;
        std::optional<uint64_t> bytesDiscardedOnSend;
        std::optional<uint32_t> fecPacketsSent;
        std::optional<uint64_t> retransmittedPacketsSent;
        std::optional<uint64_t> retransmittedBytesSent;
        std::optional<double> targetBitrate;
        std::optional<uint64_t> totalEncodedBytesTarget;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<uint32_t> frameBitDepth;
        std::optional<double> framesPerSecond;
        std::optional<uint32_t> framesSent;
        std::optional<uint32_t> hugeFramesSent;
        std::optional<uint32_t> framesEncoded;
        std::optional<uint32_t> keyFramesEncoded;
        std::optional<uint32_t> framesDiscardedOnSend;
        std::optional<uint64_t> qpSum;
        std::optional<uint64_t> totalSamplesSent;
        std::optional<uint64_t> samplesEncodedWithSilk;
        std::optional<uint64_t> samplesEncodedWithCelt;
        std::optional<bool> voiceActivityFlag;
        std::optional<double> totalEncodeTime;
        std::optional<double> totalPacketSendDelay;
        std::optional<double> averageRtcpInterval;
        // std::optional<RTCQualityLimitationReason qualityLimitationReason;
        // std::optional<record<DOMString, double> qualityLimitationDurations;
        std::optional<uint32_t> qualityLimitationResolutionChanges;
        // std::optional<record<USVString, unsigned long long> perDscpPacketsSent;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<uint32_t> sliCount;
        // DOMString encoderImplementation;

        String trackId;
    };

    struct MediaStreamTrackStats : Stats {
        MediaStreamTrackStats() { type = RTCStatsReport::Type::Track; }

        String trackIdentifier;
        String kind;
        std::optional<bool> remoteSource;
        std::optional<bool> ended;
        std::optional<bool> detached;
        std::optional<uint32_t> frameWidth;
        std::optional<uint32_t> frameHeight;
        std::optional<double> framesPerSecond;
        std::optional<uint32_t> framesSent;
        std::optional<uint32_t> framesReceived;
        std::optional<uint32_t> framesDecoded;
        std::optional<uint32_t> framesDropped;
        std::optional<uint32_t> framesCorrupted;
        std::optional<uint32_t> partialFramesLost;
        std::optional<uint32_t> fullFramesLost;
        std::optional<double> audioLevel;
        std::optional<double> echoReturnLoss;
        std::optional<double> echoReturnLossEnhancement;

        std::optional<uint32_t> freezeCount;
        std::optional<uint32_t> pauseCount;
        std::optional<double> totalFreezesDuration;
        std::optional<double> totalPausesDuration;
        std::optional<double> totalFramesDuration;
        std::optional<double> sumOfSquaredFramesDuration;

        std::optional<uint64_t> jitterBufferFlushes;
    };

    struct DataChannelStats : Stats {
        DataChannelStats() { type = RTCStatsReport::Type::DataChannel; }
        
        String label;
        String protocol;
        std::optional<int> datachannelid;
        String state;
        std::optional<uint32_t> messagesSent;
        std::optional<uint64_t> bytesSent;
        std::optional<uint32_t> messagesReceived;
        std::optional<uint64_t> bytesReceived;
    };

    enum class IceCandidatePairState {
        Frozen,
        Waiting,
        Inprogress,
        Failed,
        Succeeded,
        Cancelled
    };

    struct IceCandidatePairStats : Stats {
        IceCandidatePairStats() { type = RTCStatsReport::Type::CandidatePair; }

        String transportId;
        String localCandidateId;
        String remoteCandidateId;
        IceCandidatePairState state;
        std::optional<uint64_t> priority;
        std::optional<bool> nominated;
        std::optional<bool> writable;
        std::optional<bool> readable;
        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        std::optional<double> totalRoundTripTime;
        std::optional<double> currentRoundTripTime;
        std::optional<double> availableOutgoingBitrate;
        std::optional<double> availableIncomingBitrate;
        std::optional<uint64_t> requestsReceived;
        std::optional<uint64_t> requestsSent;
        std::optional<uint64_t> responsesReceived;
        std::optional<uint64_t> responsesSent;
        std::optional<uint64_t> retransmissionsReceived;
        std::optional<uint64_t> retransmissionsSent;
        std::optional<uint64_t> consentRequestsReceived;
        std::optional<uint64_t> consentRequestsSent;
        std::optional<uint64_t> consentResponsesReceived;
        std::optional<uint64_t> consentResponsesSent;
    };

    struct IceCandidateStats : Stats {
        String transportId;
        String address;
        std::optional<int32_t> port;
        String protocol;
        std::optional<RTCIceCandidateType> candidateType;
        std::optional<int32_t> priority;
        String url;
        bool deleted { false };
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
        std::optional<CodecType> codecType;
        String transportId;
        String mimeType;
        std::optional<uint32_t> clockRate;
        std::optional<uint32_t> channels;
        String sdpFmtpLine;
        String implementation;
    };

    struct TransportStats : Stats {
        TransportStats() { type = RTCStatsReport::Type::Transport; }

        std::optional<uint64_t> bytesSent;
        std::optional<uint64_t> bytesReceived;
        String rtcpTransportStatsId;
        String selectedCandidatePairId;
        String localCertificateId;
        String remoteCertificateId;
        String dtlsState;
        String tlsVersion;
        String dtlsCipher;
        String srtpCipher;
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
    };

    struct VideoSourceStats : MediaSourceStats {
        VideoSourceStats() { type = RTCStatsReport::Type::MediaSource; }

        std::optional<unsigned long> width;
        std::optional<unsigned long> height;
        std::optional<unsigned long> bitDepth;
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
