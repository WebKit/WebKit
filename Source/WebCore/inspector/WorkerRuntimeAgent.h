/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WorkerRuntimeAgent_h
#define WorkerRuntimeAgent_h

#if ENABLE(INSPECTOR)

#include <inspector/agents/InspectorRuntimeAgent.h>

namespace WebCore {

class WorkerGlobalScope;
typedef String ErrorString;

class WorkerRuntimeAgent final : public Inspector::InspectorRuntimeAgent {
public:
    WorkerRuntimeAgent(Inspector::InjectedScriptManager*, WorkerGlobalScope*);
    virtual ~WorkerRuntimeAgent() { }

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    virtual void run(ErrorString*) override;

    void pauseWorkerGlobalScope(WorkerGlobalScope*);

private:
    virtual JSC::VM* globalVM() override;
    virtual Inspector::InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) override;
    virtual void muteConsole() override;
    virtual void unmuteConsole() override;
    WorkerGlobalScope* m_workerGlobalScope;
    RefPtr<Inspector::InspectorRuntimeBackendDispatcher> m_backendDispatcher;
    bool m_paused;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(InspectorPagerAgent_h)
