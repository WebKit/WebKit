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
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSNativeStdFunction.h"
#include "ScriptFunctionCall.h"
#include <wtf/JSONValues.h>
#include <wtf/text/MakeString.h>

namespace Inspector {

static RefPtr<JSON::Value> jsToInspectorValue(JSC::JSGlobalObject* globalObject, JSC::JSValue value, int maxDepth)
{
    if (!value) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!maxDepth)
        return nullptr;

    maxDepth--;

    if (value.isUndefinedOrNull())
        return JSON::Value::null();
    if (value.isBoolean())
        return JSON::Value::create(value.asBoolean());
    if (value.isDouble())
        return JSON::Value::create(value.asDouble());
    if (value.isInt32())
        return JSON::Value::create(value.asInt32());
    if (value.isString())
        return JSON::Value::create(asString(value)->value(globalObject).data);

    if (value.isObject()) {
        if (isJSArray(value)) {
            auto inspectorArray = JSON::Array::create();
            auto& array = *asArray(value);
            unsigned length = array.length();
            for (unsigned i = 0; i < length; i++) {
                auto elementValue = jsToInspectorValue(globalObject, array.getIndex(globalObject, i), maxDepth);
                if (!elementValue)
                    return nullptr;
                inspectorArray->pushValue(elementValue.releaseNonNull());
            }
            return inspectorArray;
        }
        JSC::VM& vm = globalObject->vm();
        auto inspectorObject = JSON::Object::create();
        auto& object = *value.getObject();
        JSC::PropertyNameArray propertyNames(vm, JSC::PropertyNameMode::Strings, JSC::PrivateSymbolMode::Exclude);
        object.methodTable()->getOwnPropertyNames(&object, globalObject, propertyNames, JSC::DontEnumPropertiesMode::Exclude);
        for (auto& name : propertyNames) {
            auto inspectorValue = jsToInspectorValue(globalObject, object.get(globalObject, name), maxDepth);
            if (!inspectorValue)
                return nullptr;
            inspectorObject->setValue(name.string(), inspectorValue.releaseNonNull());
        }
        return inspectorObject;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<JSON::Value> toInspectorValue(JSC::JSGlobalObject* globalObject, JSC::JSValue value)
{
    // FIXME: Maybe we should move the JSLockHolder stuff to the callers since this function takes a JSValue directly.
    // Doing the locking here made sense when we were trying to abstract the difference between multiple JavaScript engines.
    JSC::JSLockHolder holder(globalObject);
    return jsToInspectorValue(globalObject, value, JSON::Value::maxDepth);
}

InjectedScriptBase::InjectedScriptBase(const String& name)
    : m_name(name)
{
}

InjectedScriptBase::InjectedScriptBase(const String& name, JSC::JSGlobalObject* globalObject, JSC::JSObject* injectedScriptObject, InspectorEnvironment* environment)
    : m_name(name)
    , m_globalObject(globalObject)
    , m_injectedScriptObject(globalObject->vm(), injectedScriptObject)
    , m_environment(environment)
{
}

InjectedScriptBase::~InjectedScriptBase() = default;

bool InjectedScriptBase::hasAccessToInspectedScriptState() const
{
    return m_environment && m_environment->canAccessInspectedScriptState(m_globalObject);
}

JSC::JSObject* InjectedScriptBase::injectedScriptObject() const
{
    return m_injectedScriptObject.get();
}

Expected<JSC::JSValue, NakedPtr<JSC::Exception>> InjectedScriptBase::callFunctionWithEvalEnabled(ScriptFunctionCall& function) const
{
    JSC::DebuggerEvalEnabler evalEnabler(m_globalObject);
    return function.call();
}

Ref<JSON::Value> InjectedScriptBase::makeCall(ScriptFunctionCall& function)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState())
        return JSON::Value::null();

    auto globalObject = m_globalObject;

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
        return JSON::Value::create(makeString("Object has too long reference chain (must not be longer than "_s, JSON::Value::maxDepth, ')'));

    return resultJSONValue.releaseNonNull();
}

void InjectedScriptBase::makeEvalCall(Protocol::ErrorString& errorString, ScriptFunctionCall& function, RefPtr<Protocol::Runtime::RemoteObject>& resultObject, std::optional<bool>& wasThrown, std::optional<int>& savedResultIndex)
{
    checkCallResult(errorString, makeCall(function), resultObject, wasThrown, savedResultIndex);
}

void InjectedScriptBase::makeAsyncCall(ScriptFunctionCall& function, AsyncCallCallback&& callback)
{
    if (hasNoValue() || !hasAccessToInspectedScriptState()) {
        checkAsyncCallResult(JSON::Value::null(), callback);
        return;
    }

    auto* globalObject = m_globalObject;
    JSC::VM& vm = globalObject->vm();

    JSC::JSNativeStdFunction* jsFunction = nullptr;
    {
        JSC::JSLockHolder locker(vm);

        jsFunction = JSC::JSNativeStdFunction::create(vm, globalObject, 1, String(), [&, callback = WTFMove(callback)] (JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame) {
            if (!callFrame)
                checkAsyncCallResult(JSON::Value::create(makeString("Exception while making a call."_s)), callback);
            else if (auto resultJSONValue = toInspectorValue(globalObject, callFrame->argument(0)))
                checkAsyncCallResult(resultJSONValue, callback);
            else
                checkAsyncCallResult(JSON::Value::create(makeString("Object has too long reference chain (must not be longer than "_s, JSON::Value::maxDepth, ')')), callback);
            return JSC::JSValue::encode(JSC::jsUndefined());
        });
    }

    function.appendArgument(JSC::JSValue(jsFunction));

    auto result = callFunctionWithEvalEnabled(function);
    ASSERT_UNUSED(result, result && result.value() && result.value().isUndefined());
    if (!result || !result.value()) {
        // Since `callback` is moved above, we can't call it if there's an exception while trying to
        // execute the `JSNativeStdFunction` inside InjectedScriptSource.js.
        jsFunction->function()(globalObject, nullptr);
    }
}

void InjectedScriptBase::checkCallResult(Protocol::ErrorString& errorString, RefPtr<JSON::Value> result, RefPtr<Protocol::Runtime::RemoteObject>& resultObject, std::optional<bool>& wasThrown, std::optional<int>& savedResultIndex)
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

    auto typelessResultObject = resultTuple->getObject("result"_s);
    if (!typelessResultObject) {
        // FIXME: Why do we bother checking for null here, but not checking that the type is Protocol::Runtime::RemoteObject? Surely the two possible errors go hand in hand.
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    wasThrown = resultTuple->getBoolean("wasThrown"_s);
    if (!wasThrown) {
        errorString = "Internal error: result is not a pair of value and wasThrown flag"_s;
        return;
    }

    resultObject = Protocol::BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(typelessResultObject.releaseNonNull());
    savedResultIndex = resultTuple->getInteger("savedResultIndex"_s);
}

void InjectedScriptBase::checkAsyncCallResult(RefPtr<JSON::Value> result, const AsyncCallCallback& callback)
{
    Protocol::ErrorString errorString;
    RefPtr<Protocol::Runtime::RemoteObject> resultObject;
    std::optional<bool> wasThrown;
    std::optional<int> savedResultIndex;

    checkCallResult(errorString, result, resultObject, wasThrown, savedResultIndex);

    callback(errorString, WTFMove(resultObject), WTFMove(wasThrown), WTFMove(savedResultIndex));
}

} // namespace Inspector

