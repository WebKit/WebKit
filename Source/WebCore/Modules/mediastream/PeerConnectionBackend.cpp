/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PeerConnectionBackend.h"

#if ENABLE(WEB_RTC)

#include "EventNames.h"
#include "JSRTCSessionDescription.h"
#include "Logging.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionIceEvent.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

void PeerConnectionBackend::createOffer(RTCOfferOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerPromise = WTFMove(promise);
    doCreateOffer(WTFMove(options));
}

void PeerConnectionBackend::createOfferSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Creating offer succeeded:\n%{public}s\n", sdp.utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->resolve(RTCSessionDescription::Init { RTCSdpType::Offer, filterSDP(WTFMove(sdp)) });
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createOfferFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Creating offer failed:\n%{public}s\n", exception.message().utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->reject(WTFMove(exception));
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createAnswer(RTCAnswerOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerPromise = WTFMove(promise);
    doCreateAnswer(WTFMove(options));
}

void PeerConnectionBackend::createAnswerSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Creating answer succeeded:\n%{public}s\n", sdp.utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->resolve(RTCSessionDescription::Init { RTCSdpType::Answer, WTFMove(sdp) });
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createAnswerFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Creating answer failed:\n%{public}s\n", exception.message().utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->reject(WTFMove(exception));
    m_offerAnswerPromise = std::nullopt;
}

static inline bool isLocalDescriptionTypeValidForState(RTCSdpType type, RTCSignalingState state)
{
    switch (state) {
    case RTCSignalingState::Stable:
        return type == RTCSdpType::Offer;
    case RTCSignalingState::HaveLocalOffer:
        return type == RTCSdpType::Offer;
    case RTCSignalingState::HaveRemoteOffer:
        return type == RTCSdpType::Answer || type == RTCSdpType::Pranswer;
    case RTCSignalingState::HaveLocalPranswer:
        return type == RTCSdpType::Answer || type == RTCSdpType::Pranswer;
    default:
        return false;
    };

    ASSERT_NOT_REACHED();
    return false;
}

void PeerConnectionBackend::setLocalDescription(RTCSessionDescription& sessionDescription, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_peerConnection.isClosed());

    if (!isLocalDescriptionTypeValidForState(sessionDescription.type(), m_peerConnection.signalingState())) {
        promise.reject(InvalidStateError, "Description type incompatible with current signaling state");
        return;
    }

    m_setDescriptionPromise = WTFMove(promise);
    doSetLocalDescription(sessionDescription);
}

void PeerConnectionBackend::setLocalDescriptionSucceeded()
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Setting local description succeeded\n");

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->resolve();
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::setLocalDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Setting local description failed:\n%{public}s\n", exception.message().utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->reject(WTFMove(exception));
    m_setDescriptionPromise = std::nullopt;
}

static inline bool isRemoteDescriptionTypeValidForState(RTCSdpType type, RTCSignalingState state)
{
    switch (state) {
    case RTCSignalingState::Stable:
        return type == RTCSdpType::Offer;
    case RTCSignalingState::HaveLocalOffer:
        return type == RTCSdpType::Answer || type == RTCSdpType::Pranswer;
    case RTCSignalingState::HaveRemoteOffer:
        return type == RTCSdpType::Offer;
    case RTCSignalingState::HaveRemotePranswer:
        return type == RTCSdpType::Answer || type == RTCSdpType::Pranswer;
    default:
        return false;
    };

    ASSERT_NOT_REACHED();
    return false;
}

void PeerConnectionBackend::setRemoteDescription(RTCSessionDescription& sessionDescription, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_peerConnection.isClosed());

    if (!isRemoteDescriptionTypeValidForState(sessionDescription.type(), m_peerConnection.signalingState())) {
        promise.reject(InvalidStateError, "Description type incompatible with current signaling state");
        return;
    }

    m_setDescriptionPromise = WTFMove(promise);
    doSetRemoteDescription(sessionDescription);
}

void PeerConnectionBackend::setRemoteDescriptionSucceeded()
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Setting remote description succeeded\n");

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->resolve();
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::setRemoteDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Setting remote description failed:\n%{public}s\n", exception.message().utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->reject(WTFMove(exception));
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::addIceCandidate(RTCIceCandidate* iceCandidate, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_peerConnection.isClosed());

    if (!iceCandidate) {
        endOfIceCandidates(WTFMove(promise));
        return;
    }

    // FIXME: As per https://w3c.github.io/webrtc-pc/#dom-rtcpeerconnection-addicecandidate(), this check should be done before enqueuing the task.
    if (iceCandidate->sdpMid().isNull() && !iceCandidate->sdpMLineIndex()) {
        promise.reject(Exception { TypeError, ASCIILiteral("Trying to add a candidate that is missing both sdpMid and sdpMLineIndex") });
        return;
    }
    m_addIceCandidatePromise = WTFMove(promise);
    doAddIceCandidate(*iceCandidate);
}

