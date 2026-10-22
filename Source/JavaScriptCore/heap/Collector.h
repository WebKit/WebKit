/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <JavaScriptCore/CollectorPhase.h>
#include <JavaScriptCore/ConstraintConcurrency.h>
#include <JavaScriptCore/ConstraintParallelism.h>
#include <JavaScriptCore/ConstraintVolatility.h>
#include <JavaScriptCore/GCConductor.h>
#include <JavaScriptCore/GCRequest.h>
#include <JavaScriptCore/MachineStackMarker.h>
#include <wtf/AutomaticThread.h>
#include <wtf/Box.h>
#include <wtf/ConcurrentPtrHashSet.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Noncopyable.h>
#include <wtf/ParallelHelperPool.h>
#include <wtf/SharedTask.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace JSC {

class Heap;
class HeapCell;
class MarkStackArray;
class MarkingConstraint;
class MarkingConstraintSet;
class MutatorScheduler;
class SlotVisitor;
class VerifierSlotVisitor;

// State belonging to a collection cycle.
class Collector {
    WTF_MAKE_NONCOPYABLE(Collector);
    WTF_MAKE_TZONE_ALLOCATED(Collector);
public:
    Collector();
    ~Collector();

    // The process singleton Collector that drives global collections across all heaps. Exists only with useGlobalGC.
    static Collector& globalCollector();

    // FIXME: remove all uses of this. This exists only so multi-heap support can be added incrementally.
    Heap& heap()
    {
        ASSERT(m_heaps.size() == 1);
        return *m_heaps.first();
    }

    template<typename Func>
    void forEachHeap(NOESCAPE const Func& func)
    {
        for (Heap* heap : m_heaps)
            func(*heap);
    }

    bool hasHeap(Heap& heap) const { return m_heaps.contains(&heap); }

    SlotVisitor& collectorSlotVisitor() LIFETIME_BOUND { return *m_collectorSlotVisitor; }

    // The GC verifier (Options::verifyGC()).
    bool isMarkingForGCVerifier() const { return m_isMarkingForGCVerifier; }
    void setKeepVerifierSlotVisitor() { m_keepVerifierSlotVisitor = true; }
    void clearVerifierSlotVisitor();
    // This is a debug function for checking who marked the target cell.
    void dumpVerifierMarkerData(HeapCell*);

    // Every marking worker of the cycle: this Collector's own visitors, plus each participant's.
    template<typename Func>
    inline void forEachSlotVisitor(NOESCAPE const Func&);

    void runTaskInParallel(RefPtr<SharedTask<void(SlotVisitor&)>>);

    template<typename Func>
    void runFunctionInParallel(const Func& func)
    {
        runTaskInParallel(createSharedTask<void(SlotVisitor&)>(func));
    }

    // A heap joining or leaving a Collector, or adding a marking constraint to it. Applied between collections: by the
    // requester when the Collector is idle, and otherwise by whichever thread conducts next.
    struct MembershipChange {
        using Ticket = uint64_t;
        enum class Kind : uint8_t { Join, Leave, AddConstraint };
        Kind kind;
        Heap* heap;
        std::unique_ptr<MarkingConstraint> constraint;
    };
    MembershipChange::Ticket requestMembershipChange(MembershipChange&&);

    // Summed over the heaps, for the MutatorScheduler.
    size_t LEGACY_NODELETE totalBytesAllocatedThisCycle();
    size_t LEGACY_NODELETE totalMaxEdenSize();

    void dump(PrintStream&) const;

private:
    class CollectorThread;
    friend class Heap;
    friend class MarkStackMergingConstraint;
    friend class SlotVisitor;
    friend class VerifierSlotVisitor;

    GCRequest::Ticket requestCollection(Heap& requester, GCRequest);
    bool isSubsumedByQueuedRequest(const GCRequest&);

    bool hasPendingGCRequest()
    {
        Locker locker { *m_threadLock };
        return hasPendingGCRequestWithLock();
    }
    bool hasPendingGCRequestWithLock() const WTF_REQUIRES_LOCK(*m_threadLock)
    {
        RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
        // A request stays queued until it is served.
        ASSERT(m_requests.size() == m_lastGrantedTicket - m_lastServedTicket);
        return m_lastServedTicket < m_lastGrantedTicket;
    }
    bool hasServedTicketWithLock(GCRequest::Ticket ticket) const WTF_REQUIRES_LOCK(*m_threadLock) { return m_lastServedTicket >= ticket; }

