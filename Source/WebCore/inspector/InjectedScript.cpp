/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "Frame.h"
#include "InjectedScriptHost.h"
#include "InspectorValues.h"
#include "Node.h"
#include "PlatformString.h"
#include "ScriptFunctionCall.h"

namespace WebCore {

InjectedScript::InjectedScript(ScriptObject injectedScriptObject)
    : m_injectedScriptObject(injectedScriptObject)
{
}

void InjectedScript::evaluate(const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    makeCall(function, result);
}

void InjectedScript::evaluateOn(PassRefPtr<InspectorObject> objectId, const String& expression, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "evaluateOn");
    function.appendArgument(objectId->toJSONString());
    function.appendArgument(expression);
    makeCall(function, result);
}

void InjectedScript::evaluateOnCallFrame(PassRefPtr<InspectorObject> callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "evaluateOnCallFrame");
    function.appendArgument(callFrameId->toJSONString());
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    makeCall(function, result);
}

void InjectedScript::getProperties(PassRefPtr<InspectorObject> objectId, bool ignoreHasOwnProperty, bool abbreviate, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "getProperties");
    String objectIdString = objectId->toJSONString();
    function.appendArgument(objectIdString);
    function.appendArgument(ignoreHasOwnProperty);
    function.appendArgument(abbreviate);
    makeCall(function, result);
}

Node* InjectedScript::nodeForObjectId(PassRefPtr<InspectorObject> objectId)
{
    if (hasNoValue() || !canAccessInspectedWindow())
        return 0;

    ScriptFunctionCall function(m_injectedScriptObject, "nodeForObjectId");
    function.appendArgument(objectId->toJSONString());

    bool hadException = false;
    ScriptValue resultValue = function.call(hadException);
    ASSERT(!hadException);

    return InjectedScriptHost::scriptValueAsNode(resultValue);
}

void InjectedScript::resolveNode(long nodeId, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "resolveNode");
    function.appendArgument(nodeId);
    makeCall(function, result);
}

void InjectedScript::setPropertyValue(PassRefPtr<InspectorObject> objectId, const String& propertyName, const String& expression, RefPtr<InspectorValue>* result)
{
    ScriptFunctionCall function(m_injectedScriptObject, "setPropertyValue");
    function.appendArgument(objectId->toJSONString());
    function.appendArgument(propertyName);
    function.appendArgument(expression);
    makeCall(function, result);
}

void InjectedScript::releaseObject(PassRefPtr<InspectorObject> objectId)
{
    ScriptFunctionCall function(m_injectedScriptObject, "releaseObject");
    function.appendArgument(objectId->toJSONString());
    RefPtr<InspectorValue> result;
    makeCall(function, &result);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
PassRefPtr<InspectorValue> InjectedScript::callFrames()
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall function(m_injectedScriptObject, "callFrames");
    ScriptValue callFramesValue = function.call();
    return callFramesValue.toInspectorValue(m_injectedScriptObject.scriptState());
}
#endif

PassRefPtr<InspectorObject> InjectedScript::wrapObject(ScriptValue value, const String& groupName)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall wrapFunction(m_injectedScriptObject, "wrapObject");
    wrapFunction.appendArgument(value);
    wrapFunction.appendArgument(groupName);
    wrapFunction.appendArgument(canAccessInspectedWindow());
    bool hadException = false;
    ScriptValue r = wrapFunction.call(hadException);
    if (hadException) {
        RefPtr<InspectorObject> result = InspectorObject::create();
        result->setString("description", "<exception>");
        return result;
    }
    return r.toInspectorValue(m_injectedScriptObject.scriptState())->asObject();
}

PassRefPtr<InspectorObject> InjectedScript::wrapNode(Node* node, const String& groupName)
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

void InjectedScript::releaseWrapperObjectGroup(const String& objectGroup)
{
    ASSERT(!hasNoValue());
    ScriptFunctionCall releaseFunction(m_injectedScriptObject, "releaseWrapperObjectGroup");
    releaseFunction.appendArgument(objectGroup);
    releaseFunction.call();
}

bool InjectedScript::canAccessInspectedWindow()
{
    return InjectedScriptHost::canAccessInspectedWindow(m_injectedScriptObject.scriptState());
}

void InjectedScript::makeCall(ScriptFunctionCall& function, RefPtr<InspectorValue>* result)
{
    if (hasNoValue() || !canAccessInspectedWindow()) {
        *result = InspectorValue::null();
        return;
    }

    bool hadException = false;
    ScriptValue resultValue = function.call(hadException);

    ASSERT(!hadException);
    if (!hadException)
        *result = resultValue.toInspectorValue(m_injectedScriptObject.scriptState());
    else
        *result = InspectorValue::null();
}

ScriptValue InjectedScript::nodeAsScriptValue(Node* node)
{
    return InjectedScriptHost::nodeAsScriptValue(m_injectedScriptObject.scriptState(), node);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
