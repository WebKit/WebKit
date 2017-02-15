/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2016 Apple INC. All rights reserved.
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
#include "RTCIceCandidate.h"
#include "RTCIceCandidateEvent.h"
#include "RTCPeerConnection.h"

namespace WebCore {

void PeerConnectionBackend::createOffer(RTCOfferOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(m_peerConnection.internalSignalingState() != PeerConnectionStates::SignalingState::Closed);

    m_offerAnswerPromise = WTFMove(promise);
    doCreateOffer(WTFMove(options));
}

void PeerConnectionBackend::createOfferSucceeded(String&& sdp)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->resolve(RTCSessionDescription::create(RTCSessionDescription::SdpType::Offer, WTFMove(sdp)));
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createOfferFailed(Exception&& exception)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->reject(WTFMove(exception));
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createAnswer(RTCAnswerOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(m_peerConnection.internalSignalingState() != PeerConnectionStates::SignalingState::Closed);

    m_offerAnswerPromise = WTFMove(promise);
    doCreateAnswer(WTFMove(options));
}

void PeerConnectionBackend::createAnswerSucceeded(String&& sdp)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->resolve(RTCSessionDescription::create(RTCSessionDescription::SdpType::Answer, WTFMove(sdp)));
    m_offerAnswerPromise = std::nullopt;
}

void PeerConnectionBackend::createAnswerFailed(Exception&& exception)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_offerAnswerPromise);
    m_offerAnswerPromise->reject(WTFMove(exception));
    m_offerAnswerPromise = std::nullopt;
}

static inline bool isLocalDescriptionTypeValidForState(RTCSessionDescription::SdpType type, PeerConnectionStates::SignalingState state)
{
    switch (state) {
    case PeerConnectionStates::SignalingState::Stable:
        return type == RTCSessionDescription::SdpType::Offer;
    case PeerConnectionStates::SignalingState::HaveLocalOffer:
        return type == RTCSessionDescription::SdpType::Offer;
    case PeerConnectionStates::SignalingState::HaveRemoteOffer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    case PeerConnectionStates::SignalingState::HaveLocalPrAnswer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    default:
        return false;
    };

    ASSERT_NOT_REACHED();
    return false;
}

void PeerConnectionBackend::setLocalDescription(RTCSessionDescription& sessionDescription, DOMPromise<void>&& promise)
{
    ASSERT(m_peerConnection.internalSignalingState() != PeerConnectionStates::SignalingState::Closed);

    if (!isLocalDescriptionTypeValidForState(sessionDescription.type(), m_peerConnection.internalSignalingState())) {
        promise.reject(INVALID_STATE_ERR, "Description type incompatible with current signaling state");
        return;
    }

    m_setDescriptionPromise = WTFMove(promise);
    doSetLocalDescription(sessionDescription);
}

void PeerConnectionBackend::setLocalDescriptionSucceeded()
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->resolve();
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::setLocalDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->reject(WTFMove(exception));
    m_setDescriptionPromise = std::nullopt;
}

static inline bool isRemoteDescriptionTypeValidForState(RTCSessionDescription::SdpType type, PeerConnectionStates::SignalingState state)
{
    switch (state) {
    case PeerConnectionStates::SignalingState::Stable:
        return type == RTCSessionDescription::SdpType::Offer;
    case PeerConnectionStates::SignalingState::HaveLocalOffer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    case PeerConnectionStates::SignalingState::HaveRemoteOffer:
        return type == RTCSessionDescription::SdpType::Offer;
    case PeerConnectionStates::SignalingState::HaveRemotePrAnswer:
        return type == RTCSessionDescription::SdpType::Answer || type == RTCSessionDescription::SdpType::Pranswer;
    default:
        return false;
    };

    ASSERT_NOT_REACHED();
    return false;
}

void PeerConnectionBackend::setRemoteDescription(RTCSessionDescription& sessionDescription, DOMPromise<void>&& promise)
{
    ASSERT(m_peerConnection.internalSignalingState() != PeerConnectionStates::SignalingState::Closed);

    if (!isRemoteDescriptionTypeValidForState(sessionDescription.type(), m_peerConnection.internalSignalingState())) {
        promise.reject(INVALID_STATE_ERR, "Description type incompatible with current signaling state");
        return;
    }

    m_setDescriptionPromise = WTFMove(promise);
    doSetRemoteDescription(sessionDescription);
}

void PeerConnectionBackend::setRemoteDescriptionSucceeded()
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->resolve();
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::setRemoteDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_setDescriptionPromise);

    m_setDescriptionPromise->reject(WTFMove(exception));
    m_setDescriptionPromise = std::nullopt;
}

void PeerConnectionBackend::addIceCandidate(RTCIceCandidate& iceCandidate, DOMPromise<void>&& promise)
{
    ASSERT(m_peerConnection.internalSignalingState() != PeerConnectionStates::SignalingState::Closed);

    if (iceCandidate.sdpMid().isNull() && !iceCandidate.sdpMLineIndex()) {
        promise.reject(Exception { TypeError, ASCIILiteral("Trying to add a candidate that is missing both sdpMid and sdpMLineIndex") });
        return;
    }
    m_addIceCandidatePromise = WTFMove(promise);
    doAddIceCandidate(iceCandidate);
}

void PeerConnectionBackend::addIceCandidateSucceeded()
{
    ASSERT(isMainThread());

    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    // FIXME: Update remote description and set ICE connection state to checking if not already done so.
    ASSERT(m_addIceCandidatePromise);

    m_addIceCandidatePromise->resolve();
    m_addIceCandidatePromise = std::nullopt;
}

void PeerConnectionBackend::addIceCandidateFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Closed)
        return;

    ASSERT(m_addIceCandidatePromise);

    m_addIceCandidatePromise->reject(WTFMove(exception));
    m_addIceCandidatePromise = std::nullopt;
}

void PeerConnectionBackend::fireICECandidateEvent(RefPtr<RTCIceCandidate>&& candidate)
{
    ASSERT(isMainThread());

    m_peerConnection.fireEvent(RTCIceCandidateEvent::create(false, false, WTFMove(candidate)));
}

void PeerConnectionBackend::doneGatheringCandidates()
{
    ASSERT(isMainThread());

    m_peerConnection.fireEvent(RTCIceCandidateEvent::create(false, false, nullptr));
    m_peerConnection.updateIceGatheringState(PeerConnectionStates::IceGatheringState::Complete);
}

void PeerConnectionBackend::updateSignalingState(PeerConnectionStates::SignalingState newSignalingState)
{
    ASSERT(isMainThread());

    if (newSignalingState != m_peerConnection.internalSignalingState()) {
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
    
    if (m_peerConnection.internalSignalingState() == PeerConnectionStates::SignalingState::Stable)
        m_peerConnection.scheduleNegotiationNeededEvent();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