    bool hasPendingMembershipChangeWithLock() const WTF_REQUIRES_LOCK(*m_threadLock) { return !m_membershipChanges.isEmpty(); }
    bool hasAppliedMembershipChangeWithLock(MembershipChange::Ticket ticket) const WTF_REQUIRES_LOCK(*m_threadLock) { return m_lastAppliedMembershipChange >= ticket; }
    void applyAllMembershipChangesWithLock(Vector<Heap*, 8>& changedHeaps) WTF_REQUIRES_LOCK(*m_threadLock);
    void applyMembershipChangeWithLock(MembershipChange&) WTF_REQUIRES_LOCK(*m_threadLock);

    bool hasWorkWithLock() const WTF_REQUIRES_LOCK(*m_threadLock) { return hasPendingGCRequestWithLock() || hasPendingMembershipChangeWithLock(); }

    // No collection is running or queued and nobody holds the conn, so nothing can start one while the lock is held.
    bool isIdleWithLock() WTF_REQUIRES_LOCK(*m_threadLock);

    bool anyMutatorHasConnWithLock() WTF_REQUIRES_LOCK(*m_threadLock);
    void assertConn(GCConductor);

    bool shouldCollectInCollectorThread() WTF_REQUIRES_LOCK(*m_threadLock);
    CollectionScope decideCollectionScope();
    void collectInCollectorThread();
    void collectInMutatorThread(Heap& conductor);

    void startCollectingContinuously();
    void stopCollectingContinuously();

    void shutDown();
    void stopThread();

    enum class RunCurrentPhaseResult {
        // This conductor stops for now: no collection is queued (including once one ends), the conn went to a mutator,
        // or a mutator conducting the Concurrent phase goes back to running JS while marking continues.
        Finished,
        // Run the next phase.
        Continue,
        // A mutator must capture its machine state before conducting Fixpoint.
        NeedCurrentThreadState
    };
    RunCurrentPhaseResult runCurrentPhase(GCConductor, CurrentThreadState*);

    // Returns true if we should keep doing things.
    bool runNotRunningPhase(GCConductor);
    bool runBeginPhase(GCConductor);
    bool runFixpointPhase(GCConductor);
    bool runConcurrentPhase(GCConductor);
    bool runReloopPhase(GCConductor);
    bool runEndPhase(GCConductor);
    bool changePhase(GCConductor, CollectorPhase);
    bool finishChangingPhase(GCConductor);

    // The per-heap part of the Begin and End phases.
    void beginCollectionInEachHeap(CollectionScope, MonotonicTime startTime);
    void setUpVisitors(CollectionScope);
    void endCollectionInEachHeap();

    void stopThePeriphery();
    void resumeThePeriphery();

    bool suspendCompilerThreads();
    void resumeCompilerThreads();

    void verifyGC();
    void verifierMark();

    void assertMarkStacksEmpty();

    size_t bytesMarked();
    UTF8CString bytesMarkedPerVisitorDump();

    // Summed over the heaps, for logging.
    size_t capacity();
    uintptr_t barriersExecuted();
    size_t mutatorMarkStacksSize();

    void beginSignpost(CollectionScope, GCConductor);
    void endSignpost();

    // The heaps this Collector's collections cover.
    Vector<Heap*, 1> m_heaps;

    // Mutated by the marking threads, grouped so that marking dirties as few cache lines as possible and
    // so that fields the mutator reads are not on those lines.
    unsigned m_numberOfActiveParallelMarkers { 0 };
    unsigned m_numberOfWaitingParallelMarkers WTF_GUARDED_BY_LOCK(m_markingMutex) { 0 };
    Lock m_markingMutex;
    Condition m_markingConditionVariable;
    Condition m_bonusVisitorTaskConditionVariable;
    bool m_parallelMarkersShouldExit { false };
    Lock m_parallelSlotVisitorLock;
    Lock m_raceMarkStackLock;
    RefPtr<SharedTask<void(SlotVisitor&)>> m_bonusVisitorTask WTF_GUARDED_BY_LOCK(m_markingMutex);
    std::unique_ptr<MarkStackArray> m_sharedCollectorMarkStack;
    std::unique_ptr<MarkStackArray> m_sharedMutatorMarkStack;

