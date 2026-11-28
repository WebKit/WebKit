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

#pragma once

#if ENABLE(JIT)

#include "HelperThreadPool.h"
#include "JITPlan.h"
#include "JITSafepoint.h"
#include <wtf/CheckedRef.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/FixedVector.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;
class VM;

// The state of one in-flight compile. The set of slots is fixed, so the GC can enumerate them.
class JITWorkerSlot final : public CanMakeThreadSafeCheckedPtr<JITWorkerSlot> {
    WTF_MAKE_NONCOPYABLE(JITWorkerSlot);
    WTF_MAKE_TZONE_ALLOCATED(JITWorkerSlot);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(JITWorkerSlot);

    friend class JITWorklist;
    friend class Safepoint;

public:
    JITWorkerSlot() = default;

    const Safepoint* safepoint() const { return m_safepoint; }

private:
    // The GC suspends compiles by locking every slot's m_rightToRun, and a compile
    // releases it only inside a Safepoint.
    Lock m_rightToRun;
    RefPtr<JITPlan> m_plan { nullptr };
    unsigned m_planLoad { 0 };
    Safepoint* m_safepoint { nullptr };
    // A pool thread owns this slot until its WorkScope ends, which is after m_plan has been handed off.
    bool m_isInUse { false };
};

class JITWorklist final : public HelperThreadClient {
    WTF_MAKE_NONCOPYABLE(JITWorklist);
    WTF_MAKE_TZONE_ALLOCATED(JITWorklist);

public:
    ~JITWorklist();

    static JITWorklist& ensureGlobalWorklist();
    NODELETE static JITWorklist* existingGlobalWorklistOrNull();

    CompilationResult enqueue(Ref<JITPlan>);
    size_t queueLength() const;

    void suspendAllThreads();
    void resumeAllThreads();

    // Until the deadline, compiler threads wait at their next safepoint instead of resuming, so that a GC can take
    // them all with trySuspendAllThreads() without blocking.
    void requestSuspension(MonotonicTime deadline);
    void cancelSuspensionRequest();
    bool trySuspendAllThreads() WTF_IGNORES_THREAD_SAFETY_ANALYSIS;

    void lockRightToRun(JITWorkerSlot& slot) WTF_ACQUIRES_LOCK(slot.m_rightToRun);

    enum State { NotKnown, Compiling, Compiled };
    State compilationState(VM&, JITCompilationKey);

    State completeAllReadyPlansForVM(VM&, JITCompilationKey = JITCompilationKey());

    // This is equivalent to:
    // worklist->waitUntilAllPlansForVMAreReady(vm);
    // worklist->completeAllReadyPlansForVM(vm);
    void completeAllPlansForVM(VM&);

    void cancelAllPlansForVM(VM&);

    void removeDeadPlans(VM&);

    unsigned setMaximumNumberOfConcurrentDFGCompilations(unsigned);
    unsigned setMaximumNumberOfConcurrentFTLCompilations(unsigned);

    // Only called on the main thread after suspending all threads.
    template<typename Visitor>
    void visitWeakReferences(VM&, Visitor&);

    template<typename Visitor>
    void iterateCodeBlocksForGC(Visitor&, VM&, NOESCAPE const Function<void(CodeBlock*)>&);

    void dump(PrintStream&) const;

    size_t totalOngoingCompilations() const;

private:
    class WorkScope;

    JITWorklist();

    void wakeThreads(const AbstractLocker&, unsigned enqueuedTier);
    unsigned numberOfRunnablePlans(const AbstractLocker&) const;
    unsigned threadsForCurrentLoad() const;
    unsigned planLoad(JITPlan&);

    bool canDequeueFromTier(unsigned tier) const;
    bool hasDequeueablePlan() const;
    bool dequeuePlan(const AbstractLocker&, JITWorkerSlot&);
    void compileOnSlot(JITWorkerSlot&);

    TaskResult runOneTask() final;
    void publishDemand(const AbstractLocker&);

    size_t queueLength(const AbstractLocker&) const;
    NODELETE size_t totalOngoingCompilations(const AbstractLocker&) const;

    void waitUntilAllPlansForVMAreReady(VM&);

    template<typename MatchFunction>
    void removeMatchingPlansForVM(VM&, NOESCAPE const MatchFunction&);

    State removeAllReadyPlansForVM(VM&, Vector<Ref<JITPlan>, 8>&, JITCompilationKey);

    void dump(const AbstractLocker&, PrintStream&) const;

    unsigned m_numberOfActiveThreads { 0 };
    unsigned m_totalLoad { 0 }; // Total load of the queues and ongoing compilations
    std::array<unsigned, static_cast<size_t>(JITPlan::Tier::Count)> m_ongoingCompilationsPerTier { 0, 0, 0 };
    std::array<unsigned, static_cast<size_t>(JITPlan::Tier::Count)> m_maximumNumberOfConcurrentCompilationsPerTier;
    std::array<unsigned, static_cast<size_t>(JITPlan::Tier::Count)> m_loadWeightsPerTier;

    bool m_isSuspended { false };

    // The GC walks this list without taking any locks. That's ok because this list is immutable after setup.
    FixedVector<JITWorkerSlot> m_slots;

    std::array<Deque<RefPtr<JITPlan>>, static_cast<size_t>(JITPlan::Tier::Count)> m_queues;

    // Used to answer questions about the current state of a code block. This
    // is particularly great for the cti_optimize OSR slow path, which wants
    // to know: did I get here because a better version of me just got
    // compiled?
    HashMap<JITCompilationKey, RefPtr<JITPlan>> m_plans;

    // Used to quickly find which plans have been compiled and are ready to
    // be completed.
    Vector<Ref<JITPlan>, 16> m_readyPlans;

    Lock m_suspensionLock;
    std::atomic<MonotonicTime> m_suspensionRequestDeadline;
    Lock m_suspensionRequestLock;
    Condition m_suspensionRequestCondition;
    mutable Lock m_lock;

    Condition m_planCompiledOrCancelled;
};

} // namespace JSC

#endif // ENABLE(JIT)
