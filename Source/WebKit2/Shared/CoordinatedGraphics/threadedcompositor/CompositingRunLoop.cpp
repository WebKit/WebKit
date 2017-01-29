/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "CompositingRunLoop.h"

#if USE(COORDINATED_GRAPHICS_THREADED)

#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>

#if USE(GLIB_EVENT_LOOP)
#include <glib.h>
#endif

namespace WebKit {

class WorkQueuePool {
    WTF_MAKE_NONCOPYABLE(WorkQueuePool);
    friend class NeverDestroyed<WorkQueuePool>;
public:
    static WorkQueuePool& singleton()
    {
        ASSERT(isMainThread());
        static NeverDestroyed<WorkQueuePool> workQueuePool;
        return workQueuePool;
    }

    void dispatch(void* context, Function<void ()>&& function)
    {
        ASSERT(isMainThread());
        getOrCreateWorkQueueForContext(context).dispatch(WTFMove(function));
    }

    RunLoop& runLoop(void* context)
    {
        return getOrCreateWorkQueueForContext(context).runLoop();
    }

    void invalidate(void* context)
    {
        auto workQueue = m_workQueueMap.take(context);
        ASSERT(workQueue);
        if (m_workQueueMap.isEmpty()) {
            m_sharedWorkQueue = nullptr;
            m_threadCount = 0;
        } else if (workQueue->hasOneRef())
            m_threadCount--;
    }

private:
    WorkQueuePool()
    {
#if PLATFORM(GTK)
        m_threadCountLimit = 1;
#else
        m_threadCountLimit = std::numeric_limits<unsigned>::max();
#endif
    }

    WorkQueue& getOrCreateWorkQueueForContext(void* context)
    {
        auto addResult = m_workQueueMap.add(context, nullptr);
        if (addResult.isNewEntry) {
            // FIXME: This is OK for now, and it works for a single-thread limit. But for configurations where more (but not unlimited)
            // threads could be used, one option would be to use a HashSet here and disperse the contexts across the available threads.
            if (m_threadCount >= m_threadCountLimit) {
                ASSERT(m_sharedWorkQueue);
                addResult.iterator->value = m_sharedWorkQueue;
            } else {
                addResult.iterator->value = WorkQueue::create("org.webkit.ThreadedCompositorWorkQueue");
                if (!m_threadCount)
                    m_sharedWorkQueue = addResult.iterator->value;
                m_threadCount++;
            }
        }

        return *addResult.iterator->value;
    }

    HashMap<void*, RefPtr<WorkQueue>> m_workQueueMap;
    RefPtr<WorkQueue> m_sharedWorkQueue;
    unsigned m_threadCount { 0 };
    unsigned m_threadCountLimit;
};

CompositingRunLoop::CompositingRunLoop(std::function<void ()>&& updateFunction)
    : m_updateTimer(WorkQueuePool::singleton().runLoop(this), this, &CompositingRunLoop::updateTimerFired)
    , m_updateFunction(WTFMove(updateFunction))
{
#if USE(GLIB_EVENT_LOOP)
    m_updateTimer.setPriority(G_PRIORITY_HIGH_IDLE);
#endif
}

CompositingRunLoop::~CompositingRunLoop()
{
    ASSERT(isMainThread());
    // Make sure the WorkQueue is deleted after the CompositingRunLoop, because m_updateTimer has a reference
    // of the WorkQueue run loop. Passing this is not a problem because the pointer will only be used as a
    // HashMap key by WorkQueuePool.
    RunLoop::main().dispatch([context = this] { WorkQueuePool::singleton().invalidate(context); });
}

void CompositingRunLoop::performTask(Function<void ()>&& function)
{
    ASSERT(isMainThread());
    WorkQueuePool::singleton().dispatch(this, WTFMove(function));
}

void CompositingRunLoop::performTaskSync(Function<void ()>&& function)
{
    ASSERT(isMainThread());
    LockHolder locker(m_dispatchSyncConditionMutex);
    WorkQueuePool::singleton().dispatch(this, [this, function = WTFMove(function)] {
        function();
        LockHolder locker(m_dispatchSyncConditionMutex);
        m_dispatchSyncCondition.notifyOne();
    });
    m_dispatchSyncCondition.wait(m_dispatchSyncConditionMutex);
}

void CompositingRunLoop::startUpdateTimer(UpdateTiming timing)
{
    if (m_updateTimer.isActive())
        return;

    const static double targetFPS = 60;
    double nextUpdateTime = 0;
    if (timing == WaitUntilNextFrame)
        nextUpdateTime = std::max((1 / targetFPS) - (monotonicallyIncreasingTime() - m_lastUpdateTime), 0.0);

    m_updateTimer.startOneShot(nextUpdateTime);
}

void CompositingRunLoop::stopUpdateTimer()
{
    m_updateTimer.stop();
}

void CompositingRunLoop::updateTimerFired()
{
    m_updateFunction();
    m_lastUpdateTime = monotonicallyIncreasingTime();
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)
