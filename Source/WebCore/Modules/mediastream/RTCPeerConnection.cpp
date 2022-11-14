/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#include "FrameDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCPeerConnection.h"
#include "JSRTCSessionDescriptionInit.h"
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "Page.h"
#include "RTCAnswerOptions.h"
#include "RTCConfiguration.h"
#include "RTCController.h"
#include "RTCDataChannel.h"
#include "RTCDtlsTransport.h"
#include "RTCDtlsTransportBackend.h"
#include "RTCIceCandidate.h"
#include "RTCIceCandidateInit.h"
#include "RTCIceTransport.h"
#include "RTCIceTransportBackend.h"
#include "RTCOfferOptions.h"
#include "RTCPeerConnectionIceErrorEvent.h"
#include "RTCPeerConnectionIceEvent.h"
#include "RTCSctpTransport.h"
#include "RTCSessionDescription.h"
#include "RTCSessionDescriptionInit.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

#if USE(LIBWEBRTC)
#include "LibWebRTCProvider.h"
#endif

namespace WebCore {

using namespace PeerConnection;

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCPeerConnection);

ExceptionOr<Ref<RTCPeerConnection>> RTCPeerConnection::create(Document& document, RTCConfiguration&& configuration)
{
    if (!document.frame())
        return Exception { NotSupportedError };

    auto peerConnection = adoptRef(*new RTCPeerConnection(document));
    peerConnection->suspendIfNeeded();

    if (!peerConnection->m_backend)
        return Exception { NotSupportedError };

    auto exception = peerConnection->initializeConfiguration(WTFMove(configuration));
    if (exception.hasException())
        return exception.releaseException();

    if (!peerConnection->isClosed()) {
        if (auto* page = document.page()) {
            peerConnection->registerToController(page->rtcController());
#if USE(LIBWEBRTC) && (!LOG_DISABLED || !RELEASE_LOG_DISABLED)
            if (!page->sessionID().isEphemeral())
                page->webRTCProvider().setLoggingLevel(LogWebRTC.level);
#endif
        }
    }
    return peerConnection;
}

RTCPeerConnection::RTCPeerConnection(Document& document)
    : ActiveDOMObject(document)
#if !RELEASE_LOG_DISABLED
    , m_logger(document.logger())
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
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

ExceptionOr<Ref<RTCRtpSender>> RTCPeerConnection::addTrack(Ref<MediaStreamTrack>&& track, const FixedVector<std::reference_wrapper<MediaStream>>& streams)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError };

    for (auto& transceiver : m_transceiverSet.list()) {
        if (transceiver->sender().trackId() == track->id())
            return Exception { InvalidAccessError };
    }

    return m_backend->addTrack(track.get(), WTF::map(streams, [](auto& stream) -> String {
        return stream.get().id();
    }));
}

ExceptionOr<void> RTCPeerConnection::removeTrack(RTCRtpSender& sender)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError, "RTCPeerConnection is closed"_s };

    if (!sender.isCreatedBy(*this))
        return Exception { InvalidAccessError, "RTCPeerConnection did not create the given sender"_s };

    bool shouldAbort = true;
    RTCRtpTransceiver* senderTransceiver = nullptr;
    for (auto& transceiver : m_transceiverSet.list()) {
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

    if (std::holds_alternative<String>(withTrack)) {
        const String& kind = std::get<String>(withTrack);
        if (kind != "audio"_s && kind != "video"_s)
            return Exception { TypeError };

        if (isClosed())
            return Exception { InvalidStateError };

        return m_backend->addTransceiver(kind, init);
    }

    if (isClosed())
        return Exception { InvalidStateError };

    auto track = std::get<RefPtr<MediaStreamTrack>>(withTrack).releaseNonNull();
    return m_backend->addTransceiver(WTFMove(track), init);
}

void RTCPeerConnection::createOffer(RTCOfferOptions&& options, Ref<DeferredPromise>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise->reject(InvalidStateError);
        return;
    }

    chainOperation(WTFMove(promise), [this, options = WTFMove(options)](auto&& promise) mutable {
        if (m_signalingState != RTCSignalingState::Stable && m_signalingState != RTCSignalingState::HaveLocalOffer) {
            promise->reject(InvalidStateError);
            return;
        }
        m_backend->createOffer(WTFMove(options), [this, protectedThis = Ref { *this }, promise = PeerConnection::SessionDescriptionPromise(WTFMove(promise))](auto&& result) mutable {
            if (isClosed())
                return;
            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }
            // https://w3c.github.io/webrtc-pc/#dfn-final-steps-to-create-an-offer steps 4,5 and 6.
            m_lastCreatedOffer = result.returnValue().sdp;
            promise.resolve(result.releaseReturnValue());
        });
    });
}

