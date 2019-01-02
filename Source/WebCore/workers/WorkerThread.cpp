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
#include <wtf/URL.h>
#include "WorkerGlobalScope.h"
#include "WorkerInspectorController.h"
#include <utility>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include "FloatingPointEnvironment.h"
#include "WebCoreThread.h"
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
#endif

namespace WebCore {

static Lock threadSetMutex;

static HashSet<WorkerThread*>& workerThreads()
{
    static NeverDestroyed<HashSet<WorkerThread*>> workerThreads;

    return workerThreads;
}

unsigned WorkerThread::workerThreadCount()
{
    std::lock_guard<Lock> lock(threadSetMutex);

    return workerThreads().size();
}

struct WorkerThreadStartupData {
    WTF_MAKE_NONCOPYABLE(WorkerThreadStartupData); WTF_MAKE_FAST_ALLOCATED;
public:
    WorkerThreadStartupData(const URL& scriptURL, const String& name, const String& identifier, const String& userAgent, bool isOnline, const String& sourceCode, WorkerThreadStartMode, const ContentSecurityPolicyResponseHeaders&, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin, PAL::SessionID);

    URL m_scriptURL;
    Ref<SecurityOrigin> m_origin;
    String m_name;
    String m_identifier;
    String m_userAgent;
    String m_sourceCode;
    WorkerThreadStartMode m_startMode;
    ContentSecurityPolicyResponseHeaders m_contentSecurityPolicyResponseHeaders;
    bool m_shouldBypassMainWorldContentSecurityPolicy;
    bool m_isOnline;
    Ref<SecurityOrigin> m_topOrigin;
    MonotonicTime m_timeOrigin;
    PAL::SessionID m_sessionID;
};

WorkerThreadStartupData::WorkerThreadStartupData(const URL& scriptURL, const String& name, const String& identifier, const String& userAgent, bool isOnline, const String& sourceCode, WorkerThreadStartMode startMode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin, PAL::SessionID sessionID)
    : m_scriptURL(scriptURL.isolatedCopy())
    , m_origin(SecurityOrigin::create(m_scriptURL)->isolatedCopy())
    , m_name(name.isolatedCopy())
    , m_identifier(identifier.isolatedCopy())
    , m_userAgent(userAgent.isolatedCopy())
    , m_sourceCode(sourceCode.isolatedCopy())
    , m_startMode(startMode)
    , m_contentSecurityPolicyResponseHeaders(contentSecurityPolicyResponseHeaders.isolatedCopy())
    , m_shouldBypassMainWorldContentSecurityPolicy(shouldBypassMainWorldContentSecurityPolicy)
    , m_isOnline(isOnline)
    , m_topOrigin(topOrigin.isolatedCopy())
    , m_timeOrigin(timeOrigin)
    , m_sessionID(sessionID.isolatedCopy())
{
}

WorkerThread::WorkerThread(const URL& scriptURL, const String& name, const String& identifier, const String& userAgent, bool isOnline, const String& sourceCode, WorkerLoaderProxy& workerLoaderProxy, WorkerDebuggerProxy& workerDebuggerProxy, WorkerReportingProxy& workerReportingProxy, WorkerThreadStartMode startMode, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders, bool shouldBypassMainWorldContentSecurityPolicy, const SecurityOrigin& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, JSC::RuntimeFlags runtimeFlags, PAL::SessionID sessionID)
    : m_workerLoaderProxy(workerLoaderProxy)
    , m_workerDebuggerProxy(workerDebuggerProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_runtimeFlags(runtimeFlags)
    , m_startupData(std::make_unique<WorkerThreadStartupData>(scriptURL, name, identifier, userAgent, isOnline, sourceCode, startMode, contentSecurityPolicyResponseHeaders, shouldBypassMainWorldContentSecurityPolicy, topOrigin, timeOrigin, sessionID))
#if ENABLE(INDEXED_DATABASE)
    , m_idbConnectionProxy(connectionProxy)
#endif
    , m_socketProvider(socketProvider)
{
#if !ENABLE(INDEXED_DATABASE)
    UNUSED_PARAM(connectionProxy);
#endif

    std::lock_guard<Lock> lock(threadSetMutex);

    workerThreads().add(this);
}

WorkerThread::~WorkerThread()
{
    std::lock_guard<Lock> lock(threadSetMutex);

    ASSERT(workerThreads().contains(this));
    workerThreads().remove(this);
}

void WorkerThread::start(WTF::Function<void(const String&)>&& evaluateCallback)
{
    // Mutex protection is necessary to ensure that m_thread is initialized when the thread starts.
    LockHolder lock(m_threadCreationAndWorkerGlobalScopeMutex);

    if (m_thread)
        return;

    m_evaluateCallback = WTFMove(evaluateCallback);

    m_thread = Thread::create(isServiceWorkerThread() ? "WebCore: Service Worker" : "WebCore: Worker", [this] {
        workerThread();
    });
}

void WorkerThread::workerThread()
{
    auto protectedThis = makeRef(*this);

    // Propagate the mainThread's fenv to workers.
#if PLATFORM(IOS_FAMILY)
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
        m_workerGlobalScope = createWorkerGlobalScope(m_startupData->m_scriptURL, WTFMove(m_startupData->m_origin), m_startupData->m_name, m_startupData->m_identifier, m_startupData->m_userAgent, m_startupData->m_isOnline, m_startupData->m_contentSecurityPolicyResponseHeaders, m_startupData->m_shouldBypassMainWorldContentSecurityPolicy, WTFMove(m_startupData->m_topOrigin), m_startupData->m_timeOrigin, m_startupData->m_sessionID);

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

    String exceptionMessage;
    scriptController->evaluate(ScriptSourceCode(m_startupData->m_sourceCode, URL(m_startupData->m_scriptURL)), &exceptionMessage);
    
    callOnMainThread([evaluateCallback = WTFMove(m_evaluateCallback), message = exceptionMessage.isolatedCopy()] {
        if (evaluateCallback)
            evaluateCallback(message);
    });
    
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

        if (m_stoppedCallback)
            callOnMainThread(WTFMove(m_stoppedCallback));
    }

    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    workerGlobalScopeToDelete = nullptr;

    // Clean up WebCore::ThreadGlobalData before WTF::Thread goes away!
    threadGlobalData().destroy();

    // Send the last WorkerThread Ref to be Deref'ed on the main thread.
    callOnMainThread([protectedThis = WTFMove(protectedThis)] { });

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

void WorkerThread::stop(WTF::Function<void()>&& stoppedCallback)
{
    // Mutex protection is necessary to ensure that m_workerGlobalScope isn't changed by
    // WorkerThread::workerThread() while we're accessing it. Note also that stop() can
    // be called before m_workerGlobalScope is fully created.
    auto locker = Locker<Lock>::tryLock(m_threadCreationAndWorkerGlobalScopeMutex);
    if (!locker) {
        // The thread is still starting, spin the runloop and try again to avoid deadlocks if the worker thread
        // needs to interact with the main thread during startup.
        callOnMainThread([this, stoppedCallback = WTFMove(stoppedCallback)]() mutable {
            stop(WTFMove(stoppedCallback));
        });
        return;
    }

    ASSERT(!m_stoppedCallback);
    m_stoppedCallback = WTFMove(stoppedCallback);

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    if (m_workerGlobalScope) {
        m_workerGlobalScope->script()->scheduleExecutionTermination();

        m_runLoop.postTaskAndTerminate({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context ) {
            WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);

            workerGlobalScope.prepareForTermination();

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
    std::lock_guard<Lock> lock(threadSetMutex);

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
