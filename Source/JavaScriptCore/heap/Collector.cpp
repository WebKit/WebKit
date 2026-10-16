/*
 *  Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2026 Igalia S.L.
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

#include "config.h"
#include "Collector.h"

#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "CollectorInlines.h"
#include "HeapHelperPool.h"
#include "HeapInlines.h"
#include "HeapVerifier.h"
#include "IncrementalSweeper.h"
#include "JITWorklistInlines.h"
#include "JSCInlines.h"
#include "MarkingConstraintSet.h"
#include "MutatorScheduler.h"
#include "SpaceTimeMutatorScheduler.h"
#include "StochasticSpaceTimeMutatorScheduler.h"
#include "SynchronousStopTheWorldMutatorScheduler.h"
#include "TypeProfiler.h"
#include "VerifierSlotVisitorInlines.h"
#include <wtf/ListDump.h>
#include <wtf/ParkingLot.h>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

namespace CollectorInternal {
static constexpr bool verbose = false;
} // namespace CollectorInternal

namespace {

static double maxPauseMS(double thisPauseMS)
{
    static double maxPauseMS;
    maxPauseMS = std::max(thisPauseMS, maxPauseMS);
    return maxPauseMS;
}

} // anonymous namespace

class Collector::CollectorThread final : public AutomaticThread {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CollectorThread);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CollectorThread);
public:
    CollectorThread(const AbstractLocker& locker, Collector& collector)
        : AutomaticThread(locker, collector.m_threadLock, collector.m_threadCondition.copyRef())
        , m_collector(collector)
    {
    }

    ASCIILiteral name() const final
    {
        return "JSC Heap Collector Thread"_s;
    }

private:
    PollResult poll(const AbstractLocker&) final
    {
        assertIsHeld(*m_collector.m_threadLock);
        if (m_collector.m_threadShouldStop) {
            m_collector.m_threadIsWorking = false;
            return PollResult::Stop;
        }
        if (m_collector.shouldCollectInCollectorThread()) {
            m_collector.m_threadIsWorking = true;
            return PollResult::Work;
        }
        m_collector.m_threadIsWorking = false;
        return PollResult::Wait;
    }

    WorkResult work() final
    {
        m_collector.collectInCollectorThread();
        return WorkResult::Continue;
    }

    void threadDidStart() final
    {
        Thread::registerGCThread(GCThreadType::Main);
    }

    void threadIsStopping(const AbstractLocker&) final
    {
        assertIsHeld(*m_collector.m_threadLock);
        ASSERT(!m_collector.m_threadIsWorking);
    }

    Collector& m_collector;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(Collector);

Collector::Collector()
    : m_sharedCollectorMarkStack(makeUnique<MarkStackArray>())
    , m_sharedMutatorMarkStack(makeUnique<MarkStackArray>())
    , m_raceMarkStack(makeUnique<MarkStackArray>())
    , m_collectorSlotVisitor(makeUnique<SlotVisitor>(*this, "C"_s))
    , m_helperClient(&heapHelperPool())
    , m_constraintSet(makeUnique<MarkingConstraintSet>())
    , m_threadLock(Box<Lock>::create())
    , m_threadCondition(AutomaticThreadCondition::create())
{
    for (unsigned i = 0, numberOfParallelThreads = heapHelperPool().numberOfThreads(); i < numberOfParallelThreads; ++i) {
        std::unique_ptr<SlotVisitor> visitor = makeUnique<SlotVisitor>(*this, toASCIICString("P", i + 1));
        m_availableParallelSlotVisitors.append(visitor.get());
        m_parallelSlotVisitors.append(WTF::move(visitor));
    }

    if (Options::useConcurrentGC()) {
        if (Options::useStochasticMutatorScheduler())
            m_scheduler = makeUnique<StochasticSpaceTimeMutatorScheduler>(*this);
        else
            m_scheduler = makeUnique<SpaceTimeMutatorScheduler>(*this);
    } else {
        // We simulate turning off concurrent GC by making the scheduler say that the world
        // should always be stopped when the collector is running.
        m_scheduler = makeUnique<SynchronousStopTheWorldMutatorScheduler>();
    }

    Locker locker { *m_threadLock };
    lazyInitialize(m_thread, adoptRef(*new CollectorThread(locker, *this)));
}

Collector& Collector::globalCollector()
{
    RELEASE_ASSERT(Options::useGlobalGC());
    // Never destroyed, since it outlives every heap.
    static Collector* collector;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        collector = new Collector;
    });
    return *collector;
}

Collector::~Collector()
{
    ASSERT(m_heaps.isEmpty());
    stopCollectingContinuously();
    stopThread();
    m_raceMarkStack->clear();
}

void Collector::stopThread()
{
    bool stopped = false;
    {
        Locker locker { *m_threadLock };
        RELEASE_ASSERT(!hasWorkWithLock());
        stopped = m_thread->tryStop(locker);
        m_threadShouldStop = true;
        if (!stopped)
            m_threadCondition->notifyOne(locker);
    }
    if (!stopped)
        m_thread->join();
}

void Collector::assertMarkStacksEmpty()
{
    bool ok = true;

    if (!m_sharedCollectorMarkStack->isEmpty()) {
        dataLog("FATAL: Shared collector mark stack not empty! It has ", m_sharedCollectorMarkStack->size(), " elements.\n");
        ok = false;
    }

    if (!m_sharedMutatorMarkStack->isEmpty()) {
        dataLog("FATAL: Shared mutator mark stack not empty! It has ", m_sharedMutatorMarkStack->size(), " elements.\n");
        ok = false;
    }

    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            if (visitor.isEmpty())
                return;

            dataLog("FATAL: Visitor ", RawPointer(&visitor), " is not empty!\n");
            ok = false;
        });

    RELEASE_ASSERT(ok);
}

bool Collector::isSubsumedByQueuedRequest(const GCRequest& request)
{
    Locker locker { *m_threadLock };
    for (const GCRequest& queuedRequest : m_requests) {
        if (request.subsumedBy(queuedRequest))
            return true;
    }
    return false;
}

GCRequest::Ticket Collector::requestCollection(Heap& requester, GCRequest request)
{
    Locker locker { *m_threadLock };
    // Once the thread is stopping, nothing would serve a request.
    RELEASE_ASSERT(!m_threadShouldStop);
    // We may be able to steal the conn. That only works if the collector is definitely not running
    // right now. This is an optimization that prevents the collector thread from ever starting in most
    // cases.
    ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
    // It's okay for a heap to request a GC while its membership is pending, but it can't be the conductor yet.
    if ((m_lastServedTicket == m_lastGrantedTicket) && !m_threadIsWorking && hasHeap(requester)) {
        dataLogLnIf(CollectorInternal::verbose, "Taking the conn.");
        requester.m_worldState.exchangeOr(Heap::mutatorHasConnBit);
    }

    m_requests.append(request);
    m_lastGrantedTicket++;
    // A mutator that has the conn serves the request at its next poll, and otherwise the collector thread does.
    if (!anyMutatorHasConnWithLock())
        m_threadCondition->notifyOne(locker);
    return m_lastGrantedTicket;
}

bool Collector::shouldCollectInCollectorThread()
{
    dataLogLnIf(CollectorInternal::verbose, "Mutator has the conn = ", anyMutatorHasConnWithLock());

    return hasWorkWithLock() && !anyMutatorHasConnWithLock();
}

CollectionScope Collector::decideCollectionScope()
{
    if (!Options::useGenerationalGC() || VM::isInMiniMode())
        return CollectionScope::Full;
    if (m_currentRequest.scope)
        return *m_currentRequest.scope;
    // FIXME: This policy probably needs rethinking for global collections, and overCriticalMemoryThreshold()
    // should be process-wide, not per heap, but Heap::collectNow depends on the Heap cache given a consistent result.
    bool anyHeapNeedsFullCollection = false;
    forEachHeap([&](Heap& heap) {
        if (heap.m_shouldDoFullCollection || heap.overCriticalMemoryThreshold())
            anyHeapNeedsFullCollection = true;
    });
    return anyHeapNeedsFullCollection ? CollectionScope::Full : CollectionScope::Eden;
}

void Collector::collectInCollectorThread()
{
    for (;;) {
        RunCurrentPhaseResult result = runCurrentPhase(GCConductor::Collector, nullptr);
        switch (result) {
        case RunCurrentPhaseResult::Finished:
            return;
        case RunCurrentPhaseResult::Continue:
            break;
        case RunCurrentPhaseResult::NeedCurrentThreadState:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }
}

void Collector::collectInMutatorThread(Heap& conductor)
{
    ASSERT(hasHeap(conductor));
    ASSERT(!m_conductingHeap);
    SetForScope setConductingHeap { m_conductingHeap, &conductor };
    for (;;) {
        RunCurrentPhaseResult result = runCurrentPhase(GCConductor::Mutator, nullptr);
        switch (result) {
        case RunCurrentPhaseResult::Finished:
            return;
        case RunCurrentPhaseResult::Continue:
            break;
        case RunCurrentPhaseResult::NeedCurrentThreadState:
            sanitizeStackForVM(conductor.vm());
            auto lambda = [&] (CurrentThreadState& state) {
                for (;;) {
                    RunCurrentPhaseResult result = runCurrentPhase(GCConductor::Mutator, &state);
                    switch (result) {
                    case RunCurrentPhaseResult::Finished:
                        return;
                    case RunCurrentPhaseResult::Continue:
                        break;
                    case RunCurrentPhaseResult::NeedCurrentThreadState:
                        RELEASE_ASSERT_NOT_REACHED();
                        break;
                    }
                }
            };
            callWithCurrentThreadState(lambda);
            return;
        }
    }
}

auto Collector::runCurrentPhase(GCConductor conn, CurrentThreadState* conductingMutatorState) -> RunCurrentPhaseResult
{
    assertConn(conn);
    m_conductingMutatorState = conductingMutatorState;
    m_conductorThread = &Thread::currentSingleton();

    if (conn == GCConductor::Mutator)
        sanitizeStackForVM(m_conductingHeap->vm());

    // If the collector transfers the conn to the mutator, it leaves us in between phases.
    if (!finishChangingPhase(conn)) {
        // A mischevious mutator could repeatedly relinquish the conn back to us. We try to avoid doing
        // this, but it's probably not the end of the world if it did happen.
        dataLogLnIf(CollectorInternal::verbose, "Conn bounce-back.");
        return RunCurrentPhaseResult::Finished;
    }

    bool result = false;
    switch (m_currentPhase) {
    case CollectorPhase::NotRunning:
        result = runNotRunningPhase(conn);
        break;

    case CollectorPhase::Begin:
        result = runBeginPhase(conn);
        break;

    case CollectorPhase::Fixpoint:
        if (!conductingMutatorState && conn == GCConductor::Mutator)
            return RunCurrentPhaseResult::NeedCurrentThreadState;

        result = runFixpointPhase(conn);
        break;

    case CollectorPhase::Concurrent:
        result = runConcurrentPhase(conn);
        break;

    case CollectorPhase::Reloop:
        result = runReloopPhase(conn);
        break;

    case CollectorPhase::End:
        result = runEndPhase(conn);
        break;
    }

    return result ? RunCurrentPhaseResult::Continue : RunCurrentPhaseResult::Finished;
}

NEVER_INLINE bool Collector::runNotRunningPhase(GCConductor conn)
{
    Vector<Heap*, 8> changedHeaps;
    bool shouldBegin;
    {
        Locker locker { *m_threadLock };
        applyAllMembershipChangesWithLock(changedHeaps);
        // If every heap has left, then any pending GC requests have no work left to do.
        if (m_heaps.isEmpty() && hasPendingGCRequestWithLock()) {
            m_requests.clear();
            m_lastServedTicket = m_lastGrantedTicket;
        }
        // A mutator whose heap just left must not begin a collection, since it is no longer in the set.
        shouldBegin = hasPendingGCRequestWithLock() && (conn == GCConductor::Collector || hasHeap(*m_conductingHeap));
    }
    for (Heap* heap : changedHeaps)
        ParkingLot::unparkAll(&heap->m_worldState);

    if (!shouldBegin)
        return false;
    return changePhase(conn, CollectorPhase::Begin);
}

NEVER_INLINE bool Collector::runBeginPhase(GCConductor conn)
{
    MonotonicTime startTime = MonotonicTime::now();

    {
        Locker locker { *m_threadLock };
        RELEASE_ASSERT(hasPendingGCRequestWithLock());
        m_currentRequest = m_requests.first();
    }
    dataLogIf(Options::logGC(), "[GC<", *this, ">: START ", gcConductorShortName(conn), " ", capacity() / 1024, "kb ");

    m_beforeGC = MonotonicTime::now();
    CollectionScope scope = decideCollectionScope();

    beginSignpost(scope, conn);
    beginCollectionInEachHeap(scope, startTime);

    if (scope == CollectionScope::Full) {
        m_opaqueRoots.clear();
        m_collectorSlotVisitor->clearMarkStacks();
    }
    RELEASE_ASSERT(m_raceMarkStack->isEmpty());

    // After each heap's beginMarking has determined this collection's marking version.
    setUpVisitors(scope);

    m_parallelMarkersShouldExit = false;

    m_helperClient.setFunction(
        [this] () {
            SlotVisitor* visitor;
            {
                Locker locker { m_parallelSlotVisitorLock };
                RELEASE_ASSERT_WITH_MESSAGE(!m_availableParallelSlotVisitors.isEmpty(), "Parallel SlotVisitors are allocated apriori");
                visitor = m_availableParallelSlotVisitors.takeLast();
            }

            Thread::registerGCThread(GCThreadType::Helper);

            {
                ParallelModeEnabler parallelModeEnabler(*visitor);
                visitor->drainFromShared(SlotVisitor::HelperDrain);
            }

            {
                Locker locker { m_parallelSlotVisitorLock };
                m_availableParallelSlotVisitors.append(visitor);
            }
        });

    m_constraintSet->didStartMarking();

    m_scheduler->beginCollection();
    if (Options::logGC()) [[unlikely]]
        m_scheduler->log();

    SlotVisitor& visitor = *m_collectorSlotVisitor;
    // Marking starts with nothing to drain, so Fixpoint's first iteration runs the root constraints.
    if (!visitor.didReachTermination()) {
        dataLog("Fatal: SlotVisitor should think that GC should terminate before constraint solving, but it does not think this.\n");
        dataLog("visitor.isEmpty(): ", visitor.isEmpty(), "\n");
        dataLog("visitor.collectorMarkStack().isEmpty(): ", visitor.collectorMarkStack().isEmpty(), "\n");
        dataLog("visitor.mutatorMarkStack().isEmpty(): ", visitor.mutatorMarkStack().isEmpty(), "\n");
        dataLog("m_numberOfActiveParallelMarkers: ", m_numberOfActiveParallelMarkers, "\n");
        dataLog("m_sharedCollectorMarkStack->isEmpty(): ", m_sharedCollectorMarkStack->isEmpty(), "\n");
        dataLog("m_sharedMutatorMarkStack->isEmpty(): ", m_sharedMutatorMarkStack->isEmpty(), "\n");
        dataLog("visitor.didReachTermination(): ", visitor.didReachTermination(), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }

    return changePhase(conn, CollectorPhase::Fixpoint);
}

void Collector::beginCollectionInEachHeap(CollectionScope scope, MonotonicTime startTime)
{
    forEachHeap([&](Heap& heap) {
        heap.m_currentGCStartTime = startTime;
        if (!Options::seedOfVMRandomForFuzzer())
            heap.vm().random().setSeed(cryptographicallyRandomNumber<uint32_t>());
        heap.willStartCollection(scope);
        heap.beginMarking();
    });
}

void Collector::setUpVisitors(CollectionScope scope)
{
    bool isAnalyzingHeap = false;
    forEachHeap([&](Heap& heap) {
        if (heap.vm().isAnalyzingHeap())
            isAnalyzingHeap = true;
    });
    forEachSlotVisitor(
        [&](SlotVisitor& visitor) {
            visitor.didStartMarking(scope, isAnalyzingHeap);
        });
    if (Options::verifyGC()) [[unlikely]] {
        m_verifierSlotVisitor = makeUnique<VerifierSlotVisitor>(*this);
        m_verifierSlotVisitor->didStartMarking(scope, isAnalyzingHeap);
    }
}

NEVER_INLINE bool Collector::runFixpointPhase(GCConductor conn)
{
    RELEASE_ASSERT_IMPLIES(conn == GCConductor::Mutator, m_conductingMutatorState);

    SlotVisitor& visitor = *m_collectorSlotVisitor;

    dataLogIf(Options::logGC(), "v=", bytesMarked() / 1024, "kb (", bytesMarkedPerVisitorDump(), ") o=", m_opaqueRoots.size(), " b=", barriersExecuted(), " ");

    if (visitor.didReachTermination()) {
        m_opaqueRoots.deleteOldTables();

        m_scheduler->didReachTermination();

        assertMarkStacksEmpty();

        // FIXME: Take m_mutatorDidRun into account when scheduling constraints. Most likely,
        // we don't have to execute root constraints again unless the mutator did run. At a
        // minimum, we could use this for work estimates - but it's probably more than just an
        // estimate.
        // https://bugs.webkit.org/show_bug.cgi?id=166828

        // Wondering what this does? Look at Heap::createCoreConstraints(). The DOM and others can also
        // add their own using Heap::addMarkingConstraint().
        bool converged = m_constraintSet->executeConvergence(visitor);

        // FIXME: The visitor.isEmpty() check is most likely not needed.
        // https://bugs.webkit.org/show_bug.cgi?id=180310
        if (converged && visitor.isEmpty()) {
            assertMarkStacksEmpty();
            return changePhase(conn, CollectorPhase::End);
        }

        m_scheduler->didExecuteConstraints();
    }

    dataLogIf(Options::logGC(), visitor.collectorMarkStack().size(), "+", mutatorMarkStacksSize() + visitor.mutatorMarkStack().size(), " ");

    {
        ParallelModeEnabler enabler(visitor);
        visitor.drainInParallel(m_scheduler->timeToResume());
    }

    m_scheduler->synchronousDrainingDidStall();

    // This is kinda tricky. The termination check looks at:
    //
    // - Whether the marking threads are active. If they are not, this means that the marking threads'
    //   SlotVisitors are empty.
    // - Whether the collector's slot visitor is empty.
    // - Whether the shared mark stacks are empty.
    //
    // This doesn't have to check the mutator SlotVisitor because that one becomes empty after every GC
    // work increment, so it must be empty now.
    if (visitor.didReachTermination())
        return true; // This is like relooping to the top of runFixpointPhase().

    if (!m_scheduler->shouldResume())
        return true;

    m_scheduler->willResume();

    if (Options::logGC()) [[unlikely]] {
        double thisPauseMS = (MonotonicTime::now() - m_stopTime).milliseconds();
        dataLog("p=", thisPauseMS, "ms (max ", maxPauseMS(thisPauseMS), ")...]\n");
    }

    // Forgive the mutator for its past failures to keep up.
    // FIXME: Figure out if moving this to different places results in perf changes.
    forEachHeap([](Heap& heap) {
        heap.m_incrementBalance = 0;
    });

    return changePhase(conn, CollectorPhase::Concurrent);
}

NEVER_INLINE bool Collector::runConcurrentPhase(GCConductor conn)
{
    SlotVisitor& visitor = *m_collectorSlotVisitor;

    switch (conn) {
    case GCConductor::Mutator: {
        // When the mutator has the conn, we poll runConcurrentPhase() on every time someone says
        // stopIfNecessary(), so on every allocation slow path. When that happens we poll if it's time
        // to stop and do some work.
        if (visitor.didReachTermination()
            || m_scheduler->shouldStop())
            return changePhase(conn, CollectorPhase::Reloop);

        // We could be coming from a collector phase that stuffed our SlotVisitor, so make sure we donate
        // everything. This is super cheap if the SlotVisitor is already empty.
        visitor.donateAll();
        return false;
    }
    case GCConductor::Collector: {
        {
            ParallelModeEnabler enabler(visitor);
            visitor.drainInParallelPassively(m_scheduler->timeToStop());
        }
        return changePhase(conn, CollectorPhase::Reloop);
    } }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

NEVER_INLINE bool Collector::runReloopPhase(GCConductor conn)
{
    dataLogIf(Options::logGC(), "[GC<", *this, ">: ", gcConductorShortName(conn), " ");

    m_scheduler->didStop();

    if (Options::logGC()) [[unlikely]]
        m_scheduler->log();

    return changePhase(conn, CollectorPhase::Fixpoint);
}

NEVER_INLINE bool Collector::runEndPhase(GCConductor conn)
{
    m_scheduler->endCollection();

    {
        Locker locker { m_markingMutex };
        m_parallelMarkersShouldExit = true;
        m_markingConditionVariable.notifyAll();
    }
    m_helperClient.finish();

    assertMarkStacksEmpty();
    RELEASE_ASSERT(m_raceMarkStack->isEmpty());

    // Each heap's End step reads these counts, so flush them now.
    forEachSlotVisitor(
        [&](SlotVisitor& visitor) {
            visitor.flushBytesMarked();
            visitor.flushExtraMemoryVisited();
        });

    endCollectionInEachHeap();

    forEachSlotVisitor(
        [&](SlotVisitor& visitor) {
            visitor.reset();
        });

    m_afterGC = MonotonicTime::now();

    if (Options::logGC()) [[unlikely]] {
        double thisPauseMS = (m_afterGC - m_stopTime).milliseconds();
        dataLog("p=", thisPauseMS, "ms (max ", maxPauseMS(thisPauseMS), "), cycle ", (m_afterGC - m_beforeGC).milliseconds(), "ms END]\n");
    }

    {
        Locker locker { *m_threadLock };
        m_requests.removeFirst();
        m_lastServedTicket++;
        forEachHeap([](Heap& heap) {
            heap.clearMutatorWaiting();
        });
    }
    forEachHeap([](Heap& heap) {
        ParkingLot::unparkAll(&heap.m_worldState);
    });

    dataLogLnIf(Options::logGC(), "GC END!");
    endSignpost();

    forEachHeap([&](Heap& heap) {
        heap.setNeedCollectionEpilogue();
        heap.recordCollectionTime(m_afterGC - m_beforeGC);
    });
    return changePhase(conn, CollectorPhase::NotRunning);
}

void Collector::endCollectionInEachHeap()
{
    forEachHeap([&](Heap& heap) {
        ASSERT(heap.m_mutatorMarkStack->isEmpty());

        // Executing CodeBlocks keep writing their profiles without barriers after this collection. Remembering
        // them makes the next collection reconcile those profiles even if it is an Eden collection.
        heap.rememberExecutingAndCompilingCodeBlocks();
        heap.endMarking();
    });

    // Verify after every heap has finished marking, but before any heap prunes. Pruning applies the marking results
    // to the heap, and the constraints would then see different heap state.
    if (m_verifierSlotVisitor) [[unlikely]]
        verifyGC();

    forEachHeap([&](Heap& heap) {
        heap.verifyHeapAfterMarking();
        heap.pruneDeadReferences();
        heap.prepareForAllocation();
        heap.didFinishCollection();
    });
}

bool Collector::changePhase(GCConductor conn, CollectorPhase nextPhase)
{
    assertConn(conn);

    m_lastPhase = m_currentPhase;
    m_nextPhase = nextPhase;

    return finishChangingPhase(conn);
}

// Returns false only when the collector thread conducts and cannot stop the mutator because it holds heap access. The
// mutator is handed the conn instead, and finishes the transition at its next poll.
NEVER_INLINE bool Collector::finishChangingPhase(GCConductor conn)
{
    assertConn(conn);

    if (m_nextPhase == m_currentPhase)
        return true;

    dataLogLnIf(CollectorInternal::verbose, conn, ": Going to phase: ", m_nextPhase, " (from ", m_currentPhase, ")");

    m_phaseVersion++;

    bool suspendedBefore = worldShouldBeSuspended(m_currentPhase);
    bool suspendedAfter = worldShouldBeSuspended(m_nextPhase);

    if (suspendedBefore != suspendedAfter) {
        if (suspendedBefore) {
            RELEASE_ASSERT(!suspendedAfter);

            resumeThePeriphery();
            if (conn == GCConductor::Collector)
                heap().resumeTheMutator();
            else
                heap().handleNeedCollectionEpilogue();
        } else {
            RELEASE_ASSERT(!suspendedBefore);
            RELEASE_ASSERT(suspendedAfter);
            // FIXME: Stopping the mutators of several heaps is not implemented yet.
            RELEASE_ASSERT(m_heaps.size() <= 1);

            if (conn == GCConductor::Collector) {
                heap().waitWhileNeedCollectionEpilogue();
                if (!heap().stopTheMutator()) {
                    dataLogLnIf(CollectorInternal::verbose, "Returning false.");
                    return false;
                }
            } else {
                sanitizeStackForVM(heap().vm());
                heap().handleNeedCollectionEpilogue();
            }
            stopThePeriphery();
        }
    }

    m_currentPhase = m_nextPhase;
    return true;
}

void Collector::stopThePeriphery()
{
    m_isCompilerThreadsSuspended = suspendCompilerThreads();
    forEachHeap([](Heap& heap) {
        heap.stopThePeriphery();
    });
    m_collectorSlotVisitor->setMutatorsAreStopped(true);

    m_stopTime = MonotonicTime::now();
}

NEVER_INLINE void Collector::resumeThePeriphery()
{
    forEachHeap([](Heap& heap) {
        heap.resumeThePeriphery();
    });
    // Before any mutator runs again.
    m_collectorSlotVisitor->setMutatorsAreStopped(false);

    if (std::exchange(m_isCompilerThreadsSuspended, false))
        resumeCompilerThreads();
}

bool Collector::suspendCompilerThreads()
{
#if ENABLE(JIT)
    if (!Options::useJIT())
        return false;
    // The worklist is process-wide, so one heap's active plans are enough to need every compiler thread suspended.
    if (std::ranges::none_of(m_heaps, [](Heap* heap) { return heap->vm().numberOfActiveJITPlans(); }))
        return false;
    JITWorklist::ensureGlobalWorklist().suspendAllThreads();
    return true;
#else
    return false;
#endif
}

void Collector::resumeCompilerThreads()
{
#if ENABLE(JIT)
    JITWorklist::ensureGlobalWorklist().resumeAllThreads();
#endif
}

auto Collector::requestMembershipChange(MembershipChange&& change) -> MembershipChange::Ticket
{
    Locker locker { *m_threadLock };
    auto ticket = ++m_lastRequestedMembershipChange;
    // If the Collector is idle, apply the change now: no collection can start while this thread holds the lock. This
    // way, a heap joining its own Collector never starts that Collector's thread.
    if (isIdleWithLock()) {
        applyMembershipChangeWithLock(change);
        ++m_lastAppliedMembershipChange;
        return ticket;
    }
    m_membershipChanges.append(WTF::move(change));
    // A mutator that has the conn applies the change at its next poll, and otherwise the collector thread does.
    if (!anyMutatorHasConnWithLock())
        m_threadCondition->notifyOne(locker);
    return ticket;
}

bool Collector::isIdleWithLock()
{
    return !m_threadIsWorking && !hasWorkWithLock() && !anyMutatorHasConnWithLock();
}

void Collector::applyAllMembershipChangesWithLock(Vector<Heap*, 8>& changedHeaps)
{
    while (!m_membershipChanges.isEmpty()) {
        auto& change = m_membershipChanges.first();
        Heap& heap = *change.heap;
        applyMembershipChangeWithLock(change);
        heap.clearMutatorWaiting();
        changedHeaps.append(&heap);
        m_membershipChanges.removeFirst();
        ++m_lastAppliedMembershipChange;
    }
}

void Collector::applyMembershipChangeWithLock(MembershipChange& change)
{
    RELEASE_ASSERT(m_currentPhase == CollectorPhase::NotRunning);
    Heap& heap = *change.heap;
    switch (change.kind) {
    case MembershipChange::Kind::Join:
        for (auto& constraint : heap.createCoreConstraints())
            m_constraintSet->add(heap, WTF::move(constraint));
        m_heaps.append(&heap);
        break;
    case MembershipChange::Kind::Leave: {
        m_constraintSet->remove(heap);
        bool removed = m_heaps.removeFirst(&heap);
        RELEASE_ASSERT(removed);
        break;
    }
    case MembershipChange::Kind::AddConstraint:
        m_constraintSet->add(heap, WTF::move(change.constraint));
        break;
    }
}

// Only the conducting heap's mutator has the conn, and none does while the collector thread conducts.
void Collector::assertConn(GCConductor conn)
{
    RELEASE_ASSERT_IMPLIES(conn == GCConductor::Mutator, m_conductingHeap);
    forEachHeap([&](Heap& heap) {
        heap.checkConn(conn == GCConductor::Mutator && &heap == m_conductingHeap ? GCConductor::Mutator : GCConductor::Collector);
    });
}

bool Collector::anyMutatorHasConnWithLock()
{
    for (Heap* heap : m_heaps) {
        if (heap->m_worldState.load() & Heap::mutatorHasConnBit)
            return true;
    }
    return false;
}

void Collector::dump(PrintStream& out) const
{
    // A collection of one heap is named by that heap, as in Heap's own log lines. Every set of several
    // heaps is the set of all heaps.
    if (m_heaps.size() == 1)
        out.print(RawPointer(m_heaps.first()));
    else
        out.print("all");
}

// FIXME: SUPPRESS_NODELETE shouldn't be necessary here and below. The lambdas destruct nothing; the checker just
// cannot see through a lambda.
SUPPRESS_NODELETE size_t Collector::totalBytesAllocatedThisCycle()
{
    size_t result = 0;
    forEachHeap([&](Heap& heap) {
        result += heap.totalBytesAllocatedThisCycle();
    });
    return result;
}

SUPPRESS_NODELETE size_t Collector::totalMaxEdenSize()
{
    size_t result = 0;
    forEachHeap([&](Heap& heap) {
        result += heap.m_maxEdenSize;
    });
    return result;
}

size_t Collector::bytesMarked()
{
    size_t result = 0;
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            result += visitor.bytesMarked();
        });
    return result;
}

UTF8CString Collector::bytesMarkedPerVisitorDump()
{
    // Every heap's mutator visitor has the same name, so their counts are summed.
    UncheckedKeyHashMap<ASCIICString, size_t> visitMap;
    forEachSlotVisitor(
        [&](SlotVisitor& visitor) {
            visitMap.add(visitor.codeName(), 0).iterator->value += visitor.bytesMarked() / 1024;
        });
    return sortedMapDump(visitMap, std::less<>(), ":"_s, " "_s);
}

size_t Collector::capacity()
{
    size_t result = 0;
    forEachHeap([&](Heap& heap) {
        result += heap.capacity();
    });
    return result;
}

uintptr_t Collector::barriersExecuted()
{
    uintptr_t result = 0;
    forEachHeap([&](Heap& heap) {
        result += heap.m_barriersExecuted;
    });
    return result;
}

size_t Collector::mutatorMarkStacksSize()
{
    size_t result = 0;
    forEachHeap([&](Heap& heap) {
        result += heap.m_mutatorMarkStack->size();
    });
    return result;
}

void Collector::beginSignpost(CollectionScope scope, GCConductor conn)
{
    ++m_signpostVersion;
    if (!Options::useGCSignpost()) [[likely]]
        return;
    StringPrintStream stream;
    stream.print("GC:(", *this, "),mode:(", scope, "),version:(", m_signpostVersion, "),conn:(", gcConductorShortName(conn), "),capacity(", capacity() / 1024, "kb)");
    m_signpostMessage = stream.toUTF8CString();
    WTFBeginSignpost(this, JSCGarbageCollector, "%" PUBLIC_LOG_STRING, m_signpostMessage.isNull() ? "(nullptr)"_s : m_signpostMessage);
}

void Collector::endSignpost()
{
    if (!Options::useGCSignpost()) [[likely]]
        return;
    WTFEndSignpost(this, JSCGarbageCollector, "%" PUBLIC_LOG_STRING, m_signpostMessage.isNull() ? "(nullptr)"_s : m_signpostMessage);
    m_signpostMessage = { };
}

void Collector::runTaskInParallel(RefPtr<SharedTask<void(SlotVisitor&)>> task)
{
    unsigned initialRefCount = task->refCount();
    {
        Locker locker { m_markingMutex };
        m_bonusVisitorTask = task;
        m_markingConditionVariable.notifyAll();
    }

    task->run(*m_collectorSlotVisitor);

    {
        Locker locker { m_markingMutex };
        m_bonusVisitorTask = nullptr;

        // The constraint solver expects return of this function to imply termination of the task in all
        // threads. This ensures that property.
        while (task->refCount() > initialRefCount)
            m_bonusVisitorTaskConditionVariable.wait(m_markingMutex);
    }
}

void Collector::verifyGC()
{
    verifierMark();
    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    RELEASE_ASSERT(visitor.doneMarking());

    visitor.forEachLiveCell([&](HeapCell* cell) {
        Heap& heap = *cell->heap();
        if (heap.isMarked(cell))
            return;

        dataLogLn("\n" "GC Verifier: ERROR cell ", RawPointer(cell), " was not marked");
        if (Options::verboseVerifyGC()) [[unlikely]]
            visitor.dumpMarkerData(cell);
        RELEASE_ASSERT(heap.isMarked(cell));
    });

    if (!m_keepVerifierSlotVisitor)
        clearVerifierSlotVisitor();
}

void Collector::verifierMark()
{
    RELEASE_ASSERT(!m_isMarkingForGCVerifier);
    forEachHeap([](Heap& heap) {
        RELEASE_ASSERT(heap.m_collectionScope);
    });
    m_isMarkingForGCVerifier = true;

    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    do {
        while (!visitor.isEmpty())
            visitor.drain();
        m_constraintSet->executeAllSynchronously(visitor);
        visitor.executeConstraintTasks();
    } while (!visitor.isEmpty());

    visitor.setDoneMarking();

    m_isMarkingForGCVerifier = false;
}

void Collector::clearVerifierSlotVisitor()
{
    m_verifierSlotVisitor = nullptr;
    m_keepVerifierSlotVisitor = false;
}

void Collector::dumpVerifierMarkerData(HeapCell* cell)
{
    if (!Options::verifyGC())
        return;

    if (!cell->heap()->isMarked(cell)) {
        dataLogLn("\n" "GC Verifier: cell ", RawPointer(cell), " was not marked by SlotVisitor");
        return;
    }

    // Use VerifierSlotVisitorScope to keep it live.
    RELEASE_ASSERT(m_verifierSlotVisitor && !m_isMarkingForGCVerifier);
    VerifierSlotVisitor& visitor = *m_verifierSlotVisitor;
    RELEASE_ASSERT(visitor.doneMarking());

    if (!visitor.isMarked(cell)) {
        dataLogLn("\n" "GC Verifier: ERROR cell ", RawPointer(cell), " was not marked by VerifierSlotVisitor");
        return;
    }

    dataLogLn("\n" "GC Verifier: Found marked cell ", RawPointer(cell), " with MarkerData:");
    visitor.dumpMarkerData(cell);
}

void Collector::startCollectingContinuously()
{
    if (m_collectContinuouslyThread)
        return;

    m_collectContinuouslyThread = Thread::create(
        "JSC DEBUG Continuous GC"_s,
        [this] () {
            MonotonicTime initialTime = MonotonicTime::now();
            Seconds period = Seconds::fromMilliseconds(Options::collectContinuouslyPeriodMS());
            while (true) {
                Locker locker { m_collectContinuouslyLock };
                {
                    Locker locker { *m_threadLock };
                    if (!hasPendingGCRequestWithLock() && !m_heaps.isEmpty()) {
                        m_requests.append(std::nullopt);
                        m_lastGrantedTicket++;
                        m_threadCondition->notifyOne(locker);
                    }
                }

                Seconds elapsed = MonotonicTime::now() - initialTime;
                Seconds elapsedInPeriod = elapsed % period;
                MonotonicTime timeToWakeUp =
                    initialTime + elapsed - elapsedInPeriod + period;
                while (!hasElapsed(timeToWakeUp) && !m_shouldStopCollectingContinuously) {
                    m_collectContinuouslyCondition.waitUntil(
                        m_collectContinuouslyLock, timeToWakeUp);
                }
                if (m_shouldStopCollectingContinuously)
                    break;
            }
        }, ThreadType::GarbageCollection);
}

void Collector::stopCollectingContinuously()
{
    if (!m_collectContinuouslyThread)
        return;

    {
        Locker locker { m_collectContinuouslyLock };
        m_shouldStopCollectingContinuously = true;
        m_collectContinuouslyCondition.notifyOne();
    }
    RefPtr { m_collectContinuouslyThread }->waitForCompletion();
}

} // namespace JSC
