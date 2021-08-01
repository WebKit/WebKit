/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "HeapInlines.h"
#include "JITSafepoint.h"
#include "JITWorklistThread.h"
#include "SlotVisitorInlines.h"
#include "VMInlines.h"
#include <wtf/CompilationThread.h>

namespace JSC {

JITWorklist::JITWorklist()
    : m_lock(Box<Lock>::create())
    , m_planEnqueued(AutomaticThreadCondition::create())
{
    m_maximumNumberOfConcurrentCompilationsPerTier = {
        Options::numberOfWorklistThreads(),
        Options::numberOfDFGCompilerThreads(),
        Options::numberOfFTLCompilerThreads(),
    };

    Locker locker { *m_lock };
    for (unsigned i = 0; i < Options::numberOfWorklistThreads(); ++i)
        m_threads.append(new JITWorklistThread(locker, *this));
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

CompilationResult JITWorklist::enqueue(Ref<JITPlan> plan)
{
    if (!Options::useConcurrentJIT()) {
        plan->compileInThread(nullptr);
        return plan->finalize();
    }

    Locker locker { *m_lock };
    if (Options::verboseCompilationQueue()) {
        dump(locker, WTF::dataFile());
        dataLog(": Enqueueing plan to optimize ", plan->key(), "\n");
    }
    ASSERT(m_plans.find(plan->key()) == m_plans.end());
    m_plans.add(plan->key(), plan.copyRef());
    m_queues[static_cast<unsigned>(plan->tier())].append(WTFMove(plan));
    m_planEnqueued->notifyOne(locker);
    return CompilationDeferred;
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

void JITWorklist::suspendAllThreads() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    m_suspensionLock.lock();
    for (unsigned i = m_threads.size(); i--;)
        m_threads[i]->m_rightToRun.lock();
}

void JITWorklist::resumeAllThreads() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    for (unsigned i = m_threads.size(); i--;)
        m_threads[i]->m_rightToRun.unlock();
    m_suspensionLock.unlock();
}

auto JITWorklist::compilationState(JITCompilationKey key) -> State
{
    Locker locker { *m_lock };
    const auto& iter = m_plans.find(key);
    if (iter == m_plans.end())
        return NotKnown;
    return iter->value->stage() == JITPlanStage::Ready ? Compiled : Compiling;
}

auto JITWorklist::completeAllReadyPlansForVM(VM& vm, JITCompilationKey requestedKey) -> State
{
    DeferGC deferGC(vm.heap);

    Vector<RefPtr<JITPlan>, 8> myReadyPlans;
    removeAllReadyPlansForVM(vm, myReadyPlans);

    State resultingState = NotKnown;
    while (!myReadyPlans.isEmpty()) {
        RefPtr<JITPlan> plan = myReadyPlans.takeLast();
        JITCompilationKey currentKey = plan->key();

        dataLogLnIf(Options::verboseCompilationQueue(), *this, ": Completing ", currentKey);

        RELEASE_ASSERT(plan->stage() == JITPlanStage::Ready);

        plan->finalize();

        if (currentKey == requestedKey)
            resultingState = Compiled;
    }

    if (!!requestedKey && resultingState == NotKnown) {
        Locker locker { *m_lock };
        if (m_plans.contains(requestedKey))
            resultingState = Compiling;
    }

    return resultingState;
}


void JITWorklist::waitUntilAllPlansForVMAreReady(VM& vm)
{
    DeferGC deferGC(vm.heap);

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
    DeferGC deferGC(vm.heap);
    waitUntilAllPlansForVMAreReady(vm);
    completeAllReadyPlansForVM(vm);
}

void JITWorklist::cancelAllPlansForVM(VM& vm)
{
    removeMatchingPlansForVM(vm, [&](JITPlan& plan) {
        return plan.stage() != JITPlanStage::Compiling;
    });

    waitUntilAllPlansForVMAreReady(vm);

    Vector<RefPtr<JITPlan>, 8> myReadyPlans;
    removeAllReadyPlansForVM(vm, myReadyPlans);
}

void JITWorklist::removeDeadPlans(VM& vm)
{
    removeMatchingPlansForVM(vm, [&](JITPlan& plan) {
        if (!plan.isKnownToBeLiveAfterGC())
            return true;
        plan.finalizeInGC();
        return false;
    });

    // No locking needed for this part, see comment in visitWeakReferences().
    for (unsigned i = m_threads.size(); i--;) {
        Safepoint* safepoint = m_threads[i].get()->m_safepoint;
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
    for (unsigned i = m_threads.size(); i--;) {
        Safepoint* safepoint = m_threads[i]->m_safepoint;
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

void JITWorklist::removeAllReadyPlansForVM(VM& vm, Vector<RefPtr<JITPlan>, 8>& myReadyPlans)
{
    DeferGC deferGC(vm.heap);
    Locker locker { *m_lock };
    for (size_t i = 0; i < m_readyPlans.size(); ++i) {
        RefPtr<JITPlan> plan = m_readyPlans[i];
        if (plan->vm() != &vm)
            continue;
        if (plan->stage() != JITPlanStage::Ready)
            continue;
        myReadyPlans.append(plan);
        m_readyPlans[i--] = m_readyPlans.last();
        m_readyPlans.removeLast();
        m_plans.remove(plan->key());
    }
}

template<typename MatchFunction>
void JITWorklist::removeMatchingPlansForVM(VM& vm, const MatchFunction& matches)
{
    Locker locker { *m_lock };
    HashSet<JITCompilationKey> deadPlanKeys;
    for (auto& entry : m_plans) {
        JITPlan* plan = entry.value.get();
        if (plan->vm() != &vm)
            continue;
        if (!matches(*plan))
            continue;
        RELEASE_ASSERT(plan->stage() != JITPlanStage::Canceled);
        deadPlanKeys.add(plan->key());
    }
    bool didCancelPlans = !deadPlanKeys.isEmpty();
    for (JITCompilationKey key : deadPlanKeys)
        m_plans.take(key)->cancel();
    for (auto& queue : m_queues) {
        Deque<RefPtr<JITPlan>> newQueue;
        while (!queue.isEmpty()) {
            RefPtr<JITPlan> plan = queue.takeFirst();
            if (plan->stage() != JITPlanStage::Canceled)
                newQueue.append(plan);
        }
        queue.swap(newQueue);
    }
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

