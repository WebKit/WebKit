/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RealtimeAudioThread.h"

#include <wtf/MonotonicTime.h>

namespace WebCore {

static constexpr uint8_t s_maximumConcurrentRealtimeThreads { 3 };

Ref<Thread> createMaybeRealtimeAudioThread(ASCIILiteral threadName, Function<void()>&& entryPoint, Seconds rawRenderingQuantumDuration)
{
    // Create a thread with Realtime scheduling, so that this thread does not become deprioritized
    // during heavy loads. Realtime threads have a runtime cost, so ensure no more than three realtime
    // audio threads can be created simultaneously.
    // FIXME: Coalesce these threads to allow a single realtime thread to service every audio instance.

    bool shouldCreateRealtimeThread = [] {
        Locker threadLocker { Thread::allThreadsLock() };
        uint8_t numberOfRealtimeThreads = 0;
        for (RefPtr thread : Thread::allThreads()) {
            if (thread && thread->isRealtime())
                ++numberOfRealtimeThreads;
        }

        return numberOfRealtimeThreads < s_maximumConcurrentRealtimeThreads;
    }();

    auto schedulingPolicy = shouldCreateRealtimeThread ? Thread::SchedulingPolicy::Realtime : Thread::SchedulingPolicy::Other;

    auto thread = Thread::create(threadName, WTFMove(entryPoint), ThreadType::Audio, Thread::QOS::UserInteractive, schedulingPolicy);

#if HAVE(THREAD_TIME_CONSTRAINTS)
    if (shouldCreateRealtimeThread) {
        // Add thread time constraints to allow the system to ensure contiguous scheduling for this thread
        auto renderingQuantumDuration = MonotonicTime::fromRawSeconds(rawRenderingQuantumDuration.seconds());
        auto renderingTimeConstraint = MonotonicTime::fromRawSeconds(rawRenderingQuantumDuration.seconds() * 2);
        thread->setThreadTimeConstraints(renderingQuantumDuration, renderingQuantumDuration, renderingTimeConstraint, true);
    }
#else
    UNUSED_PARAM(rawRenderingQuantumDuration);
#endif

    // Roughly match the priority of the Audio IO thread in the GPU process.
    thread->changePriority(60);

    return thread;
}

}
