/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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
#include <wtf/SuspendableWorkQueue.h>

#include <wtf/Logging.h>

namespace WTF {

Ref<SuspendableWorkQueue> SuspendableWorkQueue::create(const char* name, WorkQueue::QOS qos, ShouldLog shouldLog)
{
    return adoptRef(*new SuspendableWorkQueue(name, qos, shouldLog));
}

SuspendableWorkQueue::SuspendableWorkQueue(const char* name, QOS qos, ShouldLog shouldLog)
    : WorkQueue(name, qos)
    , m_shouldLog(shouldLog == ShouldLog::Yes)
{
    ASSERT(isMainThread());
}

const char* SuspendableWorkQueue::stateString(State state)
{
    switch (state) {
    case State::Running:
        return "Running";
    case State::WillSuspend:
        return "WillSuspend";
    case State::Suspended:
        return "Suspended";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void SuspendableWorkQueue::suspend(Function<void()>&& suspendFunction, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    Locker suspensionLocker { m_suspensionLock };

    RELEASE_LOG_IF(m_shouldLog, SuspendableWorkQueue, "%p - SuspendableWorkQueue::suspend current state %" PUBLIC_LOG_STRING, this, stateString(m_state));
    if (m_state == State::Suspended)
        return completionHandler();

    // Last suspend function will be the one that is used.
    m_suspendFunction = WTFMove(suspendFunction);
    m_suspensionCompletionHandlers.append(WTFMove(completionHandler));
    if (m_state == State::WillSuspend)
        return;

    m_state = State::WillSuspend;
    // Make sure queue will be suspended when there is no task scheduled on the queue.
    WorkQueue::dispatch([this] {
        suspendIfNeeded();
    });
}

void SuspendableWorkQueue::resume()
{
    ASSERT(isMainThread());
    Locker suspensionLocker { m_suspensionLock };

    RELEASE_LOG_IF(m_shouldLog, SuspendableWorkQueue, "%p - SuspendableWorkQueue::resume current state %" PUBLIC_LOG_STRING, this, stateString(m_state));
    if (m_state == State::Running)
        return;

    if (m_state == State::Suspended)
        m_suspensionCondition.notifyOne();

    m_state = State::Running;
}

void SuspendableWorkQueue::dispatch(Function<void()>&& function)
{
    RELEASE_ASSERT(function);
    // WorkQueue will protect this in dispatch().
    WorkQueue::dispatch([this, function = WTFMove(function)] {
        suspendIfNeeded();
        function();
    });
}

void SuspendableWorkQueue::dispatchAfter(Seconds seconds, Function<void()>&& function)
{
    RELEASE_ASSERT(function);
    WorkQueue::dispatchAfter(seconds, [this, function = WTFMove(function)] {
        suspendIfNeeded();
        function();
    });
}

void SuspendableWorkQueue::dispatchSync(Function<void()>&& function)
{
    // This function should be called only when queue is not about to be suspended,
    // otherwise thread may be blocked.
    if (isMainThread()) {
        Locker suspensionLocker { m_suspensionLock };
        RELEASE_ASSERT(m_state == State::Running);
    }
    WorkQueue::dispatchSync(WTFMove(function));
}

void SuspendableWorkQueue::invokeAllSuspensionCompletionHandlers()
{
    ASSERT(!isMainThread());

    if (m_suspensionCompletionHandlers.isEmpty())
        return;

    callOnMainThread([completionHandlers = std::exchange(m_suspensionCompletionHandlers, { })]() mutable {
        for (auto& completionHandler : completionHandlers) {
            if (completionHandler)
                completionHandler();
        }
    });
}

void SuspendableWorkQueue::suspendIfNeeded()
{
    ASSERT(!isMainThread());
    Locker suspensionLocker { m_suspensionLock };

    auto suspendFunction = std::exchange(m_suspendFunction, { });
    if (m_state != State::WillSuspend) {
        // If state is suspended, we should not reach here.
        RELEASE_LOG_ERROR_IF(m_shouldLog && m_state == State::Suspended, SuspendableWorkQueue, "%p - SuspendableWorkQueue::suspendIfNeeded current state Suspended", this);
        return;
    }

    RELEASE_LOG_IF(m_shouldLog, SuspendableWorkQueue, "%p - SuspendableWorkQueue::suspendIfNeeded set state to Suspended, will begin suspension", this);
    m_state = State::Suspended;
    suspendFunction();
    invokeAllSuspensionCompletionHandlers();

    while (m_state != State::Running)
        m_suspensionCondition.wait(m_suspensionLock);

    RELEASE_LOG_IF(m_shouldLog, SuspendableWorkQueue, "%p - SuspendableWorkQueue::suspendIfNeeded end suspension", this);
}

} // namespace WTF
