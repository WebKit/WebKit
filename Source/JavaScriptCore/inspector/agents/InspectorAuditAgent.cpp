/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorAuditAgent);

InspectorAuditAgent::InspectorAuditAgent(AgentContext& context)
    : InspectorAgentBase("Audit"_s)
    , m_backendDispatcher(AuditBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_debugger(*context.environment.debugger())
{
}

InspectorAuditAgent::~InspectorAuditAgent() = default;

void InspectorAuditAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorAuditAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
}

Protocol::ErrorStringOr<void> InspectorAuditAgent::setup(std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    Protocol::ErrorString errorString;

    if (hasActiveAudit())
        return makeUnexpected("Must call teardown before calling setup again"_s);

    InjectedScript injectedScript = injectedScriptForEval(errorString, WTFMove(executionContextId));
    if (injectedScript.hasNoValue())
        return makeUnexpected(errorString);

    JSC::JSGlobalObject* globalObject = injectedScript.globalObject();
    if (!globalObject)
        return makeUnexpected("Missing execution state of injected script for given executionContextId"_s);

    VM& vm = globalObject->vm();

    JSC::JSLockHolder lock(globalObject);

    m_injectedWebInspectorAuditValue.set(vm, constructEmptyObject(globalObject));
    if (!m_injectedWebInspectorAuditValue)
        return makeUnexpected("Unable to construct injected WebInspectorAudit object."_s);

    populateAuditObject(globalObject, m_injectedWebInspectorAuditValue);

    return { };
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */>> InspectorAuditAgent::run(const String& test, std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = injectedScriptForEval(errorString, WTFMove(executionContextId));
    if (injectedScript.hasNoValue())
        return makeUnexpected(errorString);

    auto functionString = makeString("(function(WebInspectorAudit) { \"use strict\"; return eval(`("_s, makeStringByReplacingAll(test, '`', "\\`"_s), ")`)(WebInspectorAudit); })"_s);

    InjectedScript::ExecuteOptions options;
    options.objectGroup = "audit"_s;
    if (m_injectedWebInspectorAuditValue)
        options.args = { m_injectedWebInspectorAuditValue.get() };

    RefPtr<Protocol::Runtime::RemoteObject> result;
    std::optional<bool> wasThrown;
    std::optional<int> savedResultIndex;

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);
    temporarilyDisableExceptionBreakpoints.replace();

    muteConsole();

    injectedScript.execute(errorString, functionString, WTFMove(options), result, wasThrown, savedResultIndex);

    unmuteConsole();

    if (!result)
        return makeUnexpected(errorString);

    return { { result.releaseNonNull(), WTFMove(wasThrown) } };
}

Protocol::ErrorStringOr<void> InspectorAuditAgent::teardown()
{
    if (!hasActiveAudit())
        return makeUnexpected("Must call setup before calling teardown"_s);

    m_injectedWebInspectorAuditValue.clear();

    return { };
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

    auditObject->putDirect(vm, JSC::Identifier::fromString(vm, "Version"_s), JSC::JSValue(Protocol::Audit::VERSION));
}

} // namespace Inspector
