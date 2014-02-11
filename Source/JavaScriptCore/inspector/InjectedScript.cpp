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

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"
#include "JSCInlines.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"
#include <wtf/text/WTFString.h>

using Inspector::TypeBuilder::Array;

namespace Inspector {

InjectedScript::InjectedScript()
    : InjectedScriptBase(ASCIILiteral("InjectedScript"))
{
}

InjectedScript::InjectedScript(Deprecated::ScriptObject injectedScriptObject, InspectorEnvironment* environment)
    : InjectedScriptBase(ASCIILiteral("InjectedScript"), injectedScriptObject, environment)
{
}

InjectedScript::~InjectedScript()
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("evaluate"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("callFunctionOn"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, const Deprecated::ScriptValue& callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("evaluateOnCallFrame"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<Inspector::TypeBuilder::Debugger::FunctionDetails>* result)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("getFunctionDetails"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(functionId);

    RefPtr<InspectorValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != InspectorValue::TypeObject) {
        if (!resultValue->asString(errorString))
            *errorString = ASCIILiteral("Internal error");
        return;
    }

    *result = Inspector::TypeBuilder::Debugger::FunctionDetails::runtimeCast(resultValue);
}

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, RefPtr<Array<Inspector::TypeBuilder::Runtime::PropertyDescriptor>>* properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("getProperties"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);

    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result || result->type() != InspectorValue::TypeArray) {
        *errorString = ASCIILiteral("Internal error");
        return;
    }

    *properties = Array<Inspector::TypeBuilder::Runtime::PropertyDescriptor>::runtimeCast(result);
}

void InjectedScript::getInternalProperties(ErrorString* errorString, const String& objectId, RefPtr<Array<Inspector::TypeBuilder::Runtime::InternalPropertyDescriptor>>* properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("getInternalProperties"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);

    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result || result->type() != InspectorValue::TypeArray) {
        *errorString = ASCIILiteral("Internal error");
        return;
    }

    RefPtr<Array<Inspector::TypeBuilder::Runtime::InternalPropertyDescriptor>> array = Array<Inspector::TypeBuilder::Runtime::InternalPropertyDescriptor>::runtimeCast(result);
    if (array->length() > 0)
        *properties = array;
}

PassRefPtr<Array<Inspector::TypeBuilder::Debugger::CallFrame>> InjectedScript::wrapCallFrames(const Deprecated::ScriptValue& callFrames)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("wrapCallFrames"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(callFrames);

    bool hadException = false;
    Deprecated::ScriptValue callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<InspectorValue> result = callFramesValue.toInspectorValue(scriptState());
    if (result->type() == InspectorValue::TypeArray)
        return Array<Inspector::TypeBuilder::Debugger::CallFrame>::runtimeCast(result);

    return Array<Inspector::TypeBuilder::Debugger::CallFrame>::create();
}

PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(const Deprecated::ScriptValue& value, const String& groupName, bool generatePreview) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), ASCIILiteral("wrapObject"), inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(hasAccessToInspectedScriptState());
    wrapFunction.appendArgument(generatePreview);

    bool hadException = false;
    Deprecated::ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    RefPtr<InspectorObject> rawResult = r.toInspectorValue(scriptState())->asObject();
    return Inspector::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapTable(const Deprecated::ScriptValue& table, const Deprecated::ScriptValue& columns) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), ASCIILiteral("wrapTable"), inspectorEnvironment()->functionCallHandler());
    wrapFunction.appendArgument(hasAccessToInspectedScriptState());
    wrapFunction.appendArgument(table);
    if (columns.hasNoValue())
        wrapFunction.appendArgument(false);
    else
        wrapFunction.appendArgument(columns);

    bool hadException = false;
    Deprecated::ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return nullptr;

    RefPtr<InspectorObject> rawResult = r.toInspectorValue(scriptState())->asObject();
    return Inspector::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

Deprecated::ScriptValue InjectedScript::findObjectById(const String& objectId) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("findObjectById"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);

    bool hadException = false;
    Deprecated::ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);

    return resultValue;
}

void InjectedScript::inspectObject(Deprecated::ScriptValue value)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("inspectObject"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(value);
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

void InjectedScript::releaseObject(const String& objectId)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), ASCIILiteral("releaseObject"), inspectorEnvironment()->functionCallHandler());
    function.appendArgument(objectId);
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall releaseFunction(injectedScriptObject(), ASCIILiteral("releaseObjectGroup"), inspectorEnvironment()->functionCallHandler());
    releaseFunction.appendArgument(objectGroup);

    bool hadException = false;
    callFunctionWithEvalEnabled(releaseFunction, hadException);
    ASSERT(!hadException);
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
