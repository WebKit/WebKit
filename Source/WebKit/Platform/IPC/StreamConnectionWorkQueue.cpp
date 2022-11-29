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

#if USE(FOUNDATION)
#include <wtf/AutodrainedPool.h>
#endif

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

void StreamConnectionWorkQueue::addStreamConnection(StreamServerConnection& connection)
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

void StreamConnectionWorkQueue::removeStreamConnection(StreamServerConnection& connection)
{
    {
        Locker locker { m_lock };
        m_connections.remove(connection);
    }
    wakeUp();
}

void StreamConnectionWorkQueue::stopAndWaitForCompletion(WTF::Function<void()>&& cleanupFunction)
{
    RefPtr<Thread> processingThread;
    {
        Locker locker { m_lock };
        m_cleanupFunction = WTFMove(cleanupFunction);
        processingThread = WTFMove(m_processingThread);
    }
    m_shouldQuit = true;
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
                WTF::Function<void()> cleanup = nullptr;
                {
                    Locker locker { m_lock };
                    cleanup = WTFMove(m_cleanupFunction);

                }
                if (cleanup)
                    cleanup();
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
#if USE(FOUNDATION)
        AutodrainedPool perProcessingIterationPool;
#endif
        Deque<WTF::Function<void()>> functions;
        Vector<Ref<StreamServerConnection>> connections;
        {
            Locker locker { m_lock };
            functions.swap(m_functions);
            connections = copyToVector(m_connections.values());
        }
        for (auto& function : functions)
            WTFMove(function)();

        hasMoreToProcess = false;
        for (auto& connection : connections)
            hasMoreToProcess |= connection->dispatchStreamMessages(defaultMessageLimit) == StreamServerConnection::HasMoreMessages;
    } while (hasMoreToProcess);
}

#if ASSERT_ENABLED
void StreamConnectionWorkQueue::assertIsCurrent() const
{
    Locker locker { m_lock };
    ASSERT(m_processingThread);
    WTF::assertIsCurrent(*m_processingThread);
}
#endif
}
