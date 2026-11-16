/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include "CPU.h"
#include "WasmPlan.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Worklist);

namespace WasmWorklistInternal {
static constexpr bool verbose = false;
}

void Worklist::dump(PrintStream& out) const
{
    out.print("Queue Size = ", m_queue.size());
}

// The Thread class is designed to prevent threads from blocking when there is still work
// in the queue. Wasm's Plans have some phases, Validiation, Preparation, and Completion,
// that can only be done by one thread, and another phase, Compilation, that can be done
// many threads. In order to stop a thread from wasting time we remove any plan that is
// is currently in a single threaded state from the work queue so other plans can run.
class Worklist::Thread final : public AutomaticThread {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Thread);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(Thread);
public:
    using Base = AutomaticThread;
    static Ref<Thread> create(const AbstractLocker& locker, Worklist& work, WorkerSlot& slot)
    {
        return adoptRef(*new Thread(locker, work, slot));
    }

private:
    Thread(const AbstractLocker& locker, Worklist& work, WorkerSlot& workerSlot)
        : Base(locker, work.m_lock, work.m_planEnqueued.copyRef(), ThreadType::Compiler, compilerThreadQOS())
        , worklist(work)
        , slot(workerSlot)
    {
    }

    PollResult poll(const AbstractLocker& locker) final
    {
        return worklist.poll(locker, slot);
    }

    WorkResult work() final
    {
        worklist.runOnSlot(slot);
        return WorkResult::Continue;
    }

    ASCIILiteral name() const final
    {
        return "Wasm Worklist Helper Thread"_s;
    }

    Worklist& worklist;
    WorkerSlot& slot;
};

auto Worklist::poll(const AbstractLocker&, WorkerSlot& slot) -> AutomaticThread::PollResult
{
    while (!m_queue.isEmpty()) {
        Priority priority = m_queue.peek().priority;
        if (priority == Worklist::Priority::Shutdown)
            return AutomaticThread::PollResult::Stop;

        slot.element = m_queue.peek();
        bool multiThreaded = slot.element.plan->multiThreaded();
        // Only one thread should validate/prepare.
        if (!multiThreaded)
            m_queue.dequeue();

        if (slot.element.plan->hasWork())
            return AutomaticThread::PollResult::Work;

        // There must be another thread linking this plan so we can dequeue and see if there is other work.
        if (multiThreaded)
            m_queue.dequeue();
        slot.element = QueueElement();
    }
    return AutomaticThread::PollResult::Wait;
}

void Worklist::runOnSlot(WorkerSlot& slot)
{
#if USE(PROTECTED_JIT)
    // Must be constructed before we allocate anything using
    // SequesteredArenaMalloc
    ArenaLifetime m_saLifetime { };
#endif

    auto complete = [&](const AbstractLocker& locker) {
        // We need to hold the lock to release our plan otherwise the main thread, while canceling plans
        // might use after free the plan we are looking at
        slot.element = QueueElement();
        slot.synchronize.notifyAll();
        if (m_usesSharedHelperThreadPool) {
            // Multithreaded plans stay in m_queue while running; dequeue completed ones before demand can drop to 0.
            while (!m_queue.isEmpty() && m_queue.peek().plan && !m_queue.peek().plan->hasWork())
                m_queue.dequeue();
            publishDemand(locker);
        }
    };

    Plan* plan = slot.element.plan.get();
    ASSERT(plan);

    bool wasMultiThreaded = plan->multiThreaded();
    plan->work();

    ASSERT(!plan->hasWork() || plan->multiThreaded());
    if (plan->hasWork() && !wasMultiThreaded && plan->multiThreaded()) {
        Locker locker { *m_lock };
        slot.element.setToNextPriority();
        m_queue.enqueue(WTF::move(slot.element));
        notifyPlanEnqueued(locker, true);
        complete(locker);
        return;
    }

    complete(Locker { *m_lock });
}

void Worklist::notifyPlanEnqueued(const AbstractLocker& locker, bool multiThreaded)
{
    if (m_usesSharedHelperThreadPool)
        publishDemand(locker);
    else if (multiThreaded)
        m_planEnqueued->notifyAll(locker);
    else
        m_planEnqueued->notifyOne(locker);
}

