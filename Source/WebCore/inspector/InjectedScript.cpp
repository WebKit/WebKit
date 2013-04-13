/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#include "DOMWindow.h"
#include "Frame.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "InspectorInstrumentation.h"
#include "InspectorValues.h"
#include "Node.h"
#include "PlatformString.h"
#include "ScriptFunctionCall.h"

using WebCore::TypeBuilder::Array;
using WebCore::TypeBuilder::Debugger::CallFrame;
using WebCore::TypeBuilder::Runtime::PropertyDescriptor;
using WebCore::TypeBuilder::Debugger::FunctionDetails;
using WebCore::TypeBuilder::Runtime::RemoteObject;

namespace WebCore {

InjectedScript::InjectedScript()
    : m_inspectedStateAccessCheck(0)
{
}

InjectedScript::InjectedScript(ScriptObject injectedScriptObject, InspectedStateAccessCheck accessCheck)
    : m_injectedScriptObject(injectedScriptObject)
    , m_inspectedStateAccessCheck(accessCheck)
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(m_injectedScriptObject, "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(m_injectedScriptObject, "callFunctionOn");
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, const ScriptValue& callFrames, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, RefPtr<RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptFunctionCall function(m_injectedScriptObject, "evaluateOnCallFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "getFunctionDetails");
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

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, RefPtr<Array<PropertyDescriptor> >* properties)
{
    ScriptFunctionCall function(m_injectedScriptObject, "getProperties");
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

Node* InjectedScript::nodeForObjectId(const String& objectId)
{
    if (hasNoValue() || !canAccessInspectedWindow())
        return 0;

    ScriptFunctionCall function(m_injectedScriptObject, "nodeForObjectId");
    function.appendArgument(objectId);

    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);

    return InjectedScriptHost::scriptValueAsNode(resultValue);
}

void InjectedScript::releaseObject(const String& objectId)
{
    ScriptFunctionCall function(m_injectedScriptObject, "releaseObject");
    function.appendArgument(objectId);
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
PassRefPtr<Array<CallFrame> > InjectedScript::wrapCallFrames(const ScriptValue& callFrames)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(m_injectedScriptObject, "wrapCallFrames");
    function.appendArgument(callFrames);
    bool hadException = false;
    ScriptValue callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<InspectorValue> result = callFramesValue.toInspectorValue(m_injectedScriptObject.scriptState());
    if (result->type() == InspectorValue::TypeArray)
        return Array<CallFrame>::runtimeCast(result);
    return Array<CallFrame>::create();
}
#endif

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(ScriptValue value, const String& groupName) const
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall wrapFunction(m_injectedScriptObject, "wrapObject");
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(canAccessInspectedWindow());
    bool hadException = false;
    ScriptValue r = callFunctionWithEvalEnabled(wrapFunction, hadException);
    if (hadException) {
        return 0;
    }
    RefPtr<InspectorObject> rawResult = r.toInspectorValue(m_injectedScriptObject.scriptState())->asObject();
    return TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapNode(Node* node, const String& groupName)
{
    return wrapObject(nodeAsScriptValue(node), groupName);
}

void InjectedScript::inspectNode(Node* node)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(m_injectedScriptObject, "inspectNode");
    function.appendArgument(nodeAsScriptValue(node));
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall releaseFunction(m_injectedScriptObject, "releaseObjectGroup");
    releaseFunction.appendArgument(objectGroup);
    bool hadException = false;
    callFunctionWithEvalEnabled(releaseFunction, hadException);
    ASSERT(!hadException);
}

bool InjectedScript::canAccessInspectedWindow() const
{
    return m_inspectedStateAccessCheck(m_injectedScriptObject.scriptState());
}

ScriptValue InjectedScript::callFunctionWithEvalEnabled(ScriptFunctionCall& function, bool& hadException) const
{
    ScriptExecutionContext* scriptExecutionContext = scriptExecutionContextFromScriptState(m_injectedScriptObject.scriptState());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willCallFunction(scriptExecutionContext, "InjectedScript", 1);

    ScriptState* scriptState = m_injectedScriptObject.scriptState();
    bool evalIsDisabled = false;
    if (scriptState) {
        evalIsDisabled = !evalEnabled(scriptState);
        // Temporarily enable allow evals for inspector.
        if (evalIsDisabled)
            setEvalEnabled(scriptState, true);
    }

    ScriptValue resultValue = function.call(hadException);

    if (evalIsDisabled)
        setEvalEnabled(scriptState, false);

    InspectorInstrumentation::didCallFunction(cookie);
    return resultValue;
}

void InjectedScript::makeCall(ScriptFunctionCall& function, RefPtr<InspectorValue>* result)
{
    if (hasNoValue() || !canAccessInspectedWindow()) {
        *result = InspectorValue::null();
        return;
    }

    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (!hadException) {
        *result = resultValue.toInspectorValue(m_injectedScriptObject.scriptState());
        if (!*result)
            *result = InspectorString::create(String::format("Object has too long reference chain(must not be longer than %d)", InspectorValue::maxDepth));
    } else
        *result = InspectorString::create("Exception while making a call.");
}

void InjectedScript::makeEvalCall(ErrorString* errorString, ScriptFunctionCall& function, RefPtr<TypeBuilder::Runtime::RemoteObject>* objectResult, TypeBuilder::OptOutput<bool>* wasThrown)
{
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
    if (!result) {
        *errorString = "Internal error: result value is empty";
        return;
    }
    if (result->type() == InspectorValue::TypeString) {
        result->asString(errorString);
        return;
    }
    RefPtr<InspectorObject> resultPair = result->asObject();
    if (!resultPair) {
        *errorString = "Internal error: result is not an Object";
        return;
    }
    RefPtr<InspectorObject> resultObj = resultPair->getObject("result");
    bool wasThrownVal = false;
    if (!resultObj || !resultPair->getBoolean("wasThrown", &wasThrownVal)) {
        *errorString = "Internal error: result is not a pair of value and wasThrown flag";
        return;
    }
    *objectResult = TypeBuilder::Runtime::RemoteObject::runtimeCast(resultObj);
    *wasThrown = wasThrownVal;
}

ScriptValue InjectedScript::nodeAsScriptValue(Node* node)
{
    return InjectedScriptHost::nodeAsScriptValue(m_injectedScriptObject.scriptState(), node);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
