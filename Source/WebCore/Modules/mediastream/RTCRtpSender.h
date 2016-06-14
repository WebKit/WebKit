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

#if ENABLE(WEB_RTC)

#include "PeerConnectionBackend.h"
#include "RTCRtpSenderReceiverBase.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCRtpSenderClient {
public:
    virtual void replaceTrack(RTCRtpSender&, RefPtr<MediaStreamTrack>&&, PeerConnection::VoidPromise&&) = 0;

    virtual ~RTCRtpSenderClient() { }
};

class RTCRtpSender : public RTCRtpSenderReceiverBase {
public:
    static Ref<RTCRtpSender> create(Ref<MediaStreamTrack>&&, Vector<String>&& mediaStreamIds, RTCRtpSenderClient&);
    static Ref<RTCRtpSender> create(const String& trackKind, Vector<String>&& mediaStreamIds, RTCRtpSenderClient&);

    const String& trackId() const { return m_trackId; }
    const String& trackKind() const { return m_trackKind; }

    const Vector<String>& mediaStreamIds() const { return m_mediaStreamIds; }
    void setMediaStreamIds(Vector<String>&& mediaStreamIds) { m_mediaStreamIds = WTFMove(mediaStreamIds); }

    bool isStopped() const { return !m_client; }
    void stop() { m_client = nullptr; }
    void setTrack(RefPtr<MediaStreamTrack>&&);

    void replaceTrack(Ref<MediaStreamTrack>&&, PeerConnection::VoidPromise&&, ExceptionCode&);

private:
    RTCRtpSender(RefPtr<MediaStreamTrack>&&, const String& trackKind, Vector<String>&& mediaStreamIds, RTCRtpSenderClient&);

    String m_trackId;
    String m_trackKind;
    Vector<String> m_mediaStreamIds;
    RTCRtpSenderClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
#endif // RTCRtpSender_h
