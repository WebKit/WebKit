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
#include "JSRTCCertificate.h"
#include "LibWebRTCCertificateGenerator.h"
#include "Logging.h"
#include "Page.h"
#include "RTCDataChannelEvent.h"
#include "RTCDtlsTransport.h"
#include "RTCIceCandidate.h"
#include "RTCPeerConnection.h"
#include "RTCPeerConnectionIceEvent.h"
#include "RTCRtpCapabilities.h"
#include "RTCSctpTransportBackend.h"
#include "RTCSessionDescriptionInit.h"
#include "RTCTrackEvent.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/UUID.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

#if !USE(LIBWEBRTC)
static std::unique_ptr<PeerConnectionBackend> createNoPeerConnectionBackend(RTCPeerConnection&)
{
    return nullptr;
}

CreatePeerConnectionBackend PeerConnectionBackend::create = createNoPeerConnectionBackend;

std::optional<RTCRtpCapabilities> PeerConnectionBackend::receiverCapabilities(ScriptExecutionContext&, const String&)
{
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<RTCRtpCapabilities> PeerConnectionBackend::senderCapabilities(ScriptExecutionContext&, const String&)
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
#if USE(LIBWEBRTC)
    auto* document = peerConnection.document();
    if (auto* page = document ? document->page() : nullptr)
        m_shouldFilterICECandidates = page->libWebRTCProvider().isSupportingMDNS();
#endif
}

PeerConnectionBackend::~PeerConnectionBackend() = default;

void PeerConnectionBackend::createOffer(RTCOfferOptions&& options, CreateCallback&& callback)
{
    ASSERT(!m_offerAnswerCallback);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerCallback = WTFMove(callback);
    doCreateOffer(WTFMove(options));
}

void PeerConnectionBackend::createOfferSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create offer succeeded:\n", sdp);

    ASSERT(m_offerAnswerCallback);
    validateSDP(sdp);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), sdp = WTFMove(sdp)]() mutable {
        callback(RTCSessionDescriptionInit { RTCSdpType::Offer, sdp });
    });
}

void PeerConnectionBackend::createOfferFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create offer failed:", exception.message());

    ASSERT(m_offerAnswerCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), exception = WTFMove(exception)]() mutable {
        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::createAnswer(RTCAnswerOptions&& options, CreateCallback&& callback)
{
    ASSERT(!m_offerAnswerCallback);
    ASSERT(!m_peerConnection.isClosed());

    m_offerAnswerCallback = WTFMove(callback);
    doCreateAnswer(WTFMove(options));
}

void PeerConnectionBackend::createAnswerSucceeded(String&& sdp)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create answer succeeded:\n", sdp);

    ASSERT(m_offerAnswerCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), sdp = WTFMove(sdp)]() mutable {
        callback(RTCSessionDescriptionInit { RTCSdpType::Answer, sdp });
    });
}

void PeerConnectionBackend::createAnswerFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Create answer failed:", exception.message());

    ASSERT(m_offerAnswerCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [callback = WTFMove(m_offerAnswerCallback), exception = WTFMove(exception)]() mutable {
        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::setLocalDescription(const RTCSessionDescription* sessionDescription, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection.isClosed());

    m_setDescriptionCallback = WTFMove(callback);
    doSetLocalDescription(sessionDescription);
}

void PeerConnectionBackend::setLocalDescriptionSucceeded(std::optional<DescriptionStates>&& descriptionStates, std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(m_setDescriptionCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), descriptionStates = WTFMove(descriptionStates), sctpBackend = WTFMove(sctpBackend)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        if (descriptionStates)
            m_peerConnection.updateDescriptions(WTFMove(*descriptionStates));
        m_peerConnection.updateTransceiversAfterSuccessfulLocalDescription();
        m_peerConnection.updateSctpBackend(WTFMove(sctpBackend));
        m_peerConnection.processIceTransportChanges();
        callback({ });
    });
}

void PeerConnectionBackend::setLocalDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set local description failed:", exception.message());

    ASSERT(m_setDescriptionCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::setRemoteDescription(const RTCSessionDescription& sessionDescription, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection.isClosed());

    m_setDescriptionCallback = WTFMove(callback);
    doSetRemoteDescription(sessionDescription);
}

