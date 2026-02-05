/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015, 2016 Ericsson AB. All rights reserved.
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "DNS.h"
#include "DocumentPage.h"
#include "Event.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "JSRTCPeerConnection.h"
#include "JSRTCSessionDescriptionInit.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaEndpointConfiguration.h"
#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "Page.h"
#include "RTCAnswerOptions.h"
#include "RTCConfiguration.h"
#include "RTCController.h"
#include "RTCDataChannel.h"
#include "RTCDataChannelEvent.h"
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
#include <algorithm>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/text/Base64.h>

#if USE(LIBWEBRTC)
#include "LibWebRTCProvider.h"
#endif

namespace WebCore {

using namespace PeerConnection;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCPeerConnection);

ExceptionOr<Ref<RTCPeerConnection>> RTCPeerConnection::create(Document& document, RTCConfiguration&& configuration)
{
    if (!document.frame() || !document.settings().peerConnectionEnabled())
        return Exception { ExceptionCode::NotSupportedError };

    auto peerConnection = adoptRef(*new RTCPeerConnection(document));
    peerConnection->suspendIfNeeded();

    auto exception = peerConnection->initializeWithConfiguration(WTF::move(configuration));
    if (exception.hasException())
        return exception.releaseException();

    if (!peerConnection->isClosed()) {
        if (RefPtr page = document.page()) {
            peerConnection->registerToController(page->rtcController());
#if USE(LIBWEBRTC) && (!LOG_DISABLED || !RELEASE_LOG_DISABLED)
            if (page->isAlwaysOnLoggingAllowed()) {
                WTFLogLevel level = LogWebRTC.level;
                if (level != WTFLogLevel::Debug && document.settings().webRTCMediaPipelineAdditionalLoggingEnabled())
                    level = WTFLogLevel::Info;
                bool setLoggingLevel = document.settings().peerConnectionEnabled();
#if ENABLE(WEB_CODECS)
                setLoggingLevel = setLoggingLevel || document.settings().webCodecsVideoEnabled();
#endif
                if (setLoggingLevel)
                    page->webRTCProvider().setLoggingLevel(level);
            }
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
{
    ALWAYS_LOG(LOGIDENTIFIER);
    relaxAdoptionRequirement();
}

RTCPeerConnection::~RTCPeerConnection()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    unregisterFromController();
    stop();
}

RefPtr<PeerConnectionBackend> RTCPeerConnection::protectedBackend() const
{
    return m_backend.get();
}

ExceptionOr<Ref<RTCRtpSender>> RTCPeerConnection::addTrack(Ref<MediaStreamTrack>&& track, const FixedVector<std::reference_wrapper<MediaStream>>& streams)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError };

    for (auto& transceiver : m_transceiverSet.list()) {
        if (transceiver->sender().trackId() == track->id())
            return Exception { ExceptionCode::InvalidAccessError };
    }

    return protectedBackend()->addTrack(track.get(), WTF::map(streams, [](auto& stream) -> String {
        return stream.get().id();
    }));
}

ExceptionOr<void> RTCPeerConnection::removeTrack(RTCRtpSender& sender)
{
    INFO_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError, "RTCPeerConnection is closed"_s };

    if (!sender.isCreatedBy(*this))
        return Exception { ExceptionCode::InvalidAccessError, "RTCPeerConnection did not create the given sender"_s };

    bool shouldAbort = true;
    RTCRtpTransceiver* senderTransceiver = nullptr;
    for (auto& transceiver : m_transceiverSet.list()) {
        if (&sender == &transceiver->sender()) {
            senderTransceiver = transceiver.ptr();
            shouldAbort = sender.isStopped() || !sender.track();
            break;
        }
    }
    if (shouldAbort)
        return { };

    sender.setTrackToNull();
    senderTransceiver->disableSendingDirection();
    protectedBackend()->removeTrack(sender);
    return { };
}

static bool isAudioTransceiver(const RTCPeerConnection::AddTransceiverTrackOrKind& withTrack)
{
    return switchOn(withTrack, [] (const String& type) -> bool {
        return type == "audio"_s;
    }, [] (const RefPtr<MediaStreamTrack>& track) -> bool {
        return track->isAudio();
    });
}

