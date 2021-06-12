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
#include "PageAuditAgent.h"

#include "InspectorAuditAccessibilityObject.h"
#include "InspectorAuditDOMObject.h"
#include "InspectorAuditResourcesObject.h"
#include "JSInspectorAuditAccessibilityObject.h"
#include "JSInspectorAuditDOMObject.h"
#include "JSInspectorAuditResourcesObject.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "ScriptState.h"
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

PageAuditAgent::PageAuditAgent(PageAgentContext& context)
    : InspectorAuditAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageAuditAgent::~PageAuditAgent() = default;

InjectedScript PageAuditAgent::injectedScriptForEval(std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    if (executionContextId)
        return injectedScriptManager().injectedScriptForId(*executionContextId);

    JSC::JSGlobalObject* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
    return injectedScriptManager().injectedScriptFor(scriptState);
}

InjectedScript PageAuditAgent::injectedScriptForEval(Protocol::ErrorString& errorString, std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    InjectedScript injectedScript = injectedScriptForEval(WTFMove(executionContextId));
    if (injectedScript.hasNoValue()) {
        if (executionContextId)
            errorString = "Missing injected script for given executionContextId"_s;
        else
            errorString = "Internal error: main world execution context not found"_s;
    }
    return injectedScript;
}

void PageAuditAgent::populateAuditObject(JSC::JSGlobalObject* lexicalGlobalObject, JSC::Strong<JSC::JSObject>& auditObject)
{
    InspectorAuditAgent::populateAuditObject(lexicalGlobalObject, auditObject);

    ASSERT(lexicalGlobalObject);
    if (!lexicalGlobalObject)
        return;

    if (auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(lexicalGlobalObject)) {
        JSC::VM& vm = globalObject->vm();
        JSC::JSLockHolder lock(vm);

        if (JSC::JSValue jsInspectorAuditAccessibilityObject = toJSNewlyCreated(lexicalGlobalObject, globalObject, InspectorAuditAccessibilityObject::create(*this)))
            auditObject->putDirect(vm, JSC::Identifier::fromString(vm, "Accessibility"), jsInspectorAuditAccessibilityObject);

        if (JSC::JSValue jsInspectorAuditDOMObject = toJSNewlyCreated(lexicalGlobalObject, globalObject, InspectorAuditDOMObject::create(*this)))
            auditObject->putDirect(vm, JSC::Identifier::fromString(vm, "DOM"), jsInspectorAuditDOMObject);

        if (JSC::JSValue jsInspectorAuditResourcesObject = toJSNewlyCreated(lexicalGlobalObject, globalObject, InspectorAuditResourcesObject::create(*this)))
            auditObject->putDirect(vm, JSC::Identifier::fromString(vm, "Resources"), jsInspectorAuditResourcesObject);
    }
}

void PageAuditAgent::muteConsole()
{
    InspectorAuditAgent::muteConsole();
    PageConsoleClient::mute();
}

void PageAuditAgent::unmuteConsole()
{
    PageConsoleClient::unmute();
    InspectorAuditAgent::unmuteConsole();
}

} // namespace WebCore
