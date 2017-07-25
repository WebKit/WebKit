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

namespace WebCore {

Ref<RTCRtpSender> RTCRtpSender::create(Ref<MediaStreamTrack>&& track, Vector<String>&& mediaStreamIds, Backend& backend)
{
    auto sender = adoptRef(*new RTCRtpSender(String(track->kind()), WTFMove(mediaStreamIds), backend));
    sender->setTrack(WTFMove(track));
    return sender;
}

Ref<RTCRtpSender> RTCRtpSender::create(String&& trackKind, Vector<String>&& mediaStreamIds, Backend& backend)
{
    return adoptRef(*new RTCRtpSender(WTFMove(trackKind), WTFMove(mediaStreamIds), backend));
}

RTCRtpSender::RTCRtpSender(String&& trackKind, Vector<String>&& mediaStreamIds, Backend& backend)
    : RTCRtpSenderReceiverBase()
    , m_trackKind(WTFMove(trackKind))
    , m_mediaStreamIds(WTFMove(mediaStreamIds))
    , m_backend(&backend)
{
}

void RTCRtpSender::setTrackToNull()
{
    ASSERT(m_track);
    m_trackId = { };
    m_track = nullptr;
}

void RTCRtpSender::setTrack(Ref<MediaStreamTrack>&& track)
{
    ASSERT(!isStopped());
    if (!m_track)
        m_trackId = track->id();
    m_track = WTFMove(track);
}

void RTCRtpSender::replaceTrack(RefPtr<MediaStreamTrack>&& withTrack, DOMPromiseDeferred<void>&& promise)
{
    if (isStopped()) {
        promise.reject(InvalidStateError);
        return;
    }

    if (withTrack && m_trackKind != withTrack->kind()) {
        promise.reject(TypeError);
        return;
    }

    if (!withTrack && m_track)
        m_track->stopTrack();

    m_backend->replaceTrack(*this, WTFMove(withTrack), WTFMove(promise));
}

RTCRtpParameters RTCRtpSender::getParameters()
{
    if (isStopped())
        return { };
    return m_backend->getParameters(*this);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
