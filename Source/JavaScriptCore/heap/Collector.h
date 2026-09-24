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
class MarkStackArray;
class MutatorScheduler;
class SlotVisitor;

// State belonging to a collection cycle.
class Collector {
    WTF_MAKE_NONCOPYABLE(Collector);
    WTF_MAKE_TZONE_ALLOCATED(Collector);
public:
    explicit Collector(Heap&);
    ~Collector();

    Heap& heap() { return m_heap; }

    SlotVisitor& collectorSlotVisitor() LIFETIME_BOUND { return *m_collectorSlotVisitor; }

    // Every marking worker of the cycle: this Collector's own visitors, plus each participant's.
    template<typename Func>
    inline void forEachSlotVisitor(const Func&);

    void runTaskInParallel(RefPtr<SharedTask<void(SlotVisitor&)>>);

    template<typename Func>
    void runFunctionInParallel(const Func& func)
    {
        runTaskInParallel(createSharedTask<void(SlotVisitor&)>(func));
    }

private:
    class CollectorThread;
    friend class CollectorThread;
    friend class Heap;
    friend class MarkStackMergingConstraint;
    friend class SlotVisitor;
    friend class VerifierSlotVisitor;

    GCRequest::Ticket requestCollection(GCRequest);

    bool hasOutstandingRequest() const
    {
        RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
        return m_lastServedTicket < m_lastGrantedTicket;
    }
    bool hasServedTicket(GCRequest::Ticket ticket) const { return m_lastServedTicket >= ticket; }

    bool shouldCollectInCollectorThread(const AbstractLocker&);
    void startThread();
    void collectInCollectorThread();

    void startCollectingContinuously();
    void stopCollectingContinuously();

    enum class RunCurrentPhaseResult {
        Finished,
        Continue,
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

    void endMarking();
    void didFinishCollection();

    void stopThePeriphery();
    void resumeThePeriphery();

    bool suspendCompilerThreads();
    void resumeCompilerThreads();

    void assertMarkStacksEmpty();

    size_t bytesVisited();

    Heap& m_heap;

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

    // The phase machine, stepped once per transition.
    CollectorPhase m_lastPhase { CollectorPhase::NotRunning };
    CollectorPhase m_currentPhase { CollectorPhase::NotRunning };
    CollectorPhase m_nextPhase { CollectorPhase::NotRunning };
    bool m_collectorThreadIsRunning { false };
    bool m_threadShouldStop { false };
    bool m_isCompilerThreadsSuspended { false };
    uint64_t m_phaseVersion { 0 };
    std::unique_ptr<MutatorScheduler> m_scheduler;

    // The collector thread and the queue of requests it serves.
    Box<Lock> m_threadLock;
    const Ref<AutomaticThreadCondition> m_threadCondition; // The mutator must not wait on this. It would cause a deadlock.
    const RefPtr<AutomaticThread> m_thread;
    Deque<GCRequest> m_requests;
    GCRequest m_currentRequest;
    GCRequest::Ticket m_lastServedTicket { 0 };
    GCRequest::Ticket m_lastGrantedTicket { 0 };

    // Set to either the mutator or collector thread, depending on which is conducting the current phase.
    // These are valid only while that phase runs.
    SUPPRESS_UNCOUNTED_MEMBER Thread* m_conductorThread { nullptr };
    // When mutator is conducting, the mutator's machine state once in Fixpoint.
    CurrentThreadState* m_conductingMutatorState { nullptr };

    MonotonicTime m_beforeGC;
    MonotonicTime m_afterGC;
    MonotonicTime m_stopTime;

    bool m_shouldStopCollectingContinuously WTF_GUARDED_BY_LOCK(m_collectContinuouslyLock) { false };
    Lock m_collectContinuouslyLock;
    Condition m_collectContinuouslyCondition;
    RefPtr<Thread> m_collectContinuouslyThread { nullptr };

    // Describes the cycle for Instruments. Built at Begin, cleared at End.
    UTF8CString m_signpostMessage;
};

} // namespace JSC