void RTCPeerConnection::createAnswer(RTCAnswerOptions&& options, Ref<DeferredPromise>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise->reject(InvalidStateError);
        return;
    }

    chainOperation(WTFMove(promise), [this, options = WTFMove(options)](auto&& promise) mutable {
        if (m_signalingState != RTCSignalingState::HaveRemoteOffer && m_signalingState != RTCSignalingState::HaveLocalPranswer) {
            promise->reject(InvalidStateError);
            return;
        }
        m_backend->createAnswer(WTFMove(options), [this, protectedThis = Ref { *this }, promise = PeerConnection::SessionDescriptionPromise(WTFMove(promise))](auto&& result) mutable {
            if (isClosed())
                return;
            if (result.hasException()) {
                promise.reject(result.releaseException());
                return;
            }
            // https://w3c.github.io/webrtc-pc/#dfn-final-steps-to-create-an-answer steps 4,5 and 6.
            m_lastCreatedAnswer = result.returnValue().sdp;
            promise.resolve(result.releaseReturnValue());
        });
    });
}

static RTCSdpType typeForSetLocalDescription(const std::optional<RTCLocalSessionDescriptionInit>& description, RTCSignalingState signalingState)
{
    std::optional<RTCSdpType> type;
    if (description)
        type = description->type;

    // https://w3c.github.io/webrtc-pc/#dom-peerconnection-setlocaldescription step 4.1.
    if (!type) {
        bool shouldBeOffer = signalingState == RTCSignalingState::Stable || signalingState == RTCSignalingState::HaveLocalOffer || signalingState == RTCSignalingState::HaveRemotePranswer;
        return shouldBeOffer ? RTCSdpType::Offer : RTCSdpType::Answer;
    }
    return *type;
}

void RTCPeerConnection::setLocalDescription(std::optional<RTCLocalSessionDescriptionInit>&& localDescription, Ref<DeferredPromise>&& promise)
{
    if (isClosed()) {
        promise->reject(InvalidStateError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "Setting local description to:\n", localDescription ? localDescription->sdp : "''"_s);
    chainOperation(WTFMove(promise), [this, localDescription = WTFMove(localDescription)](auto&& promise) mutable {
        auto type = typeForSetLocalDescription(localDescription, m_signalingState);
        String sdp;
        if (localDescription)
            sdp = localDescription->sdp;
        if (type == RTCSdpType::Offer && sdp.isEmpty())
            sdp = m_lastCreatedOffer;
        else if (type == RTCSdpType::Answer && sdp.isEmpty())
            sdp = m_lastCreatedAnswer;

        RefPtr<RTCSessionDescription> description;
        if (!sdp.isEmpty() || (type != RTCSdpType::Offer && type != RTCSdpType::Answer))
            description = RTCSessionDescription::create(type, WTFMove(sdp));
        m_backend->setLocalDescription(description.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTFMove(promise))](auto&& result) mutable {
            if (protectedThis->isClosed())
                return;
            promise.settle(WTFMove(result));
        });
    });
}

void RTCPeerConnection::setRemoteDescription(RTCSessionDescriptionInit&& remoteDescription, Ref<DeferredPromise>&& promise)
{
    if (isClosed()) {
        promise->reject(InvalidStateError);
        return;
    }

    ALWAYS_LOG(LOGIDENTIFIER, "Setting remote description to:\n", remoteDescription.sdp);
    chainOperation(WTFMove(promise), [this, remoteDescription = WTFMove(remoteDescription)](auto&& promise) mutable {
        auto description = RTCSessionDescription::create(WTFMove(remoteDescription));
        if (description->type() == RTCSdpType::Offer && m_signalingState != RTCSignalingState::Stable && m_signalingState != RTCSignalingState::HaveRemoteOffer) {
            auto rollbackDescription = RTCSessionDescription::create(RTCSdpType::Rollback, String { emptyString() });
            m_backend->setLocalDescription(rollbackDescription.ptr(), [this, protectedThis = Ref { *this }, description = WTFMove(description), promise = WTFMove(promise)](auto&&) mutable {
                if (isClosed())
                    return;
                m_backend->setRemoteDescription(description.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTFMove(promise))](auto&& result) mutable {
                    if (protectedThis->isClosed())
                        return;
                    promise.settle(WTFMove(result));
                });
            });
            return;
        }
        m_backend->setRemoteDescription(description.get(), [promise = DOMPromiseDeferred<void>(WTFMove(promise))](auto&& result) mutable {
            promise.settle(WTFMove(result));
        });
    });
}

