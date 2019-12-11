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
        Optional<uint32_t> ssrc;
        String associateStatsId;
        bool isRemote { false };
        String mediaType;

        String trackId;
        String transportId;
        String codecId;
        Optional<uint32_t> firCount;
        Optional<uint32_t> pliCount;
        Optional<uint32_t> nackCount;
        Optional<uint32_t> sliCount;
        Optional<uint64_t> qpSum;
    };

    struct InboundRTPStreamStats : RTCRTPStreamStats {
        InboundRTPStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        Optional<uint32_t> packetsReceived;
        Optional<uint64_t> bytesReceived;
        Optional<uint32_t> packetsLost;
        Optional<double> jitter;
        Optional<double> fractionLost;
        Optional<uint32_t> packetsDiscarded;
        Optional<uint32_t> packetsRepaired;
        Optional<uint32_t> burstPacketsLost;
        Optional<uint32_t> burstPacketsDiscarded;
        Optional<uint32_t> burstLossCount;
        Optional<uint32_t> burstDiscardCount;
        Optional<double> burstLossRate;
        Optional<double> burstDiscardRate;
        Optional<double> gapLossRate;
        Optional<double> gapDiscardRate;
        Optional<uint32_t> framesDecoded;
    };

    struct OutboundRTPStreamStats : RTCRTPStreamStats {
        OutboundRTPStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        Optional<uint32_t> packetsSent;
        Optional<uint64_t> bytesSent;
        Optional<double> targetBitrate;
        Optional<uint32_t> framesEncoded;
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
    };

    struct PeerConnectionStats : Stats {
        PeerConnectionStats() { type = RTCStatsReport::Type::PeerConnection; }

        Optional<uint32_t> dataChannelsOpened;
        Optional<uint32_t> dataChannelsClosed;
    };

private:
    RTCStatsReport() = default;

private:
    RefPtr<DOMMapLike> m_mapLike;
};

} // namespace WebCore
