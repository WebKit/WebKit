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
#include "TypeProfiler.h"
#include <wtf/ListDump.h>
#include <wtf/ParkingLot.h>
#include <wtf/Scope.h>
#include <wtf/SpinBackoff.h>
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
    PollResult poll(const AbstractLocker& locker) final
    {
        if (m_collector.m_threadShouldStop) {
            m_collector.m_heap.notifyThreadStopping(locker);
            return PollResult::Stop;
        }
        if (m_collector.shouldCollectInCollectorThread(locker)) {
            m_collector.m_collectorThreadIsRunning = true;
            return PollResult::Work;
        }
        m_collector.m_collectorThreadIsRunning = false;
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
        m_collector.m_collectorThreadIsRunning = false;
    }

    Collector& m_collector;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(Collector);

Collector::Collector(Heap& heap)
    : m_heap(heap)
    , m_sharedCollectorMarkStack(makeUnique<MarkStackArray>())
    , m_sharedMutatorMarkStack(makeUnique<MarkStackArray>())
    , m_raceMarkStack(makeUnique<MarkStackArray>())
    , m_collectorSlotVisitor(makeUnique<SlotVisitor>(heap, *this, "C"_s))
    , m_helperClient(&heapHelperPool())
    , m_threadLock(Box<Lock>::create())
    , m_threadCondition(AutomaticThreadCondition::create())
{
}

Collector::~Collector() = default;

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

void Collector::startThread()
{
    Locker locker { *m_threadLock };
    lazyInitialize(m_thread, adoptRef(*new CollectorThread(locker, *this)));
}

GCRequest::Ticket Collector::requestCollection(GCRequest request)
{
    Locker locker { *m_threadLock };
    // We may be able to steal the conn. That only works if the collector is definitely not running
    // right now. This is an optimization that prevents the collector thread from ever starting in most
    // cases.
    ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
    if ((m_lastServedTicket == m_lastGrantedTicket) && !m_collectorThreadIsRunning) {
        dataLogLnIf(CollectorInternal::verbose, "Taking the conn.");
        m_heap.m_worldState.exchangeOr(Heap::mutatorHasConnBit);
    }

    m_requests.append(request);
    m_lastGrantedTicket++;
    if (!(m_heap.m_worldState.load() & Heap::mutatorHasConnBit))
        m_threadCondition->notifyOne(locker);
    return m_lastGrantedTicket;
}

bool Collector::shouldCollectInCollectorThread(const AbstractLocker&)
{
    RELEASE_ASSERT(m_requests.isEmpty() == (m_lastServedTicket == m_lastGrantedTicket));
    RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
    dataLogLnIf(CollectorInternal::verbose, "Mutator has the conn = ", !!(m_heap.m_worldState.load() & Heap::mutatorHasConnBit));

    return !m_requests.isEmpty() && !(m_heap.m_worldState.load() & Heap::mutatorHasConnBit);
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

auto Collector::runCurrentPhase(GCConductor conn, CurrentThreadState* conductingMutatorState) -> RunCurrentPhaseResult
{
    m_heap.checkConn(conn);
    m_conductingMutatorState = conductingMutatorState;
    m_conductorThread = &Thread::currentSingleton();

    if (conn == GCConductor::Mutator)
        sanitizeStackForVM(m_heap.vm());

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
    // Check m_requests since the mutator calls this to poll what's going on.
    {
        Locker locker { *m_threadLock };
        if (m_requests.isEmpty())
            return false;
    }

    return changePhase(conn, CollectorPhase::Begin);
}

NEVER_INLINE bool Collector::runBeginPhase(GCConductor conn)
{
    m_heap.m_currentGCStartTime = MonotonicTime::now();

    {
        Locker locker { *m_threadLock };
        RELEASE_ASSERT(!m_requests.isEmpty());
        m_currentRequest = m_requests.first();
    }

    dataLogIf(Options::logGC(), "[GC<", RawPointer(&m_heap), ">: START ", gcConductorShortName(conn), " ", m_heap.capacity() / 1024, "kb ");

    m_beforeGC = MonotonicTime::now();

    if (!Options::seedOfVMRandomForFuzzer())
        m_heap.vm().random().setSeed(cryptographicallyRandomNumber<uint32_t>());

    m_heap.willStartCollection();

    if (m_heap.m_verifier) [[unlikely]] {
        // Verify that live objects from the last GC cycle haven't been corrupted by
        // mutators before we begin this new GC cycle.
        m_heap.m_verifier->verify(HeapVerifier::Phase::BeforeGC);

        m_heap.m_verifier->startGC();
        m_heap.m_verifier->gatherLiveCells(HeapVerifier::Phase::BeforeMarking);
    }

    ASSERT(m_heap.m_collectionScope);
    bool isFullGC = m_heap.m_collectionScope.value() == CollectionScope::Full;
    if (Options::useGCSignpost()) [[unlikely]] {
        StringPrintStream stream;
        stream.print("GC:(", RawPointer(&m_heap), "),mode:(", (isFullGC ? "Full" : "Eden"), "),version:(", m_heap.m_gcVersion, "),conn:(", gcConductorShortName(conn), "),capacity(", m_heap.capacity() / 1024, "kb)");
        m_signpostMessage = stream.toUTF8CString();
        WTFBeginSignpost(&m_heap, JSCGarbageCollector, "%" PUBLIC_LOG_STRING, m_signpostMessage.isNull() ? "(nullptr)"_s : m_signpostMessage);
    }

    m_heap.prepareForMarking();

    if (isFullGC) {
        m_opaqueRoots.clear();
        m_collectorSlotVisitor->clearMarkStacks();
        m_heap.m_mutatorMarkStack->clear();
    } else
        m_heap.m_bytesAllocatedBeforeLastEdenCollect = m_heap.totalBytesAllocatedThisCycle();

    RELEASE_ASSERT(m_raceMarkStack->isEmpty());

    m_heap.beginMarking();

    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.didStartMarking();
        });

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

    SlotVisitor& visitor = *m_collectorSlotVisitor;

    m_heap.m_constraintSet->didStartMarking();

    m_scheduler->beginCollection();
    if (Options::logGC()) [[unlikely]]
        m_scheduler->log();

    // After this, we will almost certainly fall through all of the "visitor.isEmpty()"
    // checks because bootstrap would have put things into the visitor. So, we should fall
    // through to draining.

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

