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

#include "MediaStream.h"
#include "RTCDtlsTransport.h"
#include "RTCRtpReceiverBackend.h"
#include "RTCRtpSynchronizationSource.h"
#include "RTCRtpTransform.h"
#include "ScriptWrappable.h"

namespace WebCore {

class DeferredPromise;
class PeerConnectionBackend;
struct RTCRtpCapabilities;

class RTCRtpReceiver final : public RefCounted<RTCRtpReceiver>
    , public ScriptWrappable
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
    {
    WTF_MAKE_ISO_ALLOCATED(RTCRtpReceiver);
public:
    static Ref<RTCRtpReceiver> create(PeerConnectionBackend& connection, Ref<MediaStreamTrack>&& track, std::unique_ptr<RTCRtpReceiverBackend>&& backend)
    {
        return adoptRef(*new RTCRtpReceiver(connection, WTFMove(track), WTFMove(backend)));
    }
    virtual ~RTCRtpReceiver();

    static std::optional<RTCRtpCapabilities> getCapabilities(ScriptExecutionContext&, const String& kind);

    void stop();

    void setBackend(std::unique_ptr<RTCRtpReceiverBackend>&& backend) { m_backend = WTFMove(backend); }
    RTCRtpParameters getParameters() { return m_backend ? m_backend->getParameters() : RTCRtpParameters(); }
    Vector<RTCRtpContributingSource> getContributingSources() const { return m_backend ? m_backend->getContributingSources() : Vector<RTCRtpContributingSource> { }; }
    Vector<RTCRtpSynchronizationSource> getSynchronizationSources() const { return m_backend ? m_backend->getSynchronizationSources() : Vector<RTCRtpSynchronizationSource> { }; }

    MediaStreamTrack& track() { return m_track.get(); }

    RTCDtlsTransport* transport() { return m_transport.get(); }
    void setTransport(RefPtr<RTCDtlsTransport>&& transport) { m_transport = WTFMove(transport); }

    RTCRtpReceiverBackend* backend() { return m_backend.get(); }
    void getStats(Ref<DeferredPromise>&&);

    std::optional<RTCRtpTransform::Internal> transform();
    ExceptionOr<void> setTransform(std::unique_ptr<RTCRtpTransform>&&);

    const Vector<WeakPtr<MediaStream>>& associatedStreams() const { return m_associatedStreams; }
    void setAssociatedStreams(Vector<WeakPtr<MediaStream>>&& streams) { m_associatedStreams = WTFMove(streams); }
private:
    RTCRtpReceiver(PeerConnectionBackend&, Ref<MediaStreamTrack>&&, std::unique_ptr<RTCRtpReceiverBackend>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "RTCRtpReceiver"; }
    WTFLogChannel& logChannel() const final;
#endif

    Ref<MediaStreamTrack> m_track;
    RefPtr<RTCDtlsTransport> m_transport;
    std::unique_ptr<RTCRtpReceiverBackend> m_backend;
    WeakPtr<PeerConnectionBackend> m_connection;
    std::unique_ptr<RTCRtpTransform> m_transform;
    Vector<WeakPtr<MediaStream>> m_associatedStreams;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
