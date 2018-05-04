/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
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
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "Page.h"
#include "RTCConfiguration.h"
#include "RTCController.h"
#include "RTCDataChannel.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnectionIceEvent.h"
#include "RTCSessionDescription.h"
#include "RTCTrackEvent.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MainThread.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;

Ref<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext& context)
{
    Ref<RTCPeerConnection> peerConnection = adoptRef(*new RTCPeerConnection(context));
    peerConnection->suspendIfNeeded();
    // RTCPeerConnection may send events at about any time during its lifetime.
    // Let's make it uncollectable until the pc is closed by JS or the page stops it.
    if (!peerConnection->isClosed()) {
        peerConnection->setPendingActivity(peerConnection.ptr());
        if (auto* page = downcast<Document>(context).page())
            peerConnection->registerToController(page->rtcController());
    }
    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
#if !RELEASE_LOG_DISABLED
    , m_logger(downcast<Document>(context).logger())
    , m_logIdentifier(reinterpret_cast<const void*>(cryptographicallyRandomNumber()))
#endif
    , m_backend(PeerConnectionBackend::create(*this))
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (!m_backend)
        m_connectionState = RTCPeerConnectionState::Closed;
}

RTCPeerConnection::~RTCPeerConnection()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    unregisterFromController();
    stop();
}

ExceptionOr<void> RTCPeerConnection::initializeWith(Document& document, RTCConfiguration&& configuration)
{
    if (!document.frame())
        return Exception { NotSupportedError };

    if (!m_backend)
        return Exception { NotSupportedError };

    return initializeConfiguration(WTFMove(configuration));
}

ExceptionOr<Ref<RTCRtpSender>> RTCPeerConnection::addTrack(Ref<MediaStreamTrack>&& track, const Vector<std::reference_wrapper<MediaStream>>& streams)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError };

    for (RTCRtpSender& sender : m_transceiverSet->senders()) {
        if (sender.trackId() == track->id())
            return Exception { InvalidAccessError };
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

    m_backend->notifyAddedTrack(*sender);
    return Ref<RTCRtpSender> { *sender };
}

ExceptionOr<void> RTCPeerConnection::removeTrack(RTCRtpSender& sender)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError };

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

    m_backend->notifyRemovedTrack(sender);
    return { };
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addTransceiver(AddTransceiverTrackOrKind&& withTrack, const RTCRtpTransceiverInit& init)
{
    INFO_LOG(LOGIDENTIFIER);

    if (WTF::holds_alternative<String>(withTrack)) {
        const String& kind = WTF::get<String>(withTrack);
        if (kind != "audio" && kind != "video")
            return Exception { TypeError };

        auto sender = RTCRtpSender::create(String(kind), Vector<String>(), *this);
        return completeAddTransceiver(WTFMove(sender), init, createCanonicalUUIDString(), kind);
    }

    Ref<MediaStreamTrack> track = WTF::get<RefPtr<MediaStreamTrack>>(withTrack).releaseNonNull();
    const String& trackId = track->id();
    const String& trackKind = track->kind();

    auto sender = RTCRtpSender::create(WTFMove(track), Vector<String>(), *this);
    return completeAddTransceiver(WTFMove(sender), init, trackId, trackKind);
}

Ref<RTCRtpTransceiver> RTCPeerConnection::completeAddTransceiver(Ref<RTCRtpSender>&& sender, const RTCRtpTransceiverInit& init, const String& trackId, const String& trackKind)
{
    String transceiverMid = RTCRtpTransceiver::getNextMid();
    auto transceiver = RTCRtpTransceiver::create(WTFMove(sender), m_backend->createReceiver(transceiverMid, trackKind, trackId));

    transceiver->setProvisionalMid(transceiverMid);
    transceiver->setDirection(init.direction);

    m_transceiverSet->append(transceiver.copyRef());
    return transceiver;
}

