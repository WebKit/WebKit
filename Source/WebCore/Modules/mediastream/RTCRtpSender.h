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

#ifndef RTCRtpSender_h
#define RTCRtpSender_h

#if ENABLE(WEBRTC)

#include "PeerConnectionBackend.h"
#include "RTCRtpSenderReceiverBase.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCRtpSenderClient {
public:
    virtual void replaceTrack(RTCRtpSender&, MediaStreamTrack&, PeerConnection::VoidPromise&&) = 0;

    virtual ~RTCRtpSenderClient() { }
};

class RTCRtpSender : public RTCRtpSenderReceiverBase {
public:
    static Ref<RTCRtpSender> create(RefPtr<MediaStreamTrack>&& track, Vector<String>&& mediaStreamIds, RTCRtpSenderClient& client)
    {
        return adoptRef(*new RTCRtpSender(WTFMove(track), WTFMove(mediaStreamIds), client));
    }

    const String& trackId() { return m_trackId; }
    const Vector<String>& mediaStreamIds() const { return m_mediaStreamIds; }

    void stop() { m_client = nullptr; }

    void replaceTrack(MediaStreamTrack*, PeerConnection::VoidPromise&&, ExceptionCode&);

private:
    RTCRtpSender(RefPtr<MediaStreamTrack>&&, Vector<String>&& mediaStreamIds, RTCRtpSenderClient&);

    String m_trackId;
    Vector<String> m_mediaStreamIds;
    RTCRtpSenderClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(WEBRTC)

#endif // RTCRtpSender_h
