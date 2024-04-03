/*
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
#include "RTCRtpSender.h"

#if ENABLE(WEB_RTC)

#include "ContextDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "RTCDTMFSender.h"
#include "RTCDTMFSenderBackend.h"
#include "RTCPeerConnection.h"
#include "RTCRtpCapabilities.h"
#include "RTCRtpTransceiver.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

#if !RELEASE_LOG_DISABLED
#define LOGIDENTIFIER_SENDER Logger::LogSiteIdentifier(logClassName(), __func__, m_connection->logIdentifier())
#else
#define LOGIDENTIFIER_SENDER
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCRtpSender);

Ref<RTCRtpSender> RTCRtpSender::create(RTCPeerConnection& connection, Ref<MediaStreamTrack>&& track, std::unique_ptr<RTCRtpSenderBackend>&& backend)
{
    auto sender = adoptRef(*new RTCRtpSender(connection, String(track->kind()), WTFMove(backend)));
    sender->setTrack(WTFMove(track));
    return sender;
}

Ref<RTCRtpSender> RTCRtpSender::create(RTCPeerConnection& connection, String&& trackKind, std::unique_ptr<RTCRtpSenderBackend>&& backend)
{
    return adoptRef(*new RTCRtpSender(connection, WTFMove(trackKind), WTFMove(backend)));
}

RTCRtpSender::RTCRtpSender(RTCPeerConnection& connection, String&& trackKind, std::unique_ptr<RTCRtpSenderBackend>&& backend)
    : m_trackKind(WTFMove(trackKind))
    , m_backend(WTFMove(backend))
    , m_connection(connection)
#if !RELEASE_LOG_DISABLED
    , m_logger(connection.logger())
    , m_logIdentifier(connection.logIdentifier())
#endif
{
    ASSERT(m_backend);
}

RTCRtpSender::~RTCRtpSender()
{
    if (m_transform)
        m_transform->detachFromSender(*this);
}

void RTCRtpSender::setTrackToNull()
{
    ASSERT(m_track);
    m_trackId = { };
    m_track = nullptr;
}

void RTCRtpSender::stop()
{
    if (m_transform)
        m_transform->detachFromSender(*this);

    m_trackId = { };
    m_track = nullptr;
    m_backend = nullptr;
}

void RTCRtpSender::setTrack(Ref<MediaStreamTrack>&& track)
{
    ASSERT(!isStopped());
    if (!m_track)
        m_trackId = track->id();
    m_track = WTFMove(track);
}

void RTCRtpSender::replaceTrack(RefPtr<MediaStreamTrack>&& withTrack, Ref<DeferredPromise>&& promise)
{
    if (withTrack && m_trackKind != withTrack->kind()) {
        promise->reject(ExceptionCode::TypeError);
        return;
    }

    if (!m_connection) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    m_connection->chainOperation(WTFMove(promise), [this, weakThis = WeakPtr { *this }, withTrack = WTFMove(withTrack)](Ref<DeferredPromise>&& promise) mutable {
        if (!weakThis)
            return;
        if (isStopped()) {
            promise->reject(ExceptionCode::InvalidStateError);
            return;
        }

        if (!m_backend->replaceTrack(*this, withTrack.get())) {
            promise->reject(ExceptionCode::InvalidModificationError);
            return;
        }

        RefPtr context = m_connection->scriptExecutionContext();
        if (!context)
            return;

        context->postTask([this, protectedThis = Ref { *this }, withTrack = WTFMove(withTrack), promise = WTFMove(promise)](auto&) mutable {
            if (!m_connection || m_connection->isClosed())
                return;

            m_track = WTFMove(withTrack);
            promise->resolve();
        });
    });
}

RTCRtpSendParameters RTCRtpSender::getParameters()
{
    if (isStopped())
        return { };
    return m_backend->getParameters();
}

void RTCRtpSender::setParameters(const RTCRtpSendParameters& parameters, DOMPromiseDeferred<void>&& promise)
{
    if (isStopped()) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }
    return m_backend->setParameters(parameters, WTFMove(promise));
}

ExceptionOr<void> RTCRtpSender::setStreams(const FixedVector<std::reference_wrapper<MediaStream>>& streams)
{
    return setMediaStreamIds(WTF::map(streams, [](auto& stream) -> String {
        return stream.get().id();
    }));
}

ExceptionOr<void> RTCRtpSender::setMediaStreamIds(const FixedVector<String>& streamIds)
{
    if (!m_connection || m_connection->isClosed() || !m_backend)
        return Exception { ExceptionCode::InvalidStateError, "connection is closed"_s };
    m_backend->setMediaStreamIds(streamIds);
    return { };
}

void RTCRtpSender::getStats(Ref<DeferredPromise>&& promise)
{
    if (!m_connection) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    m_connection->getStats(*this, WTFMove(promise));
}

bool RTCRtpSender::isCreatedBy(const RTCPeerConnection& connection) const
{
    return &connection == m_connection.get();
}

std::optional<RTCRtpCapabilities> RTCRtpSender::getCapabilities(ScriptExecutionContext& context, const String& kind)
{
    return PeerConnectionBackend::senderCapabilities(context, kind);
}

RTCDTMFSender* RTCRtpSender::dtmf()
{
    if (!m_dtmfSender && m_connection && m_connection->scriptExecutionContext() && m_backend && m_trackKind == "audio"_s)
        m_dtmfSender = RTCDTMFSender::create(*m_connection->scriptExecutionContext(), *this, m_backend->createDTMFBackend());

    return m_dtmfSender.get();
}

std::optional<RTCRtpTransceiverDirection> RTCRtpSender::currentTransceiverDirection() const
{
    if (!m_connection)
        return { };

    RTCRtpTransceiver* senderTransceiver = nullptr;
    for (auto& transceiver : m_connection->currentTransceivers()) {
        if (&transceiver->sender() == this) {
            senderTransceiver = transceiver.get();
            break;
        }
    }

    if (!senderTransceiver)
        return { };

    return senderTransceiver->currentDirection();
}

ExceptionOr<void> RTCRtpSender::setTransform(std::unique_ptr<RTCRtpTransform>&& transform)
{
    ALWAYS_LOG_IF(m_connection, LOGIDENTIFIER_SENDER);

    if (transform && m_transform && *transform == *m_transform)
        return { };
    if (!transform) {
        if (m_transform) {
            m_transform->detachFromSender(*this);
            m_transform = { };
        }
        return { };
    }

    if (transform->isAttached())
        return Exception { ExceptionCode::InvalidStateError, "transform is already in use"_s };

    transform->attachToSender(*this, m_transform.get());
    m_transform = WTFMove(transform);

    return { };
}

std::optional<RTCRtpTransform::Internal> RTCRtpSender::transform()
{
    if (!m_transform)
        return { };
    return m_transform->internalTransform();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCRtpSender::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
