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
#include "SubscriberCallback.h"
#include "SubscriptionObserverCallback.h"
#include "VoidCallback.h"

namespace WebCore {

Ref<Subscriber> Subscriber::create(ScriptExecutionContext& context)
{
    return adoptRef(*new Subscriber(context, nullptr, nullptr, nullptr));
}

Ref<Subscriber> Subscriber::create(ScriptExecutionContext& context, RefPtr<SubscriptionObserverCallback> nextCallback)
{
    return adoptRef(*new Subscriber(context, nextCallback, nullptr, nullptr));
}

Ref<Subscriber> Subscriber::create(ScriptExecutionContext& context, RefPtr<SubscriptionObserverCallback> nextCallback, RefPtr<SubscriptionObserverCallback> errorCallback, RefPtr<VoidCallback> completeCallback)
{
    return adoptRef(*new Subscriber(context, nextCallback, errorCallback, completeCallback));
}

Subscriber::Subscriber(ScriptExecutionContext& context, RefPtr<SubscriptionObserverCallback> nextCallback, RefPtr<SubscriptionObserverCallback> errorCallback, RefPtr<VoidCallback> completeCallback)
    : ActiveDOMObject(&context)
    , m_abortController(AbortController::create(context))
    , m_next(WTFMove(nextCallback))
    , m_error(WTFMove(errorCallback))
    , m_complete(WTFMove(completeCallback))
{
    followSignal(m_abortController->signal());
    suspendIfNeeded();
}

void Subscriber::next(JSC::JSValue value)
{
    if (!isActive())
        return;

    if (m_next)
        m_next->handleEvent(value);
}

void Subscriber::error(JSC::JSValue error)
{
    if (!m_active) {
        reportErrorObject(error);
        return;
    }

    if (isInactiveDocument())
        return;

    auto errorCallback = m_error;

    close(JSC::jsUndefined());

    if (errorCallback)
        errorCallback->handleEvent(error);

    else
        reportErrorObject(error);
}

void Subscriber::complete()
{
    if (!isActive())
        return;

    auto complete = m_complete;

    close(JSC::jsUndefined());

    if (complete)
        complete->handleEvent();
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

    m_abortController->abort(*JSC::jsCast<JSDOMGlobalObject*>(context->globalObject()), reason);

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

WTF_MAKE_ISO_ALLOCATED_IMPL(Subscriber);

} // namespace WebCore