void RTCPeerConnection::addIceCandidate(Candidate&& rtcCandidate, Ref<DeferredPromise>&& promise)
{
    std::optional<Exception> exception;
    RefPtr<RTCIceCandidate> candidate;
    if (rtcCandidate) {
        candidate = WTF::switchOn(*rtcCandidate, [&exception](RTCIceCandidateInit& init) -> RefPtr<RTCIceCandidate> {
            if (init.candidate.isEmpty())
                return nullptr;

            auto result = RTCIceCandidate::create(WTFMove(init));
            if (result.hasException()) {
                exception = result.releaseException();
                return nullptr;
            }
            return result.releaseReturnValue();
        }, [](RefPtr<RTCIceCandidate>& iceCandidate) {
            return WTFMove(iceCandidate);
        });
    }

    ALWAYS_LOG(LOGIDENTIFIER, "Received ice candidate:\n", candidate ? candidate->candidate() : "null"_s);

    if (exception) {
        promise->reject(*exception);
        return;
    }

    if (candidate && candidate->sdpMid().isNull() && !candidate->sdpMLineIndex()) {
        promise->reject(Exception { TypeError, "Trying to add a candidate that is missing both sdpMid and sdpMLineIndex"_s });
        return;
    }

    if (isClosed())
        return;

    chainOperation(WTFMove(promise), [this, candidate = WTFMove(candidate)](auto&& promise) mutable {
        m_backend->addIceCandidate(candidate.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTFMove(promise))](auto&& result) mutable {
            if (protectedThis->isClosed())
                return;
            promise.settle(WTFMove(result));
        });
    });
}

std::optional<bool> RTCPeerConnection::canTrickleIceCandidates() const
{
    if (isClosed() || !remoteDescription())
        return { };
    return m_backend-> canTrickleIceCandidates();
}

