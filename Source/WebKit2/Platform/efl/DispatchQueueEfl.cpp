/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "DispatchQueueEfl.h"

#include <DispatchQueueWorkItemEfl.h>
#include <sys/timerfd.h>
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include <wtf/Threading.h>

static const int microSecondsPerSecond = 1000000;
static const int nanoSecondsPerSecond = 1000000000;
static const int invalidSocketDescriptor = -1;
static const char wakeUpThreadMessage = 'W';

class DispatchQueue::ThreadContext {
public:
    static void start(const char* name, PassRefPtr<DispatchQueue> dispatchQueue)
    {
        // The DispatchQueueThreadContext instance will be passed to the thread function and deleted in it.
        createThread(reinterpret_cast<WTF::ThreadFunction>(&ThreadContext::function), new ThreadContext(dispatchQueue), name);
    }

private:
    ThreadContext(PassRefPtr<DispatchQueue> dispatchQueue)
        : m_dispatchQueue(dispatchQueue)
    {
    }

    static void* function(ThreadContext* threadContext)
    {
        std::unique_ptr<ThreadContext>(threadContext)->m_dispatchQueue->dispatchQueueThread();
        return 0;
    }

    RefPtr<DispatchQueue> m_dispatchQueue;
};

PassRefPtr<DispatchQueue> DispatchQueue::create(const char* name)
{
    RefPtr<DispatchQueue> dispatchQueue = adoptRef<DispatchQueue>(new DispatchQueue());

    ThreadContext::start(name, dispatchQueue);

    return dispatchQueue.release();
}

DispatchQueue::DispatchQueue()
    : m_isThreadRunning(true)
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
}

DispatchQueue::~DispatchQueue()
{
    close(m_readFromPipeDescriptor);
    close(m_writeToPipeDescriptor);
}

void DispatchQueue::dispatch(std::unique_ptr<WorkItem> item)
{
    {
        MutexLocker locker(m_workItemsLock);
        m_workItems.append(WTF::move(item));
    }

    wakeUpThread();
}

void DispatchQueue::dispatch(std::unique_ptr<TimerWorkItem> item)
{
    insertTimerWorkItem(WTF::move(item));
    wakeUpThread();
}

void DispatchQueue::stopThread()
{
    ASSERT(m_socketDescriptor == invalidSocketDescriptor);

    m_isThreadRunning = false;
    wakeUpThread();
}

void DispatchQueue::setSocketEventHandler(int fileDescriptor, std::function<void ()> function)
{
    ASSERT(m_socketDescriptor == invalidSocketDescriptor);

    m_socketDescriptor = fileDescriptor;
    m_socketEventHandler = WTF::move(function);

    if (fileDescriptor > m_maxFileDescriptor)
        m_maxFileDescriptor = fileDescriptor;
    FD_SET(fileDescriptor, &m_fileDescriptorSet);
}

void DispatchQueue::clearSocketEventHandler()
{
    ASSERT(m_socketDescriptor != invalidSocketDescriptor);

    if (m_socketDescriptor == m_maxFileDescriptor)
        m_maxFileDescriptor = m_readFromPipeDescriptor;

    FD_CLR(m_socketDescriptor, &m_fileDescriptorSet);

    m_socketDescriptor = invalidSocketDescriptor;
}

void DispatchQueue::performWork()
{
    while (true) {
        Vector<std::unique_ptr<WorkItem>> workItems;

        {
            MutexLocker locker(m_workItemsLock);
            if (m_workItems.isEmpty())
                return;

            m_workItems.swap(workItems);
        }

        for (size_t i = 0; i < workItems.size(); ++i)
            workItems[i]->dispatch();
    }
}

void DispatchQueue::performTimerWork()
{
    Vector<std::unique_ptr<TimerWorkItem>> timerWorkItems;

    {
        // Protects m_timerWorkItems.
        MutexLocker locker(m_timerWorkItemsLock);
        if (m_timerWorkItems.isEmpty())
            return;

        // Copies all the timer work items in m_timerWorkItems to local vector.
        m_timerWorkItems.swap(timerWorkItems);
    }

    double currentTimeNanoSeconds = monotonicallyIncreasingTime() * nanoSecondsPerSecond;

    for (size_t i = 0; i < timerWorkItems.size(); ++i) {
        if (!timerWorkItems[i]->hasExpired(currentTimeNanoSeconds)) {
            insertTimerWorkItem(WTF::move(timerWorkItems[i]));
            continue;
        }

        // If a timer work item has expired, dispatch the function of the work item.
        timerWorkItems[i]->dispatch();
    }
}

void DispatchQueue::performFileDescriptorWork()
{
    fd_set readFileDescriptorSet = m_fileDescriptorSet;

    if (select(m_maxFileDescriptor + 1, &readFileDescriptorSet, 0, 0, getNextTimeOut()) >= 0) {
        if (FD_ISSET(m_readFromPipeDescriptor, &readFileDescriptorSet)) {
            char message;
            if (read(m_readFromPipeDescriptor, &message, 1) == -1)
                LOG_ERROR("Failed to read from DispatchQueue Thread pipe");

            ASSERT(message == wakeUpThreadMessage);
        }

        if (m_socketDescriptor != invalidSocketDescriptor && FD_ISSET(m_socketDescriptor, &readFileDescriptorSet))
            m_socketEventHandler();
    }
}

void DispatchQueue::insertTimerWorkItem(std::unique_ptr<TimerWorkItem> item)
{
    ASSERT(item);

    size_t position = 0;

    MutexLocker locker(m_timerWorkItemsLock);
    // The items should be ordered by expire time.
    for (; position < m_timerWorkItems.size(); ++position)
        if (item->expirationTimeNanoSeconds() < m_timerWorkItems[position]->expirationTimeNanoSeconds())
            break;

    m_timerWorkItems.insert(position, WTF::move(item));
}

void DispatchQueue::dispatchQueueThread()
{
    while (m_isThreadRunning) {
        performWork();
        performTimerWork();
        performFileDescriptorWork();
    }
}

void DispatchQueue::wakeUpThread()
{
    MutexLocker locker(m_writeToPipeDescriptorLock);
    if (write(m_writeToPipeDescriptor, &wakeUpThreadMessage, sizeof(char)) == -1)
        LOG_ERROR("Failed to wake up DispatchQueue Thread");
}

timeval* DispatchQueue::getNextTimeOut() const
{
    MutexLocker locker(m_timerWorkItemsLock);
    if (m_timerWorkItems.isEmpty())
        return 0;

    static timeval timeValue;
    timeValue.tv_sec = 0;
    timeValue.tv_usec = 0;
    double timeOutSeconds = (m_timerWorkItems[0]->expirationTimeNanoSeconds() - monotonicallyIncreasingTime() * nanoSecondsPerSecond) / nanoSecondsPerSecond;
    if (timeOutSeconds > 0) {
        timeValue.tv_sec = static_cast<long>(timeOutSeconds);
        timeValue.tv_usec = static_cast<long>((timeOutSeconds - timeValue.tv_sec) * microSecondsPerSecond);
    }

    return &timeValue;
}
