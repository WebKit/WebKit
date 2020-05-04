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
#include "WorkerInspectorProxy.h"

#include "InspectorInstrumentation.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include "WorkerInspectorController.h"
#include "WorkerRunLoop.h"
#include "WorkerThread.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <wtf/NeverDestroyed.h>


namespace WebCore {
using namespace Inspector;

HashSet<WorkerInspectorProxy*>& WorkerInspectorProxy::allWorkerInspectorProxies()
{
    static NeverDestroyed<HashSet<WorkerInspectorProxy*>> proxies;
    return proxies;
}

WorkerInspectorProxy::WorkerInspectorProxy(const String& identifier)
    : m_identifier(identifier)
{
}

WorkerInspectorProxy::~WorkerInspectorProxy()
{
    ASSERT(!m_workerThread);
    ASSERT(!m_pageChannel);
}

WorkerThreadStartMode WorkerInspectorProxy::workerStartMode(ScriptExecutionContext& scriptExecutionContext)
{
    bool pauseOnStart = InspectorInstrumentation::shouldWaitForDebuggerOnStart(scriptExecutionContext);
    return pauseOnStart ? WorkerThreadStartMode::WaitForInspector : WorkerThreadStartMode::Normal;
}

void WorkerInspectorProxy::workerStarted(ScriptExecutionContext* scriptExecutionContext, WorkerThread* thread, const URL& url, const String& name)
{
    ASSERT(!m_workerThread);

    m_scriptExecutionContext = scriptExecutionContext;
    m_workerThread = thread;
    m_url = url;
    m_name = name;

    allWorkerInspectorProxies().add(this);

    InspectorInstrumentation::workerStarted(*this);
}

void WorkerInspectorProxy::workerTerminated()
{
    if (!m_workerThread)
        return;

    InspectorInstrumentation::workerTerminated(*this);

    allWorkerInspectorProxies().remove(this);

    m_scriptExecutionContext = nullptr;
    m_workerThread = nullptr;
    m_pageChannel = nullptr;
}

void WorkerInspectorProxy::resumeWorkerIfPaused()
{
    m_workerThread->runLoop().postDebuggerTask([] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).thread().stopRunningDebuggerTasks();
    });
}

void WorkerInspectorProxy::connectToWorkerInspectorController(PageChannel& channel)
{
    ASSERT(m_workerThread);
    if (!m_workerThread)
        return;

    m_pageChannel = &channel;

    m_workerThread->runLoop().postDebuggerTask([] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().connectFrontend();
    });
}

void WorkerInspectorProxy::disconnectFromWorkerInspectorController()
{
    ASSERT(m_workerThread);
    if (!m_workerThread)
        return;

    m_pageChannel = nullptr;

    m_workerThread->runLoop().postDebuggerTask([] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().disconnectFrontend(DisconnectReason::InspectorDestroyed);

        // In case the worker is paused running debugger tasks, ensure we break out of
        // the pause since this will be the last debugger task we send to the worker.
        downcast<WorkerGlobalScope>(context).thread().stopRunningDebuggerTasks();
    });
}

void WorkerInspectorProxy::sendMessageToWorkerInspectorController(const String& message)
{
    ASSERT(m_workerThread);
    if (!m_workerThread)
        return;

    m_workerThread->runLoop().postDebuggerTask([message = message.isolatedCopy()] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().dispatchMessageFromFrontend(message);
    });
}

void WorkerInspectorProxy::sendMessageFromWorkerToFrontend(const String& message)
{
    if (!m_pageChannel)
        return;

    m_pageChannel->sendMessageFromWorkerToFrontend(*this, message);
}

} // namespace WebCore
