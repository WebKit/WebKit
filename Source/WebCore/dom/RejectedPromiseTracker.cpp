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
#include "PromiseRejectionEvent.h"
#include "ScriptExecutionContext.h"
#include <heap/HeapInlines.h>
#include <heap/Strong.h>
#include <heap/StrongInlines.h>
#include <heap/Weak.h>
#include <heap/WeakInlines.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <runtime/Exception.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/JSGlobalObject.h>
#include <runtime/JSPromise.h>
#include <runtime/WeakGCMapInlines.h>

using namespace JSC;
using namespace Inspector;

namespace WebCore {

class RejectedPromise {
    WTF_MAKE_NONCOPYABLE(RejectedPromise);
public:
    RejectedPromise(VM& vm, JSDOMGlobalObject& globalObject, JSPromise& promise)
        : m_globalObject(vm, &globalObject)
        , m_promise(vm, &promise)
    {
    }

    RejectedPromise(RejectedPromise&&) = default;

    JSDOMGlobalObject& globalObject()
    {
        return *m_globalObject.get();
    }

    JSPromise& promise()
    {
        return *m_promise.get();
    }

private:
    Strong<JSDOMGlobalObject> m_globalObject;
    Strong<JSPromise> m_promise;
};

class UnhandledPromise : public RejectedPromise {
public:
    UnhandledPromise(VM& vm, JSDOMGlobalObject& globalObject, JSPromise& promise, RefPtr<ScriptCallStack>&& stack)
        : RejectedPromise(vm, globalObject, promise)
        , m_stack(WTFMove(stack))
    {
    }

    ScriptCallStack* callStack()
    {
        return m_stack.get();
    }

private:
    RefPtr<ScriptCallStack> m_stack;
};


RejectedPromiseTracker::RejectedPromiseTracker(ScriptExecutionContext& context, JSC::VM& vm)
    : m_context(context)
    , m_outstandingRejectedPromises(vm)
{
}

RejectedPromiseTracker::~RejectedPromiseTracker()
{
}

static RefPtr<ScriptCallStack> createScriptCallStackFromReason(ExecState& state, JSValue reason)
{
    VM& vm = state.vm();

    // Always capture a stack from the exception if this rejection was an exception.
    if (auto* exception = vm.lastException()) {
        if (exception->value() == reason)
            return createScriptCallStackFromException(&state, exception, ScriptCallStack::maxCallStackSizeToCapture);
    }

    // Otherwise, only capture a stack if a debugger is open.
    if (state.lexicalGlobalObject()->debugger())
        return createScriptCallStack(&state, ScriptCallStack::maxCallStackSizeToCapture);

    return nullptr;
}

void RejectedPromiseTracker::promiseRejected(ExecState& state, JSDOMGlobalObject& globalObject, JSPromise& promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    VM& vm = state.vm();
    JSValue reason = promise.result(vm);
    m_aboutToBeNotifiedRejectedPromises.append(UnhandledPromise { vm, globalObject, promise, createScriptCallStackFromReason(state, reason) });
}

void RejectedPromiseTracker::promiseHandled(ExecState& state, JSDOMGlobalObject& globalObject, JSPromise& promise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    bool removed = m_aboutToBeNotifiedRejectedPromises.removeFirstMatching([&] (UnhandledPromise& unhandledPromise) {
        return &unhandledPromise.promise() == &promise;
    });
    if (removed)
        return;

    if (!m_outstandingRejectedPromises.remove(&promise))
        return;

    VM& vm = state.vm();

    m_context.postTask([this, rejectedPromise = RejectedPromise { vm, globalObject, promise }] (ScriptExecutionContext&) mutable {
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
        ExecState& state = *unhandledPromise.globalObject().globalExec();
        auto& promise = unhandledPromise.promise();

        if (promise.isHandled(vm))
            continue;

        PromiseRejectionEvent::Init initializer;
        initializer.cancelable = true;
        initializer.promise = &promise;
        initializer.reason = promise.result(vm);

        auto event = PromiseRejectionEvent::create(state, eventNames().unhandledrejectionEvent, initializer);
        auto target = m_context.errorEventTarget();
        bool needsDefaultAction = target->dispatchEvent(event);

        if (needsDefaultAction)
            m_context.reportUnhandledPromiseRejection(state, unhandledPromise.promise(), unhandledPromise.callStack());

        if (!promise.isHandled(vm))
            m_outstandingRejectedPromises.set(&promise, &promise);
    }
}

void RejectedPromiseTracker::reportRejectionHandled(RejectedPromise&& rejectedPromise)
{
    // https://html.spec.whatwg.org/multipage/webappapis.html#the-hostpromiserejectiontracker-implementation

    VM& vm = m_context.vm();
    JSC::JSLockHolder lock(vm);

    ExecState& state = *rejectedPromise.globalObject().globalExec();

    PromiseRejectionEvent::Init initializer;
    initializer.promise = &rejectedPromise.promise();
    initializer.reason = rejectedPromise.promise().result(state.vm());

    auto event = PromiseRejectionEvent::create(state, eventNames().rejectionhandledEvent, initializer);
    auto target = m_context.errorEventTarget();
    target->dispatchEvent(event);
}

} // namespace WebCore
