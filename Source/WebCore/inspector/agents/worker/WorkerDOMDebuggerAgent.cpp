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
#include "WorkerDOMDebuggerAgent.h"

namespace WebCore {

using namespace Inspector;

WorkerDOMDebuggerAgent::WorkerDOMDebuggerAgent(WorkerAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorDOMDebuggerAgent(context, debuggerAgent)
{
}

WorkerDOMDebuggerAgent::~WorkerDOMDebuggerAgent() = default;

void WorkerDOMDebuggerAgent::setDOMBreakpoint(ErrorString& errorString, int /* nodeId */, const String& /* typeString */, const JSON::Object* /* optionsPayload */)
{
    errorString = "Not supported"_s;
}

void WorkerDOMDebuggerAgent::removeDOMBreakpoint(ErrorString& errorString, int /* nodeId */, const String& /* typeString */)
{
    errorString = "Not supported"_s;
}

void WorkerDOMDebuggerAgent::setAnimationFrameBreakpoint(ErrorString& errorString, RefPtr<JSC::Breakpoint>&&)
{
    errorString = "Not supported"_s;
}

} // namespace WebCore