void RTCPeerConnection::queuedCreateOffer(RTCOfferOptions&& options, SessionDescriptionPromise&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    m_backend->createOffer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedCreateAnswer(RTCAnswerOptions&& options, SessionDescriptionPromise&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    m_backend->createAnswer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedSetLocalDescription(RTCSessionDescription& description, DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Setting local description to:\n", description.sdp());
    if (isClosed()) {
        promise.reject(InvalidStateError);
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

void RTCPeerConnection::queuedSetRemoteDescription(RTCSessionDescription& description, DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Setting remote description to:\n", description.sdp());

    if (isClosed()) {
        promise.reject(InvalidStateError);
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

void RTCPeerConnection::queuedAddIceCandidate(RTCIceCandidate* rtcCandidate, DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Received ice candidate:\n", rtcCandidate ? rtcCandidate->candidate() : "null");

    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    m_backend->addIceCandidate(rtcCandidate, WTFMove(promise));
}

// Implementation of https://w3c.github.io/webrtc-pc/#set-pc-configuration
static inline ExceptionOr<Vector<MediaEndpointConfiguration::IceServerInfo>> iceServersFromConfiguration(RTCConfiguration& newConfiguration, const RTCConfiguration* existingConfiguration, bool isLocalDescriptionSet)
{
    if (existingConfiguration && newConfiguration.bundlePolicy != existingConfiguration->bundlePolicy)
        return Exception { InvalidModificationError, "IceTransportPolicy does not match existing policy" };

    if (existingConfiguration && newConfiguration.iceCandidatePoolSize != existingConfiguration->iceCandidatePoolSize && isLocalDescriptionSet)
        return Exception { InvalidModificationError, "IceTransportPolicy pool size does not match existing pool size" };

    Vector<MediaEndpointConfiguration::IceServerInfo> servers;
    if (newConfiguration.iceServers) {
        servers.reserveInitialCapacity(newConfiguration.iceServers->size());
        for (auto& server : newConfiguration.iceServers.value()) {
            Vector<URL> serverURLs;
            WTF::switchOn(server.urls, [&serverURLs] (const String& string) {
                serverURLs.reserveInitialCapacity(1);
                serverURLs.uncheckedAppend(URL { URL { }, string });
            }, [&serverURLs] (const Vector<String>& vector) {
                serverURLs.reserveInitialCapacity(vector.size());
                for (auto& string : vector)
                    serverURLs.uncheckedAppend(URL { URL { }, string });
            });
            for (auto& serverURL : serverURLs) {
                if (serverURL.isNull())
                    return Exception { TypeError, "Bad ICE server URL" };
                if (serverURL.protocolIs("turn") || serverURL.protocolIs("turns")) {
                    if (server.credential.isNull() || server.username.isNull())
                        return Exception { InvalidAccessError, "TURN/TURNS server requires both username and credential" };
                } else if (!serverURL.protocolIs("stun"))
                    return Exception { NotSupportedError, "ICE server protocol not supported" };
            }
            if (serverURLs.size())
                servers.uncheckedAppend({ WTFMove(serverURLs), server.credential, server.username });
        }
    }
    return WTFMove(servers);
}

ExceptionOr<void> RTCPeerConnection::initializeConfiguration(RTCConfiguration&& configuration)
{
    INFO_LOG(LOGIDENTIFIER);

    auto servers = iceServersFromConfiguration(configuration, nullptr, false);
    if (servers.hasException())
        return servers.releaseException();

    if (!m_backend->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.iceCandidatePoolSize }))
        return Exception { InvalidAccessError, "Bad Configuration Parameters" };

    m_configuration = WTFMove(configuration);
    return { };
}

ExceptionOr<void> RTCPeerConnection::setConfiguration(RTCConfiguration&& configuration)
{
    if (isClosed())
        return Exception { InvalidStateError };

    INFO_LOG(LOGIDENTIFIER);

    auto servers = iceServersFromConfiguration(configuration, &m_configuration, m_backend->isLocalDescriptionSet());
    if (servers.hasException())
        return servers.releaseException();

    if (!m_backend->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.iceCandidatePoolSize }))
        return Exception { InvalidAccessError, "Bad Configuration Parameters" };

    m_configuration = WTFMove(configuration);
    return { };
}

void RTCPeerConnection::getStats(MediaStreamTrack* selector, Ref<DeferredPromise>&& promise)
{
    m_backend->getStats(selector, WTFMove(promise));
}

ExceptionOr<Ref<RTCDataChannel>> RTCPeerConnection::createDataChannel(ScriptExecutionContext& context, String&& label, RTCDataChannelInit&& options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError };

    if (options.negotiated && !options.negotiated.value() && (label.length() > 65535 || options.protocol.length() > 65535))
        return Exception { TypeError };

    if (options.maxPacketLifeTime && options.maxRetransmits)
        return Exception { TypeError };

    if (options.id && options.id.value() > 65534)
        return Exception { TypeError };
    
    auto channelHandler = m_backend->createDataChannelHandler(label, options);
    if (!channelHandler)
        return Exception { NotSupportedError };

    return RTCDataChannel::create(context, WTFMove(channelHandler), WTFMove(label), WTFMove(options));
}

bool RTCPeerConnection::doClose()
{
    if (isClosed())
        return false;

    m_connectionState = RTCPeerConnectionState::Closed;
    m_iceConnectionState = RTCIceConnectionState::Closed;
    m_signalingState = RTCSignalingState::Closed;

    for (auto& transceiver : m_transceiverSet->list()) {
        transceiver->stop();
        transceiver->sender().stop();
        transceiver->receiver().stop();
    }

    return true;
}

void RTCPeerConnection::close()
{
    if (!doClose())
        return;

    updateConnectionState();
    ASSERT(isClosed());
    scriptExecutionContext()->postTask([protectedThis = makeRef(*this)](ScriptExecutionContext&) {
        protectedThis->doStop();
    });
}

void RTCPeerConnection::emulatePlatformEvent(const String& action)
{
    m_backend->emulatePlatformEvent(action);
}

void RTCPeerConnection::stop()
{
    if (!doClose())
        return;

    doStop();
}

void RTCPeerConnection::doStop()
{
    if (m_isStopped)
        return;

    m_isStopped = true;

    m_backend->stop();

    unsetPendingActivity(this);
}

void RTCPeerConnection::registerToController(RTCController& controller)
{
    m_controller = &controller;
    m_controller->add(*this);
}

void RTCPeerConnection::unregisterFromController()
{
    if (m_controller)
        m_controller->remove(*this);
}

const char* RTCPeerConnection::activeDOMObjectName() const
{
    return "RTCPeerConnection";
}

bool RTCPeerConnection::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

bool RTCPeerConnection::hasPendingActivity() const
{
    return !m_isStopped;
}

void RTCPeerConnection::addTransceiver(Ref<RTCRtpTransceiver>&& transceiver)
{
    INFO_LOG(LOGIDENTIFIER);
    m_transceiverSet->append(WTFMove(transceiver));
}

void RTCPeerConnection::setSignalingState(RTCSignalingState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, newState);
    m_signalingState = newState;
}

