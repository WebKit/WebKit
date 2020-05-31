/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "WasmWorklist.h"
#include "WasmLLIntGenerator.h"

#if ENABLE(WEBASSEMBLY)

#include "CPU.h"
#include "WasmPlan.h"

namespace JSC { namespace Wasm {

namespace WasmWorklistInternal {
static constexpr bool verbose = false;
}

const char* Worklist::priorityString(Priority priority)
{
    switch (priority) {
    case Priority::Preparation: return "Preparation";
    case Priority::Shutdown: return "Shutdown";
    case Priority::Compilation: return "Compilation";
    case Priority::Synchronous: return "Synchronous";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

// The Thread class is designed to prevent threads from blocking when there is still work
// in the queue. Wasm's Plans have some phases, Validiation, Preparation, and Completion,
// that can only be done by one thread, and another phase, Compilation, that can be done
// many threads. In order to stop a thread from wasting time we remove any plan that is
// is currently in a single threaded state from the work queue so other plans can run.
class Worklist::Thread final : public AutomaticThread {
public:
    using Base = AutomaticThread;
    Thread(const AbstractLocker& locker, Worklist& work)
        : Base(locker, work.m_lock, work.m_planEnqueued.copyRef())
        , worklist(work)
    {

    }

private:
    PollResult poll(const AbstractLocker&) final
    {
        auto& queue = worklist.m_queue;
        synchronize.notifyAll();

        while (!queue.isEmpty()) {
            Priority priority = queue.peek().priority;
            if (priority == Worklist::Priority::Shutdown)
                return PollResult::Stop;

            element = queue.peek();
            // Only one thread should validate/prepare.
            if (!queue.peek().plan->multiThreaded())
                queue.dequeue();

            if (element.plan->hasWork())
                return PollResult::Work;

            // There must be a another thread linking this plan so we can deque and see if there is other work.
            queue.dequeue();
            element = QueueElement();
        }
        return PollResult::Wait;
    }

    WorkResult work() final
    {
        auto complete = [&] (const AbstractLocker&) {
            // We need to hold the lock to release our plan otherwise the main thread, while canceling plans
            // might use after free the plan we are looking at
            element = QueueElement();
            return WorkResult::Continue;
        };

        Plan* plan = element.plan.get();
        ASSERT(plan);

        bool wasMultiThreaded = plan->multiThreaded();
        plan->work(Plan::Partial);

        ASSERT(!plan->hasWork() || plan->multiThreaded());
        if (plan->hasWork() && !wasMultiThreaded && plan->multiThreaded()) {
            LockHolder locker(*worklist.m_lock);
            element.setToNextPriority();
            worklist.m_queue.enqueue(WTFMove(element));
            worklist.m_planEnqueued->notifyAll(locker);
            return complete(locker);
        }

        return complete(holdLock(*worklist.m_lock));
    }

    void threadIsStopping(const AbstractLocker&) final
    {
        clearLLIntThreadSpecificCache();
        clearAssembleDataThreadSpecificCache();
    }

    const char* name() const final
    {
        return "Wasm Worklist Helper Thread";
    }

public:
    Condition synchronize;
    Worklist& worklist;
    // We can only modify element when holding the lock. A synchronous compile might look at each thread's tasks in order to boost the priority.
    QueueElement element;
};

void Worklist::QueueElement::setToNextPriority()
{
    switch (priority) {
    case Priority::Preparation:
        priority = Priority::Compilation;
        return;
    case Priority::Synchronous:
        return;
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Worklist::enqueue(Ref<Plan> plan)
{
    LockHolder locker(*m_lock);

    if (ASSERT_ENABLED) {
        for (const auto& element : m_queue)
            ASSERT_UNUSED(element, element.plan.get() != &plan.get());
    }

    dataLogLnIf(WasmWorklistInternal::verbose, "Enqueuing plan");
    bool multiThreaded = plan->multiThreaded();
    m_queue.enqueue({ multiThreaded ? Priority::Compilation : Priority::Preparation, nextTicket(),  WTFMove(plan) });
    if (multiThreaded)
        m_planEnqueued->notifyAll(locker);
    else
        m_planEnqueued->notifyOne(locker);
}

void Worklist::completePlanSynchronously(Plan& plan)
{
    {
        LockHolder locker(*m_lock);
        m_queue.decreaseKey([&] (QueueElement& element) {
            if (element.plan == &plan) {
                element.priority = Priority::Synchronous;
                return true;
            }
            return false;
        });

        for (auto& thread : m_threads) {
            if (thread->element.plan == &plan)
                thread->element.priority = Priority::Synchronous;
        }
    }

    plan.waitForCompletion();
}

void Worklist::stopAllPlansForContext(Context& context)
{
    LockHolder locker(*m_lock);
    Vector<QueueElement> elements;
    while (!m_queue.isEmpty()) {
        QueueElement element = m_queue.dequeue();
        bool didCancel = element.plan->tryRemoveContextAndCancelIfLast(context);
        if (!didCancel)
            elements.append(WTFMove(element));
    }

    for (auto& element : elements)
        m_queue.enqueue(WTFMove(element));

    for (auto& thread : m_threads) {
        if (thread->element.plan) {
            bool didCancel = thread->element.plan->tryRemoveContextAndCancelIfLast(context);
            if (didCancel) {
                // We don't have to worry about the deadlocking since the thread can't block without checking for a new plan and must hold the lock to do so.
                thread->synchronize.wait(*m_lock);
            }
        }
    }
}

Worklist::Worklist()
    : m_lock(Box<Lock>::create())
    , m_planEnqueued(AutomaticThreadCondition::create())
{
    unsigned numberOfCompilationThreads = Options::useConcurrentJIT() ? kernTCSMAwareNumberOfProcessorCores() : 1;
    m_threads.reserveCapacity(numberOfCompilationThreads);
    LockHolder locker(*m_lock);
    for (unsigned i = 0; i < numberOfCompilationThreads; i++)
        m_threads.uncheckedAppend(makeUnique<Worklist::Thread>(locker, *this));
}

Worklist::~Worklist()
{
    {
        LockHolder locker(*m_lock);
        m_queue.enqueue({ Priority::Shutdown, nextTicket(), nullptr });
        m_planEnqueued->notifyAll(locker);
    }
    for (unsigned i = 0; i < m_threads.size(); ++i)
        m_threads[i]->join();
}

static Worklist* globalWorklist;

Worklist* existingWorklistOrNull() { return globalWorklist; }
Worklist& ensureWorklist()
{
    static std::once_flag initializeWorklist;
    std::call_once(initializeWorklist, [] {
        Worklist* worklist = new Worklist();
        WTF::storeStoreFence();
        globalWorklist = worklist;
    });
    return *globalWorklist;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
