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
#include "JSDOMPromiseDeferred.h"
#include "JSRTCSessionDescription.h"
#include "LibWebRTCCertificateGenerator.h"
#include "Logging.h"
#include "Page.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionIceEvent.h"
#include "RTCRtpCapabilities.h"
#include "RTCTrackEvent.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

using namespace PAL;

#if !USE(LIBWEBRTC)
static std::unique_ptr<PeerConnectionBackend> createNoPeerConnectionBackend(RTCPeerConnection&)
{
    return nullptr;
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createNoPeerConnectionBackend;

Optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext&, const String&)
{
    ASSERT_NOT_REACHED();
    return { };
}

Optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext&, const String&)
{
    ASSERT_NOT_REACHED();
    return { };
}
#endif

PeerConnectionBackend::PeerConnectionBackend(RTCPeerConnection& peerConnection)
    : m_peerConnection(peerConnection)
#if !RELEASE_LOG_DISABLED
    , m_logger(peerConnection.logger())
    , m_logIdentifier(peerConnection.logIdentifier())
#endif
{
}

PeerConnectionBackend::~PeerConnectionBackend() = default;

void PeerConnectionBackend::createOffer(RTCOfferOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerPromise = WTF::makeUnique<PeerConnection::SessionDescriptionPromise>(WTFMove(promise));
    doCreateOffer(WTFMove(options));
}

void PeerConnectionBackend::createOfferSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create offer succeeded:\n", sdp);

    ASSERT(m_offerAnswerPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_offerAnswerPromise), sdp = filterSDP(WTFMove(sdp))]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->resolve(RTCSessionDescription::Init { RTCSdpType::Offer, sdp });
    });
}

void PeerConnectionBackend::createOfferFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create offer failed:", exception.message());

    ASSERT(m_offerAnswerPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_offerAnswerPromise), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->reject(WTFMove(exception));
    });
}

void PeerConnectionBackend::createAnswer(RTCAnswerOptions&& options, PeerConnection::SessionDescriptionPromise&& promise)
{
    ASSERT(!m_offerAnswerPromise);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerPromise = WTF::makeUnique<PeerConnection::SessionDescriptionPromise>(WTFMove(promise));
    doCreateAnswer(WTFMove(options));
}

void PeerConnectionBackend::createAnswerSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create answer succeeded:\n", sdp);

    ASSERT(m_offerAnswerPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_offerAnswerPromise), sdp = WTFMove(sdp)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->resolve(RTCSessionDescription::Init { RTCSdpType::Answer, sdp });
    });
}

void PeerConnectionBackend::createAnswerFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create answer failed:", exception.message());

    ASSERT(m_offerAnswerPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_offerAnswerPromise), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->reject(WTFMove(exception));
    });
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

    m_setDescriptionPromise = WTF::makeUnique<DOMPromiseDeferred<void>>(WTFMove(promise));
    doSetLocalDescription(sessionDescription);
}

void PeerConnectionBackend::setLocalDescriptionSucceeded()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(m_setDescriptionPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_setDescriptionPromise)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->resolve();
    });
}

void PeerConnectionBackend::setLocalDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set local description failed:", exception.message());

    ASSERT(m_setDescriptionPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_setDescriptionPromise), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->reject(WTFMove(exception));
    });
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

    m_setDescriptionPromise = WTF::makeUnique<DOMPromiseDeferred<void>>(WTFMove(promise));
    doSetRemoteDescription(sessionDescription);
}

void PeerConnectionBackend::setRemoteDescriptionSucceeded()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description succeeded");
    ASSERT(m_setDescriptionPromise);

    auto promise = WTFMove(m_setDescriptionPromise);
    auto events = WTFMove(m_pendingTrackEvents);
    for (auto& event : events) {
        auto& track = event.track.get();

        m_peerConnection.dispatchEventWhenFeasible(RTCTrackEvent::create(eventNames().trackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(event.receiver), WTFMove(event.track), WTFMove(event.streams), WTFMove(event.transceiver)));

        if (m_peerConnection.isClosed())
            return;

        // FIXME: As per spec, we should set muted to 'false' when starting to receive the content from network.
        track.source().setMuted(false);
    }

    m_peerConnection.doTask([this, promise = WTFMove(promise)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->resolve();
    });
}

void PeerConnectionBackend::setRemoteDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description failed:", exception.message());

    ASSERT(m_pendingTrackEvents.isEmpty());
    m_pendingTrackEvents.clear();

    ASSERT(m_setDescriptionPromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_setDescriptionPromise), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->reject(WTFMove(exception));
    });
}

