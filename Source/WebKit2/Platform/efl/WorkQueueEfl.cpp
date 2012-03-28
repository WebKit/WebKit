/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WorkQueue.h"

#include <wtf/Assertions.h>

class TimerWorkItem {
public:
    TimerWorkItem(int timerID, const Function<void()>& function, WorkQueue* queue)
        : m_function(function)
        , m_queue(queue)
        , m_timerID(timerID)
    {
    }
    ~TimerWorkItem() { }

    Function<void()> function() const { return m_function; }
    WorkQueue* queue() const { return m_queue; }

    int timerID() const { return m_timerID; }

private:
    Function<void()> m_function;
    WorkQueue* m_queue;
    int m_timerID;
};

static const int invalidSocketDescriptor = -1;
static const int threadMessageSize = 1;
static const char finishThreadMessage[] = "F";
static const char wakupThreadMessage[] = "W";

void WorkQueue::platformInitialize(const char* name)
{
    int fds[2];
    if (pipe(fds))
        ASSERT_NOT_REACHED();

    m_readFromPipeDescriptor = fds[0];
    m_writeToPipeDescriptor = fds[1];
    FD_ZERO(&m_fileDescriptorSet);
    FD_SET(m_readFromPipeDescriptor, &m_fileDescriptorSet);
    m_maxFileDescriptor = m_readFromPipeDescriptor;

    m_socketDescriptor = invalidSocketDescriptor;

    m_threadLoop = true;
    createThread(reinterpret_cast<WTF::ThreadFunction>(&WorkQueue::workQueueThread), this, name);
}

void WorkQueue::platformInvalidate()
{
    sendMessageToThread(finishThreadMessage);
}

void WorkQueue::performWork()
{
    m_workItemQueueLock.lock();

    while (!m_workItemQueue.isEmpty()) {
        Vector<Function<void()> > workItemQueue;
        m_workItemQueue.swap(workItemQueue);

        m_workItemQueueLock.unlock();
        for (size_t i = 0; i < workItemQueue.size(); ++i)
            workItemQueue[i]();
        m_workItemQueueLock.lock();
    }
    m_workItemQueueLock.unlock();
}

void WorkQueue::performFileDescriptorWork()
{
    fd_set readFileDescriptorSet = m_fileDescriptorSet;

    if (select(m_maxFileDescriptor + 1, &readFileDescriptorSet, 0, 0, 0) >= 0) {
        if (FD_ISSET(m_readFromPipeDescriptor, &readFileDescriptorSet)) {
            char readBuf[threadMessageSize];
            if (read(m_readFromPipeDescriptor, readBuf, threadMessageSize) == -1)
                LOG_ERROR("Failed to read from WorkQueueThread pipe");
            if (!strncmp(readBuf, finishThreadMessage, threadMessageSize))
                m_threadLoop = false;
        }

        if (m_socketDescriptor != invalidSocketDescriptor && FD_ISSET(m_socketDescriptor, &readFileDescriptorSet))
            m_socketEventHandler();
    }
}

void WorkQueue::sendMessageToThread(const char* message)
{
    if (write(m_writeToPipeDescriptor, message, threadMessageSize) == -1)
        LOG_ERROR("Failed to wake up WorkQueue Thread");
}

void* WorkQueue::workQueueThread(WorkQueue* workQueue)
{
    while (workQueue->m_threadLoop) {
        workQueue->performWork();
        workQueue->performFileDescriptorWork();
    }

    close(workQueue->m_readFromPipeDescriptor);
    close(workQueue->m_writeToPipeDescriptor);

    return 0;
}

void WorkQueue::registerSocketEventHandler(int fileDescriptor, const Function<void()>& function)
{
    if (m_socketDescriptor != invalidSocketDescriptor)
        LOG_ERROR("%d is already registerd.", fileDescriptor);

    m_socketDescriptor = fileDescriptor;
    m_socketEventHandler = function;

    if (fileDescriptor > m_maxFileDescriptor)
        m_maxFileDescriptor = fileDescriptor;
    FD_SET(fileDescriptor, &m_fileDescriptorSet);
}

void WorkQueue::unregisterSocketEventHandler(int fileDescriptor)
{
    m_socketDescriptor = invalidSocketDescriptor;

    if (fileDescriptor == m_maxFileDescriptor)
        m_maxFileDescriptor = m_readFromPipeDescriptor;
    FD_CLR(fileDescriptor, &m_fileDescriptorSet);
}

void WorkQueue::dispatch(const Function<void()>& function)
{
    MutexLocker locker(m_workItemQueueLock);
    m_workItemQueue.append(function);
    sendMessageToThread(wakupThreadMessage);
}

bool WorkQueue::timerFired(void* data)
{
    TimerWorkItem* item = static_cast<TimerWorkItem*>(data);
    if (item && item->queue()->m_isValid) {
        item->queue()->dispatch(item->function());
        item->queue()->m_timers.take(item->timerID());
        delete item;
    }

    return ECORE_CALLBACK_CANCEL;
}

void WorkQueue::dispatchAfterDelay(const Function<void()>& function, double delay)
{
    static int timerId = 0;
    m_timers.set(timerId, adoptPtr(ecore_timer_add(delay, reinterpret_cast<Ecore_Task_Cb>(timerFired), new TimerWorkItem(timerId, function, this))));
    timerId++;
}
