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
        Optional<uint64_t> packetsReceived;
        Optional<int64_t> packetsLost;
        Optional<double> jitter;
        Optional<uint64_t> packetsDiscarded;
        Optional<uint64_t> packetsRepaired;
        Optional<uint64_t> burstPacketsLost;
        Optional<uint64_t> burstPacketsDiscarded;
        Optional<uint32_t> burstLossCount;
        Optional<uint32_t> burstDiscardCount;
        Optional<double> burstLossRate;
        Optional<double> burstDiscardRate;
        Optional<double> gapLossRate;
        Optional<double> gapDiscardRate;
        Optional<uint32_t> framesDropped;
        Optional<uint32_t> partialFramesLost;
        Optional<uint32_t> fullFramesLost;
    };

    struct InboundRtpStreamStats : ReceivedRtpStreamStats {
        InboundRtpStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        String receiverId;
        String remoteId;
        Optional<uint32_t> framesDecoded;
        Optional<uint32_t> keyFramesDecoded;
        Optional<uint32_t> frameWidth;
        Optional<uint32_t> frameHeight;
        Optional<uint32_t> frameBitDepth;
        Optional<double> framesPerSecond;
        Optional<uint64_t> qpSum;
        Optional<double> totalDecodeTime;
        Optional<double> totalInterFrameDelay;
        Optional<double> totalSquaredInterFrameDelay;
        Optional<bool>  voiceActivityFlag;
        Optional<double> lastPacketReceivedTimestamp;
        Optional<double> averageRtcpInterval;
        Optional<uint64_t> headerBytesReceived;
        Optional<uint64_t> fecPacketsReceived;
        Optional<uint64_t> fecPacketsDiscarded;
        Optional<uint64_t> bytesReceived;
        Optional<uint64_t> packetsFailedDecryption;
        Optional<uint64_t> packetsDuplicated;
        Optional<uint32_t> nackCount;
        Optional<uint32_t> firCount;
        Optional<uint32_t> pliCount;
        Optional<uint32_t> sliCount;
        Optional<double> estimatedPlayoutTimestamp;
        Optional<double> jitterBufferDelay;
        Optional<uint64_t> jitterBufferEmittedCount;
        Optional<uint64_t> totalSamplesReceived;
        Optional<uint64_t> samplesDecodedWithSilk;
        Optional<uint64_t> samplesDecodedWithCelt;
        Optional<uint64_t> concealedSamples;
        Optional<uint64_t> silentConcealedSamples;
        Optional<uint64_t> concealmentEvents;
        Optional<uint64_t> insertedSamplesForDeceleration;
        Optional<uint64_t> removedSamplesForAcceleration;
        Optional<double> audioLevel;
        Optional<double> totalAudioEnergy;
        Optional<double> totalSamplesDuration;
        Optional<uint32_t> framesReceived;

        String trackId;
    };

    struct RemoteInboundRtpStreamStats : ReceivedRtpStreamStats {
        RemoteInboundRtpStreamStats() { type = RTCStatsReport::Type::RemoteInboundRtp; }

        String localId;
        Optional<double> roundTripTime;
        Optional<double> totalRoundTripTime;
        Optional<double> fractionLost;
        Optional<uint64_t> reportsReceived;
        Optional<uint64_t> roundTripTimeMeasurements;
    };

    struct SentRtpStreamStats : RtpStreamStats {
        Optional<uint32_t> packetsSent;
        Optional<uint64_t> bytesSent;
    };

    struct OutboundRtpStreamStats : SentRtpStreamStats {
        OutboundRtpStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        Optional<uint32_t> rtxSsrc;
        String mediaSourceId;
        String senderId;
        String remoteId;
        String rid;
        Optional<double> lastPacketSentTimestamp;
        Optional<uint64_t> headerBytesSent;
        Optional<uint32_t> packetsDiscardedOnSend;
        Optional<uint64_t> bytesDiscardedOnSend;
        Optional<uint32_t> fecPacketsSent;
        Optional<uint64_t> retransmittedPacketsSent;
        Optional<uint64_t> retransmittedBytesSent;
        Optional<double> targetBitrate;
        Optional<uint64_t> totalEncodedBytesTarget;
        Optional<uint32_t> frameWidth;
        Optional<uint32_t> frameHeight;
        Optional<uint32_t> frameBitDepth;
        Optional<double> framesPerSecond;
        Optional<uint32_t> framesSent;
        Optional<uint32_t> hugeFramesSent;
        Optional<uint32_t> framesEncoded;
        Optional<uint32_t> keyFramesEncoded;
        Optional<uint32_t> framesDiscardedOnSend;
        Optional<uint64_t> qpSum;
        Optional<uint64_t> totalSamplesSent;
        Optional<uint64_t> samplesEncodedWithSilk;
        Optional<uint64_t> samplesEncodedWithCelt;
        Optional<bool> voiceActivityFlag;
        Optional<double> totalEncodeTime;
        Optional<double> totalPacketSendDelay;
        Optional<double> averageRtcpInterval;
        // Optional<RTCQualityLimitationReason qualityLimitationReason;
        // Optional<record<DOMString, double> qualityLimitationDurations;
        Optional<uint32_t> qualityLimitationResolutionChanges;
        // Optional<record<USVString, unsigned long long> perDscpPacketsSent;
        Optional<uint32_t> nackCount;
        Optional<uint32_t> firCount;
        Optional<uint32_t> pliCount;
        Optional<uint32_t> sliCount;
        // DOMString encoderImplementation;

        String trackId;
    };

    struct MediaStreamTrackStats : Stats {
        MediaStreamTrackStats() { type = RTCStatsReport::Type::Track; }

        String trackIdentifier;
        Optional<bool> remoteSource;
        Optional<bool> ended;
        Optional<bool> detached;
        Optional<uint32_t> frameWidth;
        Optional<uint32_t> frameHeight;
        Optional<double> framesPerSecond;
        Optional<uint32_t> framesSent;
        Optional<uint32_t> framesReceived;
        Optional<uint32_t> framesDecoded;
        Optional<uint32_t> framesDropped;
        Optional<uint32_t> framesCorrupted;
        Optional<uint32_t> partialFramesLost;
        Optional<uint32_t> fullFramesLost;
        Optional<double> audioLevel;
        Optional<double> echoReturnLoss;
        Optional<double> echoReturnLossEnhancement;

        Optional<uint32_t> freezeCount;
        Optional<uint32_t> pauseCount;
        Optional<double> totalFreezesDuration;
        Optional<double> totalPausesDuration;
        Optional<double> totalFramesDuration;
        Optional<double> sumOfSquaredFramesDuration;

        Optional<uint64_t> jitterBufferFlushes;
    };

    struct DataChannelStats : Stats {
        DataChannelStats() { type = RTCStatsReport::Type::DataChannel; }
        
        String label;
        String protocol;
        Optional<int> datachannelid;
        String state;
        Optional<uint32_t> messagesSent;
        Optional<uint64_t> bytesSent;
        Optional<uint32_t> messagesReceived;
        Optional<uint64_t> bytesReceived;
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
        Optional<uint64_t> priority;
        Optional<bool> nominated;
        Optional<bool> writable;
        Optional<bool> readable;
        Optional<uint64_t> bytesSent;
        Optional<uint64_t> bytesReceived;
        Optional<double> totalRoundTripTime;
        Optional<double> currentRoundTripTime;
        Optional<double> availableOutgoingBitrate;
        Optional<double> availableIncomingBitrate;
        Optional<uint64_t> requestsReceived;
        Optional<uint64_t> requestsSent;
        Optional<uint64_t> responsesReceived;
        Optional<uint64_t> responsesSent;
        Optional<uint64_t> retransmissionsReceived;
        Optional<uint64_t> retransmissionsSent;
        Optional<uint64_t> consentRequestsReceived;
        Optional<uint64_t> consentRequestsSent;
        Optional<uint64_t> consentResponsesReceived;
        Optional<uint64_t> consentResponsesSent;
    };

    enum class IceCandidateType { Host, Srflx, Prflx, Relay };

    struct IceCandidateStats : Stats {
        String transportId;
        String address;
        Optional<int32_t> port;
        String protocol;
        Optional<IceCandidateType> candidateType;
        Optional<int32_t> priority;
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

        Optional<uint32_t> payloadType;
        Optional<CodecType> codecType;
        String transportId;
        String mimeType;
        Optional<uint32_t> clockRate;
        Optional<uint32_t> channels;
        String sdpFmtpLine;
        String implementation;
    };

    struct TransportStats : Stats {
        TransportStats() { type = RTCStatsReport::Type::Transport; }

        Optional<uint64_t> bytesSent;
        Optional<uint64_t> bytesReceived;
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

        Optional<uint32_t> dataChannelsOpened;
        Optional<uint32_t> dataChannelsClosed;
    };

    struct MediaSourceStats : Stats {
        String trackIdentifier;
        String kind;
        Optional<bool> relayedSource;
    };

    struct AudioSourceStats : MediaSourceStats {
        AudioSourceStats() { type = RTCStatsReport::Type::MediaSource; }

        Optional<double> audioLevel;
        Optional<double> totalAudioEnergy;
        Optional<double> totalSamplesDuration;
        Optional<double> echoReturnLoss;
        Optional<double> echoReturnLossEnhancement;
    };

    struct VideoSourceStats : MediaSourceStats {
        VideoSourceStats() { type = RTCStatsReport::Type::MediaSource; }

        Optional<unsigned long> width;
        Optional<unsigned long> height;
        Optional<unsigned long> bitDepth;
        Optional<unsigned long> frames;
        Optional<double> framesPerSecond;
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
