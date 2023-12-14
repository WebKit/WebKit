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
#include "EventTarget.h"
#include "JSDOMPromiseDeferredForward.h"
#include "RTCRtpSFrameTransformer.h"
#include <wtf/WeakPtr.h>

namespace JSC {
class JSGlobalObject;
};

namespace WebCore {

class CryptoKey;
class RTCRtpTransformBackend;
class ReadableStream;
class SimpleReadableStreamSource;
class WritableStream;

class RTCRtpSFrameTransform : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RTCRtpSFrameTransform>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(RTCRtpSFrameTransform);
public:
    enum class Role { Encrypt, Decrypt };
    using CompatibilityMode = RTCRtpSFrameTransformer::CompatibilityMode;
    struct Options {
        Role role { Role::Encrypt };
        uint64_t authenticationSize { 10 };
        CompatibilityMode compatibilityMode { CompatibilityMode::None };
    };

    static Ref<RTCRtpSFrameTransform> create(ScriptExecutionContext&, Options);
    ~RTCRtpSFrameTransform();

    void setEncryptionKey(CryptoKey&, std::optional<uint64_t>, DOMPromiseDeferred<void>&&);

    bool isAttached() const;
    void initializeBackendForReceiver(RTCRtpTransformBackend&);
    void initializeBackendForSender(RTCRtpTransformBackend&);
    void willClearBackend(RTCRtpTransformBackend&);

    WEBCORE_EXPORT void setCounterForTesting(uint64_t);
    WEBCORE_EXPORT uint64_t counterForTesting() const;
    WEBCORE_EXPORT uint64_t keyIdForTesting() const;

    ExceptionOr<RefPtr<ReadableStream>> readable();
    ExceptionOr<RefPtr<WritableStream>> writable();

    bool hasKey(uint64_t) const;

    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref;
    using ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref;

private:
    RTCRtpSFrameTransform(ScriptExecutionContext&, Options);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "RTCRtpSFrameTransform"; }
    bool virtualHasPendingActivity() const final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return RTCRtpSFrameTransformEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    enum class Side { Sender, Receiver };
    void initializeTransformer(RTCRtpTransformBackend&, Side);
    ExceptionOr<void> createStreams();

    bool m_isAttached { false };
    bool m_hasWritable { false };
    Ref<RTCRtpSFrameTransformer> m_transformer;
    RefPtr<ReadableStream> m_readable;
    RefPtr<WritableStream> m_writable;
    RefPtr<SimpleReadableStreamSource> m_readableStreamSource;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
