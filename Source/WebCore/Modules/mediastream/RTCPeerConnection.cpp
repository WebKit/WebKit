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
#include "JSDOMPromiseDeferred.h"
#include "JSRTCPeerConnection.h"
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
#include "Settings.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

namespace WebCore {

using namespace PeerConnection;

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCPeerConnection);

Ref<RTCPeerConnection> RTCPeerConnection::create(ScriptExecutionContext& context)
{
    auto& document = downcast<Document>(context);
    auto peerConnection = adoptRef(*new RTCPeerConnection(document));
    peerConnection->suspendIfNeeded();
    if (!peerConnection->isClosed()) {
        if (auto* page = document.page()) {
            peerConnection->registerToController(page->rtcController());
            page->libWebRTCProvider().setEnableLogging(!page->sessionID().isEphemeral());
        }
    }
    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(Document& document)
    : ActiveDOMObject(document)
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(reinterpret_cast<const void*>(cryptographicallyRandomNumber()))
#endif
    , m_backend(PeerConnectionBackend::create(*this))
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if !RELEASE_LOG_DISABLED
    auto* page = document.page();
    if (page && !page->settings().webRTCEncryptionEnabled())
        ALWAYS_LOG(LOGIDENTIFIER, "encryption is disabled");
#endif

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

    for (auto& transceiver : m_transceiverSet->list()) {
        if (transceiver->sender().trackId() == track->id())
            return Exception { InvalidAccessError };
    }

    Vector<String> mediaStreamIds;
    for (auto stream : streams)
        mediaStreamIds.append(stream.get().id());

    return m_backend->addTrack(track.get(), WTFMove(mediaStreamIds));
}

ExceptionOr<void> RTCPeerConnection::removeTrack(RTCRtpSender& sender)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError, "RTCPeerConnection is closed"_s };

    if (!sender.isCreatedBy(*m_backend))
        return Exception { InvalidAccessError, "RTCPeerConnection did not create the given sender"_s };

    bool shouldAbort = true;
    RTCRtpTransceiver* senderTransceiver = nullptr;
    for (auto& transceiver : m_transceiverSet->list()) {
        if (&sender == &transceiver->sender()) {
            senderTransceiver = transceiver.get();
            shouldAbort = sender.isStopped() || !sender.track();
            break;
        }
    }
    if (shouldAbort)
        return { };

    sender.setTrackToNull();
    senderTransceiver->disableSendingDirection();
    m_backend->removeTrack(sender);
    return { };
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addTransceiver(AddTransceiverTrackOrKind&& withTrack, const RTCRtpTransceiverInit& init)
{
    INFO_LOG(LOGIDENTIFIER);

    if (WTF::holds_alternative<String>(withTrack)) {
        const String& kind = WTF::get<String>(withTrack);
        if (kind != "audio"_s && kind != "video"_s)
            return Exception { TypeError };

        if (isClosed())
            return Exception { InvalidStateError };

        return m_backend->addTransceiver(kind, init);
    }

    if (isClosed())
        return Exception { InvalidStateError };

    auto track = WTF::get<RefPtr<MediaStreamTrack>>(withTrack).releaseNonNull();
    return m_backend->addTransceiver(WTFMove(track), init);
}

