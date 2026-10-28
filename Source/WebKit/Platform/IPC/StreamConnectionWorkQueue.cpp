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
#include <wtf/Borrow.h>

namespace IPC {

StreamConnectionWorkQueue::StreamConnectionWorkQueue(ASCIILiteral name)
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
    {
        Locker locker { m_lock };
        // Work dispatched while the work queue is stopping still runs. Work dispatched after it
        // has stopped is dropped.
        if (m_isStopped)
            return;
        m_functions.append(WTF::move(function));
        if (!m_processingThread) {
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
        ASSERT(m_connections.findIf([&connection](StreamServerConnection& other) { return &other == &connection; }) == notFound); // NOLINT
        m_connections.append(connection);
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
        bool didRemove = m_connections.removeFirstMatching([&connection](StreamServerConnection& other) {
            return &other == &connection;
        });
        ASSERT_UNUSED(didRemove, didRemove);
    }
    wakeUp();
}

void StreamConnectionWorkQueue::stopAndWaitForCompletion(NOESCAPE WTF::Function<void()>&& cleanupFunction)
{
    RefPtr<Thread> processingThread;
    {
        Locker locker { m_lock };
        m_cleanupFunction = WTF::move(cleanupFunction);
        processingThread = m_processingThread;
        m_shouldQuit = true;
        if (!processingThread)
            m_isStopped = true;
    }
    if (!processingThread)
        return;
    ASSERT(Thread::currentSingleton().uid() != processingThread->uid());
    wakeUp();
    processingThread->waitForCompletion();
    {
        Locker locker { m_lock };
        m_processingThread = nullptr;
    }
}

void StreamConnectionWorkQueue::wakeUp()
{
    m_wakeUpSemaphore.signal();
}

void StreamConnectionWorkQueue::startProcessingThread()
{
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto task = [this]() mutable {
        for (;;) {
            processStreams();
            if (m_shouldQuit) {
                // Run the work dispatched while stopping, including the work that the work dispatches,
                // until there is none left. The cleanup function is the last work to run.
                WTF::Function<void()> cleanup = nullptr;
                for (;;) {
                    processStreams();
                    Locker locker { m_lock };
                    if (m_functions.isEmpty()) {
                        m_isStopped = true;
                        cleanup = WTF::move(m_cleanupFunction);
                        break;
                    }
                }
                if (cleanup)
                    cleanup();
                return;
            }
            m_wakeUpSemaphore.wait();
        }
    };
    m_processingThread = Thread::create(m_name, WTF::move(task), ThreadType::Graphics, Thread::QOS::UserInteractive);
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
            connections = m_connections;
        }
        for (auto& function : functions)
            WTF::move(function)();

        hasMoreToProcess = false;
        for (auto& connection : borrow(connections))
            hasMoreToProcess |= connection->dispatchStreamMessages(defaultMessageLimit) == StreamServerConnection::HasMoreMessages;
    } while (hasMoreToProcess);
}

bool StreamConnectionWorkQueue::isCurrent() const
{
    Locker locker { m_lock };
    return m_processingThread ? m_processingThread->uid() == Thread::currentSingleton().uid() : false;
}

}
