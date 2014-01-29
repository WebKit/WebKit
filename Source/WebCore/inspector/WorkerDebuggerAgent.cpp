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

#include "config.h"
#include "WorkerDebuggerAgent.h"

#if ENABLE(INSPECTOR)

#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/ScriptDebugServer.h>
#include <mutex>
#include <wtf/MessageQueue.h>
#include <wtf/NeverDestroyed.h>

using namespace Inspector;

namespace WebCore {

namespace {

std::mutex& workerDebuggerAgentsMutex()
{
    static std::once_flag onceFlag;
    static std::mutex* mutex;

    std::call_once(onceFlag, []{
        mutex = std::make_unique<std::mutex>().release();
    });

    return *mutex;
}

typedef HashMap<WorkerThread*, WorkerDebuggerAgent*> WorkerDebuggerAgents;

WorkerDebuggerAgents& workerDebuggerAgents()
{
    static NeverDestroyed<WorkerDebuggerAgents> agents;

    return agents;
}

class RunInspectorCommandsTask : public ScriptDebugServer::Task {
public:
    RunInspectorCommandsTask(WorkerThread* thread, WorkerGlobalScope* workerGlobalScope)
        : m_thread(thread)
        , m_workerGlobalScope(workerGlobalScope) { }
    virtual ~RunInspectorCommandsTask() { }
    virtual void run() override
    {
        // Process all queued debugger commands. It is safe to use m_workerGlobalScope here
        // because it is alive if RunWorkerLoop is not terminated, otherwise it will
        // just be ignored. WorkerThread is certainly alive if this task is being executed.
        while (MessageQueueMessageReceived == m_thread->runLoop().runInMode(m_workerGlobalScope, WorkerDebuggerAgent::debuggerTaskMode, WorkerRunLoop::DontWaitForMessage)) { }
    }

private:
    WorkerThread* m_thread;
    WorkerGlobalScope* m_workerGlobalScope;
};

} // namespace

const char* WorkerDebuggerAgent::debuggerTaskMode = "debugger";

WorkerDebuggerAgent::WorkerDebuggerAgent(InjectedScriptManager* injectedScriptManager, InstrumentingAgents* instrumentingAgents, WorkerGlobalScope* inspectedWorkerGlobalScope)
    : WebDebuggerAgent(injectedScriptManager, instrumentingAgents)
    , m_scriptDebugServer(inspectedWorkerGlobalScope, WorkerDebuggerAgent::debuggerTaskMode)
    , m_inspectedWorkerGlobalScope(inspectedWorkerGlobalScope)
{
    std::lock_guard<std::mutex> lock(workerDebuggerAgentsMutex());
    workerDebuggerAgents().set(inspectedWorkerGlobalScope->thread(), this);
}

WorkerDebuggerAgent::~WorkerDebuggerAgent()
{
    std::lock_guard<std::mutex> lock(workerDebuggerAgentsMutex());

    ASSERT(workerDebuggerAgents().contains(m_inspectedWorkerGlobalScope->thread()));
    workerDebuggerAgents().remove(m_inspectedWorkerGlobalScope->thread());
}

void WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(WorkerThread* thread)
{
    std::lock_guard<std::mutex> lock(workerDebuggerAgentsMutex());

    if (WorkerDebuggerAgent* agent = workerDebuggerAgents().get(thread))
        agent->m_scriptDebugServer.interruptAndRunTask(adoptPtr(new RunInspectorCommandsTask(thread, agent->m_inspectedWorkerGlobalScope)));
}

void WorkerDebuggerAgent::startListeningScriptDebugServer()
{
    scriptDebugServer().addListener(this);
}

void WorkerDebuggerAgent::stopListeningScriptDebugServer(bool isBeingDestroyed)
{
    scriptDebugServer().removeListener(this, isBeingDestroyed);
}

void WorkerDebuggerAgent::breakpointActionLog(JSC::ExecState*, const String& message)
{
    m_inspectedWorkerGlobalScope->addConsoleMessage(JSMessageSource, LogMessageLevel, message);
}

WorkerScriptDebugServer& WorkerDebuggerAgent::scriptDebugServer()
{
    return m_scriptDebugServer;
}

InjectedScript WorkerDebuggerAgent::injectedScriptForEval(ErrorString* error, const int* executionContextId)
{
    if (executionContextId) {
        *error = ASCIILiteral("Execution context id is not supported for workers as there is only one execution context.");
        return InjectedScript();
    }

    JSC::ExecState* scriptState = execStateFromWorkerGlobalScope(m_inspectedWorkerGlobalScope);
    return injectedScriptManager()->injectedScriptFor(scriptState);
}

void WorkerDebuggerAgent::muteConsole()
{
    // We don't need to mute console for workers.
}

void WorkerDebuggerAgent::unmuteConsole()
{
    // We don't need to mute console for workers.
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
