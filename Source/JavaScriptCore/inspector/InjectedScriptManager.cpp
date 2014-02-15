/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InjectedScriptManager.h"

#if ENABLE(INSPECTOR)

#include "Completion.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptSource.h"
#include "InspectorValues.h"
#include "JSInjectedScriptHost.h"
#include "JSLock.h"
#include "ScriptObject.h"
#include "SourceCode.h"

using namespace JSC;

namespace Inspector {

InjectedScriptManager::InjectedScriptManager(InspectorEnvironment& environment, PassRefPtr<InjectedScriptHost> injectedScriptHost)
    : m_environment(environment)
    , m_injectedScriptHost(injectedScriptHost)
    , m_nextInjectedScriptId(1)
{
}

InjectedScriptManager::~InjectedScriptManager()
{
}

void InjectedScriptManager::disconnect()
{
    discardInjectedScripts();
}

InjectedScriptHost* InjectedScriptManager::injectedScriptHost()
{
    return m_injectedScriptHost.get();
}

InjectedScript InjectedScriptManager::injectedScriptForId(int id)
{
    auto it = m_idToInjectedScript.find(id);
    if (it != m_idToInjectedScript.end())
        return it->value;

    for (auto it = m_scriptStateToId.begin(); it != m_scriptStateToId.end(); ++it) {
        if (it->value == id)
            return injectedScriptFor(it->key);
    }

    return InjectedScript();
}

int InjectedScriptManager::injectedScriptIdFor(ExecState* scriptState)
{
    auto it = m_scriptStateToId.find(scriptState);
    if (it != m_scriptStateToId.end())
        return it->value;

    int id = m_nextInjectedScriptId++;
    m_scriptStateToId.set(scriptState, id);
    return id;
}

InjectedScript InjectedScriptManager::injectedScriptForObjectId(const String& objectId)
{
    RefPtr<InspectorValue> parsedObjectId = InspectorValue::parseJSON(objectId);
    if (parsedObjectId && parsedObjectId->type() == InspectorValue::TypeObject) {
        long injectedScriptId = 0;
        bool success = parsedObjectId->asObject()->getNumber(ASCIILiteral("injectedScriptId"), &injectedScriptId);
        if (success)
            return m_idToInjectedScript.get(injectedScriptId);
    }

    return InjectedScript();
}

void InjectedScriptManager::discardInjectedScripts()
{
    m_injectedScriptHost->clearAllWrappers();
    m_idToInjectedScript.clear();
    m_scriptStateToId.clear();
}

void InjectedScriptManager::releaseObjectGroup(const String& objectGroup)
{
    for (auto it = m_idToInjectedScript.begin(); it != m_idToInjectedScript.end(); ++it)
        it->value.releaseObjectGroup(objectGroup);
}

String InjectedScriptManager::injectedScriptSource()
{
    return String(reinterpret_cast<const char*>(InjectedScriptSource_js), sizeof(InjectedScriptSource_js));
}

Deprecated::ScriptObject InjectedScriptManager::createInjectedScript(const String& source, ExecState* scriptState, int id)
{
    JSLockHolder lock(scriptState);

    SourceCode sourceCode = makeSource(source);
    JSGlobalObject* globalObject = scriptState->lexicalGlobalObject();
    JSValue globalThisValue = scriptState->globalThisValue();

    JSValue evaluationException;
    InspectorEvaluateHandler evaluateHandler = m_environment.evaluateHandler();
    JSValue functionValue = evaluateHandler(scriptState, sourceCode, globalThisValue, &evaluationException);
    if (evaluationException)
        return Deprecated::ScriptObject();

    CallData callData;
    CallType callType = getCallData(functionValue, callData);
    if (callType == CallTypeNone)
        return Deprecated::ScriptObject();

    MarkedArgumentBuffer args;
    args.append(m_injectedScriptHost->jsWrapper(scriptState, globalObject));
    args.append(globalThisValue);
    args.append(jsNumber(id));

    JSValue result = JSC::call(scriptState, functionValue, callType, callData, globalThisValue, args);
    if (result.isObject())
        return Deprecated::ScriptObject(scriptState, result.getObject());

    return Deprecated::ScriptObject();
}

InjectedScript InjectedScriptManager::injectedScriptFor(ExecState* inspectedExecState)
{
    auto it = m_scriptStateToId.find(inspectedExecState);
    if (it != m_scriptStateToId.end()) {
        auto it1 = m_idToInjectedScript.find(it->value);
        if (it1 != m_idToInjectedScript.end())
            return it1->value;
    }

    if (!m_environment.canAccessInspectedScriptState(inspectedExecState))
        return InjectedScript();

    int id = injectedScriptIdFor(inspectedExecState);
    Deprecated::ScriptObject injectedScriptObject = createInjectedScript(injectedScriptSource(), inspectedExecState, id);
    InjectedScript result(injectedScriptObject, &m_environment);
    m_idToInjectedScript.set(id, result);
    didCreateInjectedScript(result);
    return result;
}

void InjectedScriptManager::didCreateInjectedScript(InjectedScript)
{
    // Intentionally empty. This allows for subclasses to inject additional scripts.
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
