/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
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
#include "RTCPeerConnection.h"

#if ENABLE(WEB_RTC)

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "MediaEndpointConfiguration.h"
#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "RTCConfiguration.h"
#include "RTCDataChannel.h"
#include "RTCIceCandidate.h"
#include "RTCIceCandidateEvent.h"
#include "RTCOfferAnswerOptions.h"
#include "RTCSessionDescription.h"
#include "RTCTrackEvent.h"
#include "UUID.h"
#include <wtf/MainThread.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;
using namespace PeerConnectionStates;

Ref<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext& context)
{
    Ref<RTCPeerConnection> peerConnection = adoptRef(*new RTCPeerConnection(context));
    peerConnection->suspendIfNeeded();

    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_backend(PeerConnectionBackend::create(*this))
{
}

RTCPeerConnection::~RTCPeerConnection()
{
    stop();
}

ExceptionOr<void> RTCPeerConnection::initializeWith(Document& document, RTCConfiguration&& configuration)
{
    if (!document.frame())
        return Exception { NOT_SUPPORTED_ERR };

    if (!m_backend)
        return Exception { NOT_SUPPORTED_ERR };

    return setConfiguration(WTFMove(configuration));
}

ExceptionOr<Ref<RTCRtpSender>> RTCPeerConnection::addTrack(Ref<MediaStreamTrack>&& track, const Vector<std::reference_wrapper<MediaStream>>& streams)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    // Require at least one stream until https://github.com/w3c/webrtc-pc/issues/288 is resolved
    if (!streams.size())
        return Exception { NOT_SUPPORTED_ERR };

    for (RTCRtpSender& sender : m_transceiverSet->senders()) {
        if (sender.trackId() == track->id())
            return Exception { INVALID_ACCESS_ERR };
    }

    Vector<String> mediaStreamIds;
    for (auto stream : streams)
        mediaStreamIds.append(stream.get().id());

    RTCRtpSender* sender = nullptr;

    // Reuse an existing sender with the same track kind if it has never been used to send before.
    for (auto& transceiver : m_transceiverSet->list()) {
        auto& existingSender = transceiver->sender();
        if (existingSender.trackKind() == track->kind() && existingSender.trackId().isNull() && !transceiver->hasSendingDirection()) {
            existingSender.setTrack(WTFMove(track));
            existingSender.setMediaStreamIds(WTFMove(mediaStreamIds));
            transceiver->enableSendingDirection();
            sender = &existingSender;
            break;
        }
    }

    if (!sender) {
        String transceiverMid = RTCRtpTransceiver::getNextMid();
        const String& trackKind = track->kind();
        String trackId = createCanonicalUUIDString();

        auto newSender = RTCRtpSender::create(WTFMove(track), WTFMove(mediaStreamIds), *this);
        auto receiver = m_backend->createReceiver(transceiverMid, trackKind, trackId);
        auto transceiver = RTCRtpTransceiver::create(WTFMove(newSender), WTFMove(receiver));

        // This transceiver is not yet associated with an m-line (null mid), but we need a
        // provisional mid if the transceiver is used to create an offer.
        transceiver->setProvisionalMid(transceiverMid);

        sender = &transceiver->sender();
        m_transceiverSet->append(WTFMove(transceiver));
    }

    m_backend->markAsNeedingNegotiation();

    return Ref<RTCRtpSender> { *sender };
}

ExceptionOr<void> RTCPeerConnection::removeTrack(RTCRtpSender& sender)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    bool shouldAbort = true;
    for (RTCRtpSender& senderInSet : m_transceiverSet->senders()) {
        if (&senderInSet == &sender) {
            shouldAbort = sender.isStopped();
            break;
        }
    }
    if (shouldAbort)
        return { };

    sender.stop();

    m_backend->markAsNeedingNegotiation();
    return { };
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addTransceiver(Ref<MediaStreamTrack>&& track, const RtpTransceiverInit& init)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    String transceiverMid = RTCRtpTransceiver::getNextMid();
    const String& trackKind = track->kind();
    const String& trackId = track->id();

    auto sender = RTCRtpSender::create(WTFMove(track), Vector<String>(), *this);
    auto receiver = m_backend->createReceiver(transceiverMid, trackKind, trackId);
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver));
    transceiver->setProvisionalMid(transceiverMid);

    completeAddTransceiver(transceiver, init);
    return WTFMove(transceiver);
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addTransceiver(const String& kind, const RtpTransceiverInit& init)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    if (kind != "audio" && kind != "video")
        return Exception { TypeError };

    String transceiverMid = RTCRtpTransceiver::getNextMid();
    String trackId = createCanonicalUUIDString();

    auto sender = RTCRtpSender::create(kind, Vector<String>(), *this);
    auto receiver = m_backend->createReceiver(transceiverMid, kind, trackId);
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), WTFMove(receiver));
    transceiver->setProvisionalMid(transceiverMid);

    completeAddTransceiver(transceiver, init);
    return WTFMove(transceiver);
}

void RTCPeerConnection::completeAddTransceiver(RTCRtpTransceiver& transceiver, const RtpTransceiverInit& init)
{
    transceiver.setDirection(static_cast<RTCRtpTransceiver::Direction>(init.direction));

    m_transceiverSet->append(transceiver);
    m_backend->markAsNeedingNegotiation();
}

