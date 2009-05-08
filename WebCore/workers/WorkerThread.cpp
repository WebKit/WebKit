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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if ENABLE(WORKERS)

#include "WorkerThread.h"

#include "KURL.h"
#include "PlatformString.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "WorkerContext.h"
#include "WorkerObjectProxy.h"

#include <utility>
#include <wtf/Noncopyable.h>

namespace WebCore {
struct WorkerThreadStartupData : Noncopyable {
public:
    static std::auto_ptr<WorkerThreadStartupData> create(const KURL& scriptURL, const String& userAgent, const String& sourceCode)
    {
        return std::auto_ptr<WorkerThreadStartupData>(new WorkerThreadStartupData(scriptURL, userAgent, sourceCode));
    }

    KURL m_scriptURL;
    String m_userAgent;
    String m_sourceCode;
private:
    WorkerThreadStartupData(const KURL& scriptURL, const String& userAgent, const String& sourceCode);
};

WorkerThreadStartupData::WorkerThreadStartupData(const KURL& scriptURL, const String& userAgent, const String& sourceCode)
    : m_scriptURL(scriptURL.copy())
    , m_userAgent(userAgent.copy())
    , m_sourceCode(sourceCode.copy())
{
}

PassRefPtr<WorkerThread> WorkerThread::create(const KURL& scriptURL, const String& userAgent, const String& sourceCode, WorkerObjectProxy* workerObjectProxy)
{
    return adoptRef(new WorkerThread(scriptURL, userAgent, sourceCode, workerObjectProxy));
}

WorkerThread::WorkerThread(const KURL& scriptURL, const String& userAgent, const String& sourceCode, WorkerObjectProxy* workerObjectProxy)
    : m_threadID(0)
    , m_workerObjectProxy(workerObjectProxy)
    , m_startupData(WorkerThreadStartupData::create(scriptURL, userAgent, sourceCode))
{
}

WorkerThread::~WorkerThread()
{
}

bool WorkerThread::start()
{
    // Mutex protection is necessary to ensure that m_threadID is initialized when the thread starts.
    MutexLocker lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(WorkerThread::workerThreadStart, this, "WebCore: Worker");

    return m_threadID;
}

void* WorkerThread::workerThreadStart(void* thread)
{
    return static_cast<WorkerThread*>(thread)->workerThread();
}

void* WorkerThread::workerThread()
{
    {
        MutexLocker lock(m_threadCreationMutex);
        m_workerContext = WorkerContext::create(m_startupData->m_scriptURL, m_startupData->m_userAgent, this);
        if (m_runLoop.terminated()) {
            // The worker was terminated before the thread had a chance to run. Since the context didn't exist yet, 
            // forbidExecution() couldn't be called from stop().
           m_workerContext->script()->forbidExecution();
        }
    }

    WorkerScriptController* script = m_workerContext->script();
    script->evaluate(ScriptSourceCode(m_startupData->m_sourceCode, m_startupData->m_scriptURL));
    // Free the startup data to cause its member variable deref's happen on the worker's thread (since
    // all ref/derefs of these objects are happening on the thread at this point). Note that
    // WorkerThread::~WorkerThread happens on a different thread where it was created.
    m_startupData.clear();

    m_workerObjectProxy->reportPendingActivity(m_workerContext->hasPendingActivity());

    // Blocks until terminated.
    m_runLoop.run(m_workerContext.get());

    ThreadIdentifier threadID = m_threadID;

    m_workerContext->stopActiveDOMObjects();
    m_workerContext->clearScript();
    ASSERT(m_workerContext->hasOneRef());
    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    m_workerContext = 0;
    
    // The thread object may be already destroyed from notification now, don't try to access "this".
    detachThread(threadID);

    return 0;
}

void WorkerThread::stop()
{
    // Mutex protection is necessary because stop() can be called before the context is fully created.
    MutexLocker lock(m_threadCreationMutex);

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    if (m_workerContext)
        m_workerContext->script()->forbidExecution();

    // FIXME: Rudely killing the thread won't work when we allow nested workers, because they will try to post notifications of their destruction.
    m_runLoop.terminate();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
