/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "JSGlobalObjectRuntimeAgent.h"

#if ENABLE(INSPECTOR)

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "JSGlobalObject.h"

using namespace JSC;

namespace Inspector {

JSGlobalObjectRuntimeAgent::JSGlobalObjectRuntimeAgent(InjectedScriptManager* injectedScriptManager, JSGlobalObject& globalObject)
    : InspectorRuntimeAgent(injectedScriptManager)
    , m_globalObject(globalObject)
{
}

void JSGlobalObjectRuntimeAgent::didCreateFrontendAndBackend(InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorRuntimeFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorRuntimeBackendDispatcher::create(backendDispatcher, this);
}

void JSGlobalObjectRuntimeAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();
}

VM& JSGlobalObjectRuntimeAgent::globalVM()
{
    return m_globalObject.vm();
}

InjectedScript JSGlobalObjectRuntimeAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    ASSERT_UNUSED(executionContextId, !executionContextId);

    JSC::ExecState* scriptState = m_globalObject.globalExec();
    InjectedScript injectedScript = injectedScriptManager()->injectedScriptFor(scriptState);
    if (injectedScript.hasNoValue())
        *errorString = ASCIILiteral("Internal error: main world execution context not found.");

    return injectedScript;
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
