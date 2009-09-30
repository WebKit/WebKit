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
 */

#include "config.h"
#include "LocalStorageThread.h"

#if ENABLE(DOM_STORAGE)

#include "LocalStorageTask.h"
#include "StorageAreaSync.h"

namespace WebCore {

PassRefPtr<LocalStorageThread> LocalStorageThread::create()
{
    return adoptRef(new LocalStorageThread);
}

LocalStorageThread::LocalStorageThread()
    : m_threadID(0)
{
    m_selfRef = this;
}

bool LocalStorageThread::start()
{
    MutexLocker lock(m_threadCreationMutex);

    if (m_threadID)
        return true;

    m_threadID = createThread(LocalStorageThread::localStorageThreadStart, this, "WebCore: LocalStorage");

    return m_threadID;
}

void* LocalStorageThread::localStorageThreadStart(void* thread)
{
    return static_cast<LocalStorageThread*>(thread)->localStorageThread();
}

void* LocalStorageThread::localStorageThread()
{
    {
        // Wait for LocalStorageThread::start() to complete.
        MutexLocker lock(m_threadCreationMutex);
    }

    while (true) {
        RefPtr<LocalStorageTask> task;
        if (!m_queue.waitForMessage(task))
            break;

        task->performTask();
    }

    // Detach the thread so its resources are no longer of any concern to anyone else
    detachThread(m_threadID);
    m_threadID = 0;

    // Clear the self refptr, possibly resulting in deletion
    m_selfRef = 0;

    return 0;
}

void LocalStorageThread::scheduleImport(StorageAreaSync* area)
{
    ASSERT(!m_queue.killed() && m_threadID);
    m_queue.append(LocalStorageTask::createImport(area));
}

void LocalStorageThread::scheduleSync(StorageAreaSync* area)
{
    ASSERT(!m_queue.killed() && m_threadID);
    m_queue.append(LocalStorageTask::createSync(area));
}

void LocalStorageThread::terminate()
{
    ASSERT(isMainThread());

    // Ideally we'd never be killing a thread that wasn't live, so ASSERT it.
    // But if we do in a release build, make sure to not wait on a condition that will never get signalled
    ASSERT(!m_queue.killed() && m_threadID);
    if (!m_threadID)
        return;

    MutexLocker locker(m_terminateLock);

    m_queue.append(LocalStorageTask::createTerminate(this));

    m_terminateCondition.wait(m_terminateLock);
}

void LocalStorageThread::performTerminate()
{
    ASSERT(!isMainThread());

    m_queue.kill();

    MutexLocker locker(m_terminateLock);
    m_terminateCondition.signal();
}

}

#endif // ENABLE(DOM_STORAGE)
