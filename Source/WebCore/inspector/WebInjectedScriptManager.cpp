/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInjectedScriptManager.h"

#include "CommandLineAPIModule.h"
#include "ScriptState.h"


namespace WebCore {

using namespace Inspector;

WebInjectedScriptManager::WebInjectedScriptManager(InspectorEnvironment& environment, Ref<InjectedScriptHost>&& host)
    : InjectedScriptManager(environment, WTFMove(host))
{
}

void WebInjectedScriptManager::connect()
{
    InjectedScriptManager::connect();

    m_commandLineAPIHost = CommandLineAPIHost::create();
}

void WebInjectedScriptManager::disconnect()
{
    InjectedScriptManager::disconnect();

    if (m_commandLineAPIHost) {
        m_commandLineAPIHost->disconnect();
        m_commandLineAPIHost = nullptr;
    }
}

void WebInjectedScriptManager::discardInjectedScripts()
{
    InjectedScriptManager::discardInjectedScripts();

    if (m_commandLineAPIHost)
        m_commandLineAPIHost->clearAllWrappers();
}

void WebInjectedScriptManager::didCreateInjectedScript(const Inspector::InjectedScript& injectedScript)
{
    CommandLineAPIModule::injectIfNeeded(this, injectedScript);
}

void WebInjectedScriptManager::discardInjectedScriptsFor(DOMWindow* window)
{
    if (m_scriptStateToId.isEmpty())
        return;

    Vector<long> idsToRemove;
    for (const auto& it : m_idToInjectedScript) {
        JSC::ExecState* scriptState = it.value.scriptState();
        if (window != domWindowFromExecState(scriptState))
            continue;
        m_scriptStateToId.remove(scriptState);
        idsToRemove.append(it.key);
    }

    for (auto& id : idsToRemove)
        m_idToInjectedScript.remove(id);

    // Now remove script states that have id but no injected script.
    Vector<JSC::ExecState*> scriptStatesToRemove;
    for (const auto& it : m_scriptStateToId) {
        JSC::ExecState* scriptState = it.key;
        if (window == domWindowFromExecState(scriptState))
            scriptStatesToRemove.append(scriptState);
    }

    for (auto& scriptState : scriptStatesToRemove)
        m_scriptStateToId.remove(scriptState);
}

} // namespace WebCore