// Implementation of https://w3c.github.io/webrtc-pc/#set-pc-configuration
ExceptionOr<Vector<MediaEndpointConfiguration::IceServerInfo>> RTCPeerConnection::iceServersFromConfiguration(RTCConfiguration& newConfiguration, const RTCConfiguration* existingConfiguration, bool isLocalDescriptionSet)
{
    if (existingConfiguration && newConfiguration.bundlePolicy != existingConfiguration->bundlePolicy)
        return Exception { InvalidModificationError, "BundlePolicy does not match existing policy"_s };

    if (existingConfiguration && newConfiguration.rtcpMuxPolicy != existingConfiguration->rtcpMuxPolicy)
        return Exception { InvalidModificationError, "RTCPMuxPolicy does not match existing policy"_s };

    if (existingConfiguration && newConfiguration.iceCandidatePoolSize != existingConfiguration->iceCandidatePoolSize && isLocalDescriptionSet)
        return Exception { InvalidModificationError, "IceTransportPolicy pool size does not match existing pool size"_s };

    Vector<MediaEndpointConfiguration::IceServerInfo> servers;
    if (newConfiguration.iceServers) {
        servers.reserveInitialCapacity(newConfiguration.iceServers->size());
        for (auto& server : newConfiguration.iceServers.value()) {
            Vector<String> urls;
            WTF::switchOn(server.urls, [&urls] (String& url) {
                urls = { WTFMove(url) };
            }, [&urls] (Vector<String>& vector) {
                urls = WTFMove(vector);
            });

            urls.removeAllMatching([&](auto& urlString) {
                URL url { URL { }, urlString };
                if (url.path().endsWithIgnoringASCIICase(".local"_s) || !portAllowed(url)) {
                    queueTaskToDispatchEvent(*this, TaskSource::MediaElement, RTCPeerConnectionIceErrorEvent::create(Event::CanBubble::No, Event::IsCancelable::No, { }, { }, WTFMove(urlString), 701, "URL is not allowed"_s));
                    return true;
                }
                return false;
            });

            auto serverURLs = WTF::map(urls, [](auto& url) -> URL {
                return { URL { }, url };
            });
            server.urls = WTFMove(urls);

            for (auto& serverURL : serverURLs) {
                if (serverURL.isNull())
                    return Exception { TypeError, "Bad ICE server URL"_s };
                if (serverURL.protocolIs("turn"_s) || serverURL.protocolIs("turns"_s)) {
                    if (server.credential.isNull() || server.username.isNull())
                        return Exception { InvalidAccessError, "TURN/TURNS server requires both username and credential"_s };
                    // https://tools.ietf.org/html/rfc8489#section-14.3
                    if (server.credential.length() > 64 || server.username.length() > 64) {
                        constexpr size_t MaxTurnUsernameLength = 509;
                        if (server.credential.utf8().length() > MaxTurnUsernameLength || server.username.utf8().length() > MaxTurnUsernameLength)
                            return Exception { TypeError, "TURN/TURNS username and/or credential are too long"_s };
                    }
                } else if (!serverURL.protocolIs("stun"_s))
                    return Exception { SyntaxError, "ICE server protocol not supported"_s };
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
            return Exception { InvalidAccessError, "Certificate does not have a valid origin"_s };

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
        return Exception { InvalidAccessError, "Bad Configuration Parameters"_s };

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
            return Exception { InvalidModificationError, "Certificates parameters are different"_s };

        for (auto& certificate : configuration.certificates) {
            bool isThere = m_configuration.certificates.findIf([&certificate](const auto& item) {
                return item.get() == certificate.get();
            }) != notFound;
            if (!isThere)
                return Exception { InvalidModificationError, "A certificate given in constructor is not present"_s };
        }
    }

    if (!m_backend->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.rtcpMuxPolicy, configuration.iceCandidatePoolSize, { } }))
        return Exception { InvalidAccessError, "Bad Configuration Parameters"_s };

    m_configuration = WTFMove(configuration);
    return { };
}

void RTCPeerConnection::getStats(MediaStreamTrack* selector, Ref<DeferredPromise>&& promise)
{
    if (selector) {
        for (auto& transceiver : m_transceiverSet.list()) {
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
    promise->whenSettled([pendingActivity = makePendingActivity(*this)] { });
    m_backend->getStats(WTFMove(promise));
}

void RTCPeerConnection::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    m_backend->gatherDecoderImplementationName(WTFMove(callback));
}

// https://w3c.github.io/webrtc-pc/#dom-peerconnection-createdatachannel
ExceptionOr<Ref<RTCDataChannel>> RTCPeerConnection::createDataChannel(String&& label, RTCDataChannelInit&& options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { InvalidStateError };

    if (label.utf8().length() > 65535)
        return Exception { TypeError, "label is too long"_s };

    if (options.protocol.utf8().length() > 65535)
        return Exception { TypeError, "protocol is too long"_s };

    if (!options.negotiated || !options.negotiated.value())
        options.id = { };
    else if (!options.id)
        return Exception { TypeError, "negotiated is true but id is null or undefined"_s };

    if (options.maxPacketLifeTime && options.maxRetransmits)
        return Exception { TypeError, "Cannot set both maxPacketLifeTime and maxRetransmits"_s };

    if (options.id && *options.id > 65534)
        return Exception { TypeError, "id is too big"_s };

    // FIXME: Provide better error reporting.
    auto channelHandler = m_backend->createDataChannelHandler(label, options);
    if (!channelHandler)
        return Exception { OperationError };

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

    for (auto& transceiver : m_transceiverSet.list()) {
        transceiver->stop();
        transceiver->sender().stop();
        transceiver->receiver().stop();
    }
    m_operations.clear();

    for (auto& transport : m_dtlsTransports)
        transport->close();

    return true;
}

void RTCPeerConnection::close()
{
    if (!doClose())
        return;

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
}

bool RTCPeerConnection::virtualHasPendingActivity() const
{
    if (m_isStopped)
        return false;

    // As long as the connection is not stopped and it has event listeners, it may dispatch events.
    return hasEventListeners();
}

void RTCPeerConnection::addInternalTransceiver(Ref<RTCRtpTransceiver>&& transceiver)
{
    transceiver->setConnection(*this);
    m_transceiverSet.append(WTFMove(transceiver));
}

void RTCPeerConnection::setSignalingState(RTCSignalingState newState)
{
    if (m_signalingState == newState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, newState);
    m_signalingState = newState;
    dispatchEvent(Event::create(eventNames().signalingstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCPeerConnection::updateIceGatheringState(RTCIceGatheringState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, newState);

    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, newState] {
        if (isClosed() || m_iceGatheringState == newState)
            return;

        m_iceGatheringState = newState;
        dispatchEvent(Event::create(eventNames().icegatheringstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        updateConnectionState();
    });
}

void RTCPeerConnection::updateIceConnectionState(RTCIceConnectionState)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
        if (isClosed())
            return;
        auto newState = computeIceConnectionStateFromIceTransports();
        if (m_iceConnectionState == newState)
            return;

        m_iceConnectionState = newState;
        dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        updateConnectionState();
    });
}

