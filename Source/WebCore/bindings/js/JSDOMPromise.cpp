/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "JSDOMPromise.h"

#include "JSDOMGlobalObject.h"
#include "LocalDOMWindow.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSPromiseConstructor.h>
#include <JavaScriptCore/TopExceptionScope.h>

using namespace JSC;

namespace WebCore {

auto DOMPromise::whenSettledWithResult(Function<void(JSDOMGlobalObject*, bool, JSC::JSValue)>&& callback) -> IsCallbackRegistered
{
    if (isSuspended())
        return IsCallbackRegistered::No;
    return whenPromiseIsSettled(globalObject(), promise(), WTF::move(callback));
}

auto DOMPromise::whenPromiseIsSettled(JSDOMGlobalObject* globalObject, JSC::JSPromise* promise, Function<void(JSDOMGlobalObject*, bool, JSC::JSValue)>&& callback) -> IsCallbackRegistered
{
    auto& lexicalGlobalObject = *globalObject;
    auto& vm = lexicalGlobalObject.vm();
    JSLockHolder lock(vm);
    auto* handler = JSC::JSNativeStdFunction::create(vm, globalObject, 1, String { }, [callback = WTF::move(callback)] (JSGlobalObject* globalObject, CallFrame* callFrame) mutable {
        auto* castedThis = JSC::jsDynamicCast<JSC::JSPromise*>(callFrame->thisValue());
        ASSERT(castedThis);
        // We exchange callback so that all captured variables are deallocated after the call. This is quicker than waiting for the handler function to be GCed.
        if (castedThis)
            std::exchange(callback, { })(JSC::jsCast<JSDOMGlobalObject*>(globalObject), castedThis->status() == JSC::JSPromise::Status::Fulfilled, castedThis->result());
        return JSC::JSValue::encode(JSC::jsUndefined());
    });

    auto* thisHandler = JSC::JSBoundFunction::create(vm, globalObject, handler, promise, { }, 0, jsEmptyString(vm), JSC::makeSource("createWhenPromiseSettledFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));
    if (!thisHandler) [[unlikely]]
        return IsCallbackRegistered::No;

    promise->performPromiseThenExported(vm, globalObject, thisHandler, thisHandler, JSC::jsUndefined());
    return IsCallbackRegistered::Yes;
}

JSC::JSValue DOMPromise::result() const
{
    return promise()->result();
}

DOMPromise::Status DOMPromise::status() const
{
    switch (promise()->status()) {
    case JSC::JSPromise::Status::Pending:
        return Status::Pending;
    case JSC::JSPromise::Status::Fulfilled:
        return Status::Fulfilled;
    case JSC::JSPromise::Status::Rejected:
        return Status::Rejected;
    };
    ASSERT_NOT_REACHED();
    return Status::Rejected;
}

void DOMPromise::markAsHandled()
{
    promise()->markAsHandled();
}

}