    // Cells whose visit lost a race, to be revisited before marking can terminate.
    std::unique_ptr<MarkStackArray> m_raceMarkStack;

    // Declared ahead of the visitors, each of which binds a reference to it on construction.
    ConcurrentPtrHashSet m_opaqueRoots;

    std::unique_ptr<SlotVisitor> m_collectorSlotVisitor;

    // We pool the slot visitors used by parallel marking threads. It's useful to be able to
    // enumerate over them, and it's useful to have them cache some small amount of memory from
    // one GC to the next.
    Vector<std::unique_ptr<SlotVisitor>> m_parallelSlotVisitors;
    // GC marking threads claim these at the start of marking, and return them at the end.
    Vector<SlotVisitor*> m_availableParallelSlotVisitors WTF_GUARDED_BY_LOCK(m_parallelSlotVisitorLock);
    ParallelHelperClient m_helperClient;

    std::unique_ptr<MarkingConstraintSet> m_constraintSet;

    // The phase machine, stepped once per transition.
    CollectorPhase m_lastPhase { CollectorPhase::NotRunning };
    CollectorPhase m_currentPhase { CollectorPhase::NotRunning };
    CollectorPhase m_nextPhase { CollectorPhase::NotRunning };
    bool m_threadIsWorking WTF_GUARDED_BY_LOCK(*m_threadLock) { false };
    bool m_threadShouldStop WTF_GUARDED_BY_LOCK(*m_threadLock) { false };
    bool m_isCompilerThreadsSuspended { false };
    uint64_t m_phaseVersion { 0 };
    std::unique_ptr<MutatorScheduler> m_scheduler;

    // The collector thread and the queue of requests it serves.
    Box<Lock> m_threadLock;
    const Ref<AutomaticThreadCondition> m_threadCondition; // The mutator must not wait on this. It would cause a deadlock.
    const RefPtr<AutomaticThread> m_thread;
    Deque<GCRequest> m_requests WTF_GUARDED_BY_LOCK(*m_threadLock);
    GCRequest m_currentRequest;
    GCRequest::Ticket m_lastServedTicket WTF_GUARDED_BY_LOCK(*m_threadLock) { 0 };
    GCRequest::Ticket m_lastGrantedTicket WTF_GUARDED_BY_LOCK(*m_threadLock) { 0 };
    Deque<MembershipChange> m_membershipChanges WTF_GUARDED_BY_LOCK(*m_threadLock);
    MembershipChange::Ticket m_lastAppliedMembershipChange WTF_GUARDED_BY_LOCK(*m_threadLock) { 0 };
    MembershipChange::Ticket m_lastRequestedMembershipChange WTF_GUARDED_BY_LOCK(*m_threadLock) { 0 };

    // The thread conducting the current phase, a mutator or the collector thread, and when a mutator conducts, its
    // machine state once in Fixpoint. Set by runCurrentPhase() when the phase starts, and valid only while it runs.
    // They are not cleared when it returns: the collector thread hands the conn to a mutator from inside
    // runCurrentPhase(), so the mutator can start running the phase before the collector thread's call has returned.
    SUPPRESS_UNCOUNTED_MEMBER Thread* m_conductorThread { nullptr };
    CurrentThreadState* m_conductingMutatorState { nullptr };
    // The heap whose mutator is conducting. Set by collectInMutatorThread() while it conducts, and null otherwise.
    Heap* m_conductingHeap { nullptr };

    MonotonicTime m_beforeGC;
    MonotonicTime m_afterGC;
    MonotonicTime m_stopTime;

    // Re-marks every heap of the collection, from the same constraints, to check that the collection marked
    // everything it should have. Created at Begin, run in End, kept afterwards only if asked.
    std::unique_ptr<VerifierSlotVisitor> m_verifierSlotVisitor;
    bool m_keepVerifierSlotVisitor { false };
    bool m_isMarkingForGCVerifier { false };

    bool m_shouldStopCollectingContinuously WTF_GUARDED_BY_LOCK(m_collectContinuouslyLock) { false };
    Lock m_collectContinuouslyLock;
    Condition m_collectContinuouslyCondition;
    RefPtr<Thread> m_collectContinuouslyThread { nullptr };

    // Describes the cycle. Built at Begin, cleared at End.
    UTF8CString m_signpostMessage;
    uint64_t m_signpostVersion { 0 };
};

} // namespace JSC