void PeerConnectionBackend::addPendingTrackEvent(PendingTrackEvent&& event)
{
    ASSERT(!m_peerConnection.isClosed());
    m_pendingTrackEvents.append(WTFMove(event));
}

static String extractIPAddres(const String& sdp)
{
    ASSERT(sdp.contains(" host "));
    unsigned counter = 0;
    for (auto item : StringView { sdp }.split(' ')) {
        if (++counter == 5)
            return item.toString();
    }
    return { };
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
        promise.reject(Exception { TypeError, "Trying to add a candidate that is missing both sdpMid and sdpMLineIndex"_s });
        return;
    }
    m_addIceCandidatePromise = WTF::makeUnique<DOMPromiseDeferred<void>>(WTFMove(promise));
    doAddIceCandidate(*iceCandidate);
}

void PeerConnectionBackend::addIceCandidateSucceeded()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Adding ice candidate succeeded");

    ASSERT(m_addIceCandidatePromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_addIceCandidatePromise)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->resolve();
    });
}

void PeerConnectionBackend::addIceCandidateFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Adding ice candidate failed:", exception.message());

    ASSERT(m_addIceCandidatePromise);
    m_peerConnection.doTask([this, promise = WTFMove(m_addIceCandidatePromise), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        promise->reject(WTFMove(exception));
    });
}

void PeerConnectionBackend::fireICECandidateEvent(RefPtr<RTCIceCandidate>&& candidate, String&& serverURL)
{
    ASSERT(isMainThread());

    m_peerConnection.dispatchEventWhenFeasible(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, WTFMove(candidate), WTFMove(serverURL)));
}

void PeerConnectionBackend::enableICECandidateFiltering()
{
    m_shouldFilterICECandidates = true;
}

void PeerConnectionBackend::disableICECandidateFiltering()
{
    m_shouldFilterICECandidates = false;
    for (auto& pendingICECandidate : m_pendingICECandidates)
        fireICECandidateEvent(RTCIceCandidate::create(WTFMove(pendingICECandidate.sdp), WTFMove(pendingICECandidate.mid), pendingICECandidate.sdpMLineIndex), WTFMove(pendingICECandidate.serverURL));
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
    sdp.split(' ', [&](StringView item) {
        if (skipNextItem) {
            skipNextItem = false;
            return;
        }
        if (item == "raddr") {
            filteredSDP.append(" raddr 0.0.0.0");
            skipNextItem = true;
            return;
        }
        if (item == "rport") {
            filteredSDP.append(" rport 0");
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
        return WTFMove(sdp);

    StringBuilder filteredSDP;
    sdp.split('\n', [&filteredSDP](StringView line) {
        if (line.startsWith("c=IN IP4"))
            filteredSDP.append("c=IN IP4 0.0.0.0\r");
        else if (line.startsWith("c=IN IP6"))
            filteredSDP.append("c=IN IP6 ::\r");
        else if (!line.startsWith("a=candidate"))
            filteredSDP.append(line);
        else if (line.find(" host ", 11) == notFound)
            filteredSDP.append(filterICECandidate(line.toString()));
        else
            return;
        filteredSDP.append('\n');
    });
    return filteredSDP.toString();
}

void PeerConnectionBackend::newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex, String&& serverURL)
{
    ALWAYS_LOG(LOGIDENTIFIER, "Gathered ice candidate:", sdp);
    m_finishedGatheringCandidates = false;

    if (!m_shouldFilterICECandidates) {
        fireICECandidateEvent(RTCIceCandidate::create(WTFMove(sdp), WTFMove(mid), sdpMLineIndex), WTFMove(serverURL));
        return;
    }
    if (sdp.find(" host ", 0) != notFound) {
        // FIXME: We might need to clear all pending candidates when setting again local description.
        m_pendingICECandidates.append(PendingICECandidate { String { sdp }, WTFMove(mid), sdpMLineIndex, WTFMove(serverURL) });
        if (RuntimeEnabledFeatures::sharedFeatures().webRTCMDNSICECandidatesEnabled()) {
            auto ipAddress = extractIPAddres(sdp);
            // We restrict to IPv4 candidates for now.
            if (ipAddress.contains('.'))
                registerMDNSName(ipAddress);
        }
        return;
    }
    fireICECandidateEvent(RTCIceCandidate::create(filterICECandidate(WTFMove(sdp)), WTFMove(mid), sdpMLineIndex), WTFMove(serverURL));
}

void PeerConnectionBackend::doneGatheringCandidates()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Finished ice candidate gathering");
    m_finishedGatheringCandidates = true;

    if (m_waitingForMDNSRegistration)
        return;

    m_peerConnection.dispatchEventWhenFeasible(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, nullptr, { }));
    m_peerConnection.updateIceGatheringState(RTCIceGatheringState::Complete);
    m_pendingICECandidates.clear();
}