void RTCPeerConnection::queuedCreateOffer(RTCOfferOptions&& options, SessionDescriptionPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    m_backend->createOffer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedCreateAnswer(RTCAnswerOptions&& options, SessionDescriptionPromise&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    m_backend->createAnswer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedSetLocalDescription(RTCSessionDescription& description, DOMPromise<void>&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    m_backend->setLocalDescription(description, WTFMove(promise));
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

void RTCPeerConnection::queuedSetRemoteDescription(RTCSessionDescription& description, DOMPromise<void>&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    m_backend->setRemoteDescription(description, WTFMove(promise));
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

void RTCPeerConnection::queuedAddIceCandidate(RTCIceCandidate& rtcCandidate, DOMPromise<void>&& promise)
{
    if (m_signalingState == SignalingState::Closed) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    m_backend->addIceCandidate(rtcCandidate, WTFMove(promise));
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

ExceptionOr<void> RTCPeerConnection::setConfiguration(RTCConfiguration&& configuration)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    Vector<MediaEndpointConfiguration::IceServerInfo> servers;
    if (configuration.iceServers) {
        servers.reserveInitialCapacity(configuration.iceServers->size());
        for (auto& server : configuration.iceServers.value()) {
            Vector<URL> serverURLs;
            WTF::switchOn(server.urls,
                [&serverURLs] (const String& string) {
                    serverURLs.reserveInitialCapacity(1);
                    serverURLs.uncheckedAppend(URL { URL { }, string });
                },
                [&serverURLs] (const Vector<String>& vector) {
                    serverURLs.reserveInitialCapacity(vector.size());
                    for (auto& string : vector)
                        serverURLs.uncheckedAppend(URL { URL { }, string });
                }
            );
            for (auto& serverURL : serverURLs) {
                if (!(serverURL.protocolIs("turn") || serverURL.protocolIs("turns") || serverURL.protocolIs("stun")))
                    return Exception { INVALID_ACCESS_ERR };
            }
            servers.uncheckedAppend({ WTFMove(serverURLs), server.credential, server.username });
        }
    }

    m_backend->setConfiguration({ WTFMove(servers), configuration.iceTransportPolicy, configuration.bundlePolicy });
    m_configuration = WTFMove(configuration);
    return { };
}

void RTCPeerConnection::getStats(MediaStreamTrack* selector, Ref<DeferredPromise>&& promise)
{
    m_backend->getStats(selector, WTFMove(promise));
}

ExceptionOr<Ref<RTCDataChannel>> RTCPeerConnection::createDataChannel(ScriptExecutionContext& context, String&& label, RTCDataChannelInit&& options)
{
    if (m_signalingState == SignalingState::Closed)
        return Exception { INVALID_STATE_ERR };

    // FIXME: Check options
    auto channelHandler = m_backend->createDataChannelHandler(label, options);
    if (!channelHandler)
        return Exception { NOT_SUPPORTED_ERR };

    return RTCDataChannel::create(context, WTFMove(channelHandler), WTFMove(label), WTFMove(options));
}

void RTCPeerConnection::close()
{
    if (m_signalingState == SignalingState::Closed)
        return;

    m_backend->stop();

    m_iceConnectionState = IceConnectionState::Closed;
    m_signalingState = SignalingState::Closed;

    for (RTCRtpSender& sender : m_transceiverSet->senders())
        sender.stop();
}

void RTCPeerConnection::emulatePlatformEvent(const String& action)
{
    m_backend->emulatePlatformEvent(action);
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

void RTCPeerConnection::addTransceiver(Ref<RTCRtpTransceiver>&& transceiver)
{
    m_transceiverSet->append(WTFMove(transceiver));
}

void RTCPeerConnection::setSignalingState(SignalingState newState)
{
    m_signalingState = newState;
}

void RTCPeerConnection::updateIceGatheringState(IceGatheringState newState)
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        if (m_signalingState == SignalingState::Closed || m_iceGatheringState == newState)
            return;

        m_iceGatheringState = newState;
        dispatchEvent(Event::create(eventNames().icegatheringstatechangeEvent, false, false));
    });
}

void RTCPeerConnection::updateIceConnectionState(IceConnectionState newState)
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        if (m_signalingState == SignalingState::Closed || m_iceConnectionState == newState)
            return;

        m_iceConnectionState = newState;
        dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, false, false));
    });
}

void RTCPeerConnection::scheduleNegotiationNeededEvent()
{
    scriptExecutionContext()->postTask([=](ScriptExecutionContext&) {
        if (m_backend->isNegotiationNeeded()) {
            m_backend->clearNegotiationNeededState();
            dispatchEvent(Event::create(eventNames().negotiationneededEvent, false, false));
        }
    });
}

void RTCPeerConnection::fireEvent(Event& event)
{
    dispatchEvent(event);
}

void RTCPeerConnection::replaceTrack(RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& withTrack, DOMPromise<void>&& promise)
{
    m_backend->replaceTrack(sender, WTFMove(withTrack), WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
