/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "DedicatedWorkerGlobalScope.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "ThreadGlobalData.h"
#include "URL.h"
#include <utility>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include "FloatingPointEnvironment.h"
#include "WebCoreThread.h"
#endif

#if PLATFORM(GTK)
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
    WorkerThreadStartupData(const URL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode, const String& contentSecurityPolicy, ContentSecurityPolicy::HeaderType contentSecurityPolicyType, const SecurityOrigin* topOrigin);

    URL m_scriptURL;
    String m_userAgent;
    String m_sourceCode;
    WorkerThreadStartMode m_startMode;
    String m_contentSecurityPolicy;
    ContentSecurityPolicy::HeaderType m_contentSecurityPolicyType;
    RefPtr<SecurityOrigin> m_topOrigin;
};

WorkerThreadStartupData::WorkerThreadStartupData(const URL& scriptURL, const String& userAgent, const String& sourceCode, WorkerThreadStartMode startMode, const String& contentSecurityPolicy, ContentSecurityPolicy::HeaderType contentSecurityPolicyType, const SecurityOrigin* topOrigin)
    : m_scriptURL(scriptURL.isolatedCopy())
    , m_userAgent(userAgent.isolatedCopy())
    , m_sourceCode(sourceCode.isolatedCopy())
    , m_startMode(startMode)
    , m_contentSecurityPolicy(contentSecurityPolicy.isolatedCopy())
    , m_contentSecurityPolicyType(contentSecurityPolicyType)
    , m_topOrigin(topOrigin ? &topOrigin->isolatedCopy().get() : nullptr)
{
}

WorkerThread::WorkerThread(const URL& scriptURL, const String& userAgent, const String& sourceCode, WorkerLoaderProxy& workerLoaderProxy, WorkerReportingProxy& workerReportingProxy, WorkerThreadStartMode startMode, const String& contentSecurityPolicy, ContentSecurityPolicy::HeaderType contentSecurityPolicyType, const SecurityOrigin* topOrigin)
    : m_threadID(0)
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_startupData(std::make_unique<WorkerThreadStartupData>(scriptURL, userAgent, sourceCode, startMode, contentSecurityPolicy, contentSecurityPolicyType, topOrigin))
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    , m_notificationClient(0)
#endif
{
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
    // Mutex protection is necessary to ensure that m_threadID is initialized when the thread starts.
    LockHolder lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(WorkerThread::workerThreadStart, this, "WebCore: Worker");

    return m_threadID;
}

void WorkerThread::workerThreadStart(void* thread)
{
    static_cast<WorkerThread*>(thread)->workerThread();
}

void WorkerThread::workerThread()
{
    // Propagate the mainThread's fenv to workers.
#if PLATFORM(IOS)
    FloatingPointEnvironment::singleton().propagateMainThreadEnvironment();
#endif

#if PLATFORM(GTK)
    GRefPtr<GMainContext> mainContext = adoptGRef(g_main_context_new());
    g_main_context_push_thread_default(mainContext.get());
#endif

    {
        LockHolder lock(m_threadCreationMutex);
        m_workerGlobalScope = createWorkerGlobalScope(m_startupData->m_scriptURL, m_startupData->m_userAgent, m_startupData->m_contentSecurityPolicy, m_startupData->m_contentSecurityPolicyType, m_startupData->m_topOrigin.release());

        if (m_runLoop.terminated()) {
            // The worker was terminated before the thread had a chance to run. Since the context didn't exist yet,
            // forbidExecution() couldn't be called from stop().
            m_workerGlobalScope->script()->forbidExecution();
        }
    }

    WorkerScriptController* script = m_workerGlobalScope->script();
    script->evaluate(ScriptSourceCode(m_startupData->m_sourceCode, m_startupData->m_scriptURL));
    // Free the startup data to cause its member variable deref's happen on the worker's thread (since
    // all ref/derefs of these objects are happening on the thread at this point). Note that
    // WorkerThread::~WorkerThread happens on a different thread where it was created.
    m_startupData = nullptr;

    runEventLoop();

#if PLATFORM(GTK)
    g_main_context_pop_thread_default(mainContext.get());
#endif

    ThreadIdentifier threadID = m_threadID;

    ASSERT(m_workerGlobalScope->hasOneRef());

    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    m_workerGlobalScope = nullptr;

    // Clean up WebCore::ThreadGlobalData before WTF::WTFThreadData goes away!
    threadGlobalData().destroy();

    // The thread object may be already destroyed from notification now, don't try to access "this".
    detachThread(threadID);
}

void WorkerThread::runEventLoop()
{
    // Does not return until terminated.
    m_runLoop.run(m_workerGlobalScope.get());
}

void WorkerThread::stop()
{
    // Mutex protection is necessary because stop() can be called before the context is fully created.
    LockHolder lock(m_threadCreationMutex);

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    if (m_workerGlobalScope) {
        m_workerGlobalScope->script()->scheduleExecutionTermination();

        m_runLoop.postTaskAndTerminate({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context ) {
            WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);

            workerGlobalScope.stopActiveDOMObjects();
            workerGlobalScope.notifyObserversOfStop();

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

} // namespace WebCore
