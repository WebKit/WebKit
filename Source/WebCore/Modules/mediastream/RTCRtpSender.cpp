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

#include "RTCRtpCapabilities.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

Ref<RTCRtpSender> RTCRtpSender::create(PeerConnectionBackend& connection, Ref<MediaStreamTrack>&& track, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&& backend)
{
    auto sender = adoptRef(*new RTCRtpSender(connection, String(track->kind()), WTFMove(mediaStreamIds), WTFMove(backend)));
    sender->setTrack(WTFMove(track));
    return sender;
}

Ref<RTCRtpSender> RTCRtpSender::create(PeerConnectionBackend& connection, String&& trackKind, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&& backend)
{
    return adoptRef(*new RTCRtpSender(connection, WTFMove(trackKind), WTFMove(mediaStreamIds), WTFMove(backend)));
}

RTCRtpSender::RTCRtpSender(PeerConnectionBackend& connection, String&& trackKind, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&& backend)
    : m_trackKind(WTFMove(trackKind))
    , m_mediaStreamIds(WTFMove(mediaStreamIds))
    , m_backend(WTFMove(backend))
    , m_connection(makeWeakPtr(&connection))
{
    ASSERT(!RuntimeEnabledFeatures::sharedFeatures().webRTCUnifiedPlanEnabled() || m_backend);
}

void RTCRtpSender::setTrackToNull()
{
    ASSERT(m_track);
    m_trackId = { };
    m_track = nullptr;
}

void RTCRtpSender::stop()
{
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

void RTCRtpSender::replaceTrack(ScriptExecutionContext& context, RefPtr<MediaStreamTrack>&& withTrack, DOMPromiseDeferred<void>&& promise)
{
    if (isStopped()) {
        promise.reject(InvalidStateError);
        return;
    }

    if (withTrack && m_trackKind != withTrack->kind()) {
        promise.reject(TypeError);
        return;
    }

    // FIXME: This whole function should be executed as part of the RTCPeerConnection operation queue.
    m_backend->replaceTrack(context, *this, WTFMove(withTrack), WTFMove(promise));
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
        promise.reject(InvalidStateError);
        return;
    }
    return m_backend->setParameters(parameters, WTFMove(promise));
}

void RTCRtpSender::getStats(Ref<DeferredPromise>&& promise)
{
    if (!m_connection) {
        promise->reject(InvalidStateError);
        return;
    }
    m_connection->getStats(*this, WTFMove(promise));
}

bool RTCRtpSender::isCreatedBy(const PeerConnectionBackend& connection) const
{
    return &connection == m_connection.get();
}

Optional<RTCRtpCapabilities> RTCRtpSender::getCapabilities(ScriptExecutionContext& context, const String& kind)
{
    return PeerConnectionBackend::senderCapabilities(context, kind);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
