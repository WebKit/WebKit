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
#include "JSGlobalObjectAuditAgent.h"

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

using namespace JSC;

JSGlobalObjectAuditAgent::JSGlobalObjectAuditAgent(JSAgentContext& context)
    : InspectorAuditAgent(context)
    , m_globalObject(context.inspectedGlobalObject)
{
}

JSGlobalObjectAuditAgent::~JSGlobalObjectAuditAgent() = default;

InjectedScript JSGlobalObjectAuditAgent::injectedScriptForEval(ErrorString& errorString, const int* executionContextId)
{
    if (executionContextId) {
        errorString = "executionContextId is not supported for JSContexts as there is only one execution context"_s;
        return InjectedScript();
    }

    InjectedScript injectedScript = injectedScriptManager().injectedScriptFor(&m_globalObject);
    if (injectedScript.hasNoValue())
        errorString = "Internal error: main world execution context not found"_s;

    return injectedScript;
}

} // namespace Inspector
