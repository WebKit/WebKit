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

#pragma once

#include "WebDebuggerAgent.h"

namespace WebCore {

class WorkerOrWorkletGlobalScope;

class WorkerDebuggerAgent final : public WebDebuggerAgent {
    WTF_MAKE_NONCOPYABLE(WorkerDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WorkerDebuggerAgent(WorkerAgentContext&);
    ~WorkerDebuggerAgent();

    // JSC::Debugger::Observer
    void breakpointActionLog(JSC::JSGlobalObject*, const String& data);

private:
    // We don't need to mute console for workers.
    void muteConsole() { }
    void unmuteConsole() { }

    Inspector::InjectedScript injectedScriptForEval(Inspector::Protocol::ErrorString&, std::optional<Inspector::Protocol::Runtime::ExecutionContextId>&&);

    WorkerOrWorkletGlobalScope& m_globalScope;
};

} // namespace WebCore