// https://w3c.github.io/webrtc-pc/#rtcpeerconnectionstate-enum
RTCPeerConnectionState RTCPeerConnection::computeConnectionState()
{
    if (isClosed())
        return RTCPeerConnectionState::Closed;

    auto iceTransports = m_iceTransports;
    iceTransports.removeAllMatching([&](auto& iceTransport) {
        if (m_sctpTransport && &m_sctpTransport->transport().iceTransport() == iceTransport.ptr())
            return false;
        return allOf(m_transceiverSet.list(), [&iceTransport](auto& transceiver) {
            return !isIceTransportUsedByTransceiver(iceTransport.get(), *transceiver);
        });
    });

    auto dtlsTransports = m_dtlsTransports;
    dtlsTransports.removeAllMatching([&](auto& dtlsTransport) {
        if (m_sctpTransport && &m_sctpTransport->transport() == dtlsTransport.ptr())
            return false;
        return allOf(m_transceiverSet.list(), [&dtlsTransport](auto& transceiver) {
            return transceiver->sender().transport() != dtlsTransport.ptr();
        });
    });

    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Failed; }) || anyOf(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::Failed; }))
        return RTCPeerConnectionState::Failed;

    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Disconnected; }))
        return RTCPeerConnectionState::Disconnected;

    if (allOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Closed; }) && allOf(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::New || transport->state() == RTCDtlsTransportState::Closed; }))
        return RTCPeerConnectionState::New;

    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Checking; }) || anyOf(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::New || transport->state() == RTCDtlsTransportState::Connecting; }))
        return RTCPeerConnectionState::Connecting;

    ASSERT(allOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Connected || transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }) && allOf(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::Connected || transport->state() == RTCDtlsTransportState::Closed; }));
    return RTCPeerConnectionState::Connected;
}

void RTCPeerConnection::updateConnectionState()
{
    auto state = computeConnectionState();

    if (state == m_connectionState)
        return;

    INFO_LOG(LOGIDENTIFIER, "state changed from: " , m_connectionState, " to ", state);

    m_connectionState = state;
    scheduleEvent(Event::create(eventNames().connectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

static bool isIceTransportUsedByTransceiver(const RTCIceTransport& iceTransport, RTCRtpTransceiver& transceiver)
{
    auto* dtlsTransport = transceiver.sender().transport();
    return dtlsTransport && &dtlsTransport->iceTransport() == &iceTransport;
}

// https://w3c.github.io/webrtc-pc/#dom-rtciceconnectionstate
RTCIceConnectionState RTCPeerConnection::computeIceConnectionStateFromIceTransports()
{
    if (isClosed())
        return RTCIceConnectionState::Closed;

    auto iceTransports = m_iceTransports;

    iceTransports.removeAllMatching([&](auto& iceTransport) {
        if (m_sctpTransport && &m_sctpTransport->transport().iceTransport() == iceTransport.ptr())
            return false;
        return allOf(m_transceiverSet.list(), [&iceTransport](auto& transceiver) {
            return !isIceTransportUsedByTransceiver(iceTransport.get(), *transceiver);
        });
    });

    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Failed; }))
        return RTCIceConnectionState::Failed;
    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Disconnected; }))
        return RTCIceConnectionState::Disconnected;
    if (allOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Closed; }))
        return RTCIceConnectionState::New;
    if (anyOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Checking; }))
        return RTCIceConnectionState::Checking;
    if (allOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }))
        return RTCIceConnectionState::Completed;
    ASSERT(allOf(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Connected || transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }));
    return RTCIceConnectionState::Connected;
}

