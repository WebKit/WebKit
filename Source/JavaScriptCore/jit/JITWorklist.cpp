/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include "JITWorklist.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "DeferGCInlines.h"
#include "HeapInlines.h"
#include "JITSafepoint.h"
#include "JITWorklistThread.h"
#include "SlotVisitorInlines.h"
#include "VMInlines.h"
#include <wtf/CompilationThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITWorklist);

JITWorklist::JITWorklist()
    : m_lock(Box<Lock>::create())
    , m_planEnqueued(AutomaticThreadCondition::create())
{
    m_maximumNumberOfConcurrentCompilationsPerTier = {
        Options::numberOfBaselineCompilerThreads(),
        Options::numberOfDFGCompilerThreads(),
        Options::numberOfFTLCompilerThreads(),
    };
    m_loadWeightsPerTier = {
        Options::worklistBaselineLoadWeight(),
        Options::worklistDFGLoadWeight(),
        Options::worklistFTLLoadWeight(),
    };

    Locker locker { *m_lock };
    for (unsigned i = 0; i < Options::maxNumberOfWorklistThreads(); ++i)
        m_threads.append(*new JITWorklistThread(locker, *this));
}

JITWorklist::~JITWorklist()
{
    UNREACHABLE_FOR_PLATFORM();
}

static JITWorklist* theGlobalJITWorklist { nullptr };

JITWorklist* JITWorklist::existingGlobalWorklistOrNull()
{
    return theGlobalJITWorklist;
}

JITWorklist& JITWorklist::ensureGlobalWorklist()
{
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            auto* worklist = new JITWorklist();
            WTF::storeStoreFence();
            theGlobalJITWorklist = worklist;
        });
    return *theGlobalJITWorklist;
}

unsigned JITWorklist::planLoad(JITPlan& plan)
{
    ASSERT(plan.stage() != JITPlanStage::Canceled);
    constexpr auto maxTier = static_cast<unsigned>(JITPlan::Tier::Count) - 1;

    auto tier = static_cast<unsigned>(plan.tier());
    auto size = plan.codeBlock()->instructionsSize();
    // Really large codeblocks will take more time to compile than is typical for their tier,
    // so use the load weights for a higher tier in those cases.
    if (size >= 12000)
        tier += 2;
    else if (size >= 2000)
        tier += 1;
    tier = std::min(tier, maxTier);
    return m_loadWeightsPerTier[tier];
}

// wakeThreads wakes up compiler worker threads, if appropriate.
//
// There is a cost to running more worker threads. For example, there is a direct cost
// to wake (or spawn) a thread, and additional threads lead to more synchronization
// overhead between threads, colder CPU and software (e.g. allocator) caches, more
// contention for cpu resources across the system, more cpu scheduler overhead, etc.
//
// So, it's better to have a short queue of work ready for each compiler thread rather
// than aggressively spinning up a thread whenever there is any work pending.
// Yet, the queues should not get too long as to increase compiler latency significantly.
// wakeThreads applies a load-based heuristic to determine whether it's worthwhile
// to use more compiler threads.
//
// The load is computed from the queue-depth of each tier scaled by a per-tier weight,
// to model the the increasing compile latency at each tier. The heuristic wakes
// more threads only when the capacity of the thread pool, as determined by the number
// of threads and a desired load-factor, is exceeded.
void JITWorklist::wakeThreads(const AbstractLocker& locker, unsigned enqueuedTier)
{
    unsigned targetNumThreads;

    if (m_numberOfActiveThreads < Options::minNumberOfWorklistThreads()
        && m_ongoingCompilationsPerTier[enqueuedTier] < m_maximumNumberOfConcurrentCompilationsPerTier[enqueuedTier]) {
        targetNumThreads = m_numberOfActiveThreads + 1;
    } else {
        unsigned maxThreads = 0;
        for (unsigned tier = 0; tier < static_cast<unsigned>(JITPlan::Tier::Count); tier++) {
            unsigned plansForTier = m_ongoingCompilationsPerTier[tier] + m_queues[tier].size();

            unsigned maxThreadsUsedForTier = std::min(plansForTier, m_maximumNumberOfConcurrentCompilationsPerTier[tier]);
            maxThreads += maxThreadsUsedForTier;
        }
        maxThreads = std::min(maxThreads, Options::maxNumberOfWorklistThreads());

        ASSERT(m_totalLoad);
        targetNumThreads = (m_totalLoad + Options::worklistLoadFactor() - 1) / Options::worklistLoadFactor();
        targetNumThreads = std::min(targetNumThreads, maxThreads);
    }
    while (m_numberOfActiveThreads < targetNumThreads) {
        m_planEnqueued->notifyOne(locker);
        m_numberOfActiveThreads++;
    }
    ASSERT(m_numberOfActiveThreads >= 1);
}

