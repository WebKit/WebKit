/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#if ENABLE(WEB_RTC)

#include "RTCPeerConnection.h"

#include "DOMError.h"
#include "Document.h"
#include "Event.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSDOMError.h"
#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "RTCConfiguration.h"
#include "RTCDataChannel.h"
#include "RTCIceCandidate.h"
#include "RTCIceCandidateEvent.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCSessionDescription.h"
#include "RTCTrackEvent.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;
using namespace PeerConnectionStates;

RefPtr<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext& context, const Dictionary& rtcConfiguration, ExceptionCode& ec)
{
    RefPtr<RTCConfiguration> configuration = RTCConfiguration::create(rtcConfiguration, ec);
    if (ec)
        return nullptr;

    RefPtr<RTCPeerConnection> peerConnection = adoptRef(new RTCPeerConnection(context, WTFMove(configuration), ec));
    peerConnection->suspendIfNeeded();
    if (ec)
        return nullptr;

    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(ScriptExecutionContext& context, RefPtr<RTCConfiguration>&& configuration, ExceptionCode& ec)
    : ActiveDOMObject(&context)
    , m_signalingState(SignalingState::Stable)
    , m_iceGatheringState(IceGatheringState::New)
    , m_iceConnectionState(IceConnectionState::New)
    , m_configuration(WTFMove(configuration))
{
    Document& document = downcast<Document>(context);

    if (!document.frame()) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_backend = PeerConnectionBackend::create(this);
    if (!m_backend) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_backend->setConfiguration(*m_configuration);
}

RTCPeerConnection::~RTCPeerConnection()
{
    stop();
}

RefPtr<RTCRtpSender> RTCPeerConnection::addTrack(RefPtr<MediaStreamTrack>&& track, Vector<MediaStream*> streams, ExceptionCode& ec)
{
    if (!track) {
        ec = TypeError;
        return nullptr;
    }

    if (m_signalingState == SignalingState::Closed) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    // Require at least one stream until https://github.com/w3c/webrtc-pc/issues/288 is resolved
    if (!streams.size()) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    for (auto& sender : m_senderSet) {
        if (sender->trackId() == track->id()) {
            // FIXME: Spec says InvalidParameter
            ec = INVALID_MODIFICATION_ERR;
            return nullptr;
        }
    }

    Vector<String> mediaStreamIds;
    for (auto stream : streams)
        mediaStreamIds.append(stream->id());

    RefPtr<RTCRtpSender> sender = RTCRtpSender::create(WTFMove(track), WTFMove(mediaStreamIds), *this);
    m_senderSet.append(sender);

    m_backend->markAsNeedingNegotiation();

    return sender;
}

void RTCPeerConnection::removeTrack(RTCRtpSender* sender, ExceptionCode& ec)
{
    if (!sender) {
        ec = TypeError;
        return;
    }

    if (m_signalingState == SignalingState::Closed) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!m_senderSet.contains(sender))
        return;

    sender->stop();

    m_backend->markAsNeedingNegotiation();
}

void RTCPeerConnection::queuedCreateOffer(const Dictionary& offerOptions, SessionDescriptionPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    ExceptionCode ec = 0;
    RefPtr<RTCOfferOptions> options = RTCOfferOptions::create(offerOptions, ec);
    if (ec) {
        promise.reject(DOMError::create("Invalid createOffer argument"));
        return;
    }
    ASSERT(options);

    m_backend->createOffer(*options, WTFMove(promise));
}

void RTCPeerConnection::queuedCreateAnswer(const Dictionary& answerOptions, SessionDescriptionPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    ExceptionCode ec = 0;
    RefPtr<RTCAnswerOptions> options = RTCAnswerOptions::create(answerOptions, ec);
    if (ec) {
        promise.reject(DOMError::create("Invalid createAnswer argument"));
        return;
    }

    m_backend->createAnswer(*options, WTFMove(promise));
}

void RTCPeerConnection::queuedSetLocalDescription(RTCSessionDescription* description, PeerConnection::VoidPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    ASSERT(description);
    m_backend->setLocalDescription(*description, WTFMove(promise));
}

RefPtr<RTCSessionDescription> RTCPeerConnection::localDescription() const
{
    return m_backend->localDescription();
}

RefPtr<RTCSessionDescription> RTCPeerConnection::currentLocalDescription() const
{
    return m_backend->currentLocalDescription();
}

RefPtr<RTCSessionDescription> RTCPeerConnection::pendingLocalDescription() const
{
    return m_backend->pendingLocalDescription();
}

void RTCPeerConnection::queuedSetRemoteDescription(RTCSessionDescription* description, PeerConnection::VoidPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    ASSERT(description);
    m_backend->setRemoteDescription(*description, WTFMove(promise));
}

RefPtr<RTCSessionDescription> RTCPeerConnection::remoteDescription() const
{
    return m_backend->remoteDescription();
}

RefPtr<RTCSessionDescription> RTCPeerConnection::currentRemoteDescription() const
{
    return m_backend->currentRemoteDescription();
}

