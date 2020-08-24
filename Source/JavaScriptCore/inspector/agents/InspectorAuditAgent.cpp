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

#include "Debugger.h"
#include "InjectedScript.h"
#include "JSLock.h"
#include "ObjectConstructor.h"
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
    , m_debugger(context.environment.debugger())
{
}

InspectorAuditAgent::~InspectorAuditAgent() = default;

void InspectorAuditAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorAuditAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
}

void InspectorAuditAgent::setup(ErrorString& errorString, const int* executionContextId)
{
    if (hasActiveAudit()) {
        errorString = "Must call teardown before calling setup again"_s;
        return;
    }

    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue())
        return;

    JSC::JSGlobalObject* globalObject = injectedScript.globalObject();
    if (!globalObject) {
        errorString = "Missing execution state of injected script for given executionContextId"_s;
        return;
    }

    VM& vm = globalObject->vm();

    JSC::JSLockHolder lock(globalObject);

    m_injectedWebInspectorAuditValue.set(vm, constructEmptyObject(globalObject));
    if (!m_injectedWebInspectorAuditValue) {
        errorString = "Unable to construct injected WebInspectorAudit object."_s;
        return;
    }

    populateAuditObject(globalObject, m_injectedWebInspectorAuditValue);
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

    auto previousPauseOnExceptionsState = m_debugger.pauseOnExceptionsState();

    m_debugger.setPauseOnExceptionsState(Debugger::DontPauseOnExceptions);
    muteConsole();

    injectedScript.execute(errorString, functionString.toString(), WTFMove(options), result, wasThrown, savedResultIndex);

    unmuteConsole();
    m_debugger.setPauseOnExceptionsState(previousPauseOnExceptionsState);
}

void InspectorAuditAgent::teardown(ErrorString& errorString)
{
    if (!hasActiveAudit()) {
        errorString = "Must call setup before calling teardown"_s;
        return;
    }

    m_injectedWebInspectorAuditValue.clear();
}

bool InspectorAuditAgent::hasActiveAudit() const
{
    return !!m_injectedWebInspectorAuditValue;
}

void InspectorAuditAgent::populateAuditObject(JSC::JSGlobalObject* globalObject, JSC::Strong<JSC::JSObject>& auditObject)
{
    ASSERT(globalObject);
    if (!globalObject)
        return;

    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auditObject->putDirect(vm, JSC::Identifier::fromString(vm, "Version"), JSC::JSValue(Inspector::Protocol::Audit::VERSION));
}

} // namespace Inspector