// https://w3c.github.io/webrtc-pc/#rtcicetransport, algorithm to handle a change of RTCIceTransport state.
void RTCPeerConnection::processIceTransportStateChange(RTCIceTransport& iceTransport)
{
    auto newIceConnectionState = computeIceConnectionStateFromIceTransports();
    bool iceConnectionStateChanged = m_iceConnectionState != newIceConnectionState;
    m_iceConnectionState = newIceConnectionState;

    auto newConnectionState = computeConnectionState();
    bool connectionStateChanged = m_connectionState != newConnectionState;
    m_connectionState = newConnectionState;

    iceTransport.dispatchEvent(Event::create(eventNames().statechangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    if (iceConnectionStateChanged && !isClosed())
        dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    if (connectionStateChanged && !isClosed())
        dispatchEvent(Event::create(eventNames().connectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCPeerConnection::processIceTransportChanges()
{
    auto newIceConnectionState = computeIceConnectionStateFromIceTransports();
    bool iceConnectionStateChanged = m_iceConnectionState != newIceConnectionState;
    m_iceConnectionState = newIceConnectionState;

    if (iceConnectionStateChanged && !isClosed())
        dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void RTCPeerConnection::updateNegotiationNeededFlag(std::optional<uint32_t> eventId)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, eventId]() mutable {
        if (isClosed())
            return;
        if (!eventId) {
            if (!m_negotiationNeededEventId)
                return;
            eventId = m_negotiationNeededEventId;
        }
        if (m_hasPendingOperation) {
            m_negotiationNeededEventId = *eventId;
            return;
        }
        if (signalingState() != RTCSignalingState::Stable) {
            m_negotiationNeededEventId = *eventId;
            return;
        }

        if (!m_backend->isNegotiationNeeded(*eventId))
            return;

        dispatchEvent(Event::create(eventNames().negotiationneededEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void RTCPeerConnection::scheduleEvent(Ref<Event>&& event)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, event = WTFMove(event)]() mutable {
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
    if (std::holds_alternative<String>(algorithmIdentifier))
        return Exception { NotSupportedError, "Algorithm is not supported"_s };

    auto& value = std::get<JSC::Strong<JSC::JSObject>>(algorithmIdentifier);

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
    return m_transceiverSet.senders();
}

Vector<std::reference_wrapper<RTCRtpReceiver>> RTCPeerConnection::getReceivers() const
{
    return m_transceiverSet.receivers();
}

const Vector<RefPtr<RTCRtpTransceiver>>& RTCPeerConnection::getTransceivers() const
{
    return m_transceiverSet.list();
}

void RTCPeerConnection::chainOperation(Ref<DeferredPromise>&& promise, Function<void(Ref<DeferredPromise>&&)>&& operation)
{
    if (isClosed()) {
        promise->reject(InvalidStateError, "RTCPeerConnection is closed"_s);
        return;
    }

    promise->whenSettled([this, pendingActivity = makePendingActivity(*this)] {
        ASSERT(m_hasPendingOperation);
        if (isClosed()) {
            for (auto& operation : std::exchange(m_operations, { }))
                operation.first->reject(InvalidStateError, "RTCPeerConnection is closed"_s);
            m_hasPendingOperation = false;
            return;
        }

        if (!m_operations.isEmpty()) {
            auto promiseOperation = m_operations.takeFirst();
            promiseOperation.second(WTFMove(promiseOperation.first));
            return;
        }

        m_hasPendingOperation = false;
        if (m_negotiationNeededEventId)
            updateNegotiationNeededFlag({ });
    });

    if (m_hasPendingOperation || !m_operations.isEmpty()) {
        m_operations.append(std::make_pair(WTFMove(promise), WTFMove(operation)));
        return;
    }

    m_hasPendingOperation = true;
    operation(WTFMove(promise));
}

Document* RTCPeerConnection::document()
{
    return downcast<Document>(scriptExecutionContext());
}

Ref<RTCIceTransport> RTCPeerConnection::getOrCreateIceTransport(UniqueRef<RTCIceTransportBackend>&& backend)
{
    auto index = m_iceTransports.findIf([&backend](auto& transport) { return backend.get() == transport->backend(); });
    if (index == notFound) {
        index = m_iceTransports.size();
        m_iceTransports.append(RTCIceTransport::create(*scriptExecutionContext(), WTFMove(backend), *this));
    }

    return m_iceTransports[index].copyRef();
}


RefPtr<RTCDtlsTransport> RTCPeerConnection::getOrCreateDtlsTransport(std::unique_ptr<RTCDtlsTransportBackend>&& backend)
{
    if (!backend)
        return nullptr;

    auto* context = scriptExecutionContext();
    if (!context)
        return nullptr;

    auto index = m_dtlsTransports.findIf([&backend](auto& transport) { return *backend == transport->backend(); });
    if (index == notFound) {
        index = m_dtlsTransports.size();
        auto iceTransportBackend = backend->iceTransportBackend();
        m_dtlsTransports.append(RTCDtlsTransport::create(*context, makeUniqueRefFromNonNullUniquePtr(WTFMove(backend)), getOrCreateIceTransport(WTFMove(iceTransportBackend))));
    }

    return m_dtlsTransports[index].copyRef();
}

static void updateDescription(RefPtr<RTCSessionDescription>& description, std::optional<RTCSdpType> type, String&& sdp)
{
    if (description && type && description->sdp() == sdp && description->type() == *type)
        return;
    if (!type || sdp.isEmpty()) {
        description = nullptr;
        return;
    }
    description = RTCSessionDescription::create(*type, WTFMove(sdp));
}

void RTCPeerConnection::updateDescriptions(PeerConnectionBackend::DescriptionStates&& states)
{
    updateDescription(m_currentLocalDescription, states.currentLocalDescriptionSdpType, WTFMove(states.currentLocalDescriptionSdp));
    updateDescription(m_pendingLocalDescription, states.pendingLocalDescriptionSdpType, WTFMove(states.pendingLocalDescriptionSdp));
    updateDescription(m_currentRemoteDescription, states.currentRemoteDescriptionSdpType, WTFMove(states.currentRemoteDescriptionSdp));
    updateDescription(m_pendingRemoteDescription, states.pendingRemoteDescriptionSdpType, WTFMove(states.pendingRemoteDescriptionSdp));

    if (states.signalingState)
        setSignalingState(*states.signalingState);

    if (!m_pendingRemoteDescription && !m_pendingLocalDescription) {
        m_lastCreatedOffer = { };
        m_lastCreatedAnswer = { };
    }
}

void RTCPeerConnection::updateTransceiverTransports()
{
    for (auto& transceiver : m_transceiverSet.list()) {
        auto& sender = transceiver->sender();
        if (auto* senderBackend = sender.backend())
            sender.setTransport(getOrCreateDtlsTransport(senderBackend->dtlsTransportBackend()));

        auto& receiver = transceiver->receiver();
        if (auto* receiverBackend = receiver.backend())
            receiver.setTransport(getOrCreateDtlsTransport(receiverBackend->dtlsTransportBackend()));
    }
}

// https://w3c.github.io/webrtc-pc/#set-description step 4.9.1
void RTCPeerConnection::updateTransceiversAfterSuccessfulLocalDescription()
{
    m_backend->collectTransceivers();
    updateTransceiverTransports();
}

// https://w3c.github.io/webrtc-pc/#set-description step 4.9.2
void RTCPeerConnection::updateTransceiversAfterSuccessfulRemoteDescription()
{
    m_backend->collectTransceivers();
    updateTransceiverTransports();
}

void RTCPeerConnection::updateSctpBackend(std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend)
{
    if (!sctpBackend) {
        m_sctpTransport = nullptr;
        return;
    }
    if (m_sctpTransport && m_sctpTransport->backend() == *sctpBackend) {
        m_sctpTransport->update();
        return;
    }
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto dtlsTransport = getOrCreateDtlsTransport(sctpBackend->dtlsTransportBackend().moveToUniquePtr());
    if (!dtlsTransport)
        return;
    m_sctpTransport = RTCSctpTransport::create(*context, makeUniqueRefFromNonNullUniquePtr(WTFMove(sctpBackend)), dtlsTransport.releaseNonNull());
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCPeerConnection::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
