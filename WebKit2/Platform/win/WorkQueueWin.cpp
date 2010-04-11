/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WorkQueue.h"

#include <process.h>
#include <wtf/Threading.h>

void WorkQueue::registerHandle(HANDLE handle, std::auto_ptr<WorkItem> item)
{
    // Add the item.
    {
        MutexLocker locker(m_handlesLock);
        m_handles.set(handle, item.release());
    }

    // Set the work event.
    ::SetEvent(m_performWorkEvent);
}

void* WorkQueue::workQueueThreadBody(void *context)
{
    static_cast<WorkQueue*>(context)->workQueueThreadBody();
    return 0;
}

void WorkQueue::workQueueThreadBody()
{
    while (true) {
        Vector<HANDLE> handles;
        {
            // Copy the handles to our handles vector.
            MutexLocker locker(m_handlesLock);
            copyKeysToVector(m_handles, handles);
        }

        // Add the "perform work" event handle.
        handles.append(m_performWorkEvent);

        // Now we wait.
        DWORD result = ::WaitForMultipleObjects(handles.size(), handles.data(), FALSE, INFINITE);

        if (result == handles.size() - 1)
            performWork();
        else {
            // FIXME: If we ever decide to support unregistering handles we would need to copy the hash map.
            WorkItem* workItem;
            HANDLE handle = handles[result];

            {
                MutexLocker locker(m_handlesLock);
                workItem = m_handles.get(handle);
            }

            // Execute the work item.
            workItem->execute();
        }

        // Check if this queue is invalid.
        {
            MutexLocker locker(m_isValidMutex);
            if (!m_isValid)
                break;
        }
    }
}

void WorkQueue::platformInitialize(const char* name)
{
    // Create our event.
    m_performWorkEvent = ::CreateEvent(0, false, false, 0);

    m_workQueueThread = createThread(&WorkQueue::workQueueThreadBody, this, name);
}

void WorkQueue::platformInvalidate()
{
    ::CloseHandle(m_performWorkEvent);

    // FIXME: Stop the thread and do other cleanup.
}

void WorkQueue::scheduleWork(std::auto_ptr<WorkItem> item)
{
    MutexLocker locker(m_workItemQueueLock);
    m_workItemQueue.append(item.release());

    // Set the work event.
    ::SetEvent(m_performWorkEvent);
}

void WorkQueue::performWork()
{
    Vector<WorkItem*> workItemQueue;
    {
        MutexLocker locker(m_workItemQueueLock);
        m_workItemQueue.swap(workItemQueue);
    }

    for (size_t i = 0; i < workItemQueue.size(); ++i) {
        std::auto_ptr<WorkItem> item(workItemQueue[i]);

        MutexLocker locker(m_isValidMutex);
        if (m_isValid)
            item->execute();
    }
}
