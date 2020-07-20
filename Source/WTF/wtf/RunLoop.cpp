/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include <wtf/RunLoop.h>

#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSpecific.h>

namespace WTF {

static RunLoop* s_mainRunLoop;
#if USE(WEB_THREAD)
static RunLoop* s_webRunLoop;
#endif

// Helper class for ThreadSpecificData.
class RunLoop::Holder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Holder()
        : m_runLoop(adoptRef(*new RunLoop))
    {
    }

    RunLoop& runLoop() { return m_runLoop; }

private:
    Ref<RunLoop> m_runLoop;
};

void RunLoop::initializeMain()
{
    RELEASE_ASSERT(!s_mainRunLoop);
    s_mainRunLoop = &RunLoop::current();
}

RunLoop& RunLoop::current()
{
    static NeverDestroyed<ThreadSpecific<Holder>> runLoopHolder;
    return runLoopHolder.get()->runLoop();
}

RunLoop& RunLoop::main()
{
    ASSERT(s_mainRunLoop);
    return *s_mainRunLoop;
}

#if USE(WEB_THREAD)
void RunLoop::initializeWeb()
{
    RELEASE_ASSERT(!s_webRunLoop);
    s_webRunLoop = &RunLoop::current();
}

RunLoop& RunLoop::web()
{
    ASSERT(s_webRunLoop);
    return *s_webRunLoop;
}

RunLoop* RunLoop::webIfExists()
{
    return s_webRunLoop;
}
#endif

bool RunLoop::isMain()
{
    ASSERT(s_mainRunLoop);
    return s_mainRunLoop == &RunLoop::current();
}

void RunLoop::performWork()
{
    bool didSuspendFunctions = false;

    {
        auto locker = holdLock(m_nextIterationLock);

        // If the RunLoop re-enters or re-schedules, we're expected to execute all functions in order.
        while (!m_currentIteration.isEmpty())
            m_nextIteration.prepend(m_currentIteration.takeLast());

        m_currentIteration = std::exchange(m_nextIteration, { });
    }

    while (!m_currentIteration.isEmpty()) {
        if (m_isFunctionDispatchSuspended) {
            didSuspendFunctions = true;
            break;
        }

        auto function = m_currentIteration.takeFirst();
        function();
    }

    // Suspend only for a single cycle.
    m_isFunctionDispatchSuspended = false;
    m_hasSuspendedFunctions = didSuspendFunctions;

    if (m_hasSuspendedFunctions)
        wakeUp();
}

void RunLoop::dispatch(Function<void ()>&& function)
{
    bool needsWakeup = false;

    {
        auto locker = holdLock(m_nextIterationLock);
        needsWakeup = m_nextIteration.isEmpty();
        m_nextIteration.append(WTFMove(function));
    }

    if (needsWakeup)
        wakeUp();
}

void RunLoop::dispatchAfter(Seconds delay, Function<void()>&& function)
{
    auto timer = new DispatchTimer(*this);
    timer->setFunction([timer, function = WTFMove(function)] {
        function();
        delete timer;
    });
    timer->startOneShot(delay);
}

void RunLoop::suspendFunctionDispatchForCurrentCycle()
{
    // Don't suspend if there are already suspended functions to avoid unexecuted function pile-up.
    if (m_isFunctionDispatchSuspended || m_hasSuspendedFunctions)
        return;

    m_isFunctionDispatchSuspended = true;
    // Wake up (even if there is nothing to do) to disable suspension.
    wakeUp();
}

} // namespace WTF
