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

void StreamConnectionWorkQueue::dispatch(WTF::Function<void()>&& function)
{
    {
        Locker locker { m_lock };
        m_functions.append(WTFMove(function));
        ASSERT(!m_shouldQuit); // Re-entering during shutdown not supported.
    }
    wakeUpProcessingThread();

}
void StreamConnectionWorkQueue::addStreamConnection(StreamServerConnectionBase& connection)
{
    {
        Locker locker { m_lock };
        m_connections.add(connection);
        ASSERT(!m_shouldQuit); // Re-entering during shutdown not supported.
    }
    wakeUpProcessingThread();
}

void StreamConnectionWorkQueue::removeStreamConnection(StreamServerConnectionBase& connection)
{
    ASSERT(m_processingThread);
    {
        Locker locker { m_lock };
        m_connections.remove(connection);
        ASSERT(!m_shouldQuit); // Re-entering during shutdown not supported.
    }
    m_wakeUpSemaphore.signal();
}

void StreamConnectionWorkQueue::stop()
{
    m_shouldQuit = true;
    if (!m_processingThread)
        return;
    m_wakeUpSemaphore.signal();
    m_processingThread->waitForCompletion();
    m_processingThread = nullptr;
}

void StreamConnectionWorkQueue::wakeUp()
{
    m_wakeUpSemaphore.signal();
}

IPC::Semaphore& StreamConnectionWorkQueue::wakeUpSemaphore()
{
    return m_wakeUpSemaphore;
}

void StreamConnectionWorkQueue::wakeUpProcessingThread()
{
    if (m_processingThread) {
        m_wakeUpSemaphore.signal();
        return;
    }

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
        HashSet<Ref<StreamServerConnectionBase>> connections;
        {
            Locker locker { m_lock };
            functions.swap(m_functions);
            connections = m_connections;
        }
        for (auto& function : functions)
            WTFMove(function)();

        hasMoreToProcess = false;
        for (auto& connection : connections)
            hasMoreToProcess |= connection->dispatchStreamMessages(defaultMessageLimit) == StreamServerConnectionBase::HasMoreMessages;
    } while (hasMoreToProcess);
}

}
