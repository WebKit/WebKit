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
#include "JSInspectorAuditAccessibilityObject.h"
#include "JSInspectorAuditDOMObject.h"
#include "Page.h"
#include "PageConsoleClient.h"
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

InjectedScript PageAuditAgent::injectedScriptForEval(const int* executionContextId)
{
    if (executionContextId)
        return injectedScriptManager().injectedScriptForId(*executionContextId);

    JSC::ExecState* scriptState = mainWorldExecState(&m_inspectedPage.mainFrame());
    return injectedScriptManager().injectedScriptFor(scriptState);
}

InjectedScript PageAuditAgent::injectedScriptForEval(ErrorString& errorString, const int* executionContextId)
{
    InjectedScript injectedScript = injectedScriptForEval(executionContextId);
    if (injectedScript.hasNoValue()) {
        if (executionContextId)
            errorString = "Execution context with given id not found."_s;
        else
            errorString = "Internal error: main world execution context not found."_s;
    }
    return injectedScript;
}

void PageAuditAgent::populateAuditObject(JSC::ExecState* execState, JSC::Strong<JSC::JSObject>& auditObject)
{
    InspectorAuditAgent::populateAuditObject(execState, auditObject);

    ASSERT(execState);
    if (!execState)
        return;

    if (auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(execState->lexicalGlobalObject())) {
        JSC::JSLockHolder lock(execState);

        if (JSC::JSValue jsInspectorAuditAccessibilityObject = toJSNewlyCreated(execState, globalObject, InspectorAuditAccessibilityObject::create(*this))) \
            auditObject->putDirect(execState->vm(), JSC::Identifier::fromString(execState, "Accessibility"), jsInspectorAuditAccessibilityObject);

        if (JSC::JSValue jsInspectorAuditDOMObject = toJSNewlyCreated(execState, globalObject, InspectorAuditDOMObject::create(*this))) \
            auditObject->putDirect(execState->vm(), JSC::Identifier::fromString(execState, "DOM"), jsInspectorAuditDOMObject);
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
