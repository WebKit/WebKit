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
    ASSERT(m_runState != RunState::Quit); // Re-entering during shutdown not supported.
    {
        Locker locker { m_lock };
        m_functions.append(WTFMove(function));
        if (m_runState != RunState::Quit) {
            m_runState = RunState::ReloadState;
            if (!m_processingThread) {
                startProcessingThread();
                return;
            }
        }
    }
    wakeUp();
}

void StreamConnectionWorkQueue::addStreamConnection(StreamServerConnection& connection)
{
    {
        Locker locker { m_lock };
        m_connections.add(connection);
        if (m_runState != RunState::Quit) {
            m_runState = RunState::ReloadState;
            if (!m_processingThread) {
                startProcessingThread();
                return;
            }
        }
    }
    wakeUp();
}

void StreamConnectionWorkQueue::removeStreamConnection(StreamServerConnection& connection)
{
    {
        Locker locker { m_lock };
        m_connections.remove(connection);
        if (m_runState != RunState::Quit)
            m_runState = RunState::ReloadState;
    }
    wakeUp();
}

void StreamConnectionWorkQueue::stopAndWaitForCompletion()
{
    m_runState = RunState::Quit;
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
        bool shouldQuit = false;
        while (!shouldQuit) {
            Vector<Ref<StreamServerConnection>> connections;
            {
                Deque<Function<void()>> functions;
                {
                    Locker locker { m_lock };
                    functions.swap(m_functions);
                    connections = copyToVector(m_connections.values());
                    if (m_runState == RunState::Quit)
                        shouldQuit = true; // After RunState::Quit is toggled, the work queue should run the dispatched functions one last time.
                    else
                        m_runState =  RunState::Run;
                }
                for (auto& function : functions)
                    WTFMove(function)();
            }
            processStreams(WTFMove(connections));
        }
    };
    m_processingThread = Thread::create(m_name, WTFMove(task), ThreadType::Graphics, Thread::QOS::UserInteractive);
}

void StreamConnectionWorkQueue::processStreams(Vector<Ref<StreamServerConnection>>&& connections)
{
    constexpr size_t defaultMessageLimit = 1000;

    // When server consumes faster than client produces:
    //    Avoid client kernel calls to signal by not marking the server as sleeping, wait with very short deadline.
    //    Only mark the server sleeping on the last Wait step. After this, the client will spend time in signal.
    // Use the heuristic: if we did process any messages, it is probable that there will be more messages even though currently
    // we might have none.

    enum IdleStep {
        Wait100us,
        Wait,
    };
    int idleStep = IdleStep::Wait100us;

    using DispatchResult = StreamServerConnection::DispatchResult;
    using SleepIfNoMessages = StreamServerConnection::SleepIfNoMessages;

    while (m_runState == RunState::Run) {
#if USE(FOUNDATION)
        AutodrainedPool perProcessingIterationPool;
#endif
        SleepIfNoMessages willSleep = idleStep >= IdleStep::Wait ? SleepIfNoMessages::Yes : SleepIfNoMessages::No;
        for (auto& connection : connections) {
            if (connection->dispatchStreamMessages(defaultMessageLimit, willSleep) != DispatchResult::NoMessages) {
                willSleep = SleepIfNoMessages::No;
                idleStep = IdleStep::Wait100us;
            }
        }
        if (idleStep >= IdleStep::Wait) {
            m_wakeUpSemaphore.wait();
            // We don't want to set willSleep to Yes, so start next iteration with previous step.
            // For spurious wakeups, we do not reset immediately to full Wait but that is ok.
            idleStep = IdleStep::Wait - 1;
        } else if (idleStep >= IdleStep::Wait100us) {
            if (!m_wakeUpSemaphore.waitFor(100_us))
                ++idleStep;
        } else
            ++idleStep;
    }
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