// https://w3c.github.io/webrtc-pc/#dfn-addtransceiver-sendencodings-validation-steps
static std::optional<Exception> validateSendEncodings(Vector<RTCRtpEncodingParameters>& encodings, bool isAudio)
{
    size_t encodingIndex = 0;
    bool hasAnyScaleResolutionDownBy = !isAudio && std::ranges::any_of(encodings, [](auto& encoding) { return !!encoding.scaleResolutionDownBy; });
    for (auto& encoding: encodings) {
        // FIXME: Validate rid and codec
        if (isAudio) {
            encoding.scaleResolutionDownBy = { };
            encoding.maxFramerate = { };
            continue;
        }
        if (encoding.scaleResolutionDownBy && *encoding.scaleResolutionDownBy < 1)
            return Exception { ExceptionCode::RangeError, "scaleResolutionDownBy is below 1"_s };

        if (encoding.maxFramerate && *encoding.maxFramerate <= 0)
            return Exception { ExceptionCode::RangeError, "maxFrameRate is below or equal 0"_s };

        if (hasAnyScaleResolutionDownBy) {
            if (!encoding.scaleResolutionDownBy)
                encoding.scaleResolutionDownBy = 1;
        } else
            encoding.scaleResolutionDownBy = 1 << (encodings.size() - ++encodingIndex);
    }

    return { };
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addTransceiver(AddTransceiverTrackOrKind&& withTrack, RTCRtpTransceiverInit&& init)
{
    INFO_LOG(LOGIDENTIFIER);

    if (auto exception = validateSendEncodings(init.sendEncodings, isAudioTransceiver(withTrack)))
        return WTF::move(*exception);

    if (std::holds_alternative<String>(withTrack)) {
        const String& kind = std::get<String>(withTrack);
        if (kind != "audio"_s && kind != "video"_s)
            return Exception { ExceptionCode::TypeError };

        if (isClosed())
            return Exception { ExceptionCode::InvalidStateError };

        return protectedBackend()->addTransceiver(kind, init, PeerConnectionBackend::IgnoreNegotiationNeededFlag::No);
    }

    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError };

    auto track = std::get<RefPtr<MediaStreamTrack>>(withTrack).releaseNonNull();
    return protectedBackend()->addTransceiver(WTF::move(track), init);
}

ExceptionOr<Ref<RTCRtpTransceiver>> RTCPeerConnection::addReceiveOnlyTransceiver(String&& kind)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    // https://www.w3.org/TR/webrtc/#legacy-configuration-extensions Step 3.3: Let transceiver be
    // the result of invoking the equivalent of connection.addTransceiver(kind), except that this
    // operation MUST NOT update the negotiation-needed flag.
    RTCRtpTransceiverInit init { .direction = RTCRtpTransceiverDirection::Recvonly, .streams = { }, .sendEncodings = { } };
    if (kind != "audio"_s && kind != "video"_s)
        return Exception { ExceptionCode::TypeError };

    ASSERT(!isClosed());
    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError };

    return protectedBackend()->addTransceiver(kind, init, PeerConnectionBackend::IgnoreNegotiationNeededFlag::Yes);
}

