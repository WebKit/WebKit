/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "JSDOMPromiseDeferred.h"
#include "RTCRtpParameters.h"
#include "RTCSessionDescription.h"
#include "RTCSignalingState.h"

namespace WebCore {

class MediaStream;
class MediaStreamTrack;
class PeerConnectionBackend;
class RTCDataChannelHandler;
class RTCIceCandidate;
class RTCPeerConnection;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCSessionDescription;
class RTCStatsReport;

struct MediaEndpointConfiguration;
struct RTCAnswerOptions;
struct RTCDataChannelInit;
struct RTCOfferOptions;

namespace PeerConnection {
using SessionDescriptionPromise = DOMPromiseDeferred<IDLDictionary<RTCSessionDescription::Init>>;
using StatsPromise = DOMPromiseDeferred<IDLInterface<RTCStatsReport>>;
}

using CreatePeerConnectionBackend = std::unique_ptr<PeerConnectionBackend> (*)(RTCPeerConnection&);

class PeerConnectionBackend {
public:
    WEBCORE_EXPORT static CreatePeerConnectionBackend create;

    PeerConnectionBackend(RTCPeerConnection& peerConnection) : m_peerConnection(peerConnection) { }
    virtual ~PeerConnectionBackend() { }

    void createOffer(RTCOfferOptions&&, PeerConnection::SessionDescriptionPromise&&);
    void createAnswer(RTCAnswerOptions&&, PeerConnection::SessionDescriptionPromise&&);
    void setLocalDescription(RTCSessionDescription&, DOMPromiseDeferred<void>&&);
    void setRemoteDescription(RTCSessionDescription&, DOMPromiseDeferred<void>&&);
    void addIceCandidate(RTCIceCandidate*, DOMPromiseDeferred<void>&&);

    virtual std::unique_ptr<RTCDataChannelHandler> createDataChannelHandler(const String&, const RTCDataChannelInit&) = 0;

    void stop();

    virtual RefPtr<RTCSessionDescription> localDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentLocalDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingLocalDescription() const = 0;

    virtual RefPtr<RTCSessionDescription> remoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentRemoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingRemoteDescription() const = 0;

    virtual bool setConfiguration(MediaEndpointConfiguration&&) = 0;

    virtual void getStats(MediaStreamTrack*, Ref<DeferredPromise>&&) = 0;

    virtual Vector<RefPtr<MediaStream>> getRemoteStreams() const = 0;

    virtual Ref<RTCRtpReceiver> createReceiver(const String& transceiverMid, const String& trackKind, const String& trackId) = 0;
    virtual void replaceTrack(RTCRtpSender&, Ref<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&) = 0;
    virtual void notifyAddedTrack(RTCRtpSender&) { }
    virtual void notifyRemovedTrack(RTCRtpSender&) { }

    virtual RTCRtpParameters getParameters(RTCRtpSender&) const { return { }; }

    void markAsNeedingNegotiation();
    bool isNegotiationNeeded() const { return m_negotiationNeeded; };
    void clearNegotiationNeededState() { m_negotiationNeeded = false; };

    virtual void emulatePlatformEvent(const String& action) = 0;

    void newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex);
    void disableICECandidateFiltering();
    void enableICECandidateFiltering();

    virtual void applyRotationForOutgoingVideoSources() { }

protected:
    void fireICECandidateEvent(RefPtr<RTCIceCandidate>&&);
    void doneGatheringCandidates();

    void updateSignalingState(RTCSignalingState);

    void createOfferSucceeded(String&&);
    void createOfferFailed(Exception&&);

    void createAnswerSucceeded(String&&);
    void createAnswerFailed(Exception&&);

    void setLocalDescriptionSucceeded();
    void setLocalDescriptionFailed(Exception&&);

    void setRemoteDescriptionSucceeded();
    void setRemoteDescriptionFailed(Exception&&);

    void addIceCandidateSucceeded();
    void addIceCandidateFailed(Exception&&);

    String filterSDP(String&&) const;

private:
    virtual void doCreateOffer(RTCOfferOptions&&) = 0;
    virtual void doCreateAnswer(RTCAnswerOptions&&) = 0;
    virtual void doSetLocalDescription(RTCSessionDescription&) = 0;
    virtual void doSetRemoteDescription(RTCSessionDescription&) = 0;
    virtual void doAddIceCandidate(RTCIceCandidate&) = 0;
    virtual void endOfIceCandidates(DOMPromiseDeferred<void>&& promise) { promise.resolve(); }
    virtual void doStop() = 0;

protected:
    RTCPeerConnection& m_peerConnection;

private:
    std::optional<PeerConnection::SessionDescriptionPromise> m_offerAnswerPromise;
    std::optional<DOMPromiseDeferred<void>> m_setDescriptionPromise;
    std::optional<DOMPromiseDeferred<void>> m_addIceCandidatePromise;

    bool m_shouldFilterICECandidates { true };
    struct PendingICECandidate {
        // Fields described in https://www.w3.org/TR/webrtc/#idl-def-rtcicecandidateinit.
        String sdp;
        String mid;
        unsigned short sdpMLineIndex;
    };
    Vector<PendingICECandidate> m_pendingICECandidates;

    bool m_negotiationNeeded { false };
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
