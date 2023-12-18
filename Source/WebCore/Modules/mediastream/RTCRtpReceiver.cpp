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
#include "RTCRtpReceiver.h"

#if ENABLE(WEB_RTC)

#include "JSDOMPromiseDeferred.h"
#include "Logging.h"
#include "PeerConnectionBackend.h"
#include "RTCRtpCapabilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

#if !RELEASE_LOG_DISABLED
#define LOGIDENTIFIER_RECEIVER Logger::LogSiteIdentifier(logClassName(), __func__, m_connection->logIdentifier())
#else
#define LOGIDENTIFIER_RECEIVER
#endif

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCRtpReceiver);

RTCRtpReceiver::RTCRtpReceiver(PeerConnectionBackend& connection, Ref<MediaStreamTrack>&& track, std::unique_ptr<RTCRtpReceiverBackend>&& backend)
    : m_track(WTFMove(track))
    , m_backend(WTFMove(backend))
    , m_connection(connection)
#if !RELEASE_LOG_DISABLED
    , m_logger(connection.logger())
    , m_logIdentifier(connection.logIdentifier())
#endif
{
}

RTCRtpReceiver::~RTCRtpReceiver()
{
    if (m_transform)
        m_transform->detachFromReceiver(*this);
}

void RTCRtpReceiver::stop()
{
    if (!m_backend)
        return;

    if (m_transform)
        m_transform->detachFromReceiver(*this);

    m_backend = nullptr;
    m_track->stopTrack(MediaStreamTrack::StopMode::PostEvent);
}

void RTCRtpReceiver::getStats(Ref<DeferredPromise>&& promise)
{
    if (!m_connection) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    m_connection->getStats(*this, WTFMove(promise));
}

std::optional<RTCRtpCapabilities> RTCRtpReceiver::getCapabilities(ScriptExecutionContext& context, const String& kind)
{
    return PeerConnectionBackend::receiverCapabilities(context, kind);
}

ExceptionOr<void> RTCRtpReceiver::setTransform(std::unique_ptr<RTCRtpTransform>&& transform)
{
    ALWAYS_LOG_IF(m_connection, LOGIDENTIFIER_RECEIVER);

    if (transform && m_transform && *transform == *m_transform)
        return { };
    if (!transform) {
        if (m_transform) {
            m_transform->detachFromReceiver(*this);
            m_transform = { };
        }
        return { };
    }

    if (transform->isAttached())
        return Exception { ExceptionCode::InvalidStateError, "transform is already in use"_s };

    transform->attachToReceiver(*this, m_transform.get());
    m_transform = WTFMove(transform);

    return { };
}

std::optional<RTCRtpTransform::Internal> RTCRtpReceiver::transform()
{
    if (!m_transform)
        return { };
    return m_transform->internalTransform();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RTCRtpReceiver::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
