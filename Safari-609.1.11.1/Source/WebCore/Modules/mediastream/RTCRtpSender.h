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

#pragma once

#if ENABLE(WEB_RTC)

#include "MediaStreamTrack.h"
#include "PeerConnectionBackend.h"
#include "RTCRtpSenderBackend.h"
#include "RTCRtpTransceiverDirection.h"
#include "ScriptWrappable.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class PeerConnectionBackend;
class RTCDTMFSender;
struct RTCRtpCapabilities;

class RTCRtpSender final : public RefCounted<RTCRtpSender>, public ScriptWrappable, public CanMakeWeakPtr<RTCRtpSender> {
    WTF_MAKE_ISO_ALLOCATED(RTCRtpSender);
public:
    static Ref<RTCRtpSender> create(PeerConnectionBackend&, Ref<MediaStreamTrack>&&, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&&);
    static Ref<RTCRtpSender> create(PeerConnectionBackend&, String&& trackKind, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&&);
    ~RTCRtpSender();

    static Optional<RTCRtpCapabilities> getCapabilities(ScriptExecutionContext&, const String& kind);

    MediaStreamTrack* track() { return m_track.get(); }

    const String& trackId() const { return m_trackId; }
    const String& trackKind() const { return m_trackKind; }

    const Vector<String>& mediaStreamIds() const { return m_mediaStreamIds; }
    void setMediaStreamIds(Vector<String>&& mediaStreamIds) { m_mediaStreamIds = WTFMove(mediaStreamIds); }

    bool isStopped() const { return !m_backend; }
    void stop();
    void setTrack(Ref<MediaStreamTrack>&&);
    void setTrackToNull();

    void replaceTrack(ScriptExecutionContext&, RefPtr<MediaStreamTrack>&&, DOMPromiseDeferred<void>&&);

    RTCRtpSendParameters getParameters();
    void setParameters(const RTCRtpSendParameters&, DOMPromiseDeferred<void>&&);

    RTCRtpSenderBackend* backend() { return m_backend.get(); }

    void getStats(Ref<DeferredPromise>&&);

    bool isCreatedBy(const PeerConnectionBackend&) const;

    RTCDTMFSender* dtmf();
    Optional<RTCRtpTransceiverDirection> currentTransceiverDirection() const;

private:
    RTCRtpSender(PeerConnectionBackend&, String&& trackKind, Vector<String>&& mediaStreamIds, std::unique_ptr<RTCRtpSenderBackend>&&);

    RefPtr<MediaStreamTrack> m_track;
    String m_trackId;
    String m_trackKind;
    Vector<String> m_mediaStreamIds;
    std::unique_ptr<RTCRtpSenderBackend> m_backend;
    WeakPtr<PeerConnectionBackend> m_connection;
    RefPtr<RTCDTMFSender> m_dtmfSender;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