CompilationResult JITWorklist::enqueue(Ref<JITPlan> plan)
{
    if (!Options::useConcurrentJIT()) {
#if USE(PROTECTED_JIT)
        // Must be constructed before we allocate anything using SequesteredArenaMalloc
        ArenaLifetime saLifetime;
#endif
        plan->beginSignpost();
        plan->compileInThread(nullptr);
        if (plan->stage() != JITPlanStage::Canceled)
            plan->endSignpost();
        return plan->finalize();
    }
    ASSERT(plan->stage() == JITPlanStage::Preparing);
    plan->beginSignpost();

    Locker locker { *m_lock };
    if (Options::verboseCompilationQueue()) {
        dump(locker, WTF::dataFile());
        dataLog(": Enqueueing plan to optimize ", plan->key(), "\n");
    }

    auto tier = static_cast<unsigned>(plan->tier());

    ASSERT(m_plans.find(plan->key()) == m_plans.end());
    m_plans.add(plan->key(), plan.copyRef());
    m_totalLoad += planLoad(plan);
    m_queues[tier].append(WTFMove(plan));
    wakeThreads(locker, tier);
    return CompilationResult::CompilationDeferred;
}

size_t JITWorklist::queueLength() const
{
    Locker locker { *m_lock };
    return queueLength(locker);
}

size_t JITWorklist::queueLength(const AbstractLocker&) const
{
    size_t queueLength = 0;
    for (unsigned i = 0; i < static_cast<unsigned>(JITPlan::Tier::Count); ++i)
        queueLength += m_queues[i].size();
    return queueLength;
}

size_t JITWorklist::totalOngoingCompilations(const AbstractLocker&) const
{
    size_t total = 0;
    for (unsigned i = 0; i < static_cast<unsigned>(JITPlan::Tier::Count); ++i)
        total += m_ongoingCompilationsPerTier[i];
    return total;
}

void JITWorklist::suspendAllThreads() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    m_suspensionLock.lock();
    Vector<Ref<JITWorklistThread>, 8> busyThreads;
    for (auto& thread : m_threads) {
        if (!thread->m_rightToRun.tryLock())
            busyThreads.append(thread.copyRef());
    }
    for (auto& thread : busyThreads)
        thread->m_rightToRun.lock();
}

void JITWorklist::resumeAllThreads() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    for (auto& thread : m_threads)
        thread->m_rightToRun.unlock();
    m_suspensionLock.unlock();
}

auto JITWorklist::compilationState(VM& vm, JITCompilationKey key) -> State
{
    if (!vm.numberOfActiveJITPlans())
        return NotKnown;

    Locker locker { *m_lock };
    const auto& iter = m_plans.find(key);
    if (iter == m_plans.end())
        return NotKnown;
    return iter->value->stage() == JITPlanStage::Ready ? Compiled : Compiling;
}

auto JITWorklist::completeAllReadyPlansForVM(VM& vm, JITCompilationKey requestedKey) -> State
{
    if (!vm.numberOfActiveJITPlans())
        return NotKnown;

    DeferGC deferGC(vm);

    Vector<RefPtr<JITPlan>, 8> myReadyPlans;
    State resultingState = removeAllReadyPlansForVM(vm, myReadyPlans, requestedKey);
    for (auto& plan : myReadyPlans) {
        dataLogLnIf(Options::verboseCompilationQueue(), *this, ": Completing ", plan->key());
        RELEASE_ASSERT(plan->stage() == JITPlanStage::Ready);
        plan->finalize();
        plan->endSignpost();
    }
    return resultingState;
}


