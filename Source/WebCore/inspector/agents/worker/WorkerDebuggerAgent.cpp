/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WorkerDebuggerAgent.h"

#include "JSDOMGlobalObject.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerOrWorkletScriptController.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {

using namespace JSC;
using namespace Inspector;

WorkerDebuggerAgent::WorkerDebuggerAgent(WorkerAgentContext& context)
    : WebDebuggerAgent(context)
    , m_globalScope(context.globalScope)
{
    ASSERT(context.globalScope.isContextThread());
}

WorkerDebuggerAgent::~WorkerDebuggerAgent() = default;

void WorkerDebuggerAgent::breakpointActionLog(JSGlobalObject* lexicalGlobalObject, const String& message)
{
    m_globalScope.addConsoleMessage(makeUnique<ConsoleMessage>(MessageSource::JS, MessageType::Log, MessageLevel::Log, message, createScriptCallStack(lexicalGlobalObject)));
}

InjectedScript WorkerDebuggerAgent::injectedScriptForEval(Inspector::Protocol::ErrorString& errorString, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    if (executionContextId) {
        errorString = "executionContextId is not supported for workers as there is only one execution context"_s;
        return InjectedScript();
    }

    // FIXME: What guarantees m_globalScope.script() is non-null?
    // FIXME: What guarantees globalScopeWrapper() is non-null?
    return injectedScriptManager().injectedScriptFor(m_globalScope.script()->globalScopeWrapper());
}

} // namespace WebCore