void RTCPeerConnection::createOffer(RTCOfferOptions&& options, Ref<DeferredPromise>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isClosed()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    // https://www.w3.org/TR/webrtc/#legacy-configuration-extensions
    auto needsReceiveOnlyTransceiver = [&](auto option, auto&& trackKind) -> bool {
        if (!option) {
            for (auto& transceiver : currentTransceivers()) {
                if (transceiver->stopped())
                    continue;
                if (transceiver->sender().trackKind() != trackKind)
                    continue;
                if (transceiver->direction() == RTCRtpTransceiverDirection::Sendrecv)
                    transceiver->setDirection(RTCRtpTransceiverDirection::Sendonly);
                else if (transceiver->direction() == RTCRtpTransceiverDirection::Recvonly)
                    transceiver->setDirection(RTCRtpTransceiverDirection::Inactive);
            }
            return false;
        }

        for (auto& transceiver : currentTransceivers()) {
            if (transceiver->stopped())
                continue;
            if (transceiver->sender().trackKind() != trackKind)
                continue;
            auto direction = transceiver->direction();
            if (direction == RTCRtpTransceiverDirection::Sendrecv || direction == RTCRtpTransceiverDirection::Recvonly)
                return false;
        }

        return true;
    };

    if (options.offerToReceiveAudio) {
        if (needsReceiveOnlyTransceiver(*options.offerToReceiveAudio, "audio"_s)) {
            auto result = addReceiveOnlyTransceiver("audio"_s);
            if (result.hasException()) {
                promise->reject(result.releaseException());
                return;
            }
        }
    }

    if (options.offerToReceiveVideo) {
        if (needsReceiveOnlyTransceiver(*options.offerToReceiveVideo, "video"_s)) {
            auto result = addReceiveOnlyTransceiver("video"_s);
            if (result.hasException()) {
                promise->reject(result.releaseException());
                return;
            }
        }
    }

    chainOperation(WTF::move(promise), [this, options = WTF::move(options)](Ref<DeferredPromise>&& promise) mutable {
        if (m_signalingState != RTCSignalingState::Stable && m_signalingState != RTCSignalingState::HaveLocalOffer) {
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }
        protectedBackend()->createOffer(WTF::move(options), [this, protectedThis = Ref { *this }, promise = PeerConnection::SessionDescriptionPromise(WTF::move(promise))](auto&& result) mutable {
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
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    chainOperation(WTF::move(promise), [this, options = WTF::move(options)](Ref<DeferredPromise>&& promise) mutable {
        if (m_signalingState != RTCSignalingState::HaveRemoteOffer && m_signalingState != RTCSignalingState::HaveLocalPranswer) {
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }
        protectedBackend()->createAnswer(WTF::move(options), [this, protectedThis = Ref { *this }, promise = PeerConnection::SessionDescriptionPromise(WTF::move(promise))](auto&& result) mutable {
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
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

#if !RELEASE_LOG_DISABLED
    String sdp = localDescription.value_or(RTCLocalSessionDescriptionInit { }).sdp;
    logger().toObservers(LogWebRTC, WTFLogLevel::Always, LOGIDENTIFIER, "Setting local description to:\n", sdp);
    RELEASE_LOG_FORWARDABLE(WebRTC, RTCPEERCONNECTION_SETLOCALDESCRIPTION, logIdentifier(), sdp.utf8());
#endif

    chainOperation(WTF::move(promise), [this, localDescription = WTF::move(localDescription)](Ref<DeferredPromise>&& promise) mutable {
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
            description = RTCSessionDescription::create(type, WTF::move(sdp));
        protectedBackend()->setLocalDescription(description.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTF::move(promise))](ExceptionOr<void>&& result) mutable {
            if (protectedThis->isClosed())
                return;
            promise.settle(WTF::move(result));
        });
    });
}

void RTCPeerConnection::setRemoteDescription(RTCSessionDescriptionInit&& remoteDescription, Ref<DeferredPromise>&& promise)
{
    if (isClosed()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

#if !RELEASE_LOG_DISABLED
    logger().toObservers(LogWebRTC, WTFLogLevel::Always, LOGIDENTIFIER, "Setting remote description to:\n", remoteDescription.sdp);
    RELEASE_LOG_FORWARDABLE(WebRTC, RTCPEERCONNECTION_SETREMOTEDESCRIPTION, logIdentifier(), remoteDescription.sdp.utf8());
#endif

    chainOperation(WTF::move(promise), [this, remoteDescription = WTF::move(remoteDescription)](Ref<DeferredPromise>&& promise) mutable {
        auto description = RTCSessionDescription::create(WTF::move(remoteDescription));
        if (description->type() == RTCSdpType::Offer && m_signalingState != RTCSignalingState::Stable && m_signalingState != RTCSignalingState::HaveRemoteOffer) {
            auto rollbackDescription = RTCSessionDescription::create(RTCSdpType::Rollback, String { emptyString() });
            protectedBackend()->setLocalDescription(rollbackDescription.ptr(), [this, protectedThis = Ref { *this }, description = WTF::move(description), promise = WTF::move(promise)](auto&&) mutable {
                if (isClosed())
                    return;
                protectedBackend()->setRemoteDescription(description.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTF::move(promise))](ExceptionOr<void>&& result) mutable {
                    if (protectedThis->isClosed())
                        return;
                    promise.settle(WTF::move(result));
                });
            });
            return;
        }
        protectedBackend()->setRemoteDescription(description.get(), [promise = DOMPromiseDeferred<void>(WTF::move(promise))](auto&& result) mutable {
            promise.settle(WTF::move(result));
        });
    });
}

void RTCPeerConnection::addIceCandidate(Candidate&& rtcCandidate, Ref<DeferredPromise>&& promise)
{
    std::optional<Exception> exception;
    RefPtr candidate = WTF::switchOn(rtcCandidate,
        [&exception](RTCIceCandidateInit& init) -> RefPtr<RTCIceCandidate> {
            if (init.candidate.isEmpty())
                return nullptr;

            auto result = RTCIceCandidate::create(WTF::move(init));
            if (result.hasException()) {
                exception = result.releaseException();
                return nullptr;
            }
            return result.releaseReturnValue();
        },
        [](RefPtr<RTCIceCandidate>& iceCandidate) -> RefPtr<RTCIceCandidate> {
            return WTF::move(iceCandidate);
        }
    );

    ALWAYS_LOG(LOGIDENTIFIER, "Received ice candidate:\n", candidate ? candidate->candidate() : "null"_s);

    if (exception) {
        promise->reject(*exception);
        return;
    }

    if (candidate && candidate->sdpMid().isNull() && !candidate->sdpMLineIndex()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Trying to add a candidate that is missing both sdpMid and sdpMLineIndex"_s });
        return;
    }

    if (isClosed())
        return;

    chainOperation(WTF::move(promise), [this, candidate = WTF::move(candidate)](Ref<DeferredPromise>&& promise) mutable {
        protectedBackend()->addIceCandidate(candidate.get(), [protectedThis = Ref { *this }, promise = DOMPromiseDeferred<void>(WTF::move(promise))](auto&& result) mutable {
            if (protectedThis->isClosed())
                return;
            promise.settle(WTF::move(result));
        });
    });
}

std::optional<bool> RTCPeerConnection::canTrickleIceCandidates() const
{
    if (isClosed() || !remoteDescription())
        return { };
    return protectedBackend()->canTrickleIceCandidates();
}

// Implementation of https://w3c.github.io/webrtc-pc/#set-pc-configuration
ExceptionOr<Vector<MediaEndpointConfiguration::IceServerInfo>> RTCPeerConnection::iceServersFromConfiguration(RTCConfiguration& newConfiguration, const RTCConfiguration* existingConfiguration, bool isLocalDescriptionSet)
{
    if (existingConfiguration && newConfiguration.bundlePolicy != existingConfiguration->bundlePolicy)
        return Exception { ExceptionCode::InvalidModificationError, "BundlePolicy does not match existing policy"_s };

    if (existingConfiguration && newConfiguration.rtcpMuxPolicy != existingConfiguration->rtcpMuxPolicy)
        return Exception { ExceptionCode::InvalidModificationError, "RTCPMuxPolicy does not match existing policy"_s };

    if (existingConfiguration && newConfiguration.iceCandidatePoolSize != existingConfiguration->iceCandidatePoolSize && isLocalDescriptionSet)
        return Exception { ExceptionCode::InvalidModificationError, "IceTransportPolicy pool size does not match existing pool size"_s };

    Vector<MediaEndpointConfiguration::IceServerInfo> servers;
    servers.reserveInitialCapacity(newConfiguration.iceServers.size());
    for (auto& server : newConfiguration.iceServers) {
        Vector<String> urls;
        WTF::switchOn(server.urls, [&urls] (String& url) {
            urls = { WTF::move(url) };
        }, [&urls] (Vector<String>& vector) {
            urls = WTF::move(vector);
        });

        urls.removeAllMatching([&](auto& urlString) {
            URL url { URL { }, urlString };
            if (url.path().endsWithIgnoringASCIICase(".local"_s) || !portAllowed(url) || isIPAddressDisallowed(url)) {
                queueTaskToDispatchEvent(*this, TaskSource::MediaElement, RTCPeerConnectionIceErrorEvent::create(Event::CanBubble::No, Event::IsCancelable::No, { }, { }, WTF::move(urlString), 701, "URL is not allowed"_s));
                return true;
            }
            return false;
        });

        if (urls.isEmpty())
            return Exception { ExceptionCode::SyntaxError, "Empty ICE servers list"_s };

        auto serverURLs = WTF::map(urls, [](auto& url) -> URL {
            return { URL { }, url };
        });
        server.urls = WTF::move(urls);

        for (auto& serverURL : serverURLs) {
            if (serverURL.isNull())
                return Exception { ExceptionCode::TypeError, "Bad ICE server URL"_s };
            if (serverURL.protocolIs("turn"_s) || serverURL.protocolIs("turns"_s)) {
                if (server.credential.isNull() || server.username.isNull())
                    return Exception { ExceptionCode::InvalidAccessError, "TURN/TURNS server requires both username and credential"_s };
                // https://tools.ietf.org/html/rfc8489#section-14.3
                if (server.credential.length() > 64 || server.username.length() > 64) {
                    constexpr size_t MaxTurnUsernameLength = 509;
                    if (server.credential.utf8().length() > MaxTurnUsernameLength || server.username.utf8().length() > MaxTurnUsernameLength)
                        return Exception { ExceptionCode::InvalidAccessError, "TURN/TURNS username and/or credential are too long"_s };
                }
            } else if (!serverURL.protocolIs("stun"_s) && !serverURL.protocolIs("stuns"_s))
                return Exception { ExceptionCode::SyntaxError, "ICE server protocol not supported"_s };
            else if (serverURL.hasQuery())
                return Exception { ExceptionCode::SyntaxError, "Invalid STUN URL"_s };
        }
        if (serverURLs.size())
            servers.append({ WTF::move(serverURLs), server.credential, server.username });
    }
    return servers;
}

ExceptionOr<Vector<MediaEndpointConfiguration::CertificatePEM>> RTCPeerConnection::certificatesFromConfiguration(const RTCConfiguration& configuration)
{
    auto currentMilliSeconds = WallTime::now().secondsSinceEpoch().milliseconds();
    Ref origin = protectedDocument()->securityOrigin();

    Vector<MediaEndpointConfiguration::CertificatePEM> certificates;
    certificates.reserveInitialCapacity(configuration.certificates.size());
    for (auto& certificate : configuration.certificates) {
        if (!origin->isSameOriginAs(certificate->origin()))
            return Exception { ExceptionCode::InvalidAccessError, "Certificate does not have a valid origin"_s };

        if (currentMilliSeconds > certificate->expires())
            return Exception { ExceptionCode::InvalidAccessError, "Certificate has expired"_s };

        certificates.append(MediaEndpointConfiguration::CertificatePEM { certificate->pemCertificate(), certificate->pemPrivateKey(), });
    }
    return certificates;
}

ExceptionOr<void> RTCPeerConnection::initializeWithConfiguration(RTCConfiguration&& configuration)
{
    INFO_LOG(LOGIDENTIFIER);

#if !RELEASE_LOG_DISABLED
    RefPtr document = this->document();
    RefPtr page = document->page();
    ALWAYS_LOG_IF(page && !page->settings().webRTCEncryptionEnabled(), LOGIDENTIFIER, "encryption is disabled");
#endif

    auto servers = iceServersFromConfiguration(configuration, nullptr, false);
    if (servers.hasException())
        return servers.releaseException();

    auto certificates = certificatesFromConfiguration(configuration);
    if (certificates.hasException())
        return certificates.releaseException();

    lazyInitialize(m_backend, PeerConnectionBackend::create(*this, { servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.rtcpMuxPolicy, configuration.iceCandidatePoolSize, certificates.releaseReturnValue(), configuration.targetLatency == RTCConfiguration::TargetLatency::Lowest }));

    if (!m_backend)
        return Exception { ExceptionCode::InvalidAccessError, "Bad Configuration Parameters"_s };

    m_configuration = WTF::move(configuration);

    return { };
}

ExceptionOr<void> RTCPeerConnection::setConfiguration(RTCConfiguration&& configuration)
{
    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError };

    INFO_LOG(LOGIDENTIFIER);

    auto servers = iceServersFromConfiguration(configuration, &m_configuration, m_backend->isLocalDescriptionSet());
    if (servers.hasException())
        return servers.releaseException();

    if (configuration.certificates.size()) {
        if (configuration.certificates.size() != m_configuration.certificates.size())
            return Exception { ExceptionCode::InvalidModificationError, "Certificates parameters are different"_s };

        for (auto& certificate : configuration.certificates) {
            bool isThere = m_configuration.certificates.findIf([&certificate](const auto& item) {
                return item == certificate;
            }) != notFound;
            if (!isThere)
                return Exception { ExceptionCode::InvalidModificationError, "A certificate given in constructor is not present"_s };
        }
    }

    if (configuration.targetLatency != m_configuration.targetLatency)
        return Exception { ExceptionCode::TypeError, "Configuration targetLatency is immutable"_s };

    if (!protectedBackend()->setConfiguration({ servers.releaseReturnValue(), configuration.iceTransportPolicy, configuration.bundlePolicy, configuration.rtcpMuxPolicy, configuration.iceCandidatePoolSize, { } }))
        return Exception { ExceptionCode::InvalidAccessError, "Bad Configuration Parameters"_s };

    m_configuration = WTF::move(configuration);
    return { };
}

void RTCPeerConnection::getStats(MediaStreamTrack* selector, Ref<DeferredPromise>&& promise)
{
    if (selector) {
        for (auto& transceiver : m_transceiverSet.list()) {
            if (transceiver->sender().track() == selector) {
                protectedBackend()->getStats(transceiver->sender(), WTF::move(promise));
                return;
            }
            if (&transceiver->receiver().track() == selector) {
                protectedBackend()->getStats(transceiver->receiver(), WTF::move(promise));
                return;
            }
        }
    }
    promise->whenSettled([pendingActivity = makePendingActivity(*this)] { });
    protectedBackend()->getStats(WTF::move(promise));
}

void RTCPeerConnection::gatherDecoderImplementationName(Function<void(String&&)>&& callback)
{
    protectedBackend()->gatherDecoderImplementationName(WTF::move(callback));
}

// https://w3c.github.io/webrtc-pc/#dom-peerconnection-createdatachannel
ExceptionOr<Ref<RTCDataChannel>> RTCPeerConnection::createDataChannel(String&& label, RTCDataChannelInit&& options)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (isClosed())
        return Exception { ExceptionCode::InvalidStateError };

    if (label.utf8().length() > 65535)
        return Exception { ExceptionCode::TypeError, "label is too long"_s };

    if (options.protocol.utf8().length() > 65535)
        return Exception { ExceptionCode::TypeError, "protocol is too long"_s };

    if (!options.negotiated || !options.negotiated.value())
        options.id = { };
    else if (!options.id)
        return Exception { ExceptionCode::TypeError, "negotiated is true but id is null or undefined"_s };

    if (options.maxPacketLifeTime && options.maxRetransmits)
        return Exception { ExceptionCode::TypeError, "Cannot set both maxPacketLifeTime and maxRetransmits"_s };

    if (options.id && *options.id > 65534)
        return Exception { ExceptionCode::TypeError, "id is too big"_s };

    // FIXME: Provide better error reporting.
    auto channelHandler = protectedBackend()->createDataChannelHandler(label, options);
    if (!channelHandler)
        return Exception { ExceptionCode::OperationError };

    Ref channel = RTCDataChannel::create(*protectedDocument(), WTF::move(channelHandler), WTF::move(label), WTF::move(options), RTCDataChannelState::Connecting);

    m_channels.append(channel->identifier());
    return channel;
}

bool RTCPeerConnection::doClose()
{
    if (isClosed())
        return false;

#if USE(GSTREAMER_WEBRTC)
    if (auto backend = protectedBackend())
        backend->prepareForClose();
#endif

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

    for (auto identifier : std::exchange(m_channels, { }))
        RTCDataChannelHandlerClient::peerConnectionIsClosing(identifier);

    for (auto& transport : m_dtlsTransports)
        transport->close();

    return true;
}

void RTCPeerConnection::close()
{
    if (!doClose())
        return;

    ASSERT(isClosed());
    protectedBackend()->close();
}

ScriptExecutionContext* RTCPeerConnection::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
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
    if (RefPtr backend = m_backend.get())
        backend->stop();
}

void RTCPeerConnection::registerToController(RTCController& controller)
{
    m_controller = &controller;
    controller.add(*this);
}

void RTCPeerConnection::unregisterFromController()
{
    if (RefPtr controller = m_controller.get())
        controller->remove(*this);
}

void RTCPeerConnection::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    m_shouldDelayTasks = true;
    protectedBackend()->suspend();
}

void RTCPeerConnection::resume()
{
    if (!m_shouldDelayTasks)
        return;

    m_shouldDelayTasks = false;
    protectedBackend()->resume();
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
    ALWAYS_LOG(LOGIDENTIFIER, "Adding internal transceiver with mid "_s, transceiver->mid());
    transceiver->setConnection(*this);
    m_transceiverSet.append(WTF::move(transceiver));
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

    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [newState](auto& connection) {
        if (connection.isClosed() || connection.m_iceGatheringState == newState)
            return;

        connection.m_iceGatheringState = newState;
        connection.dispatchEvent(Event::create(eventNames().icegatheringstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        connection.updateConnectionState();
    });
}

void RTCPeerConnection::updateIceConnectionState(RTCIceConnectionState)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [](auto& connection) {
        if (connection.isClosed())
            return;
        auto newState = connection.computeIceConnectionStateFromIceTransports();
        if (connection.m_iceConnectionState == newState)
            return;

        connection.m_iceConnectionState = newState;
        connection.dispatchEvent(Event::create(eventNames().iceconnectionstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
        connection.updateConnectionState();
    });
}

static bool isIceTransportUsedByTransceiver(const RTCIceTransport& iceTransport, RTCRtpTransceiver& transceiver)
{
    auto* dtlsTransport = transceiver.sender().transport();
    return dtlsTransport && &dtlsTransport->iceTransport() == &iceTransport;
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
        return std::ranges::all_of(m_transceiverSet.list(), [&iceTransport](auto& transceiver) {
            return !isIceTransportUsedByTransceiver(iceTransport.get(), transceiver);
        });
    });

    auto dtlsTransports = m_dtlsTransports;
    dtlsTransports.removeAllMatching([&](auto& dtlsTransport) {
        if (m_sctpTransport && &m_sctpTransport->transport() == dtlsTransport.ptr())
            return false;
        return std::ranges::all_of(m_transceiverSet.list(), [&dtlsTransport](auto& transceiver) {
            return transceiver->sender().transport() != dtlsTransport.ptr();
        });
    });

    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Failed; }) || std::ranges::any_of(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::Failed; }))
        return RTCPeerConnectionState::Failed;

    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Disconnected; }))
        return RTCPeerConnectionState::Disconnected;

    if (std::ranges::all_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Closed; }) && std::ranges::all_of(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::New || transport->state() == RTCDtlsTransportState::Closed; }))
        return RTCPeerConnectionState::New;

    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Checking; }) || std::ranges::any_of(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::New || transport->state() == RTCDtlsTransportState::Connecting; }))
        return RTCPeerConnectionState::Connecting;

    ASSERT(std::ranges::all_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Connected || transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }) && std::ranges::all_of(dtlsTransports, [](auto& transport) { return transport->state() == RTCDtlsTransportState::Connected || transport->state() == RTCDtlsTransportState::Closed; }));
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