NEVER_INLINE bool Collector::runFixpointPhase(GCConductor conn)
{
    RELEASE_ASSERT(conn == GCConductor::Collector || m_conductingMutatorState);

    SlotVisitor& visitor = *m_collectorSlotVisitor;

    if (Options::logGC()) [[unlikely]] {
        UncheckedKeyHashMap<ASCIICString, size_t> visitMap;
        forEachSlotVisitor(
            [&] (SlotVisitor& visitor) {
                visitMap.add(visitor.codeName(), visitor.bytesVisited() / 1024);
            });

        auto perVisitorDump = sortedMapDump(visitMap, std::less<>(), ":"_s, " "_s);

        dataLog("v=", bytesVisited() / 1024, "kb (", perVisitorDump, ") o=", m_opaqueRoots.size(), " b=", m_heap.m_barriersExecuted, " ");
    }

    if (visitor.didReachTermination()) {
        m_opaqueRoots.deleteOldTables();

        m_scheduler->didReachTermination();

        assertMarkStacksEmpty();

        // FIXME: Take m_mutatorDidRun into account when scheduling constraints. Most likely,
        // we don't have to execute root constraints again unless the mutator did run. At a
        // minimum, we could use this for work estimates - but it's probably more than just an
        // estimate.
        // https://bugs.webkit.org/show_bug.cgi?id=166828

        // Wondering what this does? Look at Heap::addCoreConstraints(). The DOM and others can also
        // add their own using Heap::addMarkingConstraint().
        bool converged = m_heap.m_constraintSet->executeConvergence(visitor);

        // FIXME: The visitor.isEmpty() check is most likely not needed.
        // https://bugs.webkit.org/show_bug.cgi?id=180310
        if (converged && visitor.isEmpty()) {
            assertMarkStacksEmpty();
            return changePhase(conn, CollectorPhase::End);
        }

        m_scheduler->didExecuteConstraints();
    }

    dataLogIf(Options::logGC(), visitor.collectorMarkStack().size(), "+", m_heap.m_mutatorMarkStack->size() + visitor.mutatorMarkStack().size(), " ");

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
    m_heap.m_incrementBalance = 0;

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
    dataLogIf(Options::logGC(), "[GC<", RawPointer(&m_heap), ">: ", gcConductorShortName(conn), " ");

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

    ASSERT(m_heap.m_mutatorMarkStack->isEmpty());
    ASSERT(m_raceMarkStack->isEmpty());

    SlotVisitor& visitor = *m_collectorSlotVisitor;
    m_heap.iterateExecutingAndCompilingCodeBlocks(visitor,
        [&] (CodeBlock* codeBlock) {
            m_heap.writeBarrier(codeBlock);
        });

    m_heap.updateObjectCounts();
    endMarking();

    if (Options::verifyGC()) [[unlikely]]
        m_heap.verifyGC();

    if (m_heap.m_verifier) [[unlikely]] {
        m_heap.m_verifier->gatherLiveCells(HeapVerifier::Phase::AfterMarking);
        m_heap.m_verifier->verify(HeapVerifier::Phase::AfterMarking);
    }

    {
        auto* previous = Thread::currentSingleton().setCurrentAtomStringTable(nullptr);
        auto scopeExit = makeScopeExit([&] {
            Thread::currentSingleton().setCurrentAtomStringTable(previous);
        });

        if (m_heap.vm().typeProfiler())
            m_heap.vm().typeProfiler()->invalidateTypeSetCache(m_heap.vm());

        m_heap.cancelDeferredWorkIfNeeded();
        m_heap.reapWeakHandles();
        m_heap.reconcileWeakGCHashTables();
        m_heap.sweepArrayBuffers();
        m_heap.snapshotUnswept();
        m_heap.reconcileWeakReferencesAtGCEnd(); // Must precede clearCurrentlyExecuting: CodeBlock::reconcileWeakReferencesAtGCEnd queries which CodeBlocks are currently executing.
        m_heap.removeDeadCompilerWorklistEntries();
        m_heap.deleteUnmarkedCompiledCode();
    }

    m_heap.m_sweeper->startSweeping(m_heap);

    m_heap.m_codeBlocks->iterateCurrentlyExecuting(
        [&] (CodeBlock* codeBlock) {
            m_heap.writeBarrier(codeBlock);
        });
    m_heap.m_codeBlocks->clearCurrentlyExecutingAndRemoveDeadCodeBlocks(m_heap.vm());

    m_heap.m_objectSpace.prepareForAllocation();
    m_heap.updateAllocationLimits();

    if (m_heap.m_verifier) [[unlikely]] {
        m_heap.m_verifier->trimDeadCells();
        m_heap.m_verifier->verify(HeapVerifier::Phase::AfterGC);
    }

    auto endingCollectionScope = *m_heap.m_collectionScope;

    didFinishCollection();

    if (m_currentRequest.didFinishEndPhase)
        RefPtr { m_currentRequest.didFinishEndPhase }->run();

    if (CollectorInternal::verbose) {
        dataLogLn(CollectorInternal::verbose, "Heap state after GC:");
        m_heap.m_objectSpace.dumpBits();
    }

    if (Options::logGC()) [[unlikely]] {
        double thisPauseMS = (m_afterGC - m_stopTime).milliseconds();
        dataLog("p=", thisPauseMS, "ms (max ", maxPauseMS(thisPauseMS), "), cycle ", (m_afterGC - m_beforeGC).milliseconds(), "ms END]\n");
    }

    {
        Locker locker { *m_threadLock };
        m_requests.removeFirst();
        m_lastServedTicket++;
        m_heap.clearMutatorWaiting();
    }
    ParkingLot::unparkAll(&m_heap.m_worldState);

    dataLogLnIf(Options::logGC(), "GC END!");
    if (Options::useGCSignpost()) [[unlikely]] {
        WTFEndSignpost(&m_heap, JSCGarbageCollector, "%" PUBLIC_LOG_STRING, m_signpostMessage.isNull() ? "(nullptr)"_s : m_signpostMessage);
        m_signpostMessage = { };
    }

    m_heap.setNeedCollectionEpilogue();

    MonotonicTime now = MonotonicTime::now();
    if (m_heap.m_maxEdenSizeForRateLimiting) {
        m_heap.m_gcRateLimitingValue = m_heap.projectedGCRateLimitingValue(now);
        m_heap.m_gcRateLimitingValue += 1.0;
    }
    m_heap.m_lastGCEndTime = now;
    m_heap.m_totalGCTime += now - m_heap.m_currentGCStartTime;
    if (endingCollectionScope == CollectionScope::Full)
        m_heap.m_lastFullGCEndTime = m_heap.m_lastGCEndTime;
    return changePhase(conn, CollectorPhase::NotRunning);
}