void PeerConnectionBackend::setRemoteDescriptionSucceeded(std::optional<DescriptionStates>&& descriptionStates, std::unique_ptr<RTCSctpTransportBackend>&& sctpBackend)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description succeeded");
    ASSERT(m_setDescriptionCallback);

    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), descriptionStates = WTFMove(descriptionStates), sctpBackend = WTFMove(sctpBackend), events = WTFMove(m_pendingTrackEvents)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        if (descriptionStates)
            m_peerConnection.updateDescriptions(WTFMove(*descriptionStates));

        for (auto& event : events) {
            auto& track = event.track.get();

            m_peerConnection.dispatchEvent(RTCTrackEvent::create(eventNames().trackEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(event.receiver), WTFMove(event.track), WTFMove(event.streams), WTFMove(event.transceiver)));
            ALWAYS_LOG(LOGIDENTIFIER, "Dispatched if feasible track of type ", track.source().type());

            if (m_peerConnection.isClosed())
                return;

            // FIXME: As per spec, we should set muted to 'false' when starting to receive the content from network.
            track.source().setMuted(false);
        }

        if (m_peerConnection.isClosed())
            return;

        m_peerConnection.updateTransceiversAfterSuccessfulRemoteDescription();
        m_peerConnection.updateSctpBackend(WTFMove(sctpBackend));
        m_peerConnection.processIceTransportChanges();
        callback({ });
    });
}

void PeerConnectionBackend::setRemoteDescriptionFailed(Exception&& exception)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Set remote description failed:", exception.message());

    ASSERT(m_pendingTrackEvents.isEmpty());
    m_pendingTrackEvents.clear();

    ASSERT(m_setDescriptionCallback);
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [this, callback = WTFMove(m_setDescriptionCallback), exception = WTFMove(exception)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        callback(WTFMove(exception));
    });
}

void PeerConnectionBackend::iceGatheringStateChanged(RTCIceGatheringState state)
{
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [this, state] {
        if (state == RTCIceGatheringState::Complete) {
            doneGatheringCandidates();
            return;
        }
        m_peerConnection.updateIceGatheringState(state);
    });
}

void PeerConnectionBackend::addPendingTrackEvent(PendingTrackEvent&& event)
{
    ASSERT(!m_peerConnection.isStopped());
    m_pendingTrackEvents.append(WTFMove(event));
}

static String extractIPAddress(StringView sdp)
{
    unsigned counter = 0;
    for (auto item : StringView { sdp }.split(' ')) {
        if (++counter == 5)
            return item.toString();
    }
    return { };
}

static inline bool shouldIgnoreIceCandidate(const RTCIceCandidate& iceCandidate)
{
    auto address = extractIPAddress(iceCandidate.candidate());
    if (!address.endsWithIgnoringASCIICase(".local"_s))
        return false;

    if (!WTF::isVersion4UUID(StringView { address }.substring(0, address.length() - 6))) {
        RELEASE_LOG_ERROR(WebRTC, "mDNS candidate is not a Version 4 UUID");
        return true;
    }
    return false;
}

void PeerConnectionBackend::addIceCandidate(RTCIceCandidate* iceCandidate, Function<void(ExceptionOr<void>&&)>&& callback)
{
    ASSERT(!m_peerConnection.isClosed());

    if (!iceCandidate) {
        callback({ });
        return;
    }

    if (shouldIgnoreIceCandidate(*iceCandidate)) {
        callback({ });
        return;
    }

    doAddIceCandidate(*iceCandidate, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto&& result) mutable {
        if (!weakThis)
            return;

        auto& peerConnection = weakThis->m_peerConnection;
        peerConnection.queueTaskKeepingObjectAlive(peerConnection, TaskSource::Networking, [&peerConnection, callback = WTFMove(callback), result = WTFMove(result)]() mutable {
            if (peerConnection.isClosed())
                return;

            if (result.hasException()) {
                RELEASE_LOG_ERROR(WebRTC, "Adding ice candidate failed %d", result.exception().code());
                callback(result.releaseException());
                return;
            }

            if (auto descriptions = result.releaseReturnValue())
                peerConnection.updateDescriptions(WTFMove(*descriptions));
            callback({ });
        });
    });
}

