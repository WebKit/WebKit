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
#include "InjectedScript.h"

#include "JSCInlines.h"
#include "JSLock.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include <wtf/JSONValues.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

InjectedScript::InjectedScript()
    : InjectedScriptBase("InjectedScript"_s)
{
}

InjectedScript::InjectedScript(Deprecated::ScriptObject injectedScriptObject, InspectorEnvironment* environment)
    : InjectedScriptBase("InjectedScript"_s, injectedScriptObject, environment)
{
}

InjectedScript::~InjectedScript()
{
}

void InjectedScript::execute(ErrorString& errorString, const String& functionString, ExecuteOptions&& options, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "execute"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(functionString);
    function.appendArgument(options.objectGroup);
    function.appendArgument(options.includeCommandLineAPI);
    function.appendArgument(options.returnByValue);
    function.appendArgument(options.generatePreview);
    function.appendArgument(options.saveResult);
    function.appendArgument(arrayFromVector(WTFMove(options.args)));
    makeEvalCall(errorString, function, result, wasThrown, savedResultIndex);
}

void InjectedScript::evaluate(ErrorString& errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, bool saveResult, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "evaluate"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    function.appendArgument(saveResult);
    makeEvalCall(errorString, function, result, wasThrown, savedResultIndex);
}

void InjectedScript::awaitPromise(const String& promiseObjectId, bool returnByValue, bool generatePreview, bool saveResult, AsyncCallCallback&& callback)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "awaitPromise"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(promiseObjectId);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    function.appendArgument(saveResult);
    makeAsyncCall(function, WTFMove(callback));
}

void InjectedScript::callFunctionOn(ErrorString& errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "callFunctionOn"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);

    Optional<int> savedResultIndex;
    makeEvalCall(errorString, function, result, wasThrown, savedResultIndex);
    ASSERT(!savedResultIndex);
}

void InjectedScript::evaluateOnCallFrame(ErrorString& errorString, JSC::JSValue callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, bool saveResult, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "evaluateOnCallFrame"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    function.appendArgument(saveResult);
    makeEvalCall(errorString, function, result, wasThrown, savedResultIndex);
}

void InjectedScript::getFunctionDetails(ErrorString& errorString, const String& functionId, RefPtr<Protocol::Debugger::FunctionDetails>& result)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getFunctionDetails"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(functionId);

    RefPtr<JSON::Value> resultValue = makeCall(function);
    if (!resultValue || resultValue->type() != JSON::Value::Type::Object) {
        if (!resultValue->asString(errorString))
            errorString = "Internal error"_s;
        return;
    }

    result = BindingTraits<Protocol::Debugger::FunctionDetails>::runtimeCast(WTFMove(resultValue));
}

void InjectedScript::functionDetails(ErrorString& errorString, JSC::JSValue value, RefPtr<Protocol::Debugger::FunctionDetails>& result)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "functionDetails"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(value);

    RefPtr<JSON::Value> resultValue = makeCall(function);
    if (!resultValue || resultValue->type() != JSON::Value::Type::Object) {
        if (!resultValue->asString(errorString))
            errorString = "Internal error"_s;
        return;
    }

    result = BindingTraits<Protocol::Debugger::FunctionDetails>::runtimeCast(WTFMove(resultValue));
}

void InjectedScript::getPreview(ErrorString& errorString, const String& objectId, RefPtr<Protocol::Runtime::ObjectPreview>& result)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getPreview"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);

    RefPtr<JSON::Value> resultValue = makeCall(function);
    if (!resultValue || resultValue->type() != JSON::Value::Type::Object) {
        if (!resultValue->asString(errorString))
            errorString = "Internal error"_s;
        return;
    }

    result = BindingTraits<Protocol::Runtime::ObjectPreview>::runtimeCast(WTFMove(resultValue));
}

void InjectedScript::getProperties(ErrorString& errorString, const String& objectId, bool ownProperties, bool generatePreview, RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>& properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getProperties"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);
    function.appendArgument(generatePreview);

    RefPtr<JSON::Value> result = makeCall(function);
    if (!result || result->type() != JSON::Value::Type::Array) {
        errorString = "Internal error"_s;
        return;
    }

    properties = BindingTraits<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>::runtimeCast(WTFMove(result));
}

void InjectedScript::getDisplayableProperties(ErrorString& errorString, const String& objectId, bool generatePreview, RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>& properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getDisplayableProperties"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(generatePreview);

    RefPtr<JSON::Value> result = makeCall(function);
    if (!result || result->type() != JSON::Value::Type::Array) {
        errorString = "Internal error"_s;
        return;
    }

    properties = BindingTraits<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>::runtimeCast(WTFMove(result));
}

void InjectedScript::getInternalProperties(ErrorString& errorString, const String& objectId, bool generatePreview, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>& properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getInternalProperties"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(generatePreview);

    RefPtr<JSON::Value> result = makeCall(function);
    if (!result || result->type() != JSON::Value::Type::Array) {
        errorString = "Internal error"_s;
        return;
    }

    auto array = BindingTraits<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>::runtimeCast(WTFMove(result));
    properties = array->length() > 0 ? array : nullptr;
}

void InjectedScript::getCollectionEntries(ErrorString& errorString, const String& objectId, const String& objectGroup, int startIndex, int numberToFetch, RefPtr<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>>& entries)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getCollectionEntries"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(objectGroup);
    function.appendArgument(startIndex);
    function.appendArgument(numberToFetch);

    RefPtr<JSON::Value> result = makeCall(function);
    if (!result || result->type() != JSON::Value::Type::Array) {
        errorString = "Internal error"_s;
        return;
    }

    entries = BindingTraits<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>>::runtimeCast(WTFMove(result));
}