bool Collector::changePhase(GCConductor conn, CollectorPhase nextPhase)
{
    m_heap.checkConn(conn);

    m_lastPhase = m_currentPhase;
    m_nextPhase = nextPhase;

    return finishChangingPhase(conn);
}

NEVER_INLINE bool Collector::finishChangingPhase(GCConductor conn)
{
    m_heap.checkConn(conn);

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
                m_heap.resumeTheMutator();
            else
                m_heap.handleNeedCollectionEpilogue();
        } else {
            RELEASE_ASSERT(!suspendedBefore);
            RELEASE_ASSERT(suspendedAfter);

            if (conn == GCConductor::Collector) {
                m_heap.waitWhileNeedCollectionEpilogue();
                if (!m_heap.stopTheMutator()) {
                    dataLogLnIf(CollectorInternal::verbose, "Returning false.");
                    return false;
                }
            } else {
                sanitizeStackForVM(m_heap.vm());
                m_heap.handleNeedCollectionEpilogue();
            }
            stopThePeriphery();
        }
    }

    m_currentPhase = m_nextPhase;
    return true;
}

void Collector::endMarking()
{
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.reset();
        });

    assertMarkStacksEmpty();

    RELEASE_ASSERT(m_raceMarkStack->isEmpty());

    m_heap.endMarking();
}

