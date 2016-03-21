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

#if ENABLE(WEBRTC)

#include "DOMError.h"
#include "ExceptionCode.h"
#include "JSDOMError.h"

namespace WebCore {

RTCRtpSender::RTCRtpSender(RefPtr<MediaStreamTrack>&& track, Vector<String>&& mediaStreamIds, RTCRtpSenderClient& client)
    : RTCRtpSenderReceiverBase(WTFMove(track))
    , m_mediaStreamIds(WTFMove(mediaStreamIds))
    , m_client(&client)
{
    // The original track id is always used in negotiation even if the track is replaced.
    m_trackId = m_track->id();
}

void RTCRtpSender::replaceTrack(MediaStreamTrack* withTrack, PeerConnection::VoidPromise&& promise, ExceptionCode& ec)
{
    if (!withTrack) {
        ec = TypeError;
        return;
    }

    if (!m_client) {
        promise.reject(DOMError::create("InvalidStateError"));
        return;
    }

    if (m_track->kind() != withTrack->kind()) {
        ec = TypeError;
        return;
    }

    m_client->replaceTrack(*this, *withTrack, WTFMove(promise));
}

} // namespace WebCore

#endif // ENABLE(WEBRTC)
