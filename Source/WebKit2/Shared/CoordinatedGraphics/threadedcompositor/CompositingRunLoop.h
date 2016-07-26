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

#ifndef CompositingRunLoop_h
#define CompositingRunLoop_h

#if USE(COORDINATED_GRAPHICS_THREADED)

#include <wtf/Condition.h>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class CompositingRunLoop {
    WTF_MAKE_NONCOPYABLE(CompositingRunLoop);
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum UpdateTiming {
        Immediate,
        WaitUntilNextFrame,
    };

    CompositingRunLoop(std::function<void ()>&&);
    ~CompositingRunLoop();

    void performTask(Function<void ()>&&);
    void performTaskSync(Function<void ()>&&);

    void startUpdateTimer(UpdateTiming = Immediate);
    void stopUpdateTimer();

private:
    void updateTimerFired();

    RunLoop::Timer<CompositingRunLoop> m_updateTimer;
    std::function<void ()> m_updateFunction;
    Lock m_dispatchSyncConditionMutex;
    Condition m_dispatchSyncCondition;

    double m_lastUpdateTime { 0 };
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)

#endif // CompositingRunLoop_h
