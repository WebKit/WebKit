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

#include "FetchResponse.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableByteStreamController.h"

namespace WebCore {

std::pair<Ref<FetchBodySource>, Ref<RefCountedReadableStreamSource>> FetchBodySource::createNonByteSource(FetchBodyOwner& bodyOwner)
{
    Ref nonByteSource = NonByteSource::create(bodyOwner);
    Ref source = adoptRef(*new FetchBodySource(bodyOwner, nonByteSource.ptr()));
    return { WTFMove(source), WTFMove(nonByteSource) };
}

Ref<FetchBodySource> FetchBodySource::createByteSource(FetchBodyOwner& bodyOwner)
{
    return adoptRef(*new FetchBodySource(bodyOwner));
}

FetchBodySource::FetchBodySource(FetchBodyOwner& bodyOwner, RefPtr<NonByteSource>&& nonByteSource)
    : m_bodyOwner(bodyOwner)
    , m_nonByteSource(WTFMove(nonByteSource))
{
}

FetchBodySource::~FetchBodySource() = default;

void FetchBodySource::setByteController(ReadableByteStreamController& controller)
{
    ASSERT(!m_nonByteSource);
    ASSERT(!m_byteController);
    m_byteController = controller;

    if (RefPtr bodyOwner = m_bodyOwner.get())
        bodyOwner->consumeBodyAsStream();
}

Ref<DOMPromise> FetchBodySource::pull(JSDOMGlobalObject& globalObject, ReadableByteStreamController& controller)
{
    ASSERT_UNUSED(controller, &controller == m_byteController.get() || !m_byteController);

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    m_isPulling = true;
    m_pullPromise = WTFMove(deferred);
    return promise;
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
    return JSC::jsCast<JSDOMGlobalObject*>(context->globalObject());
}

// FIXME: We should be able to take a FragmentedSharedBuffer
bool FetchBodySource::enqueue(RefPtr<JSC::ArrayBuffer>&& chunk)
{
    if (m_nonByteSource)
        return m_nonByteSource->enqueue(WTFMove(chunk));

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
    if (m_nonByteSource) {
        m_nonByteSource->close();
        return;
    }

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
    if (m_nonByteSource) {
        m_nonByteSource->error(exception);
        return;
    }

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
    return m_nonByteSource ? m_nonByteSource->isPulling() : m_isPulling;
}

bool FetchBodySource::isCancelling() const
{
    return m_nonByteSource ? m_nonByteSource->isCancelling() : m_isCancelling;
}

void FetchBodySource::resolvePullPromise()
{
    if (m_nonByteSource) {
        m_nonByteSource->resolvePullPromise();
        return;
    }

    m_isPulling = false;
    if (auto pullPromise = std::exchange(m_pullPromise, { }))
        pullPromise->resolve();
}

void FetchBodySource::detach()
{
    if (m_nonByteSource) {
        m_nonByteSource->detach();
        return;
    }

    m_bodyOwner = nullptr;
    m_byteController = nullptr;
    m_pullPromise = nullptr;
}

FetchBodySource::NonByteSource::NonByteSource(FetchBodyOwner& owner)
    : m_bodyOwner(owner)
{
}

void FetchBodySource::NonByteSource::setActive()
{
    ASSERT(m_bodyOwner);
    ASSERT(!m_pendingActivity);
    if (RefPtr bodyOwner = m_bodyOwner.get())
        m_pendingActivity = bodyOwner->makePendingActivity(*bodyOwner);
}

void FetchBodySource::NonByteSource::setInactive()
{
    ASSERT(m_bodyOwner);
    ASSERT(m_pendingActivity);
    m_pendingActivity = nullptr;
}

void FetchBodySource::NonByteSource::doStart()
{
    ASSERT(m_bodyOwner);
    if (RefPtr bodyOwner = m_bodyOwner.get())
        bodyOwner->consumeBodyAsStream();
}

void FetchBodySource::NonByteSource::doPull()
{
    ASSERT(m_bodyOwner);
    if (RefPtr bodyOwner = m_bodyOwner.get())
        bodyOwner->feedStream();
}

void FetchBodySource::NonByteSource::doCancel(JSC::JSValue)
{
    m_isCancelling = true;
    RefPtr bodyOwner = m_bodyOwner.get();
    if (!bodyOwner)
        return;

    bodyOwner->cancel();
    m_bodyOwner = nullptr;
}

void FetchBodySource::NonByteSource::close()
{
#if ASSERT_ENABLED
    ASSERT(!m_isClosed);
    m_isClosed = true;
#endif

    controller().close();
    clean();
    m_bodyOwner = nullptr;
}

void FetchBodySource::NonByteSource::error(const Exception& value)
{
    controller().error(value);
    clean();
    m_bodyOwner = nullptr;
}

} // namespace WebCore
