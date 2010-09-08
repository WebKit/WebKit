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

#include <wtf/Threading.h>

inline WorkQueue::WorkItemWin::WorkItemWin(PassOwnPtr<WorkItem> item, WorkQueue* queue)
    : m_item(item)
    , m_queue(queue)
{
}

PassRefPtr<WorkQueue::WorkItemWin> WorkQueue::WorkItemWin::create(PassOwnPtr<WorkItem> item, WorkQueue* queue)
{
    return adoptRef(new WorkItemWin(item, queue));
}

WorkQueue::WorkItemWin::~WorkItemWin()
{
}

inline WorkQueue::HandleWorkItem::HandleWorkItem(HANDLE handle, PassOwnPtr<WorkItem> item, WorkQueue* queue)
    : WorkItemWin(item, queue)
    , m_handle(handle)
    , m_waitHandle(0)
{
    ASSERT_ARG(handle, handle);
}

PassRefPtr<WorkQueue::HandleWorkItem> WorkQueue::HandleWorkItem::createByAdoptingHandle(HANDLE handle, PassOwnPtr<WorkItem> item, WorkQueue* queue)
{
    return adoptRef(new HandleWorkItem(handle, item, queue));
}

WorkQueue::HandleWorkItem::~HandleWorkItem()
{
    ::CloseHandle(m_handle);
}

void WorkQueue::handleCallback(void* context, BOOLEAN timerOrWaitFired)
{
    ASSERT_ARG(context, context);
    ASSERT_ARG(timerOrWaitFired, !timerOrWaitFired);

    WorkItemWin* item = static_cast<WorkItemWin*>(context);
    WorkQueue* queue = item->queue();

    {
        MutexLocker lock(queue->m_workItemQueueLock);
        queue->m_workItemQueue.append(item);

        // If no other thread is performing work, we can do it on this thread.
        if (!queue->tryRegisterAsWorkThread()) {
            // Some other thread is performing work. Since we hold the queue lock, we can be sure
            // that the work thread is not exiting due to an empty queue and will process the work
            // item we just added to it. If we weren't holding the lock we'd have to signal
            // m_performWorkEvent to make sure the work item got picked up.
            return;
        }
    }

    queue->performWorkOnRegisteredWorkThread();
}

void WorkQueue::registerHandle(HANDLE handle, PassOwnPtr<WorkItem> item)
{
    RefPtr<HandleWorkItem> handleItem = HandleWorkItem::createByAdoptingHandle(handle, item, this);

    {
        MutexLocker lock(m_handlesLock);
        ASSERT_ARG(handle, !m_handles.contains(handle));
        m_handles.set(handle, handleItem);
    }

    HANDLE waitHandle;
    if (!::RegisterWaitForSingleObject(&waitHandle, handle, handleCallback, handleItem.get(), INFINITE, WT_EXECUTEDEFAULT)) {
        DWORD error = ::GetLastError();
        ASSERT_NOT_REACHED();
    }
    handleItem->setWaitHandle(waitHandle);
}

void WorkQueue::unregisterAndCloseHandle(HANDLE handle)
{
    RefPtr<HandleWorkItem> item;
    {
        MutexLocker locker(m_handlesLock);
        ASSERT_ARG(handle, m_handles.contains(handle));
        item = m_handles.take(handle);
    }

    unregisterWaitAndDestroyItemSoon(item.release());
}

DWORD WorkQueue::workThreadCallback(void* context)
{
    ASSERT_ARG(context, context);

    WorkQueue* queue = static_cast<WorkQueue*>(context);

    if (!queue->tryRegisterAsWorkThread())
        return 0;

    queue->performWorkOnRegisteredWorkThread();
    return 0;
}

void WorkQueue::performWorkOnRegisteredWorkThread()
{
    ASSERT(m_isWorkThreadRegistered);

    bool isValid = true;

    m_workItemQueueLock.lock();

    while (isValid && !m_workItemQueue.isEmpty()) {
        Vector<RefPtr<WorkItemWin> > workItemQueue;
        m_workItemQueue.swap(workItemQueue);

        // Allow more work to be scheduled while we're not using the queue directly.
        m_workItemQueueLock.unlock();
        for (size_t i = 0; i < workItemQueue.size(); ++i) {
            MutexLocker locker(m_isValidMutex);
            isValid = m_isValid;
            if (!isValid)
                break;
            workItemQueue[i]->item()->execute();
        }
        m_workItemQueueLock.lock();
    }

    // One invariant we maintain is that any work scheduled while a work thread is registered will
    // be handled by that work thread. Unregister as the work thread while the queue lock is still
    // held so that no work can be scheduled while we're still registered.
    unregisterAsWorkThread();

    m_workItemQueueLock.unlock();
}

void WorkQueue::platformInitialize(const char* name)
{
    m_isWorkThreadRegistered = 0;
}

bool WorkQueue::tryRegisterAsWorkThread()
{
    LONG result = ::InterlockedCompareExchange(&m_isWorkThreadRegistered, 1, 0);
    ASSERT(!result || result == 1);
    return !result;
}

void WorkQueue::unregisterAsWorkThread()
{
    LONG result = ::InterlockedCompareExchange(&m_isWorkThreadRegistered, 0, 1);
    ASSERT_UNUSED(result, result == 1);
}

void WorkQueue::platformInvalidate()
{
#if !ASSERT_DISABLED
    MutexLocker lock(m_handlesLock);
    ASSERT(m_handles.isEmpty());
#endif
}

void WorkQueue::scheduleWork(PassOwnPtr<WorkItem> item)
{
    MutexLocker locker(m_workItemQueueLock);

    m_workItemQueue.append(WorkItemWin::create(item, this));

    // Spawn a work thread to perform the work we just added. As an optimization, we avoid
    // spawning the thread if a work thread is already registered. This prevents multiple work
    // threads from being spawned in most cases. (Note that when a work thread has been spawned but
    // hasn't registered itself yet, m_isWorkThreadRegistered will be false and we'll end up
    // spawning a second work thread here. But work thread registration process will ensure that
    // only one thread actually ends up performing work.)
    if (!m_isWorkThreadRegistered)
        ::QueueUserWorkItem(workThreadCallback, this, WT_EXECUTEDEFAULT);
}

void WorkQueue::unregisterWaitAndDestroyItemSoon(PassRefPtr<HandleWorkItem> item)
{
    // We're going to make a blocking call to ::UnregisterWaitEx before closing the handle. (The
    // blocking version of ::UnregisterWaitEx is much simpler than the non-blocking version.) If we
    // do this on the current thread, we'll deadlock if we're currently in a callback function for
    // the wait we're unregistering. So instead we do it asynchronously on some other worker thread.

    ::QueueUserWorkItem(unregisterWaitAndDestroyItemCallback, item.leakRef(), WT_EXECUTEDEFAULT);
}

DWORD WINAPI WorkQueue::unregisterWaitAndDestroyItemCallback(void* context)
{
    ASSERT_ARG(context, context);
    RefPtr<HandleWorkItem> item = adoptRef(static_cast<HandleWorkItem*>(context));

    // Now that we know we're not in a callback function for the wait we're unregistering, we can
    // make a blocking call to ::UnregisterWaitEx.
    if (!::UnregisterWaitEx(item->waitHandle(), INVALID_HANDLE_VALUE)) {
        DWORD error = ::GetLastError();
        ASSERT_NOT_REACHED();
    }

    return 0;
}
