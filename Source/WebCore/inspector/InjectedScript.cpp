/*
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

#if ENABLE(INSPECTOR)

#include "InjectedScript.h"

#include "InjectedScriptHost.h"
#include "InjectedScriptModule.h"
#include "JSMainThreadExecState.h"
#include "Node.h"
#include <bindings/ScriptFunctionCall.h>
#include <bindings/ScriptObject.h>
#include <inspector/InspectorValues.h>
#include <wtf/text/WTFString.h>

using Inspector::TypeBuilder::Array;
using Inspector::TypeBuilder::Debugger::CallFrame;
using Inspector::TypeBuilder::Debugger::FunctionDetails;
using Inspector::TypeBuilder::Runtime::PropertyDescriptor;
using Inspector::TypeBuilder::Runtime::InternalPropertyDescriptor;
using Inspector::TypeBuilder::Runtime::RemoteObject;

using namespace Inspector;

namespace WebCore {

InjectedScript::InjectedScript()
    : InjectedScriptBase("InjectedScript")
{
}

InjectedScript::InjectedScript(Deprecated::ScriptObject injectedScriptObject, InspectedStateAccessCheck accessCheck)
    : InjectedScriptBase("InjectedScript", injectedScriptObject, accessCheck)
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "evaluate", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "callFunctionOn", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, const Deprecated::ScriptValue& callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<RemoteObject>* result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "evaluateOnCallFrame", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>* result)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getFunctionDetails", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(functionId);
    RefPtr<InspectorValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != InspectorValue::TypeObject) {
        if (!resultValue->asString(errorString))
            *errorString = "Internal error";
        return;
    }
    *result = FunctionDetails::runtimeCast(resultValue);
}

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, RefPtr<Array<PropertyDescriptor>>* properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getProperties", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);

    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result || result->type() != InspectorValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    *properties = Array<PropertyDescriptor>::runtimeCast(result);
}

void InjectedScript::getInternalProperties(ErrorString* errorString, const String& objectId, RefPtr<Array<InternalPropertyDescriptor>>* properties)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "getInternalProperties", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);

    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result || result->type() != InspectorValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    RefPtr<Array<InternalPropertyDescriptor>> array = Array<InternalPropertyDescriptor>::runtimeCast(result);
    if (array->length() > 0)
        *properties = array;
}

Node* InjectedScript::nodeForObjectId(const String& objectId)
{
    if (hasNoValue() || !canAccessInspectedWindow())
        return 0;

    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "nodeForObjectId", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);

    bool hadException = false;
    Deprecated::ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);

    return InjectedScriptHost::scriptValueAsNode(resultValue);
}

void InjectedScript::releaseObject(const String& objectId)
{
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "releaseObject", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
PassRefPtr<Array<CallFrame>> InjectedScript::wrapCallFrames(const Deprecated::ScriptValue& callFrames)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "wrapCallFrames", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(callFrames);
    bool hadException = false;
    Deprecated::ScriptValue callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<InspectorValue> result = callFramesValue.toInspectorValue(scriptState());
    if (result->type() == InspectorValue::TypeArray)
        return Array<CallFrame>::runtimeCast(result);
    return Array<CallFrame>::create();
}
#endif

PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(const Deprecated::ScriptValue& value, const String& groupName, bool generatePreview) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapObject");
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(canAccessInspectedWindow());
    wrapFunction.appendArgument(generatePreview);
    bool hadException = false;
    Deprecated::ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return 0;
    RefPtr<InspectorObject> rawResult = r.toInspectorValue(scriptState())->asObject();
    return Inspector::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapTable(const Deprecated::ScriptValue& table, const Deprecated::ScriptValue& columns) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall wrapFunction(injectedScriptObject(), "wrapTable");
    wrapFunction.appendArgument(canAccessInspectedWindow());
    wrapFunction.appendArgument(table);
    if (columns.hasNoValue())
        wrapFunction.appendArgument(false);
    else
        wrapFunction.appendArgument(columns);
    bool hadException = false;
    Deprecated::ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException)
        return 0;
    RefPtr<InspectorObject> rawResult = r.toInspectorValue(scriptState())->asObject();
    return Inspector::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<Inspector::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapNode(Node* node, const String& groupName)
{
    return wrapObject(nodeAsScriptValue(node), groupName);
}

Deprecated::ScriptValue InjectedScript::findObjectById(const String& objectId) const
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "findObjectById", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(objectId);

    bool hadException = false;
    Deprecated::ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    return resultValue;
}

void InjectedScript::inspectNode(Node* node)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall function(injectedScriptObject(), "inspectNode", WebCore::functionCallHandlerFromAnyThread);
    function.appendArgument(nodeAsScriptValue(node));
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    Deprecated::ScriptFunctionCall releaseFunction(injectedScriptObject(), "releaseObjectGroup", WebCore::functionCallHandlerFromAnyThread);
    releaseFunction.appendArgument(objectGroup);
    bool hadException = false;
    callFunctionWithEvalEnabled(releaseFunction, hadException);
    ASSERT(!hadException);
}

Deprecated::ScriptValue InjectedScript::nodeAsScriptValue(Node* node)
{
    return InjectedScriptHost::nodeAsScriptValue(scriptState(), node);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
