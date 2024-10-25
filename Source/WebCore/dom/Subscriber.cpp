/*
 * Copyright (C) 2024 Keith Cirkel <webkit@keithcirkel.co.uk>. All rights reserved.
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
#include "config.h"
#include "Subscriber.h"

#include "AbortController.h"
#include "AbortSignal.h"
#include "Document.h"
#include "InternalObserver.h"
#include "JSDOMExceptionHandling.h"
#include "SubscriberCallback.h"
#include "SubscriptionObserverCallback.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

Ref<Subscriber> Subscriber::create(ScriptExecutionContext& context, Ref<InternalObserver>&& observer, const SubscribeOptions& options)
{
    return adoptRef(*new Subscriber(context, WTFMove(observer), options));
}

Subscriber::Subscriber(ScriptExecutionContext& context, Ref<InternalObserver>&& observer, const SubscribeOptions& options)
    : ActiveDOMObject(&context)
    , m_abortController(AbortController::create(context))
    , m_observer(observer)
    , m_options(options)
{
    followSignal(m_abortController->signal());
    if (RefPtr signal = options.signal)
        followSignal(*signal);
    suspendIfNeeded();
}

void Subscriber::next(JSC::JSValue value)
{
    if (!isActive())
        return;

    m_observer->next(value);
}

void Subscriber::error(JSC::JSValue error)
{
    if (!m_active) {
        reportErrorObject(error);
        return;
    }

    if (isInactiveDocument())
        return;

    close(error);

    m_observer->error(error);
}

void Subscriber::complete()
{
    if (!isActive())
        return;

    close(JSC::jsUndefined());

    m_observer->complete();
}

void Subscriber::addTeardown(Ref<VoidCallback> callback)
{
    if (isInactiveDocument())
        return;

    if (m_active) {
        Locker locker { m_teardownsLock };
        m_teardowns.append(callback);
    } else
        callback->handleEvent();
}

void Subscriber::followSignal(AbortSignal& signal)
{
    if (signal.aborted())
        close(signal.reason().getValue());
    else {
        signal.addAlgorithm([this](JSC::JSValue reason) {
            close(reason);
        });
    }
}

void Subscriber::close(JSC::JSValue reason)
{
    auto* context = scriptExecutionContext();
    if (!context || !m_active)
        return;

    m_active = false;

    m_abortController->abort(reason);

    {
        Locker locker { m_teardownsLock };
        for (auto teardown = m_teardowns.rbegin(); teardown != m_teardowns.rend(); ++teardown) {
            if (isInactiveDocument())
                return;
            (*teardown)->handleEvent();
        }
    }

    stop();
}

bool Subscriber::isInactiveDocument() const
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    return (document && !document->isFullyActive());
}

void Subscriber::reportErrorObject(JSC::JSValue value)
{
    auto* context = scriptExecutionContext();
    if (!context)
        return;

    auto* globalObject = context->globalObject();
    if (!globalObject)
        return;

    Ref vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    reportException(globalObject, JSC::Exception::create(vm, value));
}

Vector<VoidCallback*> Subscriber::teardownCallbacksConcurrently()
{
    Locker locker { m_teardownsLock };
    return m_teardowns.map([](auto& callback) {
        return callback.ptr();
    });
}

InternalObserver* Subscriber::observerConcurrently()
{
    return &m_observer.get();
}

void Subscriber::visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor)
{
    for (auto* teardown : teardownCallbacksConcurrently())
        teardown->visitJSFunction(visitor);

    observerConcurrently()->visitAdditionalChildren(visitor);
}

void Subscriber::visitAdditionalChildren(JSC::SlotVisitor& visitor)
{
    for (auto* teardown : teardownCallbacksConcurrently())
        teardown->visitJSFunction(visitor);

    observerConcurrently()->visitAdditionalChildren(visitor);
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Subscriber);

} // namespace WebCore
