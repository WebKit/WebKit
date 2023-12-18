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
#include "RTCRtpScriptTransformer.h"
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/Strong.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RTCRtpScriptTransformer;
class RTCRtpTransformBackend;
class Worker;

class RTCRtpScriptTransform final
    : public ThreadSafeRefCounted<RTCRtpScriptTransform, WTF::DestructionThread::Main>
    , public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(RTCRtpScriptTransform);
public:
    static ExceptionOr<Ref<RTCRtpScriptTransform>> create(JSC::JSGlobalObject&, Worker&, JSC::JSValue, Vector<JSC::Strong<JSC::JSObject>>&&);
    ~RTCRtpScriptTransform();

    void setTransformer(RTCRtpScriptTransformer&);

    bool isAttached() const { return m_isAttached; }
    void initializeBackendForReceiver(RTCRtpTransformBackend&);
    void initializeBackendForSender(RTCRtpTransformBackend&);
    void willClearBackend(RTCRtpTransformBackend&);
    void backendTransferedToNewTransform() { clear(RTCRtpScriptTransformer::ClearCallback::No); }

private:
    RTCRtpScriptTransform(ScriptExecutionContext&, Ref<Worker>&&);

    void initializeTransformer(RTCRtpTransformBackend&);
    bool setupTransformer(Ref<RTCRtpTransformBackend>&&);
    void clear(RTCRtpScriptTransformer::ClearCallback);

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "RTCRtpScriptTransform"; }

    Ref<Worker> m_worker;

    bool m_isAttached { false };
    RefPtr<RTCRtpTransformBackend> m_backend;

    Lock m_transformerLock;
    bool m_isTransformerInitialized WTF_GUARDED_BY_LOCK(m_transformerLock) { false };
    WeakPtr<RTCRtpScriptTransformer> m_transformer WTF_GUARDED_BY_LOCK(m_transformerLock);
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
