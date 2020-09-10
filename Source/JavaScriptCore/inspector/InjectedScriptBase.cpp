/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InjectedScriptBase.h"

#include "DebuggerEvalEnabler.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSNativeStdFunction.h"
#include "ScriptFunctionCall.h"
#include <wtf/JSONValues.h>

namespace Inspector {

InjectedScriptBase::InjectedScriptBase(const String& name)
    : m_name(name)
{
}

InjectedScriptBase::InjectedScriptBase(const String& name, Deprecated::ScriptObject injectedScriptObject, InspectorEnvironment* environment)
    : m_name(name)
    , m_injectedScriptObject(injectedScriptObject)
    , m_environment(environment)
{
}

InjectedScriptBase::~InjectedScriptBase()
{
}

bool InjectedScriptBase::hasAccessToInspectedScriptState() const
{
    return m_environment && m_environment->canAccessInspectedScriptState(m_injectedScriptObject.globalObject());
}

const Deprecated::ScriptObject& InjectedScriptBase::injectedScriptObject() const
{
    return m_injectedScriptObject;
}

Expected<JSC::JSValue, NakedPtr<JSC::Exception>> InjectedScriptBase::callFunctionWithEvalEnabled(Deprecated::ScriptFunctionCall& function) const
{
    JSC::JSGlobalObject* globalObject = m_injectedScriptObject.globalObject();
    JSC::DebuggerEvalEnabler evalEnabler(globalObject);
    return function.call();
}

Ref<JSON::Value> InjectedScriptBase::makeCall(Deprecated::ScriptFunctionCall& function)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState())
        return JSON::Value::null();

    auto globalObject = m_injectedScriptObject.globalObject();

    auto result = callFunctionWithEvalEnabled(function);
    if (!result) {
        auto& error = result.error();
        ASSERT(error);

        return JSON::Value::create(error->value().toWTFString(globalObject));
    }

    auto value = result.value();
    if (!value)
        return JSON::Value::null();

    auto resultJSONValue = toInspectorValue(globalObject, value);
    if (!resultJSONValue)
        return JSON::Value::create(makeString("Object has too long reference chain (must not be longer than ", JSON::Value::maxDepth, ')'));

    return resultJSONValue.releaseNonNull();
}

void InjectedScriptBase::makeEvalCall(Protocol::ErrorString& errorString, Deprecated::ScriptFunctionCall& function, RefPtr<Protocol::Runtime::RemoteObject>& out_resultObject, Optional<bool>& out_wasThrown, Optional<int>& out_savedResultIndex)
{
    checkCallResult(errorString, makeCall(function), out_resultObject, out_wasThrown, out_savedResultIndex);
}

void InjectedScriptBase::makeAsyncCall(Deprecated::ScriptFunctionCall& function, AsyncCallCallback&& callback)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState()) {
        checkAsyncCallResult(JSON::Value::null(), callback);
        return;
    }

    auto* globalObject = m_injectedScriptObject.globalObject();
    JSC::VM& vm = globalObject->vm();

    JSC::JSNativeStdFunction* jsFunction = nullptr;
    {
        JSC::JSLockHolder locker(vm);

        jsFunction = JSC::JSNativeStdFunction::create(vm, globalObject, 1, String(), [&, callback = WTFMove(callback)] (JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame) {
            if (!callFrame)
                checkAsyncCallResult(JSON::Value::create("Exception while making a call."), callback);
            else if (auto resultJSONValue = toInspectorValue(globalObject, callFrame->argument(0)))
                checkAsyncCallResult(resultJSONValue, callback);
            else
                checkAsyncCallResult(JSON::Value::create(makeString("Object has too long reference chain (must not be longer than ", JSON::Value::maxDepth, ')')), callback);
            return JSC::JSValue::encode(JSC::jsUndefined());
        });
    }

    function.appendArgument(JSC::JSValue(jsFunction));

    auto result = callFunctionWithEvalEnabled(function);
    ASSERT_UNUSED(result, result.value().isUndefined());

    ASSERT(result);
    if (!result) {
        // Since `callback` is moved above, we can't call it if there's an exception while trying to
        // execute the `JSNativeStdFunction` inside InjectedScriptSource.js.
        jsFunction->function()(globalObject, nullptr);
    }
}

void InjectedScriptBase::checkCallResult(Protocol::ErrorString& errorString, RefPtr<JSON::Value> result, RefPtr<Protocol::Runtime::RemoteObject>& out_resultObject, Optional<bool>& out_wasThrown, Optional<int>& out_savedResultIndex)
{
    if (!result) {
        errorString = "Internal error: result value is empty"_s;
        return;
    }

    if (result->type() == JSON::Value::Type::String) {
        errorString = result->asString();
        return;
    }

    auto resultTuple = result->asObject();
    if (!resultTuple) {
        errorString = "Internal error: result is not an Object"_s;
        return;
    }

    auto resultObject = resultTuple->getObject("result"_s);
    if (!resultObject) {
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    out_wasThrown = resultTuple->getBoolean("wasThrown"_s);
    if (!out_wasThrown) {
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    out_resultObject = Protocol::BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject.releaseNonNull());
    out_savedResultIndex = resultTuple->getInteger("savedResultIndex"_s);
}

void InjectedScriptBase::checkAsyncCallResult(RefPtr<JSON::Value> result, const AsyncCallCallback& callback)
{
    Protocol::ErrorString errorString;
    RefPtr<Protocol::Runtime::RemoteObject> resultObject;
    Optional<bool> wasThrown;
    Optional<int> savedResultIndex;

    checkCallResult(errorString, result, resultObject, wasThrown, savedResultIndex);

    callback(errorString, WTFMove(resultObject), WTFMove(wasThrown), WTFMove(savedResultIndex));
}

} // namespace Inspector

