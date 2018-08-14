/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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
#include "RejectedPromiseTracker.h"

#include "EventNames.h"
#include "EventTarget.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromise.h"
#include "PromiseRejectionEvent.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSPromise.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/Strong.h>
#include <JavaScriptCore/StrongInlines.h>
#include <JavaScriptCore/Weak.h>
#include <JavaScriptCore/WeakGCMapInlines.h>
#include <JavaScriptCore/WeakInlines.h>

namespace WebCore {
using namespace JSC;
using namespace Inspector;

class UnhandledPromise {
    WTF_MAKE_NONCOPYABLE(UnhandledPromise);
public:
    UnhandledPromise(JSDOMGlobalObject& globalObject, JSPromise& promise, RefPtr<ScriptCallStack>&& stack)
        : m_promise(DOMPromise::create(globalObject, promise))
        , m_stack(WTFMove(stack))
    {
    }

    UnhandledPromise(UnhandledPromise&&) = default;

    ScriptCallStack* callStack()
    {
        return m_stack.get();
    }

    DOMPromise& promise()
    {
        return m_promise.get();
    }

private:
    Ref<DOMPromise> m_promise;
    RefPtr<ScriptCallStack> m_stack;
};


RejectedPromiseTracker::RejectedPromiseTracker(ScriptExecutionContext& context, JSC::VM& vm)
    : m_context(context)
    , m_outstandingRejectedPromises(vm)
{
}

RejectedPromiseTracker::~RejectedPromiseTracker() = default;

static RefPtr<ScriptCallStack> createScriptCallStackFromReason(ExecState& state, JSValue reason)
{
    VM& vm = state.vm();

    // Always capture a stack from the exception if this rejection was an exception.
    if (auto* exception = vm.lastException()) {
        if (exception->value() == reason)
            return createScriptCallStackFromException(&state, exception);
    }

    // Otherwise, only capture a stack if a debugger is open.
    if (state.lexicalGlobalObject()->debugger())
        return createScriptCallStack(&state);

    return nullptr;
}

void RejectedPromiseTracker::promiseRejected(ExecState& state, JSDOMGlobalObject& globalObject, JSPromise& promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    JSValue reason = promise.result(state.vm());
    m_aboutToBeNotifiedRejectedPromises.append(UnhandledPromise { globalObject, promise, createScriptCallStackFromReason(state, reason) });
}

void RejectedPromiseTracker::promiseHandled(ExecState&, JSDOMGlobalObject& globalObject, JSPromise& promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    bool removed = m_aboutToBeNotifiedRejectedPromises.removeFirstMatching([&] (UnhandledPromise& unhandledPromise) {
        auto& domPromise = unhandledPromise.promise();
        if (domPromise.isSuspended())
            return false;
        return domPromise.promise() == &promise;
    });
    if (removed)
        return;

    if (!m_outstandingRejectedPromises.remove(&promise))
        return;

    m_context.postTask([this, rejectedPromise = DOMPromise::create(globalObject, promise)] (ScriptExecutionContext&) mutable {
        reportRejectionHandled(WTFMove(rejectedPromise));
    });
}

void RejectedPromiseTracker::processQueueSoon()
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#notify-about-rejected-promises

    if (m_aboutToBeNotifiedRejectedPromises.isEmpty())
        return;

    Vector<UnhandledPromise> items = WTFMove(m_aboutToBeNotifiedRejectedPromises);
    m_context.postTask([this, items = WTFMove(items)] (ScriptExecutionContext&) mutable {
        reportUnhandledRejections(WTFMove(items));
    });
}

void RejectedPromiseTracker::reportUnhandledRejections(Vector<UnhandledPromise>&& unhandledPromises)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#unhandled-promise-rejections

    VM& vm = m_context.vm();
    JSC::JSLockHolder lock(vm);

    for (auto& unhandledPromise : unhandledPromises) {
        auto& domPromise = unhandledPromise.promise();
        if (domPromise.isSuspended())
            continue;
        auto& state = *domPromise.globalObject()->globalExec();
        auto& promise = *domPromise.promise();

        if (promise.isHandled(vm))
            continue;

        PromiseRejectionEvent::Init initializer;
        initializer.cancelable = true;
        initializer.promise = &domPromise;
        initializer.reason = promise.result(vm);

        auto event = PromiseRejectionEvent::create(eventNames().unhandledrejectionEvent, initializer);
        auto target = m_context.errorEventTarget();
        target->dispatchEvent(event);

        if (!event->defaultPrevented())
            m_context.reportUnhandledPromiseRejection(state, promise, unhandledPromise.callStack());

        if (!promise.isHandled(vm))
            m_outstandingRejectedPromises.set(&promise, &promise);
    }
}

void RejectedPromiseTracker::reportRejectionHandled(Ref<DOMPromise>&& rejectedPromise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    VM& vm = m_context.vm();
    JSC::JSLockHolder lock(vm);

    if (rejectedPromise->isSuspended())
        return;

    auto& promise = *rejectedPromise->promise();

    PromiseRejectionEvent::Init initializer;
    initializer.promise = rejectedPromise.ptr();
    initializer.reason = promise.result(vm);

    auto event = PromiseRejectionEvent::create(eventNames().rejectionhandledEvent, initializer);
    auto target = m_context.errorEventTarget();
    target->dispatchEvent(event);
}

} // namespace WebCore