void PeerConnectionBackend::endOfIceCandidates(DOMPromiseDeferred<void>&& promise)
{
    promise.resolve();
}

void PeerConnectionBackend::registerMDNSName(const String& ipAddress)
{
    ++m_waitingForMDNSRegistration;
    auto& document = downcast<Document>(*m_peerConnection.scriptExecutionContext());
    auto& provider = document.page()->libWebRTCProvider();
    provider.registerMDNSName(document.identifier().toUInt64(), ipAddress, [peerConnection = makeRef(m_peerConnection), this, ipAddress] (LibWebRTCProvider::MDNSNameOrError&& result) {
        if (peerConnection->isStopped())
            return;

        --m_waitingForMDNSRegistration;
        if (!result.has_value()) {
            m_peerConnection.scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, makeString("MDNS registration of a host candidate failed with error ", (unsigned)result.error()));
            return;
        }

        this->finishedRegisteringMDNSName(ipAddress, result.value());
    });
}

void PeerConnectionBackend::finishedRegisteringMDNSName(const String& ipAddress, const String& name)
{
    Vector<PendingICECandidate*> candidates;
    for (auto& candidate : m_pendingICECandidates) {
        if (candidate.sdp.find(ipAddress) != notFound) {
            auto sdp = candidate.sdp;
            sdp.replace(ipAddress, name);
            fireICECandidateEvent(RTCIceCandidate::create(WTFMove(sdp), WTFMove(candidate.mid), candidate.sdpMLineIndex), WTFMove(candidate.serverURL));
            candidates.append(&candidate);
        }
    }
    m_pendingICECandidates.removeAllMatching([&] (const auto& candidate) {
        return candidates.contains(&candidate);
    });

    if (!m_waitingForMDNSRegistration && m_finishedGatheringCandidates)
        doneGatheringCandidates();
}

void PeerConnectionBackend::updateSignalingState(RTCSignalingState newSignalingState)
{
    ASSERT(isMainThread());

    if (newSignalingState != m_peerConnection.signalingState()) {
        m_peerConnection.setSignalingState(newSignalingState);
        m_peerConnection.dispatchEventWhenFeasible(Event::create(eventNames().signalingstatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

void PeerConnectionBackend::stop()
{
    m_offerAnswerPromise = nullptr;
    m_setDescriptionPromise = nullptr;
    m_addIceCandidatePromise = nullptr;

    m_pendingTrackEvents.clear();

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

ExceptionOr<Ref<RTCRtpSender>> PeerConnectionBackend::addTrack(MediaStreamTrack&, Vector<String>&&)
{
    return Exception { NotSupportedError, "Not implemented"_s };
}

ExceptionOr<Ref<RTCRtpTransceiver>> PeerConnectionBackend::addTransceiver(const String&, const RTCRtpTransceiverInit&)
{
    return Exception { NotSupportedError, "Not implemented"_s };
}

ExceptionOr<Ref<RTCRtpTransceiver>> PeerConnectionBackend::addTransceiver(Ref<MediaStreamTrack>&&, const RTCRtpTransceiverInit&)
{
    return Exception { NotSupportedError, "Not implemented"_s };
}

void PeerConnectionBackend::generateCertificate(Document& document, const CertificateInformation& info, DOMPromiseDeferred<IDLInterface<RTCCertificate>>&& promise)
{
#if USE(LIBWEBRTC)
    LibWebRTCCertificateGenerator::generateCertificate(document.securityOrigin(), document.page()->libWebRTCProvider(), info, WTFMove(promise));
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(expires);
    UNUSED_PARAM(type);
    promise.reject(NotSupportedError);
#endif
}

ScriptExecutionContext* PeerConnectionBackend::context() const
{
    return m_peerConnection.scriptExecutionContext();
}

RTCRtpTransceiver* PeerConnectionBackend::transceiverFromSender(const RTCRtpSender& sender)
{
    for (auto& transceiver : m_peerConnection.currentTransceivers()) {
        if (&transceiver->sender() == &sender)
            return transceiver.get();
    }
    return nullptr;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PeerConnectionBackend::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