// https://w3c.github.io/webrtc-pc/#dom-rtciceconnectionstate
RTCIceConnectionState RTCPeerConnection::computeIceConnectionStateFromIceTransports()
{
    if (isClosed())
        return RTCIceConnectionState::Closed;

    auto iceTransports = m_iceTransports;

    iceTransports.removeAllMatching([&](auto& iceTransport) {
        if (m_sctpTransport && &m_sctpTransport->transport().iceTransport() == iceTransport.ptr())
            return false;
        return std::ranges::all_of(m_transceiverSet.list(), [&iceTransport](auto& transceiver) {
            return !isIceTransportUsedByTransceiver(iceTransport.get(), transceiver);
        });
    });

    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Failed; }))
        return RTCIceConnectionState::Failed;
    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Disconnected; }))
        return RTCIceConnectionState::Disconnected;
    if (std::ranges::all_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Closed; }))
        return RTCIceConnectionState::New;
    if (std::ranges::any_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::New || transport->state() == RTCIceTransportState::Checking; }))
        return RTCIceConnectionState::Checking;
    if (std::ranges::all_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }))
        return RTCIceConnectionState::Completed;
    ASSERT(std::ranges::all_of(iceTransports, [](auto& transport) { return transport->state() == RTCIceTransportState::Connected || transport->state() == RTCIceTransportState::Completed || transport->state() == RTCIceTransportState::Closed; }));
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
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [eventId](auto& connection) mutable {
        if (connection.isClosed())
            return;
        if (!eventId) {
            if (!connection.m_negotiationNeededEventId)
                return;
            eventId = connection.m_negotiationNeededEventId;
        }
        if (connection.m_hasPendingOperation) {
            connection.m_negotiationNeededEventId = *eventId;
            return;
        }
        if (connection.signalingState() != RTCSignalingState::Stable) {
            connection.m_negotiationNeededEventId = *eventId;
            return;
        }

        if (!connection.protectedBackend()->isNegotiationNeeded(*eventId))
            return;

        connection.m_negotiationNeededEventId = std::nullopt;
        connection.dispatchEvent(Event::create(eventNames().negotiationneededEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void RTCPeerConnection::scheduleEvent(Ref<Event>&& event)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [event = WTF::move(event)](auto& connection) mutable {
        connection.dispatchEvent(event);
    });
}

void RTCPeerConnection::dispatchEvent(Event& event)
{
    INFO_LOG(LOGIDENTIFIER, "dispatching '", event.type(), "'");
    EventTarget::dispatchEvent(event);
}

void RTCPeerConnection::dispatchDataChannelEvent(UniqueRef<RTCDataChannelHandler>&& channelHandler, String&& label, RTCDataChannelInit&& channelInit)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [label = WTF::move(label), channelHandler = WTF::move(channelHandler), channelInit = WTF::move(channelInit)](auto& connection) mutable {
        if (connection.isClosed())
            return;

        Ref channel = RTCDataChannel::create(*connection.document(), channelHandler.moveToUniquePtr(), WTF::move(label), WTF::move(channelInit), RTCDataChannelState::Open);
        connection.m_channels.append(channel->identifier());
        ALWAYS_LOG_WITH_THIS(&connection, LOGIDENTIFIER_WITH_THIS(&connection), makeString("Dispatching data-channel event for channel "_s, channel->label()));
        connection.dispatchEvent(RTCDataChannelEvent::create(eventNames().datachannelEvent, Event::CanBubble::No, Event::IsCancelable::No, Ref { channel }));
        channel->fireOpenEventIfNeeded();
    });
}