void Collector::didFinishCollection()
{
    m_afterGC = MonotonicTime::now();

    // m_beforeGC and m_afterGC are the cycle's own; the heap gets the duration they bound, since that
    // is what it keeps and what its consumers ask for.
    m_heap.didFinishCollection(m_afterGC - m_beforeGC);
}

void Collector::stopThePeriphery()
{
    m_isCompilerThreadsSuspended = suspendCompilerThreads();
    m_heap.stopThePeriphery();

    // updateMutatorIsStopped() recomputes from m_worldIsStopped rather than being told, so it has to
    // run after Heap::stopThePeriphery() sets it.
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.updateMutatorIsStopped(NoLockingNecessary);
        });

    m_stopTime = MonotonicTime::now();
}

NEVER_INLINE void Collector::resumeThePeriphery()
{
    m_heap.resumeThePeriphery();

    // updateMutatorIsStopped() recomputes from the current stopped state rather than being told it,
    // so it has to run after the write.
    //
    // FIXME: This could be vastly improved: we want to grab the locks in the order in which they
    // become available. We basically want a lockAny() method that will lock whatever lock is available
    // and tell you which one it locked. That would require teaching ParkingLot how to park on multiple
    // queues at once, which is totally achievable - it would just require memory allocation, which is
    // suboptimal but not a disaster. Alternatively, we could replace the SlotVisitor rightToRun lock
    // with a DLG-style handshake mechanism, but that seems not as general.
    Vector<SlotVisitor*, 8> visitorsToUpdate;

    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitorsToUpdate.append(&visitor);
        });

    SpinBackoff backoff;
    for (unsigned countdown = 40; !visitorsToUpdate.isEmpty() && countdown--;) {
        for (unsigned index = 0; index < visitorsToUpdate.size(); ++index) {
            SlotVisitor& visitor = *visitorsToUpdate[index];
            bool remove = false;
            if (visitor.hasAcknowledgedThatTheMutatorIsResumed())
                remove = true;
            else if (visitor.rightToRun().tryLock()) {
                Locker locker { AdoptLock, visitor.rightToRun() };
                visitor.updateMutatorIsStopped(locker);
                remove = true;
            }
            if (remove) {
                visitorsToUpdate[index--] = visitorsToUpdate.last();
                visitorsToUpdate.takeLast();
            }
        }
        backoff.spinOnce();
    }

    for (SlotVisitor* visitor : visitorsToUpdate)
        visitor->updateMutatorIsStopped();

    if (std::exchange(m_isCompilerThreadsSuspended, false))
        resumeCompilerThreads();
}

bool Collector::suspendCompilerThreads()
{
#if ENABLE(JIT)
    // We ensure the worklists so that it's not possible for the mutator to start a new worklist
    // after we have suspended the ones that he had started before. That's not very expensive since
    // the worklists use AutomaticThreads anyway.
    if (!Options::useJIT())
        return false;
    if (!m_heap.vm().numberOfActiveJITPlans())
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

size_t Collector::bytesVisited()
{
    size_t result = 0;
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            result += visitor.bytesVisited();
        });
    return result;
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

void Collector::startCollectingContinuously()
{
    m_collectContinuouslyThread = Thread::create(
        "JSC DEBUG Continuous GC"_s,
        [this] () {
            MonotonicTime initialTime = MonotonicTime::now();
            Seconds period = Seconds::fromMilliseconds(Options::collectContinuouslyPeriodMS());
            while (true) {
                Locker locker { m_collectContinuouslyLock };
                {
                    Locker locker { *m_threadLock };
                    if (m_requests.isEmpty()) {
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
