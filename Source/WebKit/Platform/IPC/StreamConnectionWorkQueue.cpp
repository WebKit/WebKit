/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "StreamConnectionWorkQueue.h"

namespace IPC {

StreamConnectionWorkQueue::StreamConnectionWorkQueue(const char* name)
    : m_name(name)
{
}

StreamConnectionWorkQueue::~StreamConnectionWorkQueue()
{
    // `StreamConnectionWorkQueue::stopAndWaitForCompletion()` should be called if anything has been dispatched or listened to.
    ASSERT(!m_processingThread);
}

void StreamConnectionWorkQueue::dispatch(WTF::Function<void()>&& function)
{
    ASSERT(!m_shouldQuit); // Re-entering during shutdown not supported.
    {
        Locker locker { m_lock };
        m_functions.append(WTFMove(function));
        if (!m_shouldQuit && !m_processingThread) {
            startProcessingThread();
            return;
        }
    }
    wakeUp();
}

void StreamConnectionWorkQueue::addStreamConnection(StreamServerConnectionBase& connection)
{
    {
        Locker locker { m_lock };
        m_connections.add(connection);
        if (!m_shouldQuit && !m_processingThread) {
            startProcessingThread();
            return;
        }
    }
    wakeUp();
}

void StreamConnectionWorkQueue::removeStreamConnection(StreamServerConnectionBase& connection)
{
    {
        Locker locker { m_lock };
        m_connections.remove(connection);
    }
    wakeUp();
}

void StreamConnectionWorkQueue::stopAndWaitForCompletion()
{
    m_shouldQuit = true;
    RefPtr<Thread> processingThread;
    {
        Locker locker { m_lock };
        processingThread = WTFMove(m_processingThread);
    }
    if (!processingThread)
        return;
    ASSERT(Thread::current().uid() != processingThread->uid());
    wakeUp();
    processingThread->waitForCompletion();
}

void StreamConnectionWorkQueue::wakeUp()
{
    m_wakeUpSemaphore.signal();
}

IPC::Semaphore& StreamConnectionWorkQueue::wakeUpSemaphore()
{
    return m_wakeUpSemaphore;
}

void StreamConnectionWorkQueue::startProcessingThread()
{
    auto task = [this]() mutable {
        for (;;) {
            processStreams();
            if (m_shouldQuit) {
                processStreams();
                return;
            }
            m_wakeUpSemaphore.wait();
        }
    };
    m_processingThread = Thread::create(m_name, WTFMove(task), ThreadType::Graphics, Thread::QOS::UserInteractive);
}

void StreamConnectionWorkQueue::processStreams()
{
    constexpr size_t defaultMessageLimit = 1000;
    bool hasMoreToProcess = false;
    do {
        Deque<WTF::Function<void()>> functions;
        Vector<Ref<StreamServerConnectionBase>> connections;
        {
            Locker locker { m_lock };
            functions.swap(m_functions);
            connections = copyToVector(m_connections.values());
        }
        for (auto& function : functions)
            WTFMove(function)();

        hasMoreToProcess = false;
        for (auto& connection : connections)
            hasMoreToProcess |= connection->dispatchStreamMessages(defaultMessageLimit) == StreamServerConnectionBase::HasMoreMessages;
    } while (hasMoreToProcess);
}

}