void RTCPeerConnection::updateIceGatheringState(RTCIceGatheringState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, newState);

    scriptExecutionContext()->postTask([protectedThis = makeRef(*this), newState](ScriptExecutionContext&) {
        if (protectedThis->isClosed() || protectedThis->m_iceGatheringState == newState)
            return;

        protectedThis->m_iceGatheringState = newState;
        protectedThis->dispatchEvent(Event::create(eventNames().icegatheringstatechangeEvent, false, false));
        protectedThis->updateConnectionState();
    });
}

void RTCPeerConnection::updateIceConnectionState(RTCIceConnectionState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, newState);

    scriptExecutionContext()->postTask([protectedThis = makeRef(*this), newState](ScriptExecutionContext&) {
        if (protectedThis->isClosed() || protectedThis->m_iceConnectionState == newState)
            return;

        protectedThis->m_iceConnectionState = newState;
        protectedThis->dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, false, false));
        protectedThis->updateConnectionState();
    });
}

void RTCPeerConnection::updateConnectionState()
{
    RTCPeerConnectionState state;

    if (m_iceConnectionState == RTCIceConnectionState::Closed)
        state = RTCPeerConnectionState::Closed;
    else if (m_iceConnectionState == RTCIceConnectionState::Disconnected)
        state = RTCPeerConnectionState::Disconnected;
    else if (m_iceConnectionState == RTCIceConnectionState::Failed)
        state = RTCPeerConnectionState::Failed;
    else if (m_iceConnectionState == RTCIceConnectionState::New && m_iceGatheringState == RTCIceGatheringState::New)
        state = RTCPeerConnectionState::New;
    else if (m_iceConnectionState == RTCIceConnectionState::Checking || m_iceGatheringState == RTCIceGatheringState::Gathering)
        state = RTCPeerConnectionState::Connecting;
    else if ((m_iceConnectionState == RTCIceConnectionState::Completed || m_iceConnectionState == RTCIceConnectionState::Connected) && m_iceGatheringState == RTCIceGatheringState::Complete)
        state = RTCPeerConnectionState::Connected;
    else
        return;

    if (state == m_connectionState)
        return;

    INFO_LOG(LOGIDENTIFIER, "state changed from: " , m_connectionState, " to ", state);

    m_connectionState = state;
    dispatchEvent(Event::create(eventNames().connectionstatechangeEvent, false, false));
}