void JITWorklist::waitUntilAllPlansForVMAreReady(VM& vm)
{
    DeferGC deferGC(vm);

    // While we are waiting for the compiler to finish, the collector might have already suspended
    // the compiler and then it will be waiting for us to stop. That's a deadlock. We avoid that
    // deadlock by relinquishing our heap access, so that the collector pretends that we are stopped
    // even if we aren't.
    // There can be the case where we already released heap access, for example when the VM is being
    // destroyed as a result of JSLock::unlock unlocking the last reference to the VM.
    // So we use a Release access scope that checks if we currently have access before releasing and later restoring.
    ReleaseHeapAccessIfNeededScope releaseHeapAccessScope(vm.heap);

    // Wait for all of the plans for the given VM to complete. The idea here
    // is that we want all of the caller VM's plans to be done. We don't care
    // about any other VM's plans, and we won't attempt to wait on those.
    // After we release this lock, we know that although other VMs may still
    // be adding plans, our VM will not be.
    Locker locker { *m_lock };

    if (Options::verboseCompilationQueue()) {
        dump(locker, WTF::dataFile());
        dataLog(": Waiting for all in VM to complete.\n");
    }

    for (;;) {
        bool allAreCompiled = true;
        for (const auto& entry : m_plans) {
            if (entry.value->vm() != &vm)
                continue;
            if (entry.value->stage() != JITPlanStage::Ready) {
                allAreCompiled = false;
                break;
            }
        }

        if (allAreCompiled)
            break;

        m_planCompiledOrCancelled.wait(*m_lock);
    }
}

void JITWorklist::completeAllPlansForVM(VM& vm)
{
    if (!vm.numberOfActiveJITPlans())
        return;

    DeferGC deferGC(vm);
    waitUntilAllPlansForVMAreReady(vm);
    completeAllReadyPlansForVM(vm);
}

void JITWorklist::cancelAllPlansForVM(VM& vm)
{
    if (!vm.numberOfActiveJITPlans())
        return;

    removeMatchingPlansForVM(vm, [&](JITPlan& plan) {
        return plan.stage() != JITPlanStage::Compiling;
    });

    waitUntilAllPlansForVMAreReady(vm);

    Vector<RefPtr<JITPlan>, 8> myReadyPlans;
    removeAllReadyPlansForVM(vm, myReadyPlans, { });
    for (auto& plan : myReadyPlans) {
        ASSERT(plan->stage() == JITPlanStage::Ready);
        plan->endSignpost(JITPlan::SignpostDetail::Canceled);
    }
}

void JITWorklist::removeDeadPlans(VM& vm)
{
    if (!vm.numberOfActiveJITPlans())
        return;

    removeMatchingPlansForVM(vm, [&](JITPlan& plan) {
        if (!plan.isKnownToBeLiveAfterGC())
            return true;
        plan.finalizeInGC();
        return false;
    });

    // No locking needed for this part, see comment in visitWeakReferences().
    for (auto& thread : m_threads) {
        thread->m_rightToRun.assertIsOwner();
        Safepoint* safepoint = thread->m_safepoint;
        if (!safepoint)
            continue;
        if (safepoint->vm() != &vm)
            continue;
        if (safepoint->isKnownToBeLiveAfterGC())
            continue;
        safepoint->cancel();
    }
}

unsigned JITWorklist::setMaximumNumberOfConcurrentDFGCompilations(unsigned n)
{
    unsigned oldValue = m_maximumNumberOfConcurrentCompilationsPerTier[static_cast<unsigned>(JITPlan::Tier::DFG)];
    m_maximumNumberOfConcurrentCompilationsPerTier[static_cast<unsigned>(JITPlan::Tier::DFG)] = n;
    return oldValue;
}

unsigned JITWorklist::setMaximumNumberOfConcurrentFTLCompilations(unsigned n)
{
    unsigned oldValue = m_maximumNumberOfConcurrentCompilationsPerTier[static_cast<unsigned>(JITPlan::Tier::FTL)];
    m_maximumNumberOfConcurrentCompilationsPerTier[static_cast<unsigned>(JITPlan::Tier::FTL)] = n;
    return oldValue;
}

