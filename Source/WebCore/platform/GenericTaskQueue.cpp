/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "GenericTaskQueue.h"

#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Lock MainThreadTaskDispatcher::s_sharedLock;

MainThreadTaskDispatcher::MainThreadTaskDispatcher()
{
}

void MainThreadTaskDispatcher::postTask(Function<void()>&& function)
{
    {
        Locker locker { s_sharedLock };
        m_pendingTasks.append(WTFMove(function));
        pendingDispatchers().append(makeWeakPtr(*this));
    }

    ensureOnMainThread([] {
        if (!sharedTimer().isActive())
            sharedTimer().startOneShot(0_s);
    });
}

Timer& MainThreadTaskDispatcher::sharedTimer()
{
    ASSERT(isMainThread());
    static NeverDestroyed<Timer> timer([] { MainThreadTaskDispatcher::sharedTimerFired(); });
    return timer.get();
}

void MainThreadTaskDispatcher::sharedTimerFired()
{
    ASSERT(!sharedTimer().isActive());

    // Copy the pending events first because we don't want to process synchronously the new events
    // queued by the JS events handlers that are executed in the loop below.
    Deque<WeakPtr<MainThreadTaskDispatcher>> queuedDispatchers;
    {
        Locker locker { s_sharedLock };
        queuedDispatchers = std::exchange(pendingDispatchers(), { });
    }
    while (!queuedDispatchers.isEmpty()) {
        WeakPtr<MainThreadTaskDispatcher> dispatcher = queuedDispatchers.takeFirst();
        if (!dispatcher)
            continue;
        dispatcher->dispatchOneTask();
    }
}


Deque<WeakPtr<MainThreadTaskDispatcher>>& MainThreadTaskDispatcher::pendingDispatchers()
{
    static NeverDestroyed<Deque<WeakPtr<MainThreadTaskDispatcher>>> dispatchers;
    return dispatchers.get();
}

void MainThreadTaskDispatcher::dispatchOneTask()
{
    Function<void()> task;
    {
        Locker locker { s_sharedLock };
        ASSERT(!m_pendingTasks.isEmpty());
        task = m_pendingTasks.takeFirst();
    }
    task();
}

}