void PeerConnectionBackend::enableICECandidateFiltering()
{
    m_shouldFilterICECandidates = true;
}

void PeerConnectionBackend::disableICECandidateFiltering()
{
    m_shouldFilterICECandidates = false;
}

void PeerConnectionBackend::validateSDP(const String& sdp) const
{
#ifndef NDEBUG
    if (!m_shouldFilterICECandidates)
        return;
    sdp.split('\n', [](auto line) {
        ASSERT(!line.startsWith("a=candidate") || line.contains(".local"));
    });
#else
    UNUSED_PARAM(sdp);
#endif
}

void PeerConnectionBackend::newICECandidate(String&& sdp, String&& mid, unsigned short sdpMLineIndex, String&& serverURL, std::optional<DescriptionStates>&& descriptions)
{
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [logSiteIdentifier = LOGIDENTIFIER, this, sdp = WTFMove(sdp), mid = WTFMove(mid), sdpMLineIndex, serverURL = WTFMove(serverURL), descriptions = WTFMove(descriptions)]() mutable {
        if (m_peerConnection.isClosed())
            return;

        if (descriptions)
            m_peerConnection.updateDescriptions(WTFMove(*descriptions));

        if (m_peerConnection.isClosed())
            return;

        UNUSED_PARAM(logSiteIdentifier);
        ALWAYS_LOG(logSiteIdentifier, "Gathered ice candidate:", sdp);
        m_finishedGatheringCandidates = false;

        ASSERT(!m_shouldFilterICECandidates || sdp.contains(".local") || sdp.contains(" srflx "));
        auto candidate = RTCIceCandidate::create(WTFMove(sdp), WTFMove(mid), sdpMLineIndex);
        m_peerConnection.dispatchEvent(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, WTFMove(candidate), WTFMove(serverURL)));
    });
}

void PeerConnectionBackend::newDataChannel(UniqueRef<RTCDataChannelHandler>&& channelHandler, String&& label, RTCDataChannelInit&& channelInit)
{
    m_peerConnection.queueTaskKeepingObjectAlive(m_peerConnection, TaskSource::Networking, [connection = Ref { m_peerConnection }, label = WTFMove(label), channelHandler = WTFMove(channelHandler), channelInit = WTFMove(channelInit)]() mutable {
        if (connection->isClosed())
            return;

        auto channel = RTCDataChannel::create(*connection->document(), channelHandler.moveToUniquePtr(), WTFMove(label), WTFMove(channelInit));
        connection->dispatchEvent(RTCDataChannelEvent::create(eventNames().datachannelEvent, Event::CanBubble::No, Event::IsCancelable::No, WTFMove(channel)));
    });
}

void PeerConnectionBackend::doneGatheringCandidates()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, "Finished ice candidate gathering");
    m_finishedGatheringCandidates = true;

    m_peerConnection.scheduleEvent(RTCPeerConnectionIceEvent::create(Event::CanBubble::No, Event::IsCancelable::No, nullptr, { }));
    m_peerConnection.updateIceGatheringState(RTCIceGatheringState::Complete);
}

void PeerConnectionBackend::stop()
{
    m_offerAnswerCallback = nullptr;
    m_setDescriptionCallback = nullptr;

    m_pendingTrackEvents.clear();

    doStop();
}

void PeerConnectionBackend::markAsNeedingNegotiation(uint32_t eventId)
{
    m_peerConnection.updateNegotiationNeededFlag(eventId);
}

ExceptionOr<Ref<RTCRtpSender>> PeerConnectionBackend::addTrack(MediaStreamTrack&, FixedVector<String>&&)
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
    auto* page = document.page();
    if (!page) {
        promise.reject(InvalidStateError);
        return;
    }
    LibWebRTCCertificateGenerator::generateCertificate(document.securityOrigin(), page->libWebRTCProvider(), info, [promise = WTFMove(promise)](auto&& result) mutable {
        promise.settle(WTFMove(result));
    });
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

#if !RELEASE_LOG_DISABLED
WTFLogChannel& PeerConnectionBackend::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