static inline ExceptionOr<PeerConnectionBackend::CertificateInformation> certificateTypeFromAlgorithmIdentifier(JSC::JSGlobalObject& lexicalGlobalObject, RTCPeerConnection::AlgorithmIdentifier&& algorithmIdentifier)
{
    if (std::holds_alternative<String>(algorithmIdentifier))
        return Exception { ExceptionCode::NotSupportedError, "Algorithm is not supported"_s };

    auto& value = std::get<JSC::Strong<JSC::JSObject>>(algorithmIdentifier);

    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    auto parametersConversionResult = convertDictionary<RTCPeerConnection::CertificateParameters>(lexicalGlobalObject, value.get());
    if (parametersConversionResult.hasException(scope)) [[unlikely]] {
        TRY_CLEAR_EXCEPTION(scope, Exception(ExceptionCode::TypeError, "Termination"_s));
        return Exception { ExceptionCode::TypeError, "Unable to read certificate parameters"_s };
    }
    auto parameters = parametersConversionResult.releaseReturnValue();

    if (parameters.expires && *parameters.expires < 0)
        return Exception { ExceptionCode::TypeError, "Expire value is invalid"_s };

    if (parameters.name == "RSASSA-PKCS1-v1_5"_s) {
        if (!parameters.hash.isNull() && parameters.hash != "SHA-256"_s)
            return Exception { ExceptionCode::NotSupportedError, "Only SHA-256 is supported for RSASSA-PKCS1-v1_5"_s };

        auto result = PeerConnectionBackend::CertificateInformation::RSASSA_PKCS1_v1_5();
        if (parameters.modulusLength && parameters.publicExponent) {
            int publicExponent = 0;
            int value = 1;
            for (unsigned counter = 0; counter < parameters.publicExponent->byteLength(); ++counter) {
                publicExponent += parameters.publicExponent->typedSpan()[counter] * value;
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

    return Exception { ExceptionCode::NotSupportedError, "Algorithm is not supported"_s };
}

void RTCPeerConnection::generateCertificate(JSC::JSGlobalObject& lexicalGlobalObject, AlgorithmIdentifier&& algorithmIdentifier, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
{
    auto parameters = certificateTypeFromAlgorithmIdentifier(lexicalGlobalObject, WTF::move(algorithmIdentifier));
    if (parameters.hasException()) {
        promise.reject(parameters.releaseException());
        return;
    }
    Ref document = downcast<Document>(*JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->scriptExecutionContext());
    PeerConnectionBackend::generateCertificate(document.get(), parameters.returnValue(), WTF::move(promise));
}

Vector<std::reference_wrapper<RTCRtpSender>> RTCPeerConnection::getSenders() const
{
    return m_transceiverSet.senders();
}

Vector<std::reference_wrapper<RTCRtpReceiver>> RTCPeerConnection::getReceivers() const
{
    return m_transceiverSet.receivers();
}

const Vector<Ref<RTCRtpTransceiver>>& RTCPeerConnection::getTransceivers() const
{
    return m_transceiverSet.list();
}

void RTCPeerConnection::chainOperation(Ref<DeferredPromise>&& promise, Function<void(Ref<DeferredPromise>&&)>&& operation)
{
    if (isClosed()) {
        promise->reject(ExceptionCode::InvalidStateError, "RTCPeerConnection is closed"_s);
        return;
    }

    promise->whenSettled([this, pendingActivity = makePendingActivity(*this)] {
        ASSERT(m_hasPendingOperation);
        if (isClosed()) {
            for (auto& operation : std::exchange(m_operations, { }))
                operation.first->reject(ExceptionCode::InvalidStateError, "RTCPeerConnection is closed"_s);
            m_hasPendingOperation = false;
            return;
        }

        if (!m_operations.isEmpty()) {
            auto promiseOperation = m_operations.takeFirst();
            promiseOperation.second(WTF::move(promiseOperation.first));
            return;
        }

        m_hasPendingOperation = false;
        if (m_negotiationNeededEventId)
            updateNegotiationNeededFlag({ });
    });

    if (m_hasPendingOperation || !m_operations.isEmpty()) {
        m_operations.append(std::make_pair(WTF::move(promise), WTF::move(operation)));
        return;
    }

    m_hasPendingOperation = true;
    operation(WTF::move(promise));
}

Document* RTCPeerConnection::document()
{
    return downcast<Document>(scriptExecutionContext());
}

RefPtr<Document> RTCPeerConnection::protectedDocument()
{
    return document();
}

Ref<RTCIceTransport> RTCPeerConnection::getOrCreateIceTransport(UniqueRef<RTCIceTransportBackend>&& backend)
{
    auto index = m_iceTransports.findIf([&backend](auto& transport) { return backend.get() == transport->backend(); });
    if (index == notFound) {
        index = m_iceTransports.size();
        m_iceTransports.append(RTCIceTransport::create(*protectedScriptExecutionContext(), WTF::move(backend), *this));
    }

    return m_iceTransports[index].copyRef();
}


RefPtr<RTCDtlsTransport> RTCPeerConnection::getOrCreateDtlsTransport(std::unique_ptr<RTCDtlsTransportBackend>&& backend)
{
    if (!backend)
        return nullptr;

    RefPtr context = scriptExecutionContext();
    if (!context)
        return nullptr;

    auto index = m_dtlsTransports.findIf([&backend](auto& transport) { return *backend == transport->backend(); });
    if (index == notFound) {
        index = m_dtlsTransports.size();
        auto iceTransportBackend = backend->iceTransportBackend();
        m_dtlsTransports.append(RTCDtlsTransport::create(*context, makeUniqueRefFromNonNullUniquePtr(WTF::move(backend)), getOrCreateIceTransport(WTF::move(iceTransportBackend))));
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
    description = RTCSessionDescription::create(*type, WTF::move(sdp));
}

void RTCPeerConnection::updateDescriptions(PeerConnectionBackend::DescriptionStates&& states)
{
    updateDescription(m_currentLocalDescription, states.currentLocalDescriptionSdpType, WTF::move(states.currentLocalDescriptionSdp));
    updateDescription(m_pendingLocalDescription, states.pendingLocalDescriptionSdpType, WTF::move(states.pendingLocalDescriptionSdp));
    updateDescription(m_currentRemoteDescription, states.currentRemoteDescriptionSdpType, WTF::move(states.currentRemoteDescriptionSdp));
    updateDescription(m_pendingRemoteDescription, states.pendingRemoteDescriptionSdpType, WTF::move(states.pendingRemoteDescriptionSdp));

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
        if (RefPtr senderBackend = sender.backend())
            sender.setTransport(getOrCreateDtlsTransport(senderBackend->dtlsTransportBackend()));

        auto& receiver = transceiver->receiver();
        if (auto* receiverBackend = receiver.backend())
            receiver.setTransport(getOrCreateDtlsTransport(receiverBackend->dtlsTransportBackend()));
    }
}

// https://w3c.github.io/webrtc-pc/#set-description step 4.9.1
void RTCPeerConnection::updateTransceiversAfterSuccessfulLocalDescription()
{
    protectedBackend()->collectTransceivers();
    updateTransceiverTransports();
}

// https://w3c.github.io/webrtc-pc/#set-description step 4.9.2
void RTCPeerConnection::updateTransceiversAfterSuccessfulRemoteDescription()
{
    protectedBackend()->collectTransceivers();
    updateTransceiverTransports();
}

void RTCPeerConnection::updateSctpBackend(std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend, std::optional<double> maxMessageSize)
{
    if (!sctpBackend) {
        m_sctpTransport = nullptr;
        return;
    }

    RefPtr sctpTransport = m_sctpTransport;
    if (!sctpTransport || sctpTransport->backend() != *sctpBackend) {
        RefPtr context = scriptExecutionContext();
        if (!context)
            return;

        RefPtr dtlsTransport = getOrCreateDtlsTransport(sctpBackend->dtlsTransportBackend().moveToUniquePtr());
        if (!dtlsTransport)
            return;
        sctpTransport = RTCSctpTransport::create(*context, makeUniqueRefFromNonNullUniquePtr(WTF::move(sctpBackend)), dtlsTransport.releaseNonNull());
        m_sctpTransport = sctpTransport.copyRef();
    }

    sctpTransport->updateMaxMessageSize(maxMessageSize);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCPeerConnection::logChannel() const
{
    return LogWebRTC;
}
#endif

void RTCPeerConnection::startGatheringStatLogs(Function<void(String&&)>&& callback)
{
    protectedBackend()->startGatheringStatLogs(WTF::move(callback));
}

void RTCPeerConnection::stopGatheringStatLogs()
{
    protectedBackend()->stopGatheringStatLogs();
}

void RTCPeerConnection::clearTransports()
{
    m_dtlsTransports.clear();
    m_iceTransports.clear();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