template<typename Visitor>
void JITWorklist::visitWeakReferences(Visitor& visitor)
{
    VM* vm = &visitor.heap()->vm();
    if (!vm->numberOfActiveJITPlans())
        return;
    {
        Locker locker { *m_lock };
        for (auto& entry : m_plans) {
            if (entry.value->vm() != vm)
                continue;
            entry.value->checkLivenessAndVisitChildren(visitor);
        }
    }
    // This loop doesn't need locking because:
    // (1) no new threads can be added to m_threads. Hence, it is immutable and needs no locks.
    // (2) JITWorklistThread::m_safepoint is protected by that thread's m_rightToRun which we must be
    //     holding here because of a prior call to suspendAllThreads().
    for (auto& thread : m_threads) {
        thread->m_rightToRun.assertIsOwner();
        Safepoint* safepoint = thread->m_safepoint;
        if (safepoint && safepoint->vm() == vm)
            safepoint->checkLivenessAndVisitChildren(visitor);
    }
}
template void JITWorklist::visitWeakReferences(AbstractSlotVisitor&);
template void JITWorklist::visitWeakReferences(SlotVisitor&);

void JITWorklist::dump(PrintStream& out) const
{
    Locker locker { *m_lock };
    dump(locker, out);
}

void JITWorklist::dump(const AbstractLocker& locker, PrintStream& out) const
{
    out.print(
        "JITWorklist(", RawPointer(this), ")[Queue Length = ", queueLength(locker),
        ", Map Size = ", m_plans.size(), ", Num Ready = ", m_readyPlans.size(),
        ", Num Active Threads = ", m_numberOfActiveThreads, "/", m_threads.size(), "]");
}

JITWorklist::State JITWorklist::removeAllReadyPlansForVM(VM& vm, Vector<RefPtr<JITPlan>, 8>& myReadyPlans, JITCompilationKey requestedKey)
{
    DeferGC deferGC(vm);
    Locker locker { *m_lock };

    bool isCompiled = false;
    m_readyPlans.removeAllMatching([&](RefPtr<JITPlan> plan) {
        if (plan->vm() != &vm)
            return false;
        if (plan->stage() != JITPlanStage::Ready)
            return false;
        if (plan->key() == requestedKey)
            isCompiled = true;
        m_plans.remove(plan->key());
        myReadyPlans.append(WTFMove(plan));
        return true;
    });

    if (requestedKey) {
        if (isCompiled)
            return Compiled;

        if (m_plans.contains(requestedKey))
            return Compiling;
    }
    return NotKnown;
}

template<typename MatchFunction>
void JITWorklist::removeMatchingPlansForVM(VM& vm, const MatchFunction& matches)
{
    Locker locker { *m_lock };
    UncheckedKeyHashSet<JITCompilationKey> deadPlanKeys;
    for (auto& entry : m_plans) {
        JITPlan* plan = entry.value.get();
        if (plan->vm() != &vm)
            continue;
        if (!matches(*plan))
            continue;
        RELEASE_ASSERT(plan->stage() != JITPlanStage::Canceled);
        deadPlanKeys.add(plan->key());
    }
    for (auto& queue : m_queues) {
        Deque<RefPtr<JITPlan>> newQueue;
        while (!queue.isEmpty()) {
            RefPtr<JITPlan> plan = queue.takeFirst();
            if (deadPlanKeys.contains(plan->key())) {
                ASSERT(m_totalLoad >= planLoad(*plan));
                m_totalLoad -= planLoad(*plan);
            } else
                newQueue.append(plan);
        }
        queue.swap(newQueue);
    }
    ASSERT(!m_totalLoad == (!queueLength(locker) && !totalOngoingCompilations(locker)));

    bool didCancelPlans = !deadPlanKeys.isEmpty();
    for (JITCompilationKey key : deadPlanKeys)
        m_plans.take(key)->cancel();
    for (unsigned i = 0; i < m_readyPlans.size(); ++i) {
        if (m_readyPlans[i]->stage() != JITPlanStage::Canceled)
            continue;
        m_readyPlans[i--] = m_readyPlans.last();
        m_readyPlans.removeLast();
    }
    if (didCancelPlans)
        m_planCompiledOrCancelled.notifyAll();
}

} // namespace JSC

#endif // ENABLE(JIT)