void Worklist::publishDemand(const AbstractLocker&)
{
    unsigned maxDemand = m_slots.size();
    unsigned demand = 0;
    for (auto& slot : m_slots) {
        if (slot.element.plan)
            demand++;
    }
    for (const auto& element : m_queue) {
        if (demand >= maxDemand)
            break;
        if (element.plan && element.plan->hasWork())
            demand += element.plan->multiThreaded() ? maxDemand : 1;
    }
    HelperThreadPool::singleton().setDemand(*this, std::min(demand, maxDemand));
}

auto Worklist::runOneTask() -> TaskResult
{
    WorkerSlot* slot = nullptr;
    {
        Locker locker { *m_lock };
        for (auto& candidate : m_slots) {
            if (!candidate.element.plan) {
                slot = &candidate;
                break;
            }
        }
        if (!slot || poll(locker, *slot) != AutomaticThread::PollResult::Work) {
            publishDemand(locker);
            return TaskResult::NoWork;
        }
    }
    runOnSlot(*slot);
    return TaskResult::Ran;
}

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
    Locker locker { *m_lock };

    if (ASSERT_ENABLED) {
        for (const auto& element : m_queue)
            ASSERT_UNUSED(element, element.plan.get() != &plan.get());
    }

    dataLogLnIf(WasmWorklistInternal::verbose, "Enqueuing plan");
    bool multiThreaded = plan->multiThreaded();
    m_queue.enqueue({ multiThreaded ? Priority::Compilation : Priority::Preparation, nextTicket(),  WTF::move(plan) });
    notifyPlanEnqueued(locker, multiThreaded);
}

void Worklist::completePlanSynchronously(Plan& plan)
{
    {
        Locker locker { *m_lock };
        m_queue.increaseKey([&] (QueueElement& element) {
            if (element.plan == &plan) {
                element.priority = Priority::Synchronous;
                return true;
            }
            return false;
        });

        for (auto& slot : m_slots) {
            if (slot.element.plan == &plan)
                slot.element.priority = Priority::Synchronous;
        }
    }

    plan.waitForCompletion();
}

void Worklist::stopAllPlansForContext(VM& vm)
{
    Locker locker { *m_lock };
    Vector<QueueElement> elements;
    while (!m_queue.isEmpty()) {
        QueueElement element = m_queue.dequeue();
        bool didCancel = element.plan->tryRemoveContextAndCancelIfLast(vm);
        if (!didCancel)
            elements.append(WTF::move(element));
    }

    for (auto& element : elements)
        m_queue.enqueue(WTF::move(element));

    for (auto& slot : m_slots) {
        RefPtr plan = slot.element.plan;
        if (plan && plan->tryRemoveContextAndCancelIfLast(vm)) {
            while (slot.element.plan == plan)
                slot.synchronize.wait(*m_lock);
        }
    }

    if (m_usesSharedHelperThreadPool)
        publishDemand(locker);
}

static unsigned numberOfWorkerSlots()
{
    if (Options::useSharedHelperThreadPool())
        return HelperThreadPool::singleton().maxConcurrency(HelperThreadClass::Wasm);
    return Options::useConcurrentJIT() ? Options::numberOfWasmCompilerThreads() : 1;
}

Worklist::Worklist()
    : m_usesSharedHelperThreadPool(Options::useSharedHelperThreadPool())
    , m_slots(numberOfWorkerSlots())
    , m_lock(Box<Lock>::create())
    , m_planEnqueued(AutomaticThreadCondition::create())
{
    if (m_usesSharedHelperThreadPool) {
        HelperThreadPool::singleton().registerClient(*this, HelperThreadClass::Wasm, m_slots.size());
        return;
    }

    Locker locker { *m_lock };
    m_threads = Vector<Ref<Thread>>(m_slots.size(), [&](size_t i) {
        return Worklist::Thread::create(locker, *this, m_slots[i]);
    });
}

Worklist::~Worklist()
{
    if (m_usesSharedHelperThreadPool) {
        HelperThreadPool::singleton().unregisterClient(*this);
        return;
    }

    {
        Locker locker { *m_lock };
        m_queue.enqueue({ Priority::Shutdown, nextTicket(), nullptr });
        m_planEnqueued->notifyAll(locker);
    }
    for (unsigned i = 0; i < m_threads.size(); ++i)
        Ref { m_threads[i] }->join();
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