void RTCPeerConnection::scheduleNegotiationNeededEvent()
{
    scriptExecutionContext()->postTask([protectedThis = makeRef(*this)](ScriptExecutionContext&) {
        if (protectedThis->isClosed())
            return;
        if (!protectedThis->m_backend->isNegotiationNeeded())
            return;
        protectedThis->m_backend->clearNegotiationNeededState();
        protectedThis->dispatchEvent(Event::create(eventNames().negotiationneededEvent, false, false));
    });
}

void RTCPeerConnection::fireEvent(Event& event)
{
    dispatchEvent(event);
}

void RTCPeerConnection::enqueueReplaceTrackTask(RTCRtpSender& sender, Ref<MediaStreamTrack>&& withTrack, DOMPromiseDeferred<void>&& promise)
{
    scriptExecutionContext()->postTask([protectedThis = makeRef(*this), protectedSender = makeRef(sender), promise = WTFMove(promise), withTrack = WTFMove(withTrack)](ScriptExecutionContext&) mutable {
        if (protectedThis->isClosed())
            return;
        bool hasTrack = protectedSender->track();
        protectedSender->setTrack(WTFMove(withTrack));
        if (!hasTrack)
            protectedThis->m_backend->notifyAddedTrack(protectedSender.get());
        promise.resolve();
    });
}

void RTCPeerConnection::replaceTrack(RTCRtpSender& sender, RefPtr<MediaStreamTrack>&& withTrack, DOMPromiseDeferred<void>&& promise)
{
    INFO_LOG(LOGIDENTIFIER);

    if (!withTrack) {
        scriptExecutionContext()->postTask([protectedSender = makeRef(sender), promise = WTFMove(promise)](ScriptExecutionContext&) mutable {
            protectedSender->setTrackToNull();
            promise.resolve();
        });
        return;
    }
    
    if (!sender.track()) {
        enqueueReplaceTrackTask(sender, withTrack.releaseNonNull(), WTFMove(promise));
        return;
    }

    m_backend->replaceTrack(sender, withTrack.releaseNonNull(), WTFMove(promise));
}

RTCRtpParameters RTCPeerConnection::getParameters(RTCRtpSender& sender) const
{
    return m_backend->getParameters(sender);
}

void RTCPeerConnection::dispatchEvent(Event& event)
{
    DEBUG_LOG(LOGIDENTIFIER, "dispatching '", event.type(), "'");
    EventTarget::dispatchEvent(event);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCPeerConnection::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
