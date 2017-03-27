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
        unsigned long long timestamp;
        Type type;
        String id;
    };

    struct RTCRTPStreamStats : Stats {
        uint32_t ssrc;
        String associateStatsId;
        bool isRemote { false };
        String mediaType;

        String mediaTrackId;
        String transportId;
        String codecId;
        unsigned long firCount { 0 };
        unsigned long pliCount { 0 };
        unsigned long nackCount { 0 };
        unsigned long sliCount { 0 };
        unsigned long long qpSum { 0 };
    };

    struct InboundRTPStreamStats : RTCRTPStreamStats {
        InboundRTPStreamStats() { type = RTCStatsReport::Type::InboundRtp; }

        uint32_t ssrc;
        String associateStatsId;
        bool isRemote { false };
        String mediaType;
        String mediaTrackId;
        String transportId;
        String codecId;
        unsigned long firCount { 0 };
        unsigned long pliCount { 0 };
        unsigned long nackCount { 0 };
        unsigned long sliCount { 0 };
        unsigned long long qpSum { 0 };
        unsigned long packetsReceived { 0 };
        unsigned long long bytesReceived { 0 };
        unsigned long packetsLost { 0 };
        double jitter { 0 };
        double fractionLost { 0 };
        unsigned long packetsDiscarded { 0 };
        unsigned long packetsRepaired { 0 };
        unsigned long burstPacketsLost { 0 };
        unsigned long burstPacketsDiscarded { 0 };
        unsigned long burstLossCount { 0 };
        unsigned long burstDiscardCount { 0 };
        double burstLossRate { 0 };
        double burstDiscardRate { 0 };
        double gapLossRate { 0 };
        double gapDiscardRate { 0 };
        unsigned long framesDecoded { 0 };
    };

    struct OutboundRTPStreamStats : RTCRTPStreamStats {
        OutboundRTPStreamStats() { type = RTCStatsReport::Type::OutboundRtp; }

        unsigned long packetsSent { 0 };
        unsigned long long bytesSent { 0 };
        double targetBitrate { 0 };
        unsigned long framesEncoded { 0 };
    };

    struct MediaStreamTrackStats : Stats {
        MediaStreamTrackStats() { type = RTCStatsReport::Type::Track; }

        String trackIdentifier;
        bool remoteSource { false };
        bool ended { false };
        bool detached { false };
        unsigned long frameWidth { 0 };
        unsigned long frameHeight { 0 };
        double framesPerSecond { 0 };
        unsigned long framesSent { 0 };
        unsigned long framesReceived { 0 };
        unsigned long framesDecoded { 0 };
        unsigned long framesDropped { 0 };
        unsigned long framesCorrupted { 0 };
        unsigned long partialFramesLost { 0 };
        unsigned long fullFramesLost { 0 };
        double audioLevel { 0 };
        double echoReturnLoss { 0 };
        double echoReturnLossEnhancement { 0 };
    };

    struct DataChannelStats : Stats {
        DataChannelStats() { type = RTCStatsReport::Type::DataChannel; }
        
        String label;
        String protocol;
        long datachannelid { 0 };
        String state;
        unsigned long messagesSent { 0 };
        unsigned long long bytesSent { 0 };
        unsigned long messagesReceived { 0 };
        unsigned long long bytesReceived { 0 };
    };

private:
    RTCStatsReport() = default;

private:
    RefPtr<DOMMapLike> m_mapLike;
};

} // namespace WebCore