void InjectedScript::saveResult(ErrorString& errorString, const String& callArgumentJSON, Optional<int>& savedResultIndex)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "saveResult"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(callArgumentJSON);

    RefPtr<JSON::Value> result = makeCall(function);
    if (!result || result->type() != JSON::Value::Type::Integer) {
        errorString = "Internal error"_s;
        return;
    }

    int resultIndex = 0;
    if (result->asInteger(resultIndex) && resultIndex > 0)
        savedResultIndex = resultIndex;
}

Ref<JSON::ArrayOf<Protocol::Debugger::CallFrame>> InjectedScript::wrapCallFrames(JSC::JSValue callFrames) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "wrapCallFrames"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(callFrames);

    bool hadException = false;
    auto callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    if (!callFramesValue)
        return JSON::ArrayOf<Protocol::Debugger::CallFrame>::create();
    ASSERT(!hadException);
    RefPtr<JSON::Value> result = toInspectorValue(*scriptState(), callFramesValue);
    if (result->type() == JSON::Value::Type::Array)
        return BindingTraits<JSON::ArrayOf<Protocol::Debugger::CallFrame>>::runtimeCast(WTFMove(result)).releaseNonNull();

    return JSON::ArrayOf<Protocol::Debugger::CallFrame>::create();
}

RefPtr<Protocol::Runtime::RemoteObject> InjectedScript::wrapObject(JSC::JSValue value, const String& groupName, bool generatePreview) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapObject"_s, inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(hasAccessToInspectedScriptState());
    wrapFunction.appendArgument(generatePreview);

    bool hadException = false;
    auto r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    RefPtr<JSON::Object> resultObject;
    bool castSucceeded = toInspectorValue(*scriptState(), r)->asObject(resultObject);
    ASSERT_UNUSED(castSucceeded, castSucceeded);

    return BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject);
}

RefPtr<Protocol::Runtime::RemoteObject> InjectedScript::wrapJSONString(const String& json, const String& groupName, bool generatePreview) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapJSONString"_s, inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(json);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(generatePreview);

    bool hadException = false;
    auto evalResult = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    if (evalResult.isNull())
        return nullptr;

    RefPtr<JSON::Object> resultObject;
    bool castSucceeded = toInspectorValue(*scriptState(), evalResult)->asObject(resultObject);
    ASSERT_UNUSED(castSucceeded, castSucceeded);

    return BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject);
}

RefPtr<Protocol::Runtime::RemoteObject> InjectedScript::wrapTable(JSC::JSValue table, JSC::JSValue columns) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapTable"_s, inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(hasAccessToInspectedScriptState());
    wrapFunction.appendArgument(table);
    if (!columns)
        wrapFunction.appendArgument(false);
    else
        wrapFunction.appendArgument(columns);

    bool hadException = false;
    auto r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    RefPtr<JSON::Object> resultObject;
    bool castSucceeded = toInspectorValue(*scriptState(), r)->asObject(resultObject);
    ASSERT_UNUSED(castSucceeded, castSucceeded);

    return BindingTraits<Protocol::Runtime::RemoteObject>::runtimeCast(resultObject);
}

RefPtr<Protocol::Runtime::ObjectPreview> InjectedScript::previewValue(JSC::JSValue value) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "previewValue"_s, inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(value);

    bool hadException = false;
    auto r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    RefPtr<JSON::Object> resultObject;
    bool castSucceeded = toInspectorValue(*scriptState(), r)->asObject(resultObject);
    ASSERT_UNUSED(castSucceeded, castSucceeded);

    return BindingTraits<Protocol::Runtime::ObjectPreview>::runtimeCast(resultObject);
}

void InjectedScript::setEventValue(JSC::JSValue value)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "setEventValue"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(value);
    makeCall(function);
}

void InjectedScript::clearEventValue()
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "clearEventValue"_s, inspectorEnvironment()->functionCallHandler());
    makeCall(function);
}

void InjectedScript::setExceptionValue(JSC::JSValue value)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "setExceptionValue"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(value);
    makeCall(function);
}

void InjectedScript::clearExceptionValue()
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "clearExceptionValue"_s, inspectorEnvironment()->functionCallHandler());
    makeCall(function);
}

JSC::JSValue InjectedScript::findObjectById(const String& objectId) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "findObjectById"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);

    bool hadException = false;
    auto resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);

    return resultValue;
}

void InjectedScript::inspectObject(JSC::JSValue value)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "inspectObject"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(value);
    makeCall(function);
}

void InjectedScript::releaseObject(const String& objectId)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "releaseObject"_s, inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    makeCall(function);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall releaseFunction(injectedScriptObject(), "releaseObjectGroup"_s, inspectorEnvironment()->functionCallHandler());
    releaseFunction.appendArgument(objectGroup);

    bool hadException = false;
    callFunctionWithEvalEnabled(releaseFunction, hadException);
    ASSERT(!hadException);
}

JSC::JSValue InjectedScript::arrayFromVector(Vector<JSC::JSValue>&& vector)
{
    JSC::ExecState* execState = scriptState();
    if (!execState)
        return JSC::jsUndefined();

    JSC::JSLockHolder lock(execState);

    JSC::JSArray* array = JSC::constructEmptyArray(execState, nullptr);
    if (!array)
        return JSC::jsUndefined();

    for (auto& item : vector)
        array->putDirectIndex(execState, array->length(), item);

    return array;
}

} // namespace Inspector