void RTCPeerConnection::queuedCreateOffer(RTCOfferOptions&& options, SessionDescriptionPromise&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    addPendingPromise(promise);
    m_backend->createOffer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedCreateAnswer(RTCAnswerOptions&& options, SessionDescriptionPromise&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    addPendingPromise(promise);
    m_backend->createAnswer(WTFMove(options), WTFMove(promise));
}

void RTCPeerConnection::queuedSetLocalDescription(RTCSessionDescription& description, DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Setting local description to:\n", description.sdp());
    if (isClosed()) {
        promise.reject(InvalidStateError);
        return;
    }

    addPendingPromise(promise);
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
    addPendingPromise(promise);
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

    addPendingPromise(promise);
    m_backend->addIceCandidate(rtcCandidate, WTFMove(promise));
}

// Implementation of https://w3c.github.io/webrtc-pc/#set-pc-configuration
static inline ExceptionOr<Vector<MediaEndpointConfiguration::IceServerInfo>> iceServersFromConfiguration(RTCConfiguration& newConfiguration, const RTCConfiguration* existingConfiguration, bool isLocalDescriptionSet)
{
    if (existingConfiguration && newConfiguration.bundlePolicy != existingConfiguration->bundlePolicy)
        return Exception { InvalidModificationError, "BundlePolicy does not match existing policy" };

    if (existingConfiguration && newConfiguration.rtcpMuxPolicy != existingConfiguration->rtcpMuxPolicy)
        return Exception { InvalidModificationError, "RTCPMuxPolicy does not match existing policy" };

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
    return servers;
}

ExceptionOr<Vector<MediaEndpointConfiguration::CertificatePEM>> RTCPeerConnection::certificatesFromConfiguration(const RTCConfiguration& configuration)
{
    auto currentMilliSeconds = WallTime::now().secondsSinceEpoch().milliseconds();
    auto& origin = document()->securityOrigin();

    Vector<MediaEndpointConfiguration::CertificatePEM> certificates;
    certificates.reserveInitialCapacity(configuration.certificates.size());
    for (auto& certificate : configuration.certificates) {
        if (!origin.isSameOriginAs(certificate->origin()))
            return Exception { InvalidAccessError, "Certificate does not have a valid origin" };

        if (currentMilliSeconds > certificate->expires())
            return Exception { InvalidAccessError, "Certificate has expired"_s };

        certificates.uncheckedAppend(MediaEndpointConfiguration::CertificatePEM { certificate->pemCertificate(), certificate->pemPrivateKey(), });
    }
    return certificates;
}

ExceptionOr<void> RTCPeerConnection::initializeConfiguration(RTCConfiguration&& configuration)
{
    INFO_LOG(LOGIDENTIFIER);

    auto servers = iceServersFromConfiguration(configuration, nullptr, false);
    if (servers.hasException())
        return servers.releaseException();

    auto certificates = certificatesFromConfiguration(configuration);
    if (certificates.hasException())
        return certificates.releaseException();

    if (!m_backend->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.rtcpMuxPolicy, configuration.iceCandidatePoolSize, certificates.releaseReturnValue() }))
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

    if (configuration.certificates.size()) {
        if (configuration.certificates.size() != m_configuration.certificates.size())
            return Exception { InvalidModificationError, "Certificates parameters are different" };

        for (auto& certificate : configuration.certificates) {
            bool isThere = m_configuration.certificates.findMatching([&certificate](const auto& item) {
                return item.get() == certificate.get();
            }) != notFound;
            if (!isThere)
                return Exception { InvalidModificationError, "A certificate given in constructor is not present" };
        }
    }

    if (!m_backend->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.rtcpMuxPolicy, configuration.iceCandidatePoolSize, { } }))
        return Exception { InvalidAccessError, "Bad Configuration Parameters" };

    m_configuration = WTFMove(configuration);
    return { };
}

void RTCPeerConnection::getStats(MediaStreamTrack* selector, Ref<DeferredPromise>&& promise)
{
    if (selector) {
        for (auto& transceiver : m_transceiverSet->list()) {
            if (transceiver->sender().track() == selector) {
                m_backend->getStats(transceiver->sender(), WTFMove(promise));
                return;
            }
            if (&transceiver->receiver().track() == selector) {
                m_backend->getStats(transceiver->receiver(), WTFMove(promise));
                return;
            }
        }
    }
    addPendingPromise(promise.get());
    m_backend->getStats(WTFMove(promise));
}

ExceptionOr<Ref<RTCDataChannel>> RTCPeerConnection::createDataChannel(String&& label, RTCDataChannelInit&& options)
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

    return RTCDataChannel::create(*document(), WTFMove(channelHandler), WTFMove(label), WTFMove(options));
}

bool RTCPeerConnection::doClose()
{
    if (isClosed())
        return false;

    m_shouldDelayTasks = false;
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
    m_backend->close();
}

void RTCPeerConnection::emulatePlatformEvent(const String& action)
{
    m_backend->emulatePlatformEvent(action);
}

void RTCPeerConnection::stop()
{
    doClose();
    doStop();
}

void RTCPeerConnection::doStop()
{
    if (m_isStopped)
        return;

    m_isStopped = true;
    if (m_backend)
        m_backend->stop();
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

void RTCPeerConnection::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    m_shouldDelayTasks = true;
    m_backend->suspend();
}

void RTCPeerConnection::resume()
{
    if (!m_shouldDelayTasks)
        return;

    m_shouldDelayTasks = false;
    m_backend->resume();

    scriptExecutionContext()->postTask([this, protectedThis = makeRef(*this)](auto&) {
        if (m_isStopped || m_shouldDelayTasks)
            return;

        auto tasks = WTFMove(m_pendingTasks);
        for (auto& task : tasks)
            task();
    });
}

