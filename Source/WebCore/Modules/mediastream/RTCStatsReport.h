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

#include "JSDOMMapLike.h"

namespace WebCore {

class RTCStatsReport : public RefCounted<RTCStatsReport> {
public:
    static Ref<RTCStatsReport> create() { return adoptRef(*new RTCStatsReport); }

    void synchronizeBackingMap(Ref<DOMMapLike>&& mapLike) { m_mapLike = WTFMove(mapLike); }
    DOMMapLike* backingMap() { return m_mapLike.get(); }

    template<typename Value> void addStats(typename Value::ParameterType&& value) { m_mapLike->set<IDLDOMString, Value>(value.id, std::forward<typename Value::ParameterType>(value)); }


    enum class Type {
        Codec,
        InboundRtp,
        OutboundRtp,
        PeerConnection,
        DataChannel,
        Track,
        Transport,
        CandidatePair,
        LocalCandidate,
        RemoteCandidate,
        Certificate
    };
    struct Stats {
        double timestamp;
        Type type;
        String id;
    };

    struct RTCRTPStreamStats : Stats {
        std::optional<uint32_t> ssrc;
        String associateStatsId;
        bool isRemote { false };
        String mediaType;

        String trackId;
        String transportId;
        String codecId;
        std::optional<uint32_t> firCount;
        std::optional<uint32_t> pliCount;
        std::optional<uint32_t> nackCount;
        std::optional<uint32_t> sliCount;
        std::optional<uint64_t> qpSum;
    };

    struct InboundRTPStreamStats : RTCRTPStreamStats {
        InboundRTPStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        std::optional<uint32_t> packetsReceived;
        std::optional<uint64_t> bytesReceived;
        std::optional<uint32_t> packetsLost;
        std::optional<double> jitter;
        std::optional<double> fractionLost;
        std::optional<uint32_t> packetsDiscarded;
        std::optional<uint32_t> packetsRepaired;
        std::optional<uint32_t> burstPacketsLost;
        std::optional<uint32_t> burstPacketsDiscarded;
        std::optional<uint32_t> burstLossCount;
        std::optional<uint32_t> burstDiscardCount;
        std::optional<double> burstLossRate;
        std::optional<double> burstDiscardRate;
        std::optional<double> gapLossRate;
        std::optional<double> gapDiscardRate;
        std::optional<uint32_t> framesDecoded;
    };

    struct OutboundRTPStreamStats : RTCRTPStreamStats {
        OutboundRTPStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        std::optional<uint32_t> packetsSent;
        std::optional<uint64_t> bytesSent;
        std::optional<double> targetBitrate;
        std::optional<uint32_t> framesEncoded;
    };

    struct MediaStreamTrackStats : Stats {
        MediaStreamTrackStats() { type = RTCStatsReport::Type::Track; }

        String trackIdentifier;
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

    enum class IceCandidateType { Host, Srflx, Prflx, Relay };

    struct IceCandidateStats : Stats {
        String transportId;
        String address;
        std::optional<int32_t> port;
        String protocol;
        std::optional<IceCandidateType> candidateType;
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
    };

    struct PeerConnectionStats : Stats {
        PeerConnectionStats() { type = RTCStatsReport::Type::PeerConnection; }

        std::optional<uint32_t> dataChannelsOpened;
        std::optional<uint32_t> dataChannelsClosed;
    };

private:
    RTCStatsReport() = default;

private:
    RefPtr<DOMMapLike> m_mapLike;
};

} // namespace WebCore
