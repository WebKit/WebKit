/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchBodySource.h"
#include "ContextDestructionObserverInlines.h"

#include "FetchResponse.h"
#include "FormDataConsumer.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableByteStreamController.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSArrayBufferViewInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

Ref<FetchBodySource> FetchBodySource::create(FetchBodyOwner& bodyOwner)
{
    return adoptRef(*new FetchBodySource(bodyOwner));
}

FetchBodySource::FetchBodySource(FetchBodyOwner& bodyOwner)
    : m_bodyOwner(bodyOwner)
{
}

FetchBodySource::~FetchBodySource() = default;

void FetchBodySource::setByteController(ReadableByteStreamController& controller)
{
    ASSERT(!m_byteController);
    m_byteController = controller;

    if (RefPtr bodyOwner = m_bodyOwner.get())
        bodyOwner->consumeBodyAsStream();
}

Ref<DOMPromise> FetchBodySource::pull(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller)
{
    ASSERT_UNUSED(controller, &controller == m_byteController.get() || !m_byteController);

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    if (RefPtr consumer = m_formDataConsumer)
        consumer->resume(WTF::move(deferred));
    else
        m_pullPromise = WTF::move(deferred);
    return promise;
}

void FetchBodySource::setFormDataConsumer(Ref<FormDataConsumer>&& consumer)
{
    m_formDataConsumer = WTF::move(consumer);
    if (RefPtr pullPromise = std::exchange(m_pullPromise, { }))
        protect(m_formDataConsumer)->resume(WTF::move(pullPromise));
}

Ref<DOMPromise> FetchBodySource::cancel(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller, std::optional<JSC::JSValue>&&)
{
    ASSERT_UNUSED(controller, &controller == m_byteController.get());

    m_isCancelling = true;
    if (RefPtr bodyOwner = m_bodyOwner.get())
        bodyOwner->cancel();

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    deferred->resolve();
    return promise;
}

static JSDOMGlobalObject* globalObjectFromBodyOwner(RefPtr<FetchBodyOwner>&& bodyOwner)
{
    RefPtr context = bodyOwner ? bodyOwner->scriptExecutionContext() : nullptr;
    return context ? downcast<JSDOMGlobalObject>(context->globalObject()) : nullptr;
}

// FIXME: We should be able to take a FragmentedSharedBuffer
bool FetchBodySource::enqueue(RefPtr<JSC::ArrayBuffer>&& chunk)
{
    if (!chunk)
        return false;

    RefPtr controller = m_byteController.get();
    if (!controller)
        return false;

    auto* globalObject = globalObjectFromBodyOwner(m_bodyOwner.get());
    if (!globalObject)
        return false;

    size_t byteLength = chunk->byteLength();
    auto result = controller->enqueue(*globalObject, Uint8Array::create(chunk.releaseNonNull(), 0, byteLength));
    return !result.hasException();
}

void FetchBodySource::close()
{
    RefPtr controller = m_byteController.get();
    if (!controller)
        return;

    auto* globalObject = globalObjectFromBodyOwner(m_bodyOwner.get());
    if (!globalObject)
        return;

    controller->closeAndRespondToPendingPullIntos(*globalObject);
}

void FetchBodySource::error(const Exception& exception)
{
    RefPtr controller = m_byteController.get();
    if (!controller)
        return;

    auto* globalObject = globalObjectFromBodyOwner(m_bodyOwner.get());
    if (!globalObject)
        return;

    controller->error(*globalObject, exception);
}

bool FetchBodySource::isPulling() const
{
    if (m_formDataConsumer)
        return m_formDataConsumer->hasPendingPull();
    return !!m_pullPromise;
}

void FetchBodySource::resolvePullPromise()
{
    if (auto pullPromise = std::exchange(m_pullPromise, { }))
        pullPromise->resolve();
}

void FetchBodySource::detach()
{
    m_bodyOwner = nullptr;
    m_byteController = nullptr;
    m_pullPromise = nullptr;
}

} // namespace WebCore
