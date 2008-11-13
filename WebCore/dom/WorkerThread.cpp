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

#include "DedicatedWorker.h"
#include "JSWorkerContext.h"
#include "WorkerContext.h"
#include "WorkerTask.h"

using namespace JSC;

namespace WebCore {

PassRefPtr<WorkerThread> WorkerThread::create(const KURL& scriptURL, const String& sourceCode, PassRefPtr<DedicatedWorker> workerObject)
{
    return adoptRef(new WorkerThread(scriptURL, sourceCode, workerObject));
}

WorkerThread::WorkerThread(const KURL& scriptURL, const String& sourceCode, PassRefPtr<DedicatedWorker> workerObject)
    : m_threadID(0)
    , m_scriptURL(scriptURL.string().copy())
    , m_sourceCode(sourceCode.copy())
    , m_workerObject(workerObject)
{
}

bool WorkerThread::start()
{
    if (m_threadID)
        return true;

    ref();
    m_threadID = createThread(WorkerThread::workerThreadStart, this, "WebCore::Worker");

    return m_threadID;
}

void* WorkerThread::workerThreadStart(void* thread)
{
    return static_cast<WorkerThread*>(thread)->workerThread();
}

void* WorkerThread::workerThread()
{
    RefPtr<WorkerContext> workerContext = WorkerContext::create(KURL(m_scriptURL), this);
    WorkerScriptController* script = workerContext->script();

    script->evaluate(m_scriptURL, 1, m_sourceCode);

    while (true) {
        RefPtr<WorkerTask> task;
        if (!m_messageQueue.waitForMessage(task))
            break;

        task->performTask(workerContext.get());
    }

    workerContext = 0;
    m_threadID = 0;

    deref();
    return 0;
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
