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

#include "WasmPlan.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Worklist);

namespace WasmWorklistInternal {
static constexpr bool verbose = false;
}

void Worklist::dump(PrintStream& out) const
{
    out.print("Queue Size = ", m_queue.size());
}

bool Worklist::dequeuePlan(const AbstractLocker&, WorkerSlot& slot)
{
    while (!m_queue.isEmpty()) {
        slot.element = m_queue.peek();
        bool multiThreaded = slot.element.plan->multiThreaded();
        // Only one thread should validate/prepare.
        if (!multiThreaded)
            m_queue.dequeue();

        if (slot.element.plan->hasWork())
            return true;

        // There must be another thread linking this plan so we can dequeue and see if there is other work.
        if (multiThreaded)
            m_queue.dequeue();
        slot.element = QueueElement();
    }
    return false;
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
        // Multithreaded plans stay in m_queue while running; dequeue completed ones before demand can drop to 0.
        while (!m_queue.isEmpty() && m_queue.peek().plan && !m_queue.peek().plan->hasWork())
            m_queue.dequeue();
        publishDemand(locker);
    };

    Plan* plan = slot.element.plan.get();
    ASSERT(plan);

    bool wasMultiThreaded = plan->multiThreaded();
    plan->work();

    ASSERT(!plan->hasWork() || plan->multiThreaded());
    if (plan->hasWork() && !wasMultiThreaded && plan->multiThreaded()) {
        Locker locker { m_lock };
        slot.element.setToNextPriority();
        m_queue.enqueue(WTF::move(slot.element));
        complete(locker);
        return;
    }

    complete(Locker { m_lock });
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
        Locker locker { m_lock };
        for (auto& candidate : m_slots) {
            if (!candidate.element.plan) {
                slot = &candidate;
                break;
            }
        }
        if (!slot || !dequeuePlan(locker, *slot)) {
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
    Locker locker { m_lock };

    if (ASSERT_ENABLED) {
        for (const auto& element : m_queue)
            ASSERT_UNUSED(element, element.plan.get() != &plan.get());
    }

    dataLogLnIf(WasmWorklistInternal::verbose, "Enqueuing plan");
    bool multiThreaded = plan->multiThreaded();
    m_queue.enqueue({ multiThreaded ? Priority::Compilation : Priority::Preparation, nextTicket(), WTF::move(plan) });
    publishDemand(locker);
}

void Worklist::completePlanSynchronously(Plan& plan)
{
    {
        Locker locker { m_lock };
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
    Locker locker { m_lock };
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
                slot.synchronize.wait(m_lock);
        }
    }

    publishDemand(locker);
}

Worklist::Worklist()
    : m_slots(HelperThreadPool::singleton().maxConcurrency(HelperThreadClass::Wasm))
{
    HelperThreadPool::singleton().registerClient(*this, HelperThreadClass::Wasm, m_slots.size());
}

Worklist::~Worklist()
{
    HelperThreadPool::singleton().unregisterClient(*this);
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
