/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "WorkerThread.h"

#include "ContentSecurityPolicyResponseHeaders.h"
#include "IDBConnectionProxy.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SocketProvider.h"
#include "ThreadGlobalData.h"
#include "URL.h"
#include "WorkerGlobalScope.h"
#include "WorkerInspectorController.h"
#include <utility>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include "FloatingPointEnvironment.h"
#include "WebCoreThread.h"
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

static StaticLock threadSetMutex;

static HashSet<WorkerThread*>& workerThreads()
{
    static NeverDestroyed<HashSet<WorkerThread*>> workerThreads;

    return workerThreads;
}

unsigned WorkerThread::workerThreadCount()
{
    std::lock_guard<StaticLock> lock(threadSetMutex);

    return workerThreads().size();
}

struct WorkerThreadStartupData {
    WTF_MAKE_NONCOPYABLE(WorkerThreadStartupData); WTF_MAKE_FAST_ALLOCATED;
public:
    WorkerThreadStartupData(const URL& scriptURL, const String& identifier, const String& userAgent, const String& sourceCode, WorkerThreadStartMode, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin);

    URL m_scriptURL;
    String m_identifier;
    String m_userAgent;
    String m_sourceCode;
    WorkerThreadStartMode m_startMode;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicyResponseHeaders;
    bool m_shouldBypassMainWorldContentSecurityPolicy;
    Ref<SecurityOrigin> m_topOrigin;
    MonotonicTime m_timeOrigin;
};

WorkerThreadStartupData::WorkerThreadStartupData(const URL& scriptURL, const String& identifier, const String& userAgent, const String& sourceCode, WorkerThreadStartMode startMode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin)
    : m_scriptURL(scriptURL.isolatedCopy())
    , m_identifier(identifier.isolatedCopy())
    , m_userAgent(userAgent.isolatedCopy())
    , m_sourceCode(sourceCode.isolatedCopy())
    , m_startMode(startMode)
    , m_contentSecurityPolicyResponseHeaders(contentSecurityPolicyResponseHeaders.isolatedCopy())
    , m_shouldBypassMainWorldContentSecurityPolicy(shouldBypassMainWorldContentSecurityPolicy)
    , m_topOrigin(topOrigin.isolatedCopy())
    , m_timeOrigin(timeOrigin)
{
}

WorkerThread::WorkerThread(const URL& scriptURL, const String& identifier, const String& userAgent, const String& sourceCode, WorkerLoaderProxy& workerLoaderProxy, WorkerReportingProxy& workerReportingProxy, WorkerThreadStartMode startMode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, JSC::RuntimeFlags runtimeFlags)
    : m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_runtimeFlags(runtimeFlags)
    , m_startupData(std::make_unique<WorkerThreadStartupData>(scriptURL, identifier, userAgent, sourceCode, startMode, contentSecurityPolicyResponseHeaders, shouldBypassMainWorldContentSecurityPolicy, topOrigin, timeOrigin))
#if ENABLE(INDEXED_DATABASE)
    , m_idbConnectionProxy(connectionProxy)
#endif
    , m_socketProvider(socketProvider)
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif

    std::lock_guard<StaticLock> lock(threadSetMutex);

    workerThreads().add(this);
}

WorkerThread::~WorkerThread()
{
    std::lock_guard<StaticLock> lock(threadSetMutex);

    ASSERT(workerThreads().contains(this));
    workerThreads().remove(this);
}

bool WorkerThread::start()
{
    // Mutex protection is necessary to ensure that m_thread is initialized when the thread starts.
    LockHolder lock(m_threadCreationAndWorkerGlobalScopeMutex);

    if (m_thread)
        return true;

    m_thread = Thread::create("WebCore: Worker", [this] {
        workerThread();
    });

    return m_thread;
}

