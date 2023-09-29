/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferredForward.h"
#include "RTCRtpTransformBackend.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/Deque.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class FrameRateMonitor;
class MessagePort;
class ReadableStream;
class ScriptExecutionContext;
class RTCRtpTransformBackend;
class SerializedScriptValue;
class SimpleReadableStreamSource;
class WritableStream;

struct MessageWithMessagePorts;

enum class RTCRtpScriptTransformerIdentifierType { };
using RTCRtpScriptTransformerIdentifier = AtomicObjectIdentifier<RTCRtpScriptTransformerIdentifierType>;

class RTCRtpScriptTransformer
    : public RefCounted<RTCRtpScriptTransformer>
    , public ActiveDOMObject
    , public CanMakeWeakPtr<RTCRtpScriptTransformer> {
public:
    static ExceptionOr<Ref<RTCRtpScriptTransformer>> create(ScriptExecutionContext&, MessageWithMessagePorts&&);
    ~RTCRtpScriptTransformer();

    ReadableStream& readable();
    ExceptionOr<Ref<WritableStream>> writable();
    JSC::JSValue options(JSC::JSGlobalObject&);

    void generateKeyFrame(Ref<DeferredPromise>&&);
    void sendKeyFrameRequest(Ref<DeferredPromise>&&);

    void startPendingActivity() { m_pendingActivity = makePendingActivity(*this); }
    void start(Ref<RTCRtpTransformBackend>&&);

    enum class ClearCallback : bool { No, Yes };
    void clear(ClearCallback);

private:
    RTCRtpScriptTransformer(ScriptExecutionContext&, Ref<SerializedScriptValue>&&, Vector<RefPtr<MessagePort>>&&, Ref<ReadableStream>&&, Ref<SimpleReadableStreamSource>&&);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "RTCRtpScriptTransformer"; }
    void stop() final { stopPendingActivity(); }

    void stopPendingActivity() { auto pendingActivity = WTFMove(m_pendingActivity); }

    void enqueueFrame(ScriptExecutionContext&, Ref<RTCRtpTransformableFrame>&&);

    Ref<SerializedScriptValue> m_options;
    Vector<RefPtr<MessagePort>> m_ports;

    Ref<SimpleReadableStreamSource> m_readableSource;
    Ref<ReadableStream> m_readable;
    RefPtr<WritableStream> m_writable;

    RefPtr<RTCRtpTransformBackend> m_backend;
    RefPtr<PendingActivity<RTCRtpScriptTransformer>> m_pendingActivity;

    Deque<Ref<DeferredPromise>> m_pendingKeyFramePromises;

#if !RELEASE_LOG_DISABLED
    bool m_enableAdditionalLogging { false };
    RTCRtpScriptTransformerIdentifier m_identifier;
    std::unique_ptr<FrameRateMonitor> m_readableFrameRateMonitor;
    std::unique_ptr<FrameRateMonitor> m_writableFrameRateMonitor;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
