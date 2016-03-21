/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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

#ifndef PeerConnectionBackend_h
#define PeerConnectionBackend_h

#if ENABLE(WEB_RTC)

#include "JSDOMPromise.h"
#include "PeerConnectionStates.h"

namespace WebCore {

class DOMError;
class Event;
class MediaStreamTrack;
class PeerConnectionBackend;
class RTCAnswerOptions;
class RTCConfiguration;
class RTCIceCandidate;
class RTCOfferOptions;
class RTCRtpReceiver;
class RTCRtpSender;
class RTCSessionDescription;
class RTCStatsResponse;
class ScriptExecutionContext;

namespace PeerConnection {
typedef DOMPromise<RefPtr<RTCSessionDescription>, RefPtr<DOMError>> SessionDescriptionPromise;
typedef DOMPromise<std::nullptr_t, RefPtr<DOMError>> VoidPromise;
typedef DOMPromise<RefPtr<RTCStatsResponse>, RefPtr<DOMError>> StatsPromise;
}

class PeerConnectionBackendClient {
public:
    virtual Vector<RefPtr<RTCRtpSender>> getSenders() const = 0;
    virtual void fireEvent(Event&) = 0;

    virtual void addReceiver(RTCRtpReceiver&) = 0;
    virtual void setSignalingState(PeerConnectionStates::SignalingState) = 0;
    virtual void updateIceGatheringState(PeerConnectionStates::IceGatheringState) = 0;
    virtual void updateIceConnectionState(PeerConnectionStates::IceConnectionState) = 0;

    virtual void scheduleNegotiationNeededEvent() = 0;

    virtual ScriptExecutionContext* scriptExecutionContext() const = 0;
    virtual PeerConnectionStates::SignalingState internalSignalingState() const = 0;
    virtual PeerConnectionStates::IceGatheringState internalIceGatheringState() const = 0;
    virtual PeerConnectionStates::IceConnectionState internalIceConnectionState() const = 0;

    virtual ~PeerConnectionBackendClient() { }
};

typedef std::unique_ptr<PeerConnectionBackend> (*CreatePeerConnectionBackend)(PeerConnectionBackendClient*);

class PeerConnectionBackend {
public:
    WEBCORE_EXPORT static CreatePeerConnectionBackend create;
    virtual ~PeerConnectionBackend() { }

    virtual void createOffer(RTCOfferOptions&, PeerConnection::SessionDescriptionPromise&&) = 0;
    virtual void createAnswer(RTCAnswerOptions&, PeerConnection::SessionDescriptionPromise&&) = 0;

    virtual void setLocalDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&) = 0;
    virtual RefPtr<RTCSessionDescription> localDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentLocalDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingLocalDescription() const = 0;

    virtual void setRemoteDescription(RTCSessionDescription&, PeerConnection::VoidPromise&&) = 0;
    virtual RefPtr<RTCSessionDescription> remoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> currentRemoteDescription() const = 0;
    virtual RefPtr<RTCSessionDescription> pendingRemoteDescription() const = 0;

    virtual void setConfiguration(RTCConfiguration&) = 0;
    virtual void addIceCandidate(RTCIceCandidate&, PeerConnection::VoidPromise&&) = 0;

    virtual void getStats(MediaStreamTrack*, PeerConnection::StatsPromise&&) = 0;

    virtual void replaceTrack(RTCRtpSender&, MediaStreamTrack&, PeerConnection::VoidPromise&&) = 0;

    virtual void stop() = 0;

    virtual bool isNegotiationNeeded() const = 0;
    virtual void markAsNeedingNegotiation() = 0;
    virtual void clearNegotiationNeededState() = 0;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

#endif // PeerConnectionBackend_h
