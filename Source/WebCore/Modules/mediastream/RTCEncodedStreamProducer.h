/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#pragma once

#if ENABLE(WEB_RTC)

#include "ExceptionOr.h"
#include "RTCEncodedStreams.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class FrameRateMonitor;
class JSDOMGlobalObject;
class RTCRtpTransformBackend;
class RTCRtpTransformableFrame;
class ScriptExecutionContext;
class SimpleReadableStreamSource;

enum class RTCEncodedStreamProducerIdentifierType { };
using RTCEncodedStreamProducerIdentifier = AtomicObjectIdentifier<RTCEncodedStreamProducerIdentifierType>;

class RTCEncodedStreamProducer final : public RefCounted<RTCEncodedStreamProducer>
    , public CanMakeWeakPtr<RTCEncodedStreamProducer> {
    WTF_MAKE_TZONE_ALLOCATED(RTCEncodedStreamProducer);
public:
    static ExceptionOr<Ref<RTCEncodedStreamProducer>> create(ScriptExecutionContext&);
    ~RTCEncodedStreamProducer();

    void start(Ref<RTCRtpTransformBackend>&&, bool isVideo);
    void clear(bool shouldClearCallback);

    void generateKeyFrame(ScriptExecutionContext&, const String&, Ref<DeferredPromise>&&);
    void sendKeyFrameRequest();

    bool isVideo() const { return m_isVideo; }
    RTCEncodedStreams streams() { return { m_readable.get(), m_writable.get() }; }
    ReadableStream& readable() { return m_readable.get(); }
    WritableStream& writable() { return *m_writable; }

private:
    RTCEncodedStreamProducer(ScriptExecutionContext&, Ref<ReadableStream>&&, Ref<SimpleReadableStreamSource>&&);

    void enqueueFrame(Ref<RTCRtpTransformableFrame>&&);
    ExceptionOr<void> writeFrame(ScriptExecutionContext&, JSC::JSValue);

    std::optional<Exception> initialize(JSDOMGlobalObject&);

    WeakPtr<ScriptExecutionContext> m_context;
    ScriptExecutionContextIdentifier m_contextIdentifier;

    const Ref<ReadableStream> m_readable;
    const Ref<SimpleReadableStreamSource> m_readableSource;
    const RefPtr<WritableStream> m_writable;

    RefPtr<RTCRtpTransformBackend> m_transformBackend;
    Vector<Ref<DeferredPromise>> m_pendingKeyFramePromises;
    bool m_isVideo { false };

#if !RELEASE_LOG_DISABLED
    bool m_enableAdditionalLogging { false };
    RTCEncodedStreamProducerIdentifier m_identifier;
    std::unique_ptr<FrameRateMonitor> m_readableFrameRateMonitor;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