RefPtr<RTCSessionDescription> RTCPeerConnection::pendingRemoteDescription() const
{
    return m_backend->pendingRemoteDescription();
}

void RTCPeerConnection::queuedAddIceCandidate(RTCIceCandidate* rtcCandidate, VoidPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    ASSERT(rtcCandidate);
    m_backend->addIceCandidate(*rtcCandidate, WTFMove(promise));
}

String RTCPeerConnection::signalingState() const
{
    switch (m_signalingState) {
    case SignalingState::Stable:
        return ASCIILiteral("stable");
    case SignalingState::HaveLocalOffer:
        return ASCIILiteral("have-local-offer");
    case SignalingState::HaveRemoteOffer:
        return ASCIILiteral("have-remote-offer");
    case SignalingState::HaveLocalPrAnswer:
        return ASCIILiteral("have-local-pranswer");
    case SignalingState::HaveRemotePrAnswer:
        return ASCIILiteral("have-remote-pranswer");
    case SignalingState::Closed:
        return ASCIILiteral("closed");
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceGatheringState() const
{
    switch (m_iceGatheringState) {
    case IceGatheringState::New:
        return ASCIILiteral("new");
    case IceGatheringState::Gathering:
        return ASCIILiteral("gathering");
    case IceGatheringState::Complete:
        return ASCIILiteral("complete");
    }

    ASSERT_NOT_REACHED();
    return String();
}

String RTCPeerConnection::iceConnectionState() const
{
    switch (m_iceConnectionState) {
    case IceConnectionState::New:
        return ASCIILiteral("new");
    case IceConnectionState::Checking:
        return ASCIILiteral("checking");
    case IceConnectionState::Connected:
        return ASCIILiteral("connected");
    case IceConnectionState::Completed:
        return ASCIILiteral("completed");
    case IceConnectionState::Failed:
        return ASCIILiteral("failed");
    case IceConnectionState::Disconnected:
        return ASCIILiteral("disconnected");
    case IceConnectionState::Closed:
        return ASCIILiteral("closed");
    }

    ASSERT_NOT_REACHED();
    return String();
}

RTCConfiguration* RTCPeerConnection::getConfiguration() const
{
    return m_configuration.get();
}

void RTCPeerConnection::setConfiguration(const Dictionary& configuration, ExceptionCode& ec)
{
    if (configuration.isUndefinedOrNull()) {
        ec = TypeError;
        return;
    }

    if (m_signalingState == SignalingState::Closed) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<RTCConfiguration> newConfiguration = RTCConfiguration::create(configuration, ec);
    if (ec)
        return;

    m_configuration = WTFMove(newConfiguration);
    m_backend->setConfiguration(*m_configuration);
}

void RTCPeerConnection::privateGetStats(MediaStreamTrack* selector, PeerConnection::StatsPromise&& promise)
{
    m_backend->getStats(selector, WTFMove(promise));
}

void RTCPeerConnection::privateGetStats(PeerConnection::StatsPromise&& promise)
{
    privateGetStats(nullptr, WTFMove(promise));
}

RefPtr<RTCDataChannel> RTCPeerConnection::createDataChannel(String, const Dictionary&, ExceptionCode& ec)
{
    if (m_signalingState == SignalingState::Closed) {
        ec = INVALID_STATE_ERR;
        return nullptr;
    }

    return nullptr;
}

void RTCPeerConnection::close()
{
    if (m_signalingState == SignalingState::Closed)
        return;

    m_backend->stop();

    m_iceConnectionState = IceConnectionState::Closed;
    m_signalingState = SignalingState::Closed;

    for (auto& sender : m_senderSet)
        sender->stop();
}

void RTCPeerConnection::stop()
{
    close();
}

const char* RTCPeerConnection::activeDOMObjectName() const
{
    return "RTCPeerConnection";
}

bool RTCPeerConnection::canSuspendForDocumentSuspension() const
{
    // FIXME: We should try and do better here.
    return false;
}

void RTCPeerConnection::addReceiver(RTCRtpReceiver& receiver)
{
    m_receiverSet.append(&receiver);
}

void RTCPeerConnection::setSignalingState(SignalingState newState)
{
    m_signalingState = newState;
}

void RTCPeerConnection::updateIceGatheringState(IceGatheringState newState)
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        m_iceGatheringState = newState;

        dispatchEvent(Event::create(eventNames().icegatheringstatechangeEvent, false, false));
    });
}

void RTCPeerConnection::updateIceConnectionState(IceConnectionState newState)
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        m_iceConnectionState = newState;

        dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, false, false));
    });
}

void RTCPeerConnection::scheduleNegotiationNeededEvent()
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        if (m_backend->isNegotiationNeeded()) {
            dispatchEvent(Event::create(eventNames().negotiationneededEvent, false, false));
            m_backend->clearNegotiationNeededState();
        }
    });
}

void RTCPeerConnection::fireEvent(Event& event)
{
    dispatchEvent(event);
}

void RTCPeerConnection::replaceTrack(RTCRtpSender& sender, MediaStreamTrack& withTrack, PeerConnection::VoidPromise&& promise)
{
    m_backend->replaceTrack(sender, withTrack, WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