bool RTCPeerConnection::hasPendingActivity() const
{
    if (m_isStopped)
        return false;

    // This returns true if we have pending promises to be resolved.
    if (ActiveDOMObject::hasPendingActivity())
        return true;

    // As long as the connection is not stopped and it has event listeners, it may dispatch events.
    return hasEventListeners();
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
        protectedThis->dispatchEventWhenFeasible(Event::create(eventNames().icegatheringstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
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
        protectedThis->dispatchEventWhenFeasible(Event::create(eventNames().iceconnectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
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
    dispatchEventWhenFeasible(Event::create(eventNames().connectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCPeerConnection::scheduleNegotiationNeededEvent()
{
    scriptExecutionContext()->postTask([protectedThis = makeRef(*this)](ScriptExecutionContext&) {
        if (protectedThis->isClosed())
            return;
        if (!protectedThis->m_backend->isNegotiationNeeded())
            return;
        protectedThis->m_backend->clearNegotiationNeededState();
        protectedThis->dispatchEventWhenFeasible(Event::create(eventNames().negotiationneededEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void RTCPeerConnection::doTask(Function<void()>&& task)
{
    if (m_shouldDelayTasks || !m_pendingTasks.isEmpty()) {
        m_pendingTasks.append(WTFMove(task));
        return;
    }
    task();
}

void RTCPeerConnection::dispatchEventWhenFeasible(Ref<Event>&& event)
{
    doTask([this, event = WTFMove(event)] {
        dispatchEvent(event);
    });
}

void RTCPeerConnection::dispatchEvent(Event& event)
{
    INFO_LOG(LOGIDENTIFIER, "dispatching '", event.type(), "'");
    EventTarget::dispatchEvent(event);
}

static inline ExceptionOr<PeerConnectionBackend::CertificateInformation> certificateTypeFromAlgorithmIdentifier(JSC::JSGlobalObject& lexicalGlobalObject, RTCPeerConnection::AlgorithmIdentifier&& algorithmIdentifier)
{
    if (WTF::holds_alternative<String>(algorithmIdentifier))
        return Exception { NotSupportedError, "Algorithm is not supported"_s };

    auto& value = WTF::get<JSC::Strong<JSC::JSObject>>(algorithmIdentifier);

    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto parameters = convertDictionary<RTCPeerConnection::CertificateParameters>(lexicalGlobalObject, value.get());
    if (UNLIKELY(scope.exception())) {
        scope.clearException();
        return Exception { TypeError, "Unable to read certificate parameters"_s };
    }

    if (parameters.expires && *parameters.expires < 0)
        return Exception { TypeError, "Expire value is invalid"_s };

    if (parameters.name == "RSASSA-PKCS1-v1_5"_s) {
        if (!parameters.hash.isNull() && parameters.hash != "SHA-256"_s)
            return Exception { NotSupportedError, "Only SHA-256 is supported for RSASSA-PKCS1-v1_5"_s };

        auto result = PeerConnectionBackend::CertificateInformation::RSASSA_PKCS1_v1_5();
        if (parameters.modulusLength && parameters.publicExponent) {
            int publicExponent = 0;
            int value = 1;
            for (unsigned counter = 0; counter < parameters.publicExponent->byteLength(); ++counter) {
                publicExponent += parameters.publicExponent->data()[counter] * value;
                value <<= 8;
            }

            result.rsaParameters = PeerConnectionBackend::CertificateInformation::RSA { *parameters.modulusLength, publicExponent };
        }
        result.expires = parameters.expires;
        return result;
    }
    if (parameters.name == "ECDSA"_s && parameters.namedCurve == "P-256"_s) {
        auto result = PeerConnectionBackend::CertificateInformation::ECDSA_P256();
        result.expires = parameters.expires;
        return result;
    }

    return Exception { NotSupportedError, "Algorithm is not supported"_s };
}

void RTCPeerConnection::generateCertificate(JSC::JSGlobalObject& lexicalGlobalObject, AlgorithmIdentifier&& algorithmIdentifier, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
{
    auto parameters = certificateTypeFromAlgorithmIdentifier(lexicalGlobalObject, WTFMove(algorithmIdentifier));
    if (parameters.hasException()) {
        promise.reject(parameters.releaseException());
        return;
    }
    auto& document = downcast<Document>(*JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->scriptExecutionContext());
    PeerConnectionBackend::generateCertificate(document, parameters.returnValue(), WTFMove(promise));
}

Vector<std::reference_wrapper<RTCRtpSender>> RTCPeerConnection::getSenders() const
{
    m_backend->collectTransceivers();
    return m_transceiverSet->senders();
}

Vector<std::reference_wrapper<RTCRtpReceiver>> RTCPeerConnection::getReceivers() const
{
    m_backend->collectTransceivers();
    return m_transceiverSet->receivers();
}

const Vector<RefPtr<RTCRtpTransceiver>>& RTCPeerConnection::getTransceivers() const
{
    m_backend->collectTransceivers();
    return m_transceiverSet->list();
}

Document* RTCPeerConnection::document()
{
    return downcast<Document>(scriptExecutionContext());
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCPeerConnection::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
