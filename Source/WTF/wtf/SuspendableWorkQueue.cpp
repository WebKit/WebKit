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
#include <wtf/threads/BinarySemaphore.h>

namespace WTF {

Ref<SuspendableWorkQueue> SuspendableWorkQueue::create(const char* name, WorkQueue::QOS qos)
{
    return adoptRef(*new SuspendableWorkQueue(name, qos));
}

SuspendableWorkQueue::SuspendableWorkQueue(const char* name, QOS qos)
    : WorkQueue(name, qos)
{
}

SuspendableWorkQueue::SuspendState SuspendableWorkQueue::suspend()
{
    Locker lock { m_lock };
    if (m_isSuspended)
        return WasSuspended;
    m_isSuspended = true;
    return WasRunning;
}


SuspendableWorkQueue::SuspendState SuspendableWorkQueue::resume()
{
    Locker lock { m_lock };
    if (!m_isSuspended)
        return WasRunning;
    m_isSuspended = false;
    // Dispatch with lock since other threads must not be able to submit inbetween 
    // the suspended tasks.
    for (auto& task : std::exchange(m_suspendedTasks, { }))
        WorkQueue::dispatch(WTFMove(task));
    return WasSuspended;
}

void SuspendableWorkQueue::dispatch(Function<void()>&& function)
{
    {
        Locker lock { m_lock };
        if (m_isSuspended) {
            m_suspendedTasks.append(WTFMove(function));
            return;
        }
    }
    // Dispatch without lock since ordering cannot be observed from the outside.
    WorkQueue::dispatch(WTFMove(function));
}

void SuspendableWorkQueue::dispatchAfter(Seconds seconds, Function<void()>&& function)
{
    WorkQueue::dispatchAfter(seconds, [protectedThis = Ref { *this }, function = WTFMove(function)] () mutable {
        protectedThis->dispatch(WTFMove(function));
    });
}

void SuspendableWorkQueue::dispatchSync(Function<void()>&& function)
{
    BinarySemaphore semaphore;
    dispatch([&semaphore, function = WTFMove(function)] () mutable {
        function();
        semaphore.signal();
    });
    semaphore.wait();
}

} // namespace WTF
