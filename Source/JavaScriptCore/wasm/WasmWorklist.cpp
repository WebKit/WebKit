/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmPlan.h"

#include <wtf/NumberOfCores.h>

namespace JSC { namespace Wasm {

static const bool verbose = false;

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
        : Base(locker, work.m_lock, work.m_planEnqueued)
        , worklist(work)
    {

    }

protected:
    PollResult poll(const AbstractLocker&) override
    {
        auto& queue = worklist.m_queue;
        synchronize.notifyAll();

        while (!queue.empty()) {

            Priority priority = queue.top().priority;
            if (priority == Worklist::Priority::Shutdown)
                return PollResult::Stop;

            element = queue.top();
            // Only one thread should validate/prepare.
            if (!queue.top().plan->hasBeenPrepared()) {
                queue.pop();
                return PollResult::Work;
            }

            if (element.plan->hasWork())
                return PollResult::Work;

            // There must be a another thread linking this plan so we can deque and see if there is other work.
            queue.pop();
            element = QueueElement();
        }
        return PollResult::Wait;
    }

    WorkResult work() override
    {
        auto complete = [&] () {
            element = QueueElement();
            return WorkResult::Continue;
        };

        Plan* plan = element.plan.get();
        ASSERT(plan);
        if (!plan->hasBeenPrepared()) {
            plan->parseAndValidateModule();
            if (!plan->hasWork())
                return complete();
            
            plan->prepare();

            LockHolder locker(*worklist.m_lock);
            element.setToNextPriority();
            worklist.m_queue.push(element);
            worklist.m_planEnqueued->notifyAll(locker);
        }

        // FIXME: this should check in occasionally to see if there are new, higher priority e.g. synchronous, plans that need to be run.
        // https://bugs.webkit.org/show_bug.cgi?id=170204
        plan->compileFunctions(Plan::Partial);

        if (!plan->hasWork()) {
            LockHolder locker(*worklist.m_lock);
            auto queue = worklist.m_queue;
            // Another thread may have removed our plan from the queue already.
            if (!queue.empty() && queue.top().plan == element.plan)
                queue.pop();
        }

        return complete();
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

enum class IterateResult { UpdateAndBreak, DropAndContinue, Continue };

template<typename Functor>
void Worklist::iterate(const AbstractLocker&, const Functor& func)
{
    // std::priority_queue does not have an iterator or decrease_key... :( While this is gross, this function
    // shouldn't be called very often and the queue should be small.
    // FIXME: we should have our own PriorityQueue that doesn't suck.
    // https://bugs.webkit.org/show_bug.cgi?id=170145
    std::vector<QueueElement> elements;
    while (!m_queue.empty()) {
        QueueElement element = m_queue.top();
        m_queue.pop();
        IterateResult result = func(element);
        if (result == IterateResult::UpdateAndBreak) {
            elements.push_back(element);
            break;
        }

        if (result == IterateResult::Continue)
            elements.push_back(element);
        continue;
    }
    
    for (auto element : elements)
        m_queue.push(element);
}

void Worklist::enqueue(Ref<Plan> plan)
{
    LockHolder locker(*m_lock);

    if (!ASSERT_DISABLED) {
        iterate(locker, [&] (QueueElement& element) {
            ASSERT_UNUSED(element, element.plan.get() != &plan.get());
            return IterateResult::Continue;
        });
    }

    dataLogLnIf(verbose, "Enqueuing plan");
    m_queue.push({ Priority::Preparation, nextTicket(),  WTFMove(plan) });
    m_planEnqueued->notifyOne(locker);
}

void Worklist::completePlanSynchronously(Plan& plan)
{
    {
        LockHolder locker(*m_lock);
        iterate(locker, [&] (QueueElement& element) {
            if (element.plan == &plan) {
                element.priority = Priority::Synchronous;
                return IterateResult::UpdateAndBreak;
            }
            return IterateResult::Continue;
        });

        for (auto& thread : m_threads) {
            if (thread->element.plan == &plan)
                thread->element.priority = Priority::Synchronous;
        }
    }

    plan.waitForCompletion();
}

void Worklist::stopAllPlansForVM(VM& vm)
{
    LockHolder locker(*m_lock);
    iterate(locker, [&] (QueueElement& element) {
        bool didCancel = element.plan->tryRemoveVMAndCancelIfLast(vm);
        if (didCancel)
            return IterateResult::DropAndContinue;
        return IterateResult::Continue;
    });

    for (auto& thread : m_threads) {
        if (thread->element.plan) {
            bool didCancel = thread->element.plan->tryRemoveVMAndCancelIfLast(vm);
            if (didCancel) {
                // We don't have to worry about the deadlocking since the thread can't block without clearing the plan and must hold the lock to do so.
                thread->synchronize.wait(*m_lock);
            }
        }
    }
}

Worklist::Worklist()
    : m_lock(Box<Lock>::create())
    , m_planEnqueued(AutomaticThreadCondition::create())
{
    unsigned numberOfCompilationThreads = Options::useConcurrentJIT() ? WTF::numberOfProcessorCores() : 1;
    m_threads.reserveCapacity(numberOfCompilationThreads);
    LockHolder locker(*m_lock);
    for (unsigned i = 0; i < numberOfCompilationThreads; i++)
        m_threads.uncheckedAppend(std::make_unique<Worklist::Thread>(locker, *this));
}

Worklist::~Worklist()
{
    {
        LockHolder locker(*m_lock);
        m_queue.push({ Priority::Shutdown, nextTicket(), nullptr });
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
        globalWorklist = new Worklist();
    });
    return *globalWorklist;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