void WorkerThread::workerThread()
{
    // Propagate the mainThread's fenv to workers.
#if PLATFORM(IOS)
    FloatingPointEnvironment::singleton().propagateMainThreadEnvironment();
#endif

#if USE(GLIB)
    GRefPtr<GMainContext> mainContext = adoptGRef(g_main_context_new());
    g_main_context_push_thread_default(mainContext.get());
#endif

    WorkerScriptController* scriptController;
    {
        // Mutex protection is necessary to ensure that we don't change m_workerGlobalScope
        // while WorkerThread::stop() is accessing it. Note that WorkerThread::stop() can
        // be called before we've finished creating the WorkerGlobalScope.
        LockHolder lock(m_threadCreationAndWorkerGlobalScopeMutex);
        m_workerGlobalScope = createWorkerGlobalScope(m_startupData->m_scriptURL, m_startupData->m_identifier, m_startupData->m_userAgent, m_startupData->m_contentSecurityPolicyResponseHeaders, m_startupData->m_shouldBypassMainWorldContentSecurityPolicy, WTFMove(m_startupData->m_topOrigin), m_startupData->m_timeOrigin);

        scriptController = m_workerGlobalScope->script();

        if (m_runLoop.terminated()) {
            // The worker was terminated before the thread had a chance to run. Since the context didn't exist yet,
            // forbidExecution() couldn't be called from stop().
            scriptController->scheduleExecutionTermination();
            scriptController->forbidExecution();
        }
    }

    if (m_startupData->m_startMode == WorkerThreadStartMode::WaitForInspector) {
        startRunningDebuggerTasks();

        // If the worker was somehow terminated while processing debugger commands.
        if (m_runLoop.terminated())
            scriptController->forbidExecution();
    }

    scriptController->evaluate(ScriptSourceCode(m_startupData->m_sourceCode, m_startupData->m_scriptURL));
    // Free the startup data to cause its member variable deref's happen on the worker's thread (since
    // all ref/derefs of these objects are happening on the thread at this point). Note that
    // WorkerThread::~WorkerThread happens on a different thread where it was created.
    m_startupData = nullptr;

    runEventLoop();

#if USE(GLIB)
    g_main_context_pop_thread_default(mainContext.get());
#endif

    RefPtr<Thread> protector = m_thread;

    ASSERT(m_workerGlobalScope->hasOneRef());

    RefPtr<WorkerGlobalScope> workerGlobalScopeToDelete;
    {
        // Mutex protection is necessary to ensure that we don't change m_workerGlobalScope
        // while WorkerThread::stop is accessing it.
        LockHolder lock(m_threadCreationAndWorkerGlobalScopeMutex);

        // Delay the destruction of the WorkerGlobalScope context until after we've unlocked the
        // m_threadCreationAndWorkerGlobalScopeMutex. This is needed because destructing the
        // context will trigger the main thread to race against us to delete the WorkerThread
        // object, and the WorkerThread object owns the mutex we need to unlock after this.
        workerGlobalScopeToDelete = WTFMove(m_workerGlobalScope);
    }

    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    workerGlobalScopeToDelete = nullptr;

    // Clean up WebCore::ThreadGlobalData before WTF::Thread goes away!
    threadGlobalData().destroy();

    // The thread object may be already destroyed from notification now, don't try to access "this".
    protector->detach();
}

void WorkerThread::startRunningDebuggerTasks()
{
    ASSERT(!m_pausedForDebugger);
    m_pausedForDebugger = true;

    MessageQueueWaitResult result;
    do {
        result = m_runLoop.runInMode(m_workerGlobalScope.get(), WorkerRunLoop::debuggerMode());
    } while (result != MessageQueueTerminated && m_pausedForDebugger);
}

void WorkerThread::stopRunningDebuggerTasks()
{
    m_pausedForDebugger = false;
}

void WorkerThread::runEventLoop()
{
    // Does not return until terminated.
    m_runLoop.run(m_workerGlobalScope.get());
}

void WorkerThread::stop()
{
    // Mutex protection is necessary to ensure that m_workerGlobalScope isn't changed by
    // WorkerThread::workerThread() while we're accessing it. Note also that stop() can
    // be called before m_workerGlobalScope is fully created.
    LockHolder lock(m_threadCreationAndWorkerGlobalScopeMutex);

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    if (m_workerGlobalScope) {
        m_workerGlobalScope->script()->scheduleExecutionTermination();

        m_runLoop.postTaskAndTerminate({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context ) {
            WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);

#if ENABLE(INDEXED_DATABASE)
            workerGlobalScope.stopIndexedDatabase();
#endif

            workerGlobalScope.stopActiveDOMObjects();

            workerGlobalScope.inspectorController().workerTerminating();

            // Event listeners would keep DOMWrapperWorld objects alive for too long. Also, they have references to JS objects,
            // which become dangling once Heap is destroyed.
            workerGlobalScope.removeAllEventListeners();

            // Stick a shutdown command at the end of the queue, so that we deal
            // with all the cleanup tasks the databases post first.
            workerGlobalScope.postTask({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context) {
                WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);
                // It's not safe to call clearScript until all the cleanup tasks posted by functions initiated by WorkerThreadShutdownStartTask have completed.
                workerGlobalScope.clearScript();
            } });

        } });
        return;
    }
    m_runLoop.terminate();
}

void WorkerThread::releaseFastMallocFreeMemoryInAllThreads()
{
    std::lock_guard<StaticLock> lock(threadSetMutex);

    for (auto* workerThread : workerThreads()) {
        workerThread->runLoop().postTask([] (ScriptExecutionContext&) {
            WTF::releaseFastMallocFreeMemory();
        });
    }
}

IDBClient::IDBConnectionProxy* WorkerThread::idbConnectionProxy()
{
#if ENABLE(INDEXED_DATABASE)
    return m_idbConnectionProxy.get();
#else
    return nullptr;
#endif
}

SocketProvider* WorkerThread::socketProvider()
{
    return m_socketProvider.get();
}

} // namespace WebCore
