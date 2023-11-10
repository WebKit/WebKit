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

#include "config.h"
#include "RTCRtpScriptTransform.h"

#if ENABLE(WEB_RTC)

#include "ErrorEvent.h"
#include "EventNames.h"
#include "JSDOMGlobalObject.h"
#include "MessageChannel.h"
#include "RTCRtpScriptTransformer.h"
#include "RTCRtpTransformBackend.h"
#include "Worker.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCRtpScriptTransform);

ExceptionOr<Ref<RTCRtpScriptTransform>> RTCRtpScriptTransform::create(JSC::JSGlobalObject& state, Worker& worker, JSC::JSValue options, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    if (!worker.scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "Worker frame is detached"_s };

    RefPtr context = JSC::jsCast<JSDOMGlobalObject*>(&state)->scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::InvalidStateError, "Invalid context"_s };

    Vector<RefPtr<MessagePort>> transferredPorts;
    auto serializedOptions = SerializedScriptValue::create(state, options, WTFMove(transfer), transferredPorts);
    if (serializedOptions.hasException())
        return serializedOptions.releaseException();

    auto channels = MessagePort::disentanglePorts(WTFMove(transferredPorts));
    if (channels.hasException())
        return channels.releaseException();

    auto transform = adoptRef(*new RTCRtpScriptTransform(*context, worker));
    transform->suspendIfNeeded();

    worker.createRTCRtpScriptTransformer(transform, { serializedOptions.releaseReturnValue(), channels.releaseReturnValue() });

    return transform;
}

RTCRtpScriptTransform::RTCRtpScriptTransform(ScriptExecutionContext& context, Ref<Worker>&& worker)
    : ActiveDOMObject(&context)
    , m_worker(WTFMove(worker))
{
}

RTCRtpScriptTransform::~RTCRtpScriptTransform()
{
    clear(RTCRtpScriptTransformer::ClearCallback::Yes);
}

void RTCRtpScriptTransform::setTransformer(RTCRtpScriptTransformer& transformer)
{
    ASSERT(!isMainThread());
    {
        Locker locker { m_transformerLock };
        ASSERT(!m_isTransformerInitialized);
        m_isTransformerInitialized = true;
        m_transformer = transformer;
    }
    transformer.startPendingActivity();
    callOnMainThread([this, protectedThis = Ref { *this }]() mutable {
        if (m_backend)
            setupTransformer(m_backend.releaseNonNull());
    });
}

void RTCRtpScriptTransform::initializeBackendForReceiver(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend);
}

void RTCRtpScriptTransform::initializeBackendForSender(RTCRtpTransformBackend& backend)
{
    initializeTransformer(backend);
}

void RTCRtpScriptTransform::willClearBackend(RTCRtpTransformBackend&)
{
    clear(RTCRtpScriptTransformer::ClearCallback::Yes);
}

void RTCRtpScriptTransform::initializeTransformer(RTCRtpTransformBackend& backend)
{
    m_isAttached = true;
    if (!setupTransformer(backend))
        m_backend = &backend;
}

bool RTCRtpScriptTransform::setupTransformer(Ref<RTCRtpTransformBackend>&& backend)
{
    Locker locker { m_transformerLock };
    if (!m_isTransformerInitialized)
        return false;

    m_worker->postTaskToWorkerGlobalScope([transformer = m_transformer, backend = WTFMove(backend)](auto&) mutable {
        if (transformer)
            transformer->start(WTFMove(backend));
    });
    return true;
}

void RTCRtpScriptTransform::clear(RTCRtpScriptTransformer::ClearCallback clearCallback)
{
    m_isAttached = false;

    Locker locker { m_transformerLock };
    m_isTransformerInitialized = false;
    m_worker->postTaskToWorkerGlobalScope([transformer = WTFMove(m_transformer), clearCallback](auto&) mutable {
        if (transformer)
            transformer->clear(clearCallback);
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
