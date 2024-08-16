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

#include "JSDOMPromiseDeferredForward.h"
#include "MediaStreamTrack.h"
#include "RTCDtlsTransport.h"
#include "RTCRtpSenderBackend.h"
#include "RTCRtpTransceiverDirection.h"
#include "RTCRtpTransform.h"
#include "ScriptWrappable.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaStream;
class RTCDTMFSender;
class RTCPeerConnection;
struct RTCRtpCapabilities;

class RTCRtpSender final : public RefCounted<RTCRtpSender>
    , public ScriptWrappable
    , public CanMakeWeakPtr<RTCRtpSender>
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RTCRtpSender);
public:
    static Ref<RTCRtpSender> create(RTCPeerConnection&, Ref<MediaStreamTrack>&&, std::unique_ptr<RTCRtpSenderBackend>&&);
    static Ref<RTCRtpSender> create(RTCPeerConnection&, String&& trackKind, std::unique_ptr<RTCRtpSenderBackend>&&);
    virtual ~RTCRtpSender();

    static std::optional<RTCRtpCapabilities> getCapabilities(ScriptExecutionContext&, const String& kind);

    MediaStreamTrack* track() { return m_track.get(); }

    RTCDtlsTransport* transport() { return m_transport.get(); }
    void setTransport(RefPtr<RTCDtlsTransport>&& transport) { m_transport = WTFMove(transport); }

    const String& trackId() const { return m_trackId; }
    const String& trackKind() const { return m_trackKind; }

    ExceptionOr<void> setMediaStreamIds(const FixedVector<String>&);
    ExceptionOr<void> setStreams(const FixedVector<std::reference_wrapper<MediaStream>>&);

    bool isStopped() const { return !m_backend; }
    void stop();
    void setTrack(Ref<MediaStreamTrack>&&);
    void setTrackToNull();

    void replaceTrack(RefPtr<MediaStreamTrack>&&, Ref<DeferredPromise>&&);

    RTCRtpSendParameters getParameters();
    void setParameters(const RTCRtpSendParameters&, DOMPromiseDeferred<void>&&);

    RTCRtpSenderBackend* backend() { return m_backend.get(); }

    void getStats(Ref<DeferredPromise>&&);

    bool isCreatedBy(const RTCPeerConnection&) const;

    RTCDTMFSender* dtmf();
    std::optional<RTCRtpTransceiverDirection> currentTransceiverDirection() const;

    std::optional<RTCRtpTransform::Internal> transform();
    ExceptionOr<void> setTransform(std::unique_ptr<RTCRtpTransform>&&);

private:
    RTCRtpSender(RTCPeerConnection&, String&& trackKind, std::unique_ptr<RTCRtpSenderBackend>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "RTCRtpSender"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    RefPtr<MediaStreamTrack> m_track;
    RefPtr<RTCDtlsTransport> m_transport;
    String m_trackId;
    String m_trackKind;
    std::unique_ptr<RTCRtpSenderBackend> m_backend;
    WeakPtr<RTCPeerConnection, WeakPtrImplWithEventTargetData> m_connection;
    RefPtr<RTCDTMFSender> m_dtmfSender;
    std::unique_ptr<RTCRtpTransform> m_transform;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
