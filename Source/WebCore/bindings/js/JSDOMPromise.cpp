/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "DOMWindow.h"
#include "JSDOMWindow.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSPromiseConstructor.h>

using namespace JSC;

namespace WebCore {

static inline JSC::JSValue callFunction(JSC::ExecState& state, JSC::JSValue jsFunction, JSC::JSValue thisValue, const JSC::ArgList& arguments)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::CallData callData;
    auto callType = JSC::getCallData(vm, jsFunction, callData);
    ASSERT(callType != JSC::CallType::None);
    auto result = call(&state, jsFunction, callType, callData, thisValue, arguments);

    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || isTerminatedExecutionException(state.vm(), scope.exception()));

    return result;
}

void DOMPromise::whenSettled(std::function<void()>&& callback)
{
    whenPromiseIsSettled(globalObject(), promise(), WTFMove(callback));
}

void DOMPromise::whenPromiseIsSettled(JSDOMGlobalObject* globalObject, JSC::JSObject* promise, std::function<void()>&& callback)
{
    auto& state = *globalObject->globalExec();
    auto& vm = state.vm();
    JSLockHolder lock(vm);
    auto* handler = JSC::JSNativeStdFunction::create(vm, globalObject, 1, String { }, [callback = WTFMove(callback)] (ExecState*) mutable {
        callback();
        return JSC::JSValue::encode(JSC::jsUndefined());
    });

    const JSC::Identifier& privateName = vm.propertyNames->builtinNames().thenPrivateName();
    auto thenFunction = promise->get(&state, privateName);
    ASSERT(thenFunction.isFunction(vm));

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(handler);
    arguments.append(handler);
    callFunction(state, thenFunction, promise, arguments);
}

JSC::JSValue DOMPromise::result() const
{
    return promise()->result(m_globalObject->globalExec()->vm());
}

DOMPromise::Status DOMPromise::status() const
{
    switch (promise()->status(m_globalObject->globalExec()->vm())) {
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

}
