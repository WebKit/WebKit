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
#include "JITWorklistThread.h"

#if ENABLE(JIT)

#include "HeapInlines.h"
#include "JITWorklist.h"
#include "VM.h"

namespace JSC {

class JITWorklistThread::WorkScope final {
public:
    WorkScope(JITWorklistThread& thread)
        : m_thread(thread)
        , m_tier(thread.m_plan->tier())
    {
        RELEASE_ASSERT(m_thread.m_plan);
        RELEASE_ASSERT(m_thread.m_worklist.m_numberOfActiveThreads);
    }

    ~WorkScope()
    {
        Locker locker { *m_thread.m_worklist.m_lock };
        m_thread.m_plan = nullptr;
        m_thread.m_worklist.m_numberOfActiveThreads--;
        m_thread.m_worklist.m_ongoingCompilationsPerTier[static_cast<unsigned>(m_tier)]--;
    }

private:
    JITWorklistThread& m_thread;
    JITPlan::Tier m_tier;
};

JITWorklistThread::JITWorklistThread(const AbstractLocker& locker, JITWorklist& worklist)
    : AutomaticThread(locker, worklist.m_lock, worklist.m_planEnqueued.copyRef(), ThreadType::Compiler)
    , m_worklist(worklist)
{
}
const char* JITWorklistThread::name() const
{
#if OS(LINUX)
    return "JITWorker";
#else
    return "JIT Worklist Helper Thread";
#endif
}

auto JITWorklistThread::poll(const AbstractLocker& locker) -> PollResult
{
    for (unsigned i = 0; i < static_cast<unsigned>(JITPlan::Tier::Count); ++i) {
        auto& queue = m_worklist.m_queues[i];
        if (queue.isEmpty())
            continue;


        if (m_worklist.m_ongoingCompilationsPerTier[i] >= m_worklist.m_maximumNumberOfConcurrentCompilationsPerTier[i])
            continue;

        m_plan = queue.takeFirst();
        if (UNLIKELY(!m_plan)) {
            if (Options::verboseCompilationQueue()) {
                m_worklist.dump(locker, WTF::dataFile());
                dataLog(": Thread shutting down\n");
            }
            return PollResult::Stop;
        }

        RELEASE_ASSERT(m_plan->stage() == JITPlanStage::Preparing);
        m_worklist.m_numberOfActiveThreads++;
        m_worklist.m_ongoingCompilationsPerTier[i]++;
        return PollResult::Work;
    }

    return PollResult::Wait;
}

auto JITWorklistThread::work() -> WorkResult
{
    WorkScope workScope(*this);

    Locker locker { m_rightToRun };
    {
        Locker locker { *m_worklist.m_lock };
        if (m_plan->stage() == JITPlanStage::Canceled)
            return WorkResult::Continue;
        m_plan->notifyCompiling();
    }


    dataLogLnIf(Options::verboseCompilationQueue(), m_worklist, ": Compiling ", m_plan->key(), " asynchronously");

    // There's no way for the GC to be safepointing since we own rightToRun.
    if (m_plan->vm()->heap.worldIsStopped()) {
        dataLog("Heap is stopped but here we are! (1)\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_plan->compileInThread(this);
    if (m_plan->stage() != JITPlanStage::Canceled) {
        if (m_plan->vm()->heap.worldIsStopped()) {
            dataLog("Heap is stopped but here we are! (2)\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    {
        Locker locker { *m_worklist.m_lock };
        if (m_plan->stage() == JITPlanStage::Canceled)
            return WorkResult::Continue;

        m_plan->notifyReady();

        if (Options::verboseCompilationQueue()) {
            m_worklist.dump(locker, WTF::dataFile());
            dataLog(": Compiled ", m_plan->key(), " asynchronously\n");
        }

        RELEASE_ASSERT(!m_plan->vm()->heap.worldIsStopped());
        m_worklist.m_readyPlans.append(WTFMove(m_plan));
        m_worklist.m_planCompiledOrCancelled.notifyAll();
    }

    return WorkResult::Continue;
}

void JITWorklistThread::threadDidStart()
{
    dataLogLnIf(Options::verboseCompilationQueue(), m_worklist, ": Thread started");

}

void JITWorklistThread::threadIsStopping(const AbstractLocker&)
{
    dataLogLnIf(Options::verboseCompilationQueue(), m_worklist, ": Thread will stop");
    ASSERT(!m_plan);
    m_plan = nullptr;
}

} // namespace JSC

#endif // ENABLE(JIT)