void PeerConnectionBackend::addIceCandidateSucceeded()
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Adding ice candidate succeeded\n");

    if (m_peerConnection.isClosed())
        return;

    // FIXME: Update remote description and set ICE connection state to checking if not already done so.
    ASSERT(m_addIceCandidatePromise);

    m_addIceCandidatePromise->resolve();
    m_addIceCandidatePromise = std::nullopt;
}

void PeerConnectionBackend::addIceCandidateFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Adding ice candidate failed:\n%{public}s\n", exception.message().utf8().data());

    if (m_peerConnection.isClosed())
        return;

    ASSERT(m_addIceCandidatePromise);

    m_addIceCandidatePromise->reject(WTFMove(exception));
    m_addIceCandidatePromise = std::nullopt;
}

void PeerConnectionBackend::fireICECandidateEvent(RefPtr<RTCIceCandidate>&& candidate)
{
    ASSERT(isMainThread());

    m_peerConnection.fireEvent(RTCPeerConnectionIceEvent::create(false, false, WTFMove(candidate)));
}

void PeerConnectionBackend::enableICECandidateFiltering()
{
    m_shouldFilterICECandidates = true;
}

void PeerConnectionBackend::disableICECandidateFiltering()
{
    m_shouldFilterICECandidates = false;
    for (auto& pendingICECandidate : m_pendingICECandidates)
        fireICECandidateEvent(RTCIceCandidate::create(WTFMove(pendingICECandidate.sdp), WTFMove(pendingICECandidate.mid), pendingICECandidate.sdpMLineIndex));
    m_pendingICECandidates.clear();
}

static String filterICECandidate(String&& sdp)
{
    ASSERT(!sdp.contains(" host "));

    if (!sdp.contains(" raddr "))
        return WTFMove(sdp);

    bool skipNextItem = false;
    bool isFirst = true;
    StringBuilder filteredSDP;
    sdp.split(' ', false, [&](StringView item) {
        if (skipNextItem) {
            skipNextItem = false;
            return;
        }
        if (item == "raddr" || item == "rport") {
            skipNextItem = true;
            return;
        }
        if (isFirst)
            isFirst = false;
        else
            filteredSDP.append(' ');
        filteredSDP.append(item);
    });
    return filteredSDP.toString();
}

String PeerConnectionBackend::filterSDP(String&& sdp) const
{
    if (!m_shouldFilterICECandidates)
        return sdp;

    StringBuilder filteredSDP;
    sdp.split('\n', false, [&filteredSDP](StringView line) {
        if (!line.startsWith("a=candidate"))
            filteredSDP.append(line);
        else if (line.find(" host ", 11) == notFound)
            filteredSDP.append(filterICECandidate(line.toString()));
        else
            return;
        filteredSDP.append('\n');
    });
    return filteredSDP.toString();
}

void PeerConnectionBackend::newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex)
{
    RELEASE_LOG(WebRTC, "Gathered ice candidate:\n%{public}s\n", sdp.utf8().data());

    if (!m_shouldFilterICECandidates) {
        fireICECandidateEvent(RTCIceCandidate::create(WTFMove(sdp), WTFMove(mid), sdpMLineIndex));
        return;
    }
    if (sdp.find(" host ", 0) != notFound) {
        m_pendingICECandidates.append(PendingICECandidate { WTFMove(sdp), WTFMove(mid), sdpMLineIndex});
        return;
    }
    fireICECandidateEvent(RTCIceCandidate::create(filterICECandidate(WTFMove(sdp)), WTFMove(mid), sdpMLineIndex));
}

void PeerConnectionBackend::doneGatheringCandidates()
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "Finished ice candidate gathering\n");

    m_peerConnection.fireEvent(RTCPeerConnectionIceEvent::create(false, false, nullptr));
    m_peerConnection.updateIceGatheringState(RTCIceGatheringState::Complete);
}

void PeerConnectionBackend::updateSignalingState(RTCSignalingState newSignalingState)
{
    ASSERT(isMainThread());

    if (newSignalingState != m_peerConnection.signalingState()) {
        m_peerConnection.setSignalingState(newSignalingState);
        m_peerConnection.fireEvent(Event::create(eventNames().signalingstatechangeEvent, false, false));
    }
}

void PeerConnectionBackend::stop()
{
    m_offerAnswerPromise = std::nullopt;
    m_setDescriptionPromise = std::nullopt;
    m_addIceCandidatePromise = std::nullopt;

    doStop();
}

void PeerConnectionBackend::markAsNeedingNegotiation()
{
    if (m_negotiationNeeded)
        return;

    m_negotiationNeeded = true;

    if (m_peerConnection.signalingState() == RTCSignalingState::Stable)
        m_peerConnection.scheduleNegotiationNeededEvent();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
