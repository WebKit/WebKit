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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerInspectorProxy);

using namespace Inspector;

static Lock proxiesPerWorkerGlobalScopeLock;
static HashMap<ScriptExecutionContextIdentifier, WeakHashSet<WorkerInspectorProxy>>& proxiesPerWorkerGlobalScope() WTF_REQUIRES_LOCK(proxiesPerWorkerGlobalScopeLock)
{
    static NeverDestroyed<HashMap<ScriptExecutionContextIdentifier, WeakHashSet<WorkerInspectorProxy>>> proxies;
    return proxies;
}

static HashMap<PageIdentifier, WeakHashSet<WorkerInspectorProxy>>& proxiesPerPage()
{
    static MainThreadNeverDestroyed<HashMap<PageIdentifier, WeakHashSet<WorkerInspectorProxy>>> proxies;
    return proxies;
}

void WorkerInspectorProxy::addToProxyMap()
{
    if (!m_contextIdentifier)
        return;

    switchOn(*m_contextIdentifier,
        [&](PageIdentifier pageID) {
            auto& proxiesForPage = proxiesPerPage().add(pageID, WeakHashSet<WorkerInspectorProxy> { }).iterator->value;
            proxiesForPage.add(*this);
        }, [&](ScriptExecutionContextIdentifier globalScopeIdentifier) {
            Locker lock { proxiesPerWorkerGlobalScopeLock };
            auto& proxiesForContext = proxiesPerWorkerGlobalScope().add(globalScopeIdentifier, WeakHashSet<WorkerInspectorProxy> { }).iterator->value;
            proxiesForContext.add(*this);
        }
    );
}

void WorkerInspectorProxy::removeFromProxyMap()
{
    if (!m_contextIdentifier)
        return;

    switchOn(*m_contextIdentifier,
        [&](PageIdentifier pageID) {
            auto iterator = proxiesPerPage().find(pageID);
            RELEASE_ASSERT(iterator != proxiesPerPage().end());
            auto& proxiesForContext = iterator->value;
            ASSERT(proxiesForContext.contains(*this));
            proxiesForContext.remove(*this);
            if (proxiesForContext.isEmptyIgnoringNullReferences())
                proxiesPerPage().remove(iterator);
        }, [&](ScriptExecutionContextIdentifier globalScopeIdentifier) {
            Locker lock { proxiesPerWorkerGlobalScopeLock };
            auto iterator = proxiesPerWorkerGlobalScope().find(globalScopeIdentifier);
            RELEASE_ASSERT(iterator != proxiesPerWorkerGlobalScope().end());
            auto& proxiesForContext = iterator->value;
            ASSERT(proxiesForContext.contains(*this));
            proxiesForContext.remove(*this);
            if (proxiesForContext.isEmptyIgnoringNullReferences())
                proxiesPerWorkerGlobalScope().remove(iterator);
        }
    );
}

Vector<Ref<WorkerInspectorProxy>> WorkerInspectorProxy::proxiesForPage(PageIdentifier identifier)
{
    auto iterator = proxiesPerPage().find(identifier);
    if (iterator == proxiesPerPage().end())
        return { };

    return copyToVectorOf<Ref<WorkerInspectorProxy>>(iterator->value);
}

Vector<Ref<WorkerInspectorProxy>> WorkerInspectorProxy::proxiesForWorkerGlobalScope(ScriptExecutionContextIdentifier identifier)
{
    Locker lock { proxiesPerWorkerGlobalScopeLock };
    auto iterator = proxiesPerWorkerGlobalScope().find(identifier);
    if (iterator == proxiesPerWorkerGlobalScope().end())
        return { };
    return copyToVectorOf<Ref<WorkerInspectorProxy>>(iterator->value);
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

auto WorkerInspectorProxy::pageOrWorkerGlobalScopeIdentifier(ScriptExecutionContext& context) -> std::optional<PageOrWorkerGlobalScopeIdentifier>
{
    if (auto* document = dynamicDowncast<Document>(context)) {
        if (auto* page = document->page(); page && page->identifier())
            return PageOrWorkerGlobalScopeIdentifier { *page->identifier() };
        return std::nullopt;
    }
    return context.identifier();
}

void WorkerInspectorProxy::workerStarted(ScriptExecutionContext& scriptExecutionContext, WorkerThread* thread, const URL& url, const String& name)
{
    ASSERT(!m_workerThread);
    m_scriptExecutionContext = &scriptExecutionContext;
    m_contextIdentifier = pageOrWorkerGlobalScopeIdentifier(scriptExecutionContext);

    m_workerThread = thread;
    m_url = url;
    m_name = name;
    addToProxyMap();

    InspectorInstrumentation::workerStarted(*this);
}

void WorkerInspectorProxy::workerTerminated()
{
    if (!m_workerThread)
        return;

    InspectorInstrumentation::workerTerminated(*this);
    removeFromProxyMap();

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

void WorkerInspectorProxy::sendMessageFromWorkerToFrontend(String&& message)
{
    if (RefPtr pageChannel = m_pageChannel.get())
        pageChannel->sendMessageFromWorkerToFrontend(*this, WTFMove(message));
}

} // namespace WebCore
