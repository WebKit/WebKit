/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InspectorAuditAgent.h"

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "JSLock.h"
#include "ObjectConstructor.h"
#include "ScriptDebugServer.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

using namespace JSC;

InspectorAuditAgent::InspectorAuditAgent(AgentContext& context)
    : InspectorAgentBase("Audit"_s)
    , m_backendDispatcher(AuditBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_scriptDebugServer(context.environment.scriptDebugServer())
{
}

void InspectorAuditAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorAuditAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
}

void InspectorAuditAgent::setup(ErrorString& errorString, const int* executionContextId)
{
    if (hasActiveAudit()) {
        errorString = "Must call teardown before calling setup again."_s;
        return;
    }

    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue())
        return;

    JSC::ExecState* execState = injectedScript.scriptState();
    if (!execState) {
        errorString = "Missing execution state for injected script."_s;
        return;
    }

    VM& vm = execState->vm();

    JSC::JSLockHolder lock(execState);

    m_injectedWebInspectorAuditValue.set(vm, constructEmptyObject(execState));
    if (!m_injectedWebInspectorAuditValue) {
        errorString = "Unable to construct injected WebInspectorAudit object."_s;
        return;
    }

    populateAuditObject(execState, m_injectedWebInspectorAuditValue);
}

void InspectorAuditAgent::run(ErrorString& errorString, const String& test, const int* executionContextId, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue())
        return;

    StringBuilder functionString;
    functionString.appendLiteral("(function(WebInspectorAudit) { \"use strict\"; return eval(`(");
    functionString.append(test.isolatedCopy().replace('`', "\\`"));
    functionString.appendLiteral(")`)(WebInspectorAudit); })");

    InjectedScript::ExecuteOptions options;
    options.objectGroup = "audit"_s;
    if (m_injectedWebInspectorAuditValue)
        options.args = { m_injectedWebInspectorAuditValue.get() };

    Optional<int> savedResultIndex;

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = m_scriptDebugServer.pauseOnExceptionsState();

    m_scriptDebugServer.setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
    muteConsole();

    injectedScript.execute(errorString, functionString.toString(), WTFMove(options), result, wasThrown, savedResultIndex);

    unmuteConsole();
    m_scriptDebugServer.setPauseOnExceptionsState(previousPauseOnExceptionsState);
}

void InspectorAuditAgent::teardown(ErrorString& errorString)
{
    if (!hasActiveAudit()) {
        errorString = "Must call setup before calling teardown."_s;
        return;
    }

    m_injectedWebInspectorAuditValue.clear();
}

bool InspectorAuditAgent::hasActiveAudit() const
{
    return !!m_injectedWebInspectorAuditValue;
}

} // namespace Inspector
