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
#include "MessageChannel.h"
#include "RTCRtpScriptTransformer.h"
#include "RTCRtpTransformBackend.h"
#include "Worker.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCRtpScriptTransform);

ExceptionOr<Ref<RTCRtpScriptTransform>> RTCRtpScriptTransform::create(ScriptExecutionContext& context, Worker& worker, String&& name)
{
    if (!worker.hasRTCRtpScriptTransformer(name))
        return Exception { InvalidStateError, "No RTCRtpScriptTransformer was registered with this name"_s };

    if (!worker.scriptExecutionContext())
        return Exception { InvalidStateError, "Worker frame is detached"_s };

    auto messageChannel = MessageChannel::create(*worker.scriptExecutionContext());
    auto transformMessagePort = messageChannel->port1();
    auto transformerMessagePort = messageChannel->port2();

    auto transform = adoptRef(*new RTCRtpScriptTransform(context, makeRef(worker), makeRef(*transformMessagePort)));
    transform->suspendIfNeeded();

    worker.createRTCRtpScriptTransformer(name, transformerMessagePort->disentangle(), transform);

    return transform;
}

RTCRtpScriptTransform::RTCRtpScriptTransform(ScriptExecutionContext& context, Ref<Worker>&& worker, Ref<MessagePort>&& port)
    : ActiveDOMObject(&context)
    , m_worker(WTFMove(worker))
    , m_port(WTFMove(port))
{
}

RTCRtpScriptTransform::~RTCRtpScriptTransform()
{
    clear();
}

void RTCRtpScriptTransform::setTransformer(RefPtr<RTCRtpScriptTransformer>&& transformer)
{
    ASSERT(!isMainThread());
    if (!transformer) {
        callOnMainThread([this, protectedThis = makeRef(*this)]() mutable {
            queueTaskToDispatchEvent(*this, TaskSource::MediaElement, ErrorEvent::create(eventNames().errorEvent, "An error was thrown from RTCRtpScriptTransformer constructor"_s, { }, 0, 0, { }));
        });
        return;
    }

    {
        auto locker = holdLock(m_transformerLock);
        ASSERT(!m_isTransformerInitialized);
        m_isTransformerInitialized = true;
        m_transformer = makeWeakPtr(transformer.get());
    }
    transformer->startPendingActivity();
    callOnMainThread([this, protectedThis = makeRef(*this)]() mutable {
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
    clear();
}

void RTCRtpScriptTransform::initializeTransformer(RTCRtpTransformBackend& backend)
{
    m_isAttached = true;
    if (!setupTransformer(makeRef(backend)))
        m_backend = makeRef(backend);
}

bool RTCRtpScriptTransform::setupTransformer(Ref<RTCRtpTransformBackend>&& backend)
{
    auto locker = holdLock(m_transformerLock);
    if (!m_isTransformerInitialized)
        return false;

    m_worker->postTaskToWorkerGlobalScope([transformer = m_transformer, backend = WTFMove(backend)](auto&) mutable {
        if (transformer)
            transformer->start(WTFMove(backend));
    });
    return true;
}

void RTCRtpScriptTransform::clear()
{
    m_isAttached = false;

    auto locker = holdLock(m_transformerLock);
    m_isTransformerInitialized = false;
    m_worker->postTaskToWorkerGlobalScope([transformer = WTFMove(m_transformer)](auto&) mutable {
        if (transformer)
            transformer->clear();
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
