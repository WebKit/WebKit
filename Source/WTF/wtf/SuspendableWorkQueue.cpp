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

namespace WTF {

Ref<SuspendableWorkQueue> SuspendableWorkQueue::create(const char* name, WorkQueue::QOS qos)
{
    return adoptRef(*new SuspendableWorkQueue(name, qos));
}

SuspendableWorkQueue::SuspendableWorkQueue(const char* name, QOS qos)
    : WorkQueue(name, Type::Serial, qos)
{
    ASSERT(isMainThread());
}

void SuspendableWorkQueue::suspend(Function<void()>&& suspendFunction, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());
    Locker suspensionLocker { m_suspensionLock };

    // Last suspend function will be the one that is used.
    m_suspendFunction = WTFMove(suspendFunction);
    m_suspensionCompletionHandlers.append(WTFMove(completionHandler));
    if (m_isOrWillBeSuspended)
        return;

    m_isOrWillBeSuspended = true;
    // Make sure queue will be suspended when there is no task scheduled on the queue.
    WorkQueue::dispatch([this] {
        suspendIfNeeded();
    });
}

void SuspendableWorkQueue::resume()
{
    ASSERT(isMainThread());
    Locker suspensionLocker { m_suspensionLock };

    if (!m_isOrWillBeSuspended)
        return;

    m_isOrWillBeSuspended = false;
    m_suspensionCondition.notifyOne();
}

void SuspendableWorkQueue::dispatch(Function<void()>&& function)
{
    // WorkQueue will protect this in dispatch().
    WorkQueue::dispatch([this, function = WTFMove(function)] {
        suspendIfNeeded();
        function();
    });
}

void SuspendableWorkQueue::dispatchAfter(Seconds seconds, Function<void()>&& function)
{
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
        RELEASE_ASSERT(!m_isOrWillBeSuspended);
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
    if (m_isOrWillBeSuspended)
        suspendFunction();

    invokeAllSuspensionCompletionHandlers();

    while (m_isOrWillBeSuspended)
        m_suspensionCondition.wait(m_suspensionLock);
}

} // namespace WTF
