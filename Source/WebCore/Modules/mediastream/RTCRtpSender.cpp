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

#include "ExceptionCode.h"

namespace WebCore {

Ref<RTCRtpSender> RTCRtpSender::create(Ref<MediaStreamTrack>&& track, Vector<String>&& mediaStreamIds, RTCRtpSenderClient& client)
{
    const String& trackKind = track->kind();
    return adoptRef(*new RTCRtpSender(WTFMove(track), trackKind, WTFMove(mediaStreamIds), client));
}

Ref<RTCRtpSender> RTCRtpSender::create(const String& trackKind, Vector<String>&& mediaStreamIds, RTCRtpSenderClient& client)
{
    return adoptRef(*new RTCRtpSender(nullptr, trackKind, WTFMove(mediaStreamIds), client));
}

RTCRtpSender::RTCRtpSender(RefPtr<MediaStreamTrack>&& track, const String& trackKind, Vector<String>&& mediaStreamIds, RTCRtpSenderClient& client)
    : RTCRtpSenderReceiverBase()
    , m_trackKind(trackKind)
    , m_mediaStreamIds(WTFMove(mediaStreamIds))
    , m_client(&client)
{
    setTrack(WTFMove(track));
}

void RTCRtpSender::setTrack(RefPtr<MediaStreamTrack>&& track)
{
    // Save the id from the first non-null track set. That id will be used to negotiate the sender
    // even if the track is replaced.
    if (!m_track && track)
        m_trackId = track->id();

    m_track = WTFMove(track);
}

void RTCRtpSender::replaceTrack(Ref<MediaStreamTrack>&& withTrack, PeerConnection::VoidPromise&& promise, ExceptionCode& ec)
{
    if (isStopped()) {
        promise.reject(INVALID_STATE_ERR);
        return;
    }

    if (m_trackKind != withTrack->kind()) {
        ec = TypeError;
        return;
    }

    m_client->replaceTrack(*this, WTFMove(withTrack), WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
