/*
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "Heap.h"

#include "BuiltinExecutables.h"
#include "CodeBlock.h"
#include "CodeBlockSetInlines.h"
#include "CollectingScope.h"
#include "ConservativeRoots.h"
#include "DFGWorklistInlines.h"
#include "EdenGCActivityCallback.h"
#include "Exception.h"
#include "FullGCActivityCallback.h"
#include "FunctionExecutableInlines.h"
#include "GCActivityCallback.h"
#include "GCIncomingRefCountedInlines.h"
#include "GCIncomingRefCountedSetInlines.h"
#include "GCSegmentedArrayInlines.h"
#include "GCTypeMap.h"
#include "HasOwnPropertyCache.h"
#include "HeapHelperPool.h"
#include "HeapIterationScope.h"
#include "HeapProfiler.h"
#include "HeapSnapshot.h"
#include "HeapVerifier.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "IsoCellSetInlines.h"
#include "JITStubRoutineSet.h"
#include "JITWorklist.h"
#include "JSVirtualMachineInternal.h"
#include "JSWeakMap.h"
#include "JSWeakObjectRef.h"
#include "JSWeakSet.h"
#include "JSWebAssemblyCodeBlock.h"
#include "MachineStackMarker.h"
#include "MarkStackMergingConstraint.h"
#include "MarkedJSValueRefArray.h"
#include "MarkedSpaceInlines.h"
#include "MarkingConstraintSet.h"
#include "PreventCollectionScope.h"
#include "SamplingProfiler.h"
#include "ShadowChicken.h"
#include "SpaceTimeMutatorScheduler.h"
#include "StochasticSpaceTimeMutatorScheduler.h"
#include "StopIfNecessaryTimer.h"
#include "SubspaceInlines.h"
#include "SuperSampler.h"
#include "SweepingScope.h"
#include "SymbolTableInlines.h"
#include "SynchronousStopTheWorldMutatorScheduler.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VM.h"
#include "WeakMapImplInlines.h"
#include "WeakSetInlines.h"
#include <algorithm>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/ListDump.h>
#include <wtf/RAMSize.h>
#include <wtf/Scope.h>
#include <wtf/SimpleStats.h>
#include <wtf/Threading.h>

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
#include <bmalloc/bmalloc.h>
#endif

#if USE(FOUNDATION)
#include <wtf/spi/cocoa/objcSPI.h>
#endif

#ifdef JSC_GLIB_API_ENABLED
#include "JSCGLibWrapperObject.h"
#endif

namespace JSC {

namespace {

static constexpr bool verboseStop = false;

double maxPauseMS(double thisPauseMS)
{
    static double maxPauseMS;
    maxPauseMS = std::max(thisPauseMS, maxPauseMS);
    return maxPauseMS;
}

size_t minHeapSize(HeapType heapType, size_t ramSize)
{
    if (heapType == LargeHeap) {
        double result = std::min(
            static_cast<double>(Options::largeHeapSize()),
            ramSize * Options::smallHeapRAMFraction());
        return static_cast<size_t>(result);
    }
    return Options::smallHeapSize();
}

size_t proportionalHeapSize(size_t heapSize, size_t ramSize)
{
    if (VM::isInMiniMode())
        return Options::miniVMHeapGrowthFactor() * heapSize;

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
    size_t memoryFootprint = bmalloc::api::memoryFootprint();
    if (memoryFootprint < ramSize * Options::smallHeapRAMFraction())
        return Options::smallHeapGrowthFactor() * heapSize;
    if (memoryFootprint < ramSize * Options::mediumHeapRAMFraction())
        return Options::mediumHeapGrowthFactor() * heapSize;
#else
    if (heapSize < ramSize * Options::smallHeapRAMFraction())
        return Options::smallHeapGrowthFactor() * heapSize;
    if (heapSize < ramSize * Options::mediumHeapRAMFraction())
        return Options::mediumHeapGrowthFactor() * heapSize;
#endif
    return Options::largeHeapGrowthFactor() * heapSize;
}

bool isValidSharedInstanceThreadState(VM& vm)
{
    return vm.currentThreadIsHoldingAPILock();
}

bool isValidThreadState(VM& vm)
{
    if (vm.atomStringTable() != Thread::current().atomStringTable())
        return false;

    if (vm.isSharedInstance() && !isValidSharedInstanceThreadState(vm))
        return false;

    return true;
}

void recordType(VM& vm, TypeCountSet& set, JSCell* cell)
{
    const char* typeName = "[unknown]";
    const ClassInfo* info = cell->classInfo(vm);
    if (info && info->className)
        typeName = info->className;
    set.add(typeName);
}

bool measurePhaseTiming()
{
    return false;
}

HashMap<const char*, GCTypeMap<SimpleStats>>& timingStats()
{
    static HashMap<const char*, GCTypeMap<SimpleStats>>* result;
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result = new HashMap<const char*, GCTypeMap<SimpleStats>>();
        });
    return *result;
}

SimpleStats& timingStats(const char* name, CollectionScope scope)
{
    return timingStats().add(name, GCTypeMap<SimpleStats>()).iterator->value[scope];
}

class TimingScope {
public:
    TimingScope(Optional<CollectionScope> scope, const char* name)
        : m_scope(scope)
        , m_name(name)
    {
        if (measurePhaseTiming())
            m_before = MonotonicTime::now();
    }
    
    TimingScope(Heap& heap, const char* name)
        : TimingScope(heap.collectionScope(), name)
    {
    }
    
    void setScope(Optional<CollectionScope> scope)
    {
        m_scope = scope;
    }
    
    void setScope(Heap& heap)
    {
        setScope(heap.collectionScope());
    }
    
    ~TimingScope()
    {
        if (measurePhaseTiming()) {
            MonotonicTime after = MonotonicTime::now();
            Seconds timing = after - m_before;
            SimpleStats& stats = timingStats(m_name, *m_scope);
            stats.add(timing.milliseconds());
            dataLog("[GC:", *m_scope, "] ", m_name, " took: ", timing.milliseconds(), "ms (average ", stats.mean(), "ms).\n");
        }
    }
private:
    Optional<CollectionScope> m_scope;
    MonotonicTime m_before;
    const char* m_name;
};

} // anonymous namespace

class Heap::HeapThread final : public AutomaticThread {
public:
    HeapThread(const AbstractLocker& locker, Heap& heap)
        : AutomaticThread(locker, heap.m_threadLock, heap.m_threadCondition.copyRef())
        , m_heap(heap)
    {
    }

    const char* name() const final
    {
        return "JSC Heap Collector Thread";
    }
    
private:
    PollResult poll(const AbstractLocker& locker) final
    {
        if (m_heap.m_threadShouldStop) {
            m_heap.notifyThreadStopping(locker);
            return PollResult::Stop;
        }
        if (m_heap.shouldCollectInCollectorThread(locker)) {
            m_heap.m_collectorThreadIsRunning = true;
            return PollResult::Work;
        }
        m_heap.m_collectorThreadIsRunning = false;
        return PollResult::Wait;
    }
    
    WorkResult work() final
    {
        m_heap.collectInCollectorThread();
        return WorkResult::Continue;
    }
    
    void threadDidStart() final
    {
        Thread::registerGCThread(GCThreadType::Main);
    }

    void threadIsStopping(const AbstractLocker&) final
    {
        m_heap.m_collectorThreadIsRunning = false;
    }

    Heap& m_heap;
};

Heap::Heap(VM& vm, HeapType heapType)
    : m_heapType(heapType)
    , m_ramSize(Options::forceRAMSize() ? Options::forceRAMSize() : ramSize())
    , m_minBytesPerCycle(minHeapSize(m_heapType, m_ramSize))
    , m_maxEdenSize(m_minBytesPerCycle)
    , m_maxHeapSize(m_minBytesPerCycle)
    , m_objectSpace(this)
    , m_machineThreads(makeUnique<MachineThreads>())
    , m_collectorSlotVisitor(makeUnique<SlotVisitor>(*this, "C"))
    , m_mutatorSlotVisitor(makeUnique<SlotVisitor>(*this, "M"))
    , m_mutatorMarkStack(makeUnique<MarkStackArray>())
    , m_raceMarkStack(makeUnique<MarkStackArray>())
    , m_constraintSet(makeUnique<MarkingConstraintSet>(*this))
    , m_handleSet(vm)
    , m_codeBlocks(makeUnique<CodeBlockSet>())
    , m_jitStubRoutines(makeUnique<JITStubRoutineSet>())
    , m_vm(vm)
    // We seed with 10ms so that GCActivityCallback::didAllocate doesn't continuously 
    // schedule the timer if we've never done a collection.
    , m_fullActivityCallback(GCActivityCallback::tryCreateFullTimer(this))
    , m_edenActivityCallback(GCActivityCallback::tryCreateEdenTimer(this))
    , m_sweeper(adoptRef(*new IncrementalSweeper(this)))
    , m_stopIfNecessaryTimer(adoptRef(*new StopIfNecessaryTimer(vm)))
    , m_sharedCollectorMarkStack(makeUnique<MarkStackArray>())
    , m_sharedMutatorMarkStack(makeUnique<MarkStackArray>())
    , m_helperClient(&heapHelperPool())
    , m_threadLock(Box<Lock>::create())
    , m_threadCondition(AutomaticThreadCondition::create())
{
    m_worldState.store(0);

    for (unsigned i = 0, numberOfParallelThreads = heapHelperPool().numberOfThreads(); i < numberOfParallelThreads; ++i) {
        std::unique_ptr<SlotVisitor> visitor = makeUnique<SlotVisitor>(*this, toCString("P", i + 1));
        if (Options::optimizeParallelSlotVisitorsForStoppedMutator())
            visitor->optimizeForStoppedMutator();
        m_availableParallelSlotVisitors.append(visitor.get());
        m_parallelSlotVisitors.append(WTFMove(visitor));
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
    
    if (Options::verifyHeap())
        m_verifier = makeUnique<HeapVerifier>(this, Options::numberOfGCCyclesToRecordForVerification());
    
    m_collectorSlotVisitor->optimizeForStoppedMutator();

    // When memory is critical, allow allocating 25% of the amount above the critical threshold before collecting.
    size_t memoryAboveCriticalThreshold = static_cast<size_t>(static_cast<double>(m_ramSize) * (1.0 - Options::criticalGCMemoryThreshold()));
    m_maxEdenSizeWhenCritical = memoryAboveCriticalThreshold / 4;

    LockHolder locker(*m_threadLock);
    m_thread = adoptRef(new HeapThread(locker, *this));
}

Heap::~Heap()
{
    // Scribble m_worldState to make it clear that the heap has already been destroyed if we crash in checkConn
    m_worldState.store(0xbadbeeffu);

    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.clearMarkStacks();
        });
    m_mutatorMarkStack->clear();
    m_raceMarkStack->clear();
    
    for (WeakBlock* block : m_logicallyEmptyWeakBlocks)
        WeakBlock::destroy(*this, block);
}

bool Heap::isPagedOut(MonotonicTime deadline)
{
    return m_objectSpace.isPagedOut(deadline);
}

void Heap::dumpHeapStatisticsAtVMDestruction()
{
    unsigned counter = 0;
    m_objectSpace.forEachBlock([&] (MarkedBlock::Handle* block) {
        unsigned live = 0;
        block->forEachCell([&] (size_t, HeapCell* cell, HeapCell::Kind) {
            if (cell->isLive())
                live++;
            return IterationStatus::Continue;
        });
        dataLogLn("[", counter++, "] ", block->cellSize(), ", ", live, " / ", block->cellsPerBlock(), " ", static_cast<double>(live) / block->cellsPerBlock() * 100, "% ", block->attributes(), " ", block->subspace()->name());
        block->forEachCell([&] (size_t, HeapCell* heapCell, HeapCell::Kind kind) {
            if (heapCell->isLive() && kind == HeapCell::Kind::JSCell) {
                auto* cell = static_cast<JSCell*>(heapCell);
                if (cell->isObject())
                    dataLogLn("    ", JSValue((JSObject*)cell));
                else
                    dataLogLn("    ", *cell);
            }
            return IterationStatus::Continue;
        });
    });
}

// The VM is being destroyed and the collector will never run again.
// Run all pending finalizers now because we won't get another chance.
void Heap::lastChanceToFinalize()
{
    MonotonicTime before;
    if (UNLIKELY(Options::logGC())) {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: shutdown ");
    }
    
    m_isShuttingDown = true;
    
    RELEASE_ASSERT(!m_vm.entryScope);
    RELEASE_ASSERT(m_mutatorState == MutatorState::Running);
    
    if (m_collectContinuouslyThread) {
        {
            LockHolder locker(m_collectContinuouslyLock);
            m_shouldStopCollectingContinuously = true;
            m_collectContinuouslyCondition.notifyOne();
        }
        m_collectContinuouslyThread->waitForCompletion();
    }

    dataLogIf(Options::logGC(), "1");
    
    // Prevent new collections from being started. This is probably not even necessary, since we're not
    // going to call into anything that starts collections. Still, this makes the algorithm more
    // obviously sound.
    m_isSafeToCollect = false;
    
    dataLogIf(Options::logGC(), "2");

    bool isCollecting;
    {
        auto locker = holdLock(*m_threadLock);
        RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
        isCollecting = m_lastServedTicket < m_lastGrantedTicket;
    }
    if (isCollecting) {
        dataLogIf(Options::logGC(), "...]\n");
        
        // Wait for the current collection to finish.
        waitForCollector(
            [&] (const AbstractLocker&) -> bool {
                RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
                return m_lastServedTicket == m_lastGrantedTicket;
            });
        
        dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: shutdown ");
    }
    dataLogIf(Options::logGC(), "3");

    RELEASE_ASSERT(m_requests.isEmpty());
    RELEASE_ASSERT(m_lastServedTicket == m_lastGrantedTicket);
    
    // Carefully bring the thread down.
    bool stopped = false;
    {
        LockHolder locker(*m_threadLock);
        stopped = m_thread->tryStop(locker);
        m_threadShouldStop = true;
        if (!stopped)
            m_threadCondition->notifyOne(locker);
    }

    dataLogIf(Options::logGC(), "4");
    
    if (!stopped)
        m_thread->join();
    
    dataLogIf(Options::logGC(), "5 ");

    if (UNLIKELY(Options::dumpHeapStatisticsAtVMDestruction()))
        dumpHeapStatisticsAtVMDestruction();
    
    m_arrayBuffers.lastChanceToFinalize();
    m_objectSpace.stopAllocatingForGood();
    m_objectSpace.lastChanceToFinalize();
    releaseDelayedReleasedObjects();

    sweepAllLogicallyEmptyWeakBlocks();
    
    m_objectSpace.freeMemory();
    
    dataLogIf(Options::logGC(), (MonotonicTime::now() - before).milliseconds(), "ms]\n");
}

void Heap::releaseDelayedReleasedObjects()
{
#if USE(FOUNDATION) || defined(JSC_GLIB_API_ENABLED)
    // We need to guard against the case that releasing an object can create more objects due to the
    // release calling into JS. When those JS call(s) exit and all locks are being dropped we end up
    // back here and could try to recursively release objects. We guard that with a recursive entry
    // count. Only the initial call will release objects, recursive calls simple return and let the
    // the initial call to the function take care of any objects created during release time.
    // This also means that we need to loop until there are no objects in m_delayedReleaseObjects
    // and use a temp Vector for the actual releasing.
    if (!m_delayedReleaseRecursionCount++) {
        while (!m_delayedReleaseObjects.isEmpty()) {
            ASSERT(m_vm.currentThreadIsHoldingAPILock());

            auto objectsToRelease = WTFMove(m_delayedReleaseObjects);

            {
                // We need to drop locks before calling out to arbitrary code.
                JSLock::DropAllLocks dropAllLocks(m_vm);

#if USE(FOUNDATION)
                void* context = objc_autoreleasePoolPush();
#endif
                objectsToRelease.clear();
#if USE(FOUNDATION)
                objc_autoreleasePoolPop(context);
#endif
            }
        }
    }
    m_delayedReleaseRecursionCount--;
#endif
}

void Heap::reportExtraMemoryAllocatedSlowCase(size_t size)
{
    didAllocate(size);
    collectIfNecessaryOrDefer();
}

void Heap::deprecatedReportExtraMemorySlowCase(size_t size)
{
    // FIXME: Change this to use SaturatedArithmetic when available.
    // https://bugs.webkit.org/show_bug.cgi?id=170411
    CheckedSize checkedNewSize = m_deprecatedExtraMemorySize;
    checkedNewSize += size;
    m_deprecatedExtraMemorySize = UNLIKELY(checkedNewSize.hasOverflowed()) ? std::numeric_limits<size_t>::max() : checkedNewSize.unsafeGet();
    reportExtraMemoryAllocatedSlowCase(size);
}

bool Heap::overCriticalMemoryThreshold(MemoryThresholdCallType memoryThresholdCallType)
{
#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
    if (memoryThresholdCallType == MemoryThresholdCallType::Direct || ++m_percentAvailableMemoryCachedCallCount >= 100) {
        m_overCriticalMemoryThreshold = bmalloc::api::percentAvailableMemoryInUse() > Options::criticalGCMemoryThreshold();
        m_percentAvailableMemoryCachedCallCount = 0;
    }

    return m_overCriticalMemoryThreshold;
#else
    UNUSED_PARAM(memoryThresholdCallType);
    return false;
#endif
}

void Heap::reportAbandonedObjectGraph()
{
    // Our clients don't know exactly how much memory they
    // are abandoning so we just guess for them.
    size_t abandonedBytes = static_cast<size_t>(0.1 * capacity());

    // We want to accelerate the next collection. Because memory has just 
    // been abandoned, the next collection has the potential to 
    // be more profitable. Since allocation is the trigger for collection, 
    // we hasten the next collection by pretending that we've allocated more memory. 
    if (m_fullActivityCallback) {
        m_fullActivityCallback->didAllocate(*this,
            m_sizeAfterLastCollect - m_sizeAfterLastFullCollect + m_bytesAllocatedThisCycle + m_bytesAbandonedSinceLastFullCollect);
    }
    m_bytesAbandonedSinceLastFullCollect += abandonedBytes;
}

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(m_vm.currentThreadIsHoldingAPILock());

    if (!k.isCell())
        return;

    m_protectedValues.add(k.asCell());
}

bool Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(m_vm.currentThreadIsHoldingAPILock());

    if (!k.isCell())
        return false;

    return m_protectedValues.remove(k.asCell());
}

void Heap::addReference(JSCell* cell, ArrayBuffer* buffer)
{
    if (m_arrayBuffers.addReference(cell, buffer)) {
        collectIfNecessaryOrDefer();
        didAllocate(buffer->gcSizeEstimateInBytes());
    }
}

template<typename CellType, typename CellSet>
void Heap::finalizeMarkedUnconditionalFinalizers(CellSet& cellSet)
{
    cellSet.forEachMarkedCell(
        [&] (HeapCell* cell, HeapCell::Kind) {
            static_cast<CellType*>(cell)->finalizeUnconditionally(vm());
        });
}

void Heap::finalizeUnconditionalFinalizers()
{
    vm().builtinExecutables()->finalizeUnconditionally();
    finalizeMarkedUnconditionalFinalizers<FunctionExecutable>(vm().functionExecutableSpace.space);
    finalizeMarkedUnconditionalFinalizers<SymbolTable>(vm().symbolTableSpace);
    finalizeMarkedUnconditionalFinalizers<ExecutableToCodeBlockEdge>(vm().executableToCodeBlockEdgesWithFinalizers); // We run this before CodeBlock's unconditional finalizer since CodeBlock looks at the owner executable's installed CodeBlock in its finalizeUnconditionally.
    vm().forEachCodeBlockSpace(
        [&] (auto& space) {
            this->finalizeMarkedUnconditionalFinalizers<CodeBlock>(space.set);
        });
    finalizeMarkedUnconditionalFinalizers<StructureRareData>(vm().structureRareDataSpace);
    finalizeMarkedUnconditionalFinalizers<UnlinkedFunctionExecutable>(vm().unlinkedFunctionExecutableSpace.set);
    if (vm().m_weakSetSpace)
        finalizeMarkedUnconditionalFinalizers<JSWeakSet>(*vm().m_weakSetSpace);
    if (vm().m_weakMapSpace)
        finalizeMarkedUnconditionalFinalizers<JSWeakMap>(*vm().m_weakMapSpace);
    if (vm().m_weakObjectRefSpace)
        finalizeMarkedUnconditionalFinalizers<JSWeakObjectRef>(*vm().m_weakObjectRefSpace);
    if (vm().m_errorInstanceSpace)
        finalizeMarkedUnconditionalFinalizers<ErrorInstance>(*vm().m_errorInstanceSpace);

#if ENABLE(WEBASSEMBLY)
    if (vm().m_webAssemblyCodeBlockSpace)
        finalizeMarkedUnconditionalFinalizers<JSWebAssemblyCodeBlock>(*vm().m_webAssemblyCodeBlockSpace);
#endif
}

void Heap::willStartIterating()
{
    m_objectSpace.willStartIterating();
}

void Heap::didFinishIterating()
{
    m_objectSpace.didFinishIterating();
}

void Heap::completeAllJITPlans()
{
    if (!Options::useJIT())
        return;
#if ENABLE(JIT)
    JITWorklist::ensureGlobalWorklist().completeAllForVM(m_vm);
#endif // ENABLE(JIT)
    DFG::completeAllPlansForVM(m_vm);
}

template<typename Func>
void Heap::iterateExecutingAndCompilingCodeBlocks(const Func& func)
{
    m_codeBlocks->iterateCurrentlyExecuting(func);
    if (Options::useJIT())
        DFG::iterateCodeBlocksForGC(m_vm, func);
}

template<typename Func>
void Heap::iterateExecutingAndCompilingCodeBlocksWithoutHoldingLocks(const Func& func)
{
    Vector<CodeBlock*, 256> codeBlocks;
    iterateExecutingAndCompilingCodeBlocks(
        [&] (CodeBlock* codeBlock) {
            codeBlocks.append(codeBlock);
        });
    for (CodeBlock* codeBlock : codeBlocks)
        func(codeBlock);
}

void Heap::assertMarkStacksEmpty()
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

void Heap::gatherStackRoots(ConservativeRoots& roots)
{
    m_machineThreads->gatherConservativeRoots(roots, *m_jitStubRoutines, *m_codeBlocks, m_currentThreadState, m_currentThread);
}

void Heap::gatherJSStackRoots(ConservativeRoots& roots)
{
#if ENABLE(C_LOOP)
    m_vm.interpreter->cloopStack().gatherConservativeRoots(roots, *m_jitStubRoutines, *m_codeBlocks);
#else
    UNUSED_PARAM(roots);
#endif
}

void Heap::gatherScratchBufferRoots(ConservativeRoots& roots)
{
#if ENABLE(DFG_JIT)
    if (!Options::useJIT())
        return;
    m_vm.gatherScratchBufferRoots(roots);
    m_vm.scanSideState(roots);
#else
    UNUSED_PARAM(roots);
#endif
}

void Heap::beginMarking()
{
    TimingScope timingScope(*this, "Heap::beginMarking");
    m_jitStubRoutines->clearMarks();
    m_objectSpace.beginMarking();
    setMutatorShouldBeFenced(true);
}

void Heap::removeDeadCompilerWorklistEntries()
{
#if ENABLE(DFG_JIT)
    if (!Options::useJIT())
        return;
    for (unsigned i = DFG::numberOfWorklists(); i--;)
        DFG::existingWorklistForIndex(i).removeDeadPlans(m_vm);
#endif
}

bool Heap::isAnalyzingHeap() const
{
    HeapProfiler* heapProfiler = m_vm.heapProfiler();
    if (UNLIKELY(heapProfiler))
        return heapProfiler->activeHeapAnalyzer();
    return false;
}

struct GatherExtraHeapData : MarkedBlock::CountFunctor {
    GatherExtraHeapData(VM& vm, HeapAnalyzer& analyzer)
        : m_vm(vm)
        , m_analyzer(analyzer)
    {
    }

    IterationStatus operator()(HeapCell* heapCell, HeapCell::Kind kind) const
    {
        if (isJSCellKind(kind)) {
            JSCell* cell = static_cast<JSCell*>(heapCell);
            cell->methodTable(m_vm)->analyzeHeap(cell, m_analyzer);
        }
        return IterationStatus::Continue;
    }

    VM& m_vm;
    HeapAnalyzer& m_analyzer;
};

void Heap::gatherExtraHeapData(HeapProfiler& heapProfiler)
{
    if (auto* analyzer = heapProfiler.activeHeapAnalyzer()) {
        HeapIterationScope heapIterationScope(*this);
        GatherExtraHeapData functor(m_vm, *analyzer);
        m_objectSpace.forEachLiveCell(heapIterationScope, functor);
    }
}

struct RemoveDeadHeapSnapshotNodes : MarkedBlock::CountFunctor {
    RemoveDeadHeapSnapshotNodes(HeapSnapshot& snapshot)
        : m_snapshot(snapshot)
    {
    }

    IterationStatus operator()(HeapCell* cell, HeapCell::Kind kind) const
    {
        if (isJSCellKind(kind))
            m_snapshot.sweepCell(static_cast<JSCell*>(cell));
        return IterationStatus::Continue;
    }

    HeapSnapshot& m_snapshot;
};

void Heap::removeDeadHeapSnapshotNodes(HeapProfiler& heapProfiler)
{
    if (HeapSnapshot* snapshot = heapProfiler.mostRecentSnapshot()) {
        HeapIterationScope heapIterationScope(*this);
        RemoveDeadHeapSnapshotNodes functor(*snapshot);
        m_objectSpace.forEachDeadCell(heapIterationScope, functor);
        snapshot->shrinkToFit();
    }
}

void Heap::updateObjectCounts()
{
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full)
        m_totalBytesVisited = 0;

    m_totalBytesVisitedThisCycle = bytesVisited();
    
    m_totalBytesVisited += m_totalBytesVisitedThisCycle;
}

void Heap::endMarking()
{
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.reset();
        });

    assertMarkStacksEmpty();

    RELEASE_ASSERT(m_raceMarkStack->isEmpty());
    
    m_objectSpace.endMarking();
    setMutatorShouldBeFenced(Options::forceFencedBarrier());
}

size_t Heap::objectCount()
{
    return m_objectSpace.objectCount();
}

size_t Heap::extraMemorySize()
{
    // FIXME: Change this to use SaturatedArithmetic when available.
    // https://bugs.webkit.org/show_bug.cgi?id=170411
    CheckedSize checkedTotal = m_extraMemorySize;
    checkedTotal += m_deprecatedExtraMemorySize;
    checkedTotal += m_arrayBuffers.size();
    size_t total = UNLIKELY(checkedTotal.hasOverflowed()) ? std::numeric_limits<size_t>::max() : checkedTotal.unsafeGet();

    ASSERT(m_objectSpace.capacity() >= m_objectSpace.size());
    return std::min(total, std::numeric_limits<size_t>::max() - m_objectSpace.capacity());
}

size_t Heap::size()
{
    return m_objectSpace.size() + extraMemorySize();
}

size_t Heap::capacity()
{
    return m_objectSpace.capacity() + extraMemorySize();
}

size_t Heap::protectedGlobalObjectCount()
{
    size_t result = 0;
    forEachProtectedCell(
        [&] (JSCell* cell) {
            if (cell->isObject() && asObject(cell)->isGlobalObject())
                result++;
        });
    return result;
}

size_t Heap::globalObjectCount()
{
    HeapIterationScope iterationScope(*this);
    size_t result = 0;
    m_objectSpace.forEachLiveCell(
        iterationScope,
        [&] (HeapCell* heapCell, HeapCell::Kind kind) -> IterationStatus {
            if (!isJSCellKind(kind))
                return IterationStatus::Continue;
            JSCell* cell = static_cast<JSCell*>(heapCell);
            if (cell->isObject() && asObject(cell)->isGlobalObject())
                result++;
            return IterationStatus::Continue;
        });
    return result;
}

size_t Heap::protectedObjectCount()
{
    size_t result = 0;
    forEachProtectedCell(
        [&] (JSCell*) {
            result++;
        });
    return result;
}

std::unique_ptr<TypeCountSet> Heap::protectedObjectTypeCounts()
{
    std::unique_ptr<TypeCountSet> result = makeUnique<TypeCountSet>();
    forEachProtectedCell(
        [&] (JSCell* cell) {
            recordType(vm(), *result, cell);
        });
    return result;
}

std::unique_ptr<TypeCountSet> Heap::objectTypeCounts()
{
    std::unique_ptr<TypeCountSet> result = makeUnique<TypeCountSet>();
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachLiveCell(
        iterationScope,
        [&] (HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
            if (isJSCellKind(kind))
                recordType(vm(), *result, static_cast<JSCell*>(cell));
            return IterationStatus::Continue;
        });
    return result;
}

void Heap::deleteAllCodeBlocks(DeleteAllCodeEffort effort)
{
    if (m_collectionScope && effort == DeleteAllCodeIfNotCollecting)
        return;

    VM& vm = m_vm;
    PreventCollectionScope preventCollectionScope(*this);
    
    // If JavaScript is running, it's not safe to delete all JavaScript code, since
    // we'll end up returning to deleted code.
    RELEASE_ASSERT(!vm.entryScope);
    RELEASE_ASSERT(!m_collectionScope);

    completeAllJITPlans();

    vm.forEachScriptExecutableSpace(
        [&] (auto& spaceAndSet) {
            HeapIterationScope heapIterationScope(*this);
            auto& set = spaceAndSet.set;
            set.forEachLiveCell(
                [&] (HeapCell* cell, HeapCell::Kind) {
                    ScriptExecutable* executable = static_cast<ScriptExecutable*>(cell);
                    executable->clearCode(set);
                });
        });

#if ENABLE(WEBASSEMBLY)
    {
        // We must ensure that we clear the JS call ICs from Wasm. Otherwise, Wasm will
        // have no idea that we cleared the code from all of the Executables in the
        // VM. This could leave Wasm in an inconsistent state where it has an IC that
        // points into a CodeBlock that could be dead. The IC will still succeed because
        // it uses a callee check, but then it will call into dead code.
        HeapIterationScope heapIterationScope(*this);
        if (vm.m_webAssemblyCodeBlockSpace) {
            vm.m_webAssemblyCodeBlockSpace->forEachLiveCell([&] (HeapCell* cell, HeapCell::Kind kind) {
                ASSERT_UNUSED(kind, kind == HeapCell::JSCell);
                JSWebAssemblyCodeBlock* codeBlock = static_cast<JSWebAssemblyCodeBlock*>(cell);
                codeBlock->clearJSCallICs(vm);
            });
        }
    }
#endif
}

void Heap::deleteAllUnlinkedCodeBlocks(DeleteAllCodeEffort effort)
{
    if (m_collectionScope && effort == DeleteAllCodeIfNotCollecting)
        return;

    VM& vm = m_vm;
    PreventCollectionScope preventCollectionScope(*this);

    RELEASE_ASSERT(!m_collectionScope);

    HeapIterationScope heapIterationScope(*this);
    vm.unlinkedFunctionExecutableSpace.set.forEachLiveCell(
        [&] (HeapCell* cell, HeapCell::Kind) {
            UnlinkedFunctionExecutable* executable = static_cast<UnlinkedFunctionExecutable*>(cell);
            executable->clearCode(vm);
        });
}

void Heap::deleteUnmarkedCompiledCode()
{
    vm().forEachScriptExecutableSpace([] (auto& space) { space.space.sweep(); });
    // Sweeping must occur before deleting stubs, otherwise the stubs might still think they're alive as they get deleted.
    // And CodeBlock destructor is assuming that CodeBlock gets destroyed before UnlinkedCodeBlock gets destroyed.
    vm().forEachCodeBlockSpace([] (auto& space) { space.space.sweep(); });
    m_jitStubRoutines->deleteUnmarkedJettisonedStubRoutines();
}

void Heap::addToRememberedSet(const JSCell* constCell)
{
    JSCell* cell = const_cast<JSCell*>(constCell);
    ASSERT(cell);
    ASSERT(!Options::useConcurrentJIT() || !isCompilationThread());
    m_barriersExecuted++;
    if (m_mutatorShouldBeFenced) {
        WTF::loadLoadFence();
        if (!isMarked(cell)) {
            // During a full collection a store into an unmarked object that had surivived past
            // collections will manifest as a store to an unmarked PossiblyBlack object. If the
            // object gets marked at some time after this then it will go down the normal marking
            // path. So, we don't have to remember this object. We could return here. But we go
            // further and attempt to re-white the object.
            
            RELEASE_ASSERT(m_collectionScope && m_collectionScope.value() == CollectionScope::Full);
            
            if (cell->atomicCompareExchangeCellStateStrong(CellState::PossiblyBlack, CellState::DefinitelyWhite) == CellState::PossiblyBlack) {
                // Now we protect against this race:
                //
                //     1) Object starts out black + unmarked.
                //     --> We do isMarked here.
                //     2) Object is marked and greyed.
                //     3) Object is scanned and blacked.
                //     --> We do atomicCompareExchangeCellStateStrong here.
                //
                // In this case we would have made the object white again, even though it should
                // be black. This check lets us correct our mistake. This relies on the fact that
                // isMarked converges monotonically to true.
                if (isMarked(cell)) {
                    // It's difficult to work out whether the object should be grey or black at
                    // this point. We say black conservatively.
                    cell->setCellState(CellState::PossiblyBlack);
                }
                
                // Either way, we can return. Most likely, the object was not marked, and so the
                // object is now labeled white. This means that future barrier executions will not
                // fire. In the unlikely event that the object had become marked, we can still
                // return anyway, since we proved that the object was not marked at the time that
                // we executed this slow path.
            }
            
            return;
        }
    } else
        ASSERT(isMarked(cell));
    // It could be that the object was *just* marked. This means that the collector may set the
    // state to DefinitelyGrey and then to PossiblyOldOrBlack at any time. It's OK for us to
    // race with the collector here. If we win then this is accurate because the object _will_
    // get scanned again. If we lose then someone else will barrier the object again. That would
    // be unfortunate but not the end of the world.
    cell->setCellState(CellState::PossiblyGrey);
    m_mutatorMarkStack->append(cell);
}

void Heap::sweepSynchronously()
{
    MonotonicTime before { };
    if (UNLIKELY(Options::logGC())) {
        dataLog("Full sweep: ", capacity() / 1024, "kb ");
        before = MonotonicTime::now();
    }
    m_objectSpace.sweepBlocks();
    m_objectSpace.shrink();
    if (UNLIKELY(Options::logGC())) {
        MonotonicTime after = MonotonicTime::now();
        dataLog("=> ", capacity() / 1024, "kb, ", (after - before).milliseconds(), "ms");
    }
}

void Heap::collect(Synchronousness synchronousness, GCRequest request)
{
    switch (synchronousness) {
    case Async:
        collectAsync(request);
        return;
    case Sync:
        collectSync(request);
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Heap::collectNow(Synchronousness synchronousness, GCRequest request)
{
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    switch (synchronousness) {
    case Async: {
        collectAsync(request);
        stopIfNecessary();
        return;
    }
        
    case Sync: {
        collectSync(request);
        
        DeferGCForAWhile deferGC(*this);
        if (UNLIKELY(Options::useImmortalObjects()))
            sweeper().stopSweeping();
        
        bool alreadySweptInCollectSync = shouldSweepSynchronously();
        if (!alreadySweptInCollectSync) {
            dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: ");
            sweepSynchronously();
            dataLogIf(Options::logGC(), "]\n");
        }
        m_objectSpace.assertNoUnswept();
        
        sweepAllLogicallyEmptyWeakBlocks();
        return;
    } }
    RELEASE_ASSERT_NOT_REACHED();
}

void Heap::collectAsync(GCRequest request)
{
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    bool alreadyRequested = false;
    {
        LockHolder locker(*m_threadLock);
        for (const GCRequest& previousRequest : m_requests) {
            if (request.subsumedBy(previousRequest)) {
                alreadyRequested = true;
                break;
            }
        }
    }
    if (alreadyRequested)
        return;

    requestCollection(request);
}

void Heap::collectSync(GCRequest request)
{
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    waitForCollection(requestCollection(request));
}

bool Heap::shouldCollectInCollectorThread(const AbstractLocker&)
{
    RELEASE_ASSERT(m_requests.isEmpty() == (m_lastServedTicket == m_lastGrantedTicket));
    RELEASE_ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
    
    if (false)
        dataLog("Mutator has the conn = ", !!(m_worldState.load() & mutatorHasConnBit), "\n");
    
    return !m_requests.isEmpty() && !(m_worldState.load() & mutatorHasConnBit);
}

void Heap::collectInCollectorThread()
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

ALWAYS_INLINE int asInt(CollectorPhase phase)
{
    return static_cast<int>(phase);
}

void Heap::checkConn(GCConductor conn)
{
    unsigned worldState = m_worldState.load();
    switch (conn) {
    case GCConductor::Mutator:
        RELEASE_ASSERT(worldState & mutatorHasConnBit, worldState, asInt(m_lastPhase), asInt(m_currentPhase), asInt(m_nextPhase), vm().id(), VM::numberOfIDs(), vm().isEntered());
        return;
    case GCConductor::Collector:
        RELEASE_ASSERT(!(worldState & mutatorHasConnBit), worldState, asInt(m_lastPhase), asInt(m_currentPhase), asInt(m_nextPhase), vm().id(), VM::numberOfIDs(), vm().isEntered());
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto Heap::runCurrentPhase(GCConductor conn, CurrentThreadState* currentThreadState) -> RunCurrentPhaseResult
{
    checkConn(conn);
    m_currentThreadState = currentThreadState;
    m_currentThread = &Thread::current();
    
    if (conn == GCConductor::Mutator)
        sanitizeStackForVM(vm());
    
    // If the collector transfers the conn to the mutator, it leaves us in between phases.
    if (!finishChangingPhase(conn)) {
        // A mischevious mutator could repeatedly relinquish the conn back to us. We try to avoid doing
        // this, but it's probably not the end of the world if it did happen.
        if (false)
            dataLog("Conn bounce-back.\n");
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
        if (!currentThreadState && conn == GCConductor::Mutator)
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

NEVER_INLINE bool Heap::runNotRunningPhase(GCConductor conn)
{
    // Check m_requests since the mutator calls this to poll what's going on.
    {
        auto locker = holdLock(*m_threadLock);
        if (m_requests.isEmpty())
            return false;
    }
    
    return changePhase(conn, CollectorPhase::Begin);
}

NEVER_INLINE bool Heap::runBeginPhase(GCConductor conn)
{
    m_currentGCStartTime = MonotonicTime::now();
    
    {
        LockHolder locker(*m_threadLock);
        RELEASE_ASSERT(!m_requests.isEmpty());
        m_currentRequest = m_requests.first();
    }
        
    dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: START ", gcConductorShortName(conn), " ", capacity() / 1024, "kb ");

    m_beforeGC = MonotonicTime::now();

    if (!Options::seedOfVMRandomForFuzzer())
        vm().random().setSeed(cryptographicallyRandomNumber());

    if (m_collectionScope) {
        dataLogLn("Collection scope already set during GC: ", *m_collectionScope);
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    willStartCollection();
        
    if (UNLIKELY(m_verifier)) {
        // Verify that live objects from the last GC cycle haven't been corrupted by
        // mutators before we begin this new GC cycle.
        m_verifier->verify(HeapVerifier::Phase::BeforeGC);
            
        m_verifier->startGC();
        m_verifier->gatherLiveCells(HeapVerifier::Phase::BeforeMarking);
    }
        
    prepareForMarking();
        
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        m_opaqueRoots.clear();
        m_collectorSlotVisitor->clearMarkStacks();
        m_mutatorMarkStack->clear();
    }

    RELEASE_ASSERT(m_raceMarkStack->isEmpty());

    beginMarking();

    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            visitor.didStartMarking();
        });

    m_parallelMarkersShouldExit = false;

    m_helperClient.setFunction(
        [this] () {
            SlotVisitor* slotVisitor;
            {
                LockHolder locker(m_parallelSlotVisitorLock);
                RELEASE_ASSERT_WITH_MESSAGE(!m_availableParallelSlotVisitors.isEmpty(), "Parallel SlotVisitors are allocated apriori");
                slotVisitor = m_availableParallelSlotVisitors.takeLast();
            }

            Thread::registerGCThread(GCThreadType::Helper);

            {
                ParallelModeEnabler parallelModeEnabler(*slotVisitor);
                slotVisitor->drainFromShared(SlotVisitor::HelperDrain);
            }

            {
                LockHolder locker(m_parallelSlotVisitorLock);
                m_availableParallelSlotVisitors.append(slotVisitor);
            }
        });

    SlotVisitor& slotVisitor = *m_collectorSlotVisitor;

    m_constraintSet->didStartMarking();
    
    m_scheduler->beginCollection();
    if (UNLIKELY(Options::logGC()))
        m_scheduler->log();
    
    // After this, we will almost certainly fall through all of the "slotVisitor.isEmpty()"
    // checks because bootstrap would have put things into the visitor. So, we should fall
    // through to draining.
    
    if (!slotVisitor.didReachTermination()) {
        dataLog("Fatal: SlotVisitor should think that GC should terminate before constraint solving, but it does not think this.\n");
        dataLog("slotVisitor.isEmpty(): ", slotVisitor.isEmpty(), "\n");
        dataLog("slotVisitor.collectorMarkStack().isEmpty(): ", slotVisitor.collectorMarkStack().isEmpty(), "\n");
        dataLog("slotVisitor.mutatorMarkStack().isEmpty(): ", slotVisitor.mutatorMarkStack().isEmpty(), "\n");
        dataLog("m_numberOfActiveParallelMarkers: ", m_numberOfActiveParallelMarkers, "\n");
        dataLog("m_sharedCollectorMarkStack->isEmpty(): ", m_sharedCollectorMarkStack->isEmpty(), "\n");
        dataLog("m_sharedMutatorMarkStack->isEmpty(): ", m_sharedMutatorMarkStack->isEmpty(), "\n");
        dataLog("slotVisitor.didReachTermination(): ", slotVisitor.didReachTermination(), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
        
    return changePhase(conn, CollectorPhase::Fixpoint);
}

NEVER_INLINE bool Heap::runFixpointPhase(GCConductor conn)
{
    RELEASE_ASSERT(conn == GCConductor::Collector || m_currentThreadState);
    
    SlotVisitor& slotVisitor = *m_collectorSlotVisitor;
    
    if (UNLIKELY(Options::logGC())) {
        HashMap<const char*, size_t> visitMap;
        forEachSlotVisitor(
            [&] (SlotVisitor& slotVisitor) {
                visitMap.add(slotVisitor.codeName(), slotVisitor.bytesVisited() / 1024);
            });
        
        auto perVisitorDump = sortedMapDump(
            visitMap,
            [] (const char* a, const char* b) -> bool {
                return strcmp(a, b) < 0;
            },
            ":", " ");
        
        dataLog("v=", bytesVisited() / 1024, "kb (", perVisitorDump, ") o=", m_opaqueRoots.size(), " b=", m_barriersExecuted, " ");
    }
        
    if (slotVisitor.didReachTermination()) {
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
        bool converged = m_constraintSet->executeConvergence(slotVisitor);
        
        // FIXME: The slotVisitor.isEmpty() check is most likely not needed.
        // https://bugs.webkit.org/show_bug.cgi?id=180310
        if (converged && slotVisitor.isEmpty()) {
            assertMarkStacksEmpty();
            return changePhase(conn, CollectorPhase::End);
        }
            
        m_scheduler->didExecuteConstraints();
    }
        
    dataLogIf(Options::logGC(), slotVisitor.collectorMarkStack().size(), "+", m_mutatorMarkStack->size() + slotVisitor.mutatorMarkStack().size(), " ");
        
    {
        ParallelModeEnabler enabler(slotVisitor);
        slotVisitor.drainInParallel(m_scheduler->timeToResume());
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
    if (slotVisitor.didReachTermination())
        return true; // This is like relooping to the top if runFixpointPhase().
        
    if (!m_scheduler->shouldResume())
        return true;

    m_scheduler->willResume();
        
    if (UNLIKELY(Options::logGC())) {
        double thisPauseMS = (MonotonicTime::now() - m_stopTime).milliseconds();
        dataLog("p=", thisPauseMS, "ms (max ", maxPauseMS(thisPauseMS), ")...]\n");
    }

    // Forgive the mutator for its past failures to keep up.
    // FIXME: Figure out if moving this to different places results in perf changes.
    m_incrementBalance = 0;
        
    return changePhase(conn, CollectorPhase::Concurrent);
}

NEVER_INLINE bool Heap::runConcurrentPhase(GCConductor conn)
{
    SlotVisitor& slotVisitor = *m_collectorSlotVisitor;

    switch (conn) {
    case GCConductor::Mutator: {
        // When the mutator has the conn, we poll runConcurrentPhase() on every time someone says
        // stopIfNecessary(), so on every allocation slow path. When that happens we poll if it's time
        // to stop and do some work.
        if (slotVisitor.didReachTermination()
            || m_scheduler->shouldStop())
            return changePhase(conn, CollectorPhase::Reloop);
        
        // We could be coming from a collector phase that stuffed our SlotVisitor, so make sure we donate
        // everything. This is super cheap if the SlotVisitor is already empty.
        slotVisitor.donateAll();
        return false;
    }
    case GCConductor::Collector: {
        {
            ParallelModeEnabler enabler(slotVisitor);
            slotVisitor.drainInParallelPassively(m_scheduler->timeToStop());
        }
        return changePhase(conn, CollectorPhase::Reloop);
    } }
    
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

NEVER_INLINE bool Heap::runReloopPhase(GCConductor conn)
{
    dataLogIf(Options::logGC(), "[GC<", RawPointer(this), ">: ", gcConductorShortName(conn), " ");
    
    m_scheduler->didStop();
    
    if (UNLIKELY(Options::logGC()))
        m_scheduler->log();
    
    return changePhase(conn, CollectorPhase::Fixpoint);
}

NEVER_INLINE bool Heap::runEndPhase(GCConductor conn)
{
    m_scheduler->endCollection();
        
    {
        auto locker = holdLock(m_markingMutex);
        m_parallelMarkersShouldExit = true;
        m_markingConditionVariable.notifyAll();
    }
    m_helperClient.finish();
    
    iterateExecutingAndCompilingCodeBlocks(
        [&] (CodeBlock* codeBlock) {
            writeBarrier(codeBlock);
        });
        
    updateObjectCounts();
    endMarking();

    if (UNLIKELY(m_verifier)) {
        m_verifier->gatherLiveCells(HeapVerifier::Phase::AfterMarking);
        m_verifier->verify(HeapVerifier::Phase::AfterMarking);
    }
        
    {
        auto* previous = Thread::current().setCurrentAtomStringTable(nullptr);
        auto scopeExit = makeScopeExit([&] {
            Thread::current().setCurrentAtomStringTable(previous);
        });

        if (vm().typeProfiler())
            vm().typeProfiler()->invalidateTypeSetCache(vm());

        m_structureIDTable.flushOldTables();

        reapWeakHandles();
        pruneStaleEntriesFromWeakGCMaps();
        sweepArrayBuffers();
        snapshotUnswept();
        finalizeUnconditionalFinalizers(); // We rely on these unconditional finalizers running before clearCurrentlyExecuting since CodeBlock's finalizer relies on querying currently executing.
        removeDeadCompilerWorklistEntries();
    }

    notifyIncrementalSweeper();
    
    m_codeBlocks->iterateCurrentlyExecuting(
        [&] (CodeBlock* codeBlock) {
            writeBarrier(codeBlock);
        });
    m_codeBlocks->clearCurrentlyExecuting();
        
    m_objectSpace.prepareForAllocation();
    updateAllocationLimits();

    if (UNLIKELY(m_verifier)) {
        m_verifier->trimDeadCells();
        m_verifier->verify(HeapVerifier::Phase::AfterGC);
    }

    didFinishCollection();
    
    if (m_currentRequest.didFinishEndPhase)
        m_currentRequest.didFinishEndPhase->run();
    
    if (false) {
        dataLog("Heap state after GC:\n");
        m_objectSpace.dumpBits();
    }
    
    if (UNLIKELY(Options::logGC())) {
        double thisPauseMS = (m_afterGC - m_stopTime).milliseconds();
        dataLog("p=", thisPauseMS, "ms (max ", maxPauseMS(thisPauseMS), "), cycle ", (m_afterGC - m_beforeGC).milliseconds(), "ms END]\n");
    }
    
    {
        auto locker = holdLock(*m_threadLock);
        m_requests.removeFirst();
        m_lastServedTicket++;
        clearMutatorWaiting();
    }
    ParkingLot::unparkAll(&m_worldState);

    dataLogLnIf(Options::logGC(), "GC END!");

    setNeedFinalize();

    m_lastGCStartTime = m_currentGCStartTime;
    m_lastGCEndTime = MonotonicTime::now();
    m_totalGCTime += m_lastGCEndTime - m_lastGCStartTime;
        
    return changePhase(conn, CollectorPhase::NotRunning);
}

bool Heap::changePhase(GCConductor conn, CollectorPhase nextPhase)
{
    checkConn(conn);

    m_lastPhase = m_currentPhase;
    m_nextPhase = nextPhase;

    return finishChangingPhase(conn);
}

NEVER_INLINE bool Heap::finishChangingPhase(GCConductor conn)
{
    checkConn(conn);
    
    if (m_nextPhase == m_currentPhase)
        return true;

    if (false)
        dataLog(conn, ": Going to phase: ", m_nextPhase, " (from ", m_currentPhase, ")\n");
    
    m_phaseVersion++;
    
    bool suspendedBefore = worldShouldBeSuspended(m_currentPhase);
    bool suspendedAfter = worldShouldBeSuspended(m_nextPhase);
    
    if (suspendedBefore != suspendedAfter) {
        if (suspendedBefore) {
            RELEASE_ASSERT(!suspendedAfter);
            
            resumeThePeriphery();
            if (conn == GCConductor::Collector)
                resumeTheMutator();
            else
                handleNeedFinalize();
        } else {
            RELEASE_ASSERT(!suspendedBefore);
            RELEASE_ASSERT(suspendedAfter);
            
            if (conn == GCConductor::Collector) {
                waitWhileNeedFinalize();
                if (!stopTheMutator()) {
                    if (false)
                        dataLog("Returning false.\n");
                    return false;
                }
            } else {
                sanitizeStackForVM(m_vm);
                handleNeedFinalize();
            }
            stopThePeriphery(conn);
        }
    }
    
    m_currentPhase = m_nextPhase;
    return true;
}

void Heap::stopThePeriphery(GCConductor conn)
{
    if (m_worldIsStopped) {
        dataLog("FATAL: world already stopped.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (m_mutatorDidRun)
        m_mutatorExecutionVersion++;
    
    m_mutatorDidRun = false;

    suspendCompilerThreads();
    m_worldIsStopped = true;

    forEachSlotVisitor(
        [&] (SlotVisitor& slotVisitor) {
            slotVisitor.updateMutatorIsStopped(NoLockingNecessary);
        });

#if ENABLE(JIT)
    if (Options::useJIT()) {
        DeferGCForAWhile awhile(*this);
        if (JITWorklist::ensureGlobalWorklist().completeAllForVM(m_vm)
            && conn == GCConductor::Collector)
            setGCDidJIT();
    }
#endif // ENABLE(JIT)
    UNUSED_PARAM(conn);
    
    if (auto* shadowChicken = vm().shadowChicken())
        shadowChicken->update(vm(), vm().topCallFrame);
    
    m_objectSpace.stopAllocating();
    
    m_stopTime = MonotonicTime::now();
}

NEVER_INLINE void Heap::resumeThePeriphery()
{
    // Calling resumeAllocating does the Right Thing depending on whether this is the end of a
    // collection cycle or this is just a concurrent phase within a collection cycle:
    // - At end of collection cycle: it's a no-op because prepareForAllocation already cleared the
    //   last active block.
    // - During collection cycle: it reinstates the last active block.
    m_objectSpace.resumeAllocating();
    
    m_barriersExecuted = 0;
    
    if (!m_worldIsStopped) {
        dataLog("Fatal: collector does not believe that the world is stopped.\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    m_worldIsStopped = false;
    
    // FIXME: This could be vastly improved: we want to grab the locks in the order in which they
    // become available. We basically want a lockAny() method that will lock whatever lock is available
    // and tell you which one it locked. That would require teaching ParkingLot how to park on multiple
    // queues at once, which is totally achievable - it would just require memory allocation, which is
    // suboptimal but not a disaster. Alternatively, we could replace the SlotVisitor rightToRun lock
    // with a DLG-style handshake mechanism, but that seems not as general.
    Vector<SlotVisitor*, 8> slotVisitorsToUpdate;

    forEachSlotVisitor(
        [&] (SlotVisitor& slotVisitor) {
            slotVisitorsToUpdate.append(&slotVisitor);
        });
    
    for (unsigned countdown = 40; !slotVisitorsToUpdate.isEmpty() && countdown--;) {
        for (unsigned index = 0; index < slotVisitorsToUpdate.size(); ++index) {
            SlotVisitor& slotVisitor = *slotVisitorsToUpdate[index];
            bool remove = false;
            if (slotVisitor.hasAcknowledgedThatTheMutatorIsResumed())
                remove = true;
            else if (auto locker = tryHoldLock(slotVisitor.rightToRun())) {
                slotVisitor.updateMutatorIsStopped(locker);
                remove = true;
            }
            if (remove) {
                slotVisitorsToUpdate[index--] = slotVisitorsToUpdate.last();
                slotVisitorsToUpdate.takeLast();
            }
        }
        Thread::yield();
    }
    
    for (SlotVisitor* slotVisitor : slotVisitorsToUpdate)
        slotVisitor->updateMutatorIsStopped();
    
    resumeCompilerThreads();
}

bool Heap::stopTheMutator()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (oldState & stoppedBit) {
            RELEASE_ASSERT(!(oldState & hasAccessBit));
            RELEASE_ASSERT(!(oldState & mutatorWaitingBit));
            RELEASE_ASSERT(!(oldState & mutatorHasConnBit));
            return true;
        }
        
        if (oldState & mutatorHasConnBit) {
            RELEASE_ASSERT(!(oldState & hasAccessBit));
            RELEASE_ASSERT(!(oldState & stoppedBit));
            return false;
        }

        if (!(oldState & hasAccessBit)) {
            RELEASE_ASSERT(!(oldState & mutatorHasConnBit));
            RELEASE_ASSERT(!(oldState & mutatorWaitingBit));
            // We can stop the world instantly.
            if (m_worldState.compareExchangeWeak(oldState, oldState | stoppedBit))
                return true;
            continue;
        }
        
        // Transfer the conn to the mutator and bail.
        RELEASE_ASSERT(oldState & hasAccessBit);
        RELEASE_ASSERT(!(oldState & stoppedBit));
        unsigned newState = (oldState | mutatorHasConnBit) & ~mutatorWaitingBit;
        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            if (false)
                dataLog("Handed off the conn.\n");
            m_stopIfNecessaryTimer->scheduleSoon();
            ParkingLot::unparkAll(&m_worldState);
            return false;
        }
    }
}

NEVER_INLINE void Heap::resumeTheMutator()
{
    if (false)
        dataLog("Resuming the mutator.\n");
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!!(oldState & hasAccessBit) != !(oldState & stoppedBit)) {
            dataLog("Fatal: hasAccess = ", !!(oldState & hasAccessBit), ", stopped = ", !!(oldState & stoppedBit), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (oldState & mutatorHasConnBit) {
            dataLog("Fatal: mutator has the conn.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        if (!(oldState & stoppedBit)) {
            if (false)
                dataLog("Returning because not stopped.\n");
            return;
        }
        
        if (m_worldState.compareExchangeWeak(oldState, oldState & ~stoppedBit)) {
            if (false)
                dataLog("CASing and returning.\n");
            ParkingLot::unparkAll(&m_worldState);
            return;
        }
    }
}

void Heap::stopIfNecessarySlow()
{
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    while (stopIfNecessarySlow(m_worldState.load())) { }
    
    RELEASE_ASSERT(m_worldState.load() & hasAccessBit);
    RELEASE_ASSERT(!(m_worldState.load() & stoppedBit));
    
    handleGCDidJIT();
    handleNeedFinalize();
    m_mutatorDidRun = true;
}

bool Heap::stopIfNecessarySlow(unsigned oldState)
{
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    // It's possible for us to wake up with finalization already requested but the world not yet
    // resumed. If that happens, we can't run finalization yet.
    if (handleNeedFinalize(oldState))
        return true;

    // FIXME: When entering the concurrent phase, we could arrange for this branch not to fire, and then
    // have the SlotVisitor do things to the m_worldState to make this branch fire again. That would
    // prevent us from polling this so much. Ideally, stopIfNecessary would ignore the mutatorHasConnBit
    // and there would be some other bit indicating whether we were in some GC phase other than the
    // NotRunning or Concurrent ones.
    if (oldState & mutatorHasConnBit)
        collectInMutatorThread();
    
    return false;
}

NEVER_INLINE void Heap::collectInMutatorThread()
{
    CollectingScope collectingScope(*this);
    for (;;) {
        RunCurrentPhaseResult result = runCurrentPhase(GCConductor::Mutator, nullptr);
        switch (result) {
        case RunCurrentPhaseResult::Finished:
            return;
        case RunCurrentPhaseResult::Continue:
            break;
        case RunCurrentPhaseResult::NeedCurrentThreadState:
            sanitizeStackForVM(m_vm);
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
            callWithCurrentThreadState(scopedLambda<void(CurrentThreadState&)>(WTFMove(lambda)));
            return;
        }
    }
}

template<typename Func>
void Heap::waitForCollector(const Func& func)
{
    for (;;) {
        bool done;
        {
            LockHolder locker(*m_threadLock);
            done = func(locker);
            if (!done) {
                setMutatorWaiting();
                
                // At this point, the collector knows that we intend to wait, and he will clear the
                // waiting bit and then unparkAll when the GC cycle finishes. Clearing the bit
                // prevents us from parking except if there is also stop-the-world. Unparking after
                // clearing means that if the clearing happens after we park, then we will unpark.
            }
        }
        
        // If we're in a stop-the-world scenario, we need to wait for that even if done is true.
        unsigned oldState = m_worldState.load();
        if (stopIfNecessarySlow(oldState))
            continue;
        
        // FIXME: We wouldn't need this if stopIfNecessarySlow() had a mode where it knew to just
        // do the collection.
        relinquishConn();
        
        if (done) {
            clearMutatorWaiting(); // Clean up just in case.
            return;
        }
        
        // If mutatorWaitingBit is still set then we want to wait.
        ParkingLot::compareAndPark(&m_worldState, oldState | mutatorWaitingBit);
    }
}

void Heap::acquireAccessSlow()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        RELEASE_ASSERT(!(oldState & hasAccessBit));
        
        if (oldState & stoppedBit) {
            if (verboseStop) {
                dataLog("Stopping in acquireAccess!\n");
                WTFReportBacktrace();
            }
            // Wait until we're not stopped anymore.
            ParkingLot::compareAndPark(&m_worldState, oldState);
            continue;
        }
        
        RELEASE_ASSERT(!(oldState & stoppedBit));
        unsigned newState = oldState | hasAccessBit;
        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            handleGCDidJIT();
            handleNeedFinalize();
            m_mutatorDidRun = true;
            stopIfNecessary();
            return;
        }
    }
}

void Heap::releaseAccessSlow()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!(oldState & hasAccessBit)) {
            dataLog("FATAL: Attempting to release access but the mutator does not have access.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (oldState & stoppedBit) {
            dataLog("FATAL: Attempting to release access but the mutator is stopped.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        if (handleNeedFinalize(oldState))
            continue;
        
        unsigned newState = oldState & ~(hasAccessBit | mutatorHasConnBit);
        
        if ((oldState & mutatorHasConnBit)
            && m_nextPhase != m_currentPhase) {
            // This means that the collector thread had given us the conn so that we would do something
            // for it. Stop ourselves as we release access. This ensures that acquireAccess blocks. In
            // the meantime, since we're handing the conn over, the collector will be awoken and it is
            // sure to have work to do.
            newState |= stoppedBit;
        }

        if (m_worldState.compareExchangeWeak(oldState, newState)) {
            if (oldState & mutatorHasConnBit)
                finishRelinquishingConn();
            return;
        }
    }
}

bool Heap::relinquishConn(unsigned oldState)
{
    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    if (!(oldState & mutatorHasConnBit))
        return false; // Done.
    
    if (m_threadShouldStop)
        return false;
    
    if (!m_worldState.compareExchangeWeak(oldState, oldState & ~mutatorHasConnBit))
        return true; // Loop around.
    
    finishRelinquishingConn();
    return true;
}

void Heap::finishRelinquishingConn()
{
    if (false)
        dataLog("Relinquished the conn.\n");
    
    sanitizeStackForVM(m_vm);
    
    auto locker = holdLock(*m_threadLock);
    if (!m_requests.isEmpty())
        m_threadCondition->notifyOne(locker);
    ParkingLot::unparkAll(&m_worldState);
}

void Heap::relinquishConn()
{
    while (relinquishConn(m_worldState.load())) { }
}

bool Heap::handleGCDidJIT(unsigned oldState)
{
    RELEASE_ASSERT(oldState & hasAccessBit);
    if (!(oldState & gcDidJITBit))
        return false;
    if (m_worldState.compareExchangeWeak(oldState, oldState & ~gcDidJITBit)) {
        WTF::crossModifyingCodeFence();
        return true;
    }
    return true;
}

NEVER_INLINE bool Heap::handleNeedFinalize(unsigned oldState)
{
    RELEASE_ASSERT(oldState & hasAccessBit);
    RELEASE_ASSERT(!(oldState & stoppedBit));
    
    if (!(oldState & needFinalizeBit))
        return false;
    if (m_worldState.compareExchangeWeak(oldState, oldState & ~needFinalizeBit)) {
        finalize();
        // Wake up anyone waiting for us to finalize. Note that they may have woken up already, in
        // which case they would be waiting for us to release heap access.
        ParkingLot::unparkAll(&m_worldState);
        return true;
    }
    return true;
}

void Heap::handleGCDidJIT()
{
    while (handleGCDidJIT(m_worldState.load())) { }
}

void Heap::handleNeedFinalize()
{
    while (handleNeedFinalize(m_worldState.load())) { }
}

void Heap::setGCDidJIT()
{
    m_worldState.transaction(
        [&] (unsigned& state) -> bool {
            RELEASE_ASSERT(state & stoppedBit);
            state |= gcDidJITBit;
            return true;
        });
}

void Heap::setNeedFinalize()
{
    m_worldState.exchangeOr(needFinalizeBit);
    ParkingLot::unparkAll(&m_worldState);
    m_stopIfNecessaryTimer->scheduleSoon();
}

void Heap::waitWhileNeedFinalize()
{
    for (;;) {
        unsigned oldState = m_worldState.load();
        if (!(oldState & needFinalizeBit)) {
            // This means that either there was no finalize request or the main thread will finalize
            // with heap access, so a subsequent call to stopTheWorld() will return only when
            // finalize finishes.
            return;
        }
        ParkingLot::compareAndPark(&m_worldState, oldState);
    }
}

void Heap::setMutatorWaiting()
{
    m_worldState.exchangeOr(mutatorWaitingBit);
}

void Heap::clearMutatorWaiting()
{
    m_worldState.exchangeAnd(~mutatorWaitingBit);
}

void Heap::notifyThreadStopping(const AbstractLocker&)
{
    m_threadIsStopping = true;
    clearMutatorWaiting();
    ParkingLot::unparkAll(&m_worldState);
}

void Heap::finalize()
{
    MonotonicTime before;
    if (UNLIKELY(Options::logGC())) {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: finalize ");
    }
    
    {
        SweepingScope sweepingScope(*this);
        deleteUnmarkedCompiledCode();
        deleteSourceProviderCaches();
        sweepInFinalize();
    }
    
    if (HasOwnPropertyCache* cache = vm().hasOwnPropertyCache())
        cache->clear();

    immutableButterflyToStringCache.clear();
    
    for (const HeapFinalizerCallback& callback : m_heapFinalizerCallbacks)
        callback.run(vm());
    
    if (shouldSweepSynchronously())
        sweepSynchronously();

    if (UNLIKELY(Options::logGC())) {
        MonotonicTime after = MonotonicTime::now();
        dataLog((after - before).milliseconds(), "ms]\n");
    }
}

Heap::Ticket Heap::requestCollection(GCRequest request)
{
    stopIfNecessary();
    
    ASSERT(vm().currentThreadIsHoldingAPILock());
    RELEASE_ASSERT(vm().atomStringTable() == Thread::current().atomStringTable());
    
    LockHolder locker(*m_threadLock);
    // We may be able to steal the conn. That only works if the collector is definitely not running
    // right now. This is an optimization that prevents the collector thread from ever starting in most
    // cases.
    ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
    if ((m_lastServedTicket == m_lastGrantedTicket) && !m_collectorThreadIsRunning) {
        if (false)
            dataLog("Taking the conn.\n");
        m_worldState.exchangeOr(mutatorHasConnBit);
    }
    
    m_requests.append(request);
    m_lastGrantedTicket++;
    if (!(m_worldState.load() & mutatorHasConnBit))
        m_threadCondition->notifyOne(locker);
    return m_lastGrantedTicket;
}

void Heap::waitForCollection(Ticket ticket)
{
    waitForCollector(
        [&] (const AbstractLocker&) -> bool {
            return m_lastServedTicket >= ticket;
        });
}

void Heap::sweepInFinalize()
{
    m_objectSpace.sweepPreciseAllocations();
#if ENABLE(WEBASSEMBLY)
    // We hold onto a lot of memory, so it makes a lot of sense to be swept eagerly.
    if (vm().m_webAssemblyMemorySpace)
        vm().m_webAssemblyMemorySpace->sweep();
#endif
}

void Heap::suspendCompilerThreads()
{
#if ENABLE(DFG_JIT)
    // We ensure the worklists so that it's not possible for the mutator to start a new worklist
    // after we have suspended the ones that he had started before. That's not very expensive since
    // the worklists use AutomaticThreads anyway.
    if (!Options::useJIT())
        return;
    for (unsigned i = DFG::numberOfWorklists(); i--;)
        DFG::ensureWorklistForIndex(i).suspendAllThreads();
#endif
}

void Heap::willStartCollection()
{
    dataLogIf(Options::logGC(), "=> ");
    
    if (shouldDoFullCollection()) {
        m_collectionScope = CollectionScope::Full;
        m_shouldDoFullCollection = false;
        dataLogIf(Options::logGC(), "FullCollection, ");
    } else {
        m_collectionScope = CollectionScope::Eden;
        dataLogIf(Options::logGC(), "EdenCollection, ");
    }
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        m_sizeBeforeLastFullCollect = m_sizeAfterLastCollect + m_bytesAllocatedThisCycle;
        m_extraMemorySize = 0;
        m_deprecatedExtraMemorySize = 0;
#if ENABLE(RESOURCE_USAGE)
        m_externalMemorySize = 0;
#endif

        if (m_fullActivityCallback)
            m_fullActivityCallback->willCollect();
    } else {
        ASSERT(m_collectionScope && m_collectionScope.value() == CollectionScope::Eden);
        m_sizeBeforeLastEdenCollect = m_sizeAfterLastCollect + m_bytesAllocatedThisCycle;
    }

    if (m_edenActivityCallback)
        m_edenActivityCallback->willCollect();

    for (auto* observer : m_observers)
        observer->willGarbageCollect();
}

void Heap::prepareForMarking()
{
    m_objectSpace.prepareForMarking();
}

void Heap::reapWeakHandles()
{
    m_objectSpace.reapWeakSets();
}

void Heap::pruneStaleEntriesFromWeakGCMaps()
{
    if (!m_collectionScope || m_collectionScope.value() != CollectionScope::Full)
        return;
    for (WeakGCMapBase* weakGCMap : m_weakGCMaps)
        weakGCMap->pruneStaleEntries();
}

void Heap::sweepArrayBuffers()
{
    m_arrayBuffers.sweep(vm());
}

void Heap::snapshotUnswept()
{
    TimingScope timingScope(*this, "Heap::snapshotUnswept");
    m_objectSpace.snapshotUnswept();
}

void Heap::deleteSourceProviderCaches()
{
    if (m_lastCollectionScope && m_lastCollectionScope.value() == CollectionScope::Full)
        m_vm.clearSourceProviderCaches();
}

void Heap::notifyIncrementalSweeper()
{
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        if (!m_logicallyEmptyWeakBlocks.isEmpty())
            m_indexOfNextLogicallyEmptyWeakBlockToSweep = 0;
    }

    m_sweeper->startSweeping(*this);
}

void Heap::updateAllocationLimits()
{
    constexpr bool verbose = false;
    
    if (verbose) {
        dataLog("\n");
        dataLog("bytesAllocatedThisCycle = ", m_bytesAllocatedThisCycle, "\n");
    }
    
    // Calculate our current heap size threshold for the purpose of figuring out when we should
    // run another collection. This isn't the same as either size() or capacity(), though it should
    // be somewhere between the two. The key is to match the size calculations involved calls to
    // didAllocate(), while never dangerously underestimating capacity(). In extreme cases of
    // fragmentation, we may have size() much smaller than capacity().
    size_t currentHeapSize = 0;

    // For marked space, we use the total number of bytes visited. This matches the logic for
    // BlockDirectory's calls to didAllocate(), which effectively accounts for the total size of
    // objects allocated rather than blocks used. This will underestimate capacity(), and in case
    // of fragmentation, this may be substantial. Fortunately, marked space rarely fragments because
    // cells usually have a narrow range of sizes. So, the underestimation is probably OK.
    currentHeapSize += m_totalBytesVisited;
    if (verbose)
        dataLog("totalBytesVisited = ", m_totalBytesVisited, ", currentHeapSize = ", currentHeapSize, "\n");

    // It's up to the user to ensure that extraMemorySize() ends up corresponding to allocation-time
    // extra memory reporting.
    currentHeapSize += extraMemorySize();
    if (ASSERT_ENABLED) {
        CheckedSize checkedCurrentHeapSize = m_totalBytesVisited;
        checkedCurrentHeapSize += extraMemorySize();
        ASSERT(!checkedCurrentHeapSize.hasOverflowed() && checkedCurrentHeapSize.unsafeGet() == currentHeapSize);
    }

    if (verbose)
        dataLog("extraMemorySize() = ", extraMemorySize(), ", currentHeapSize = ", currentHeapSize, "\n");
    
    if (m_collectionScope && m_collectionScope.value() == CollectionScope::Full) {
        // To avoid pathological GC churn in very small and very large heaps, we set
        // the new allocation limit based on the current size of the heap, with a
        // fixed minimum.
        m_maxHeapSize = std::max(minHeapSize(m_heapType, m_ramSize), proportionalHeapSize(currentHeapSize, m_ramSize));
        if (verbose)
            dataLog("Full: maxHeapSize = ", m_maxHeapSize, "\n");
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        if (verbose)
            dataLog("Full: maxEdenSize = ", m_maxEdenSize, "\n");
        m_sizeAfterLastFullCollect = currentHeapSize;
        if (verbose)
            dataLog("Full: sizeAfterLastFullCollect = ", currentHeapSize, "\n");
        m_bytesAbandonedSinceLastFullCollect = 0;
        if (verbose)
            dataLog("Full: bytesAbandonedSinceLastFullCollect = ", 0, "\n");
    } else {
        ASSERT(currentHeapSize >= m_sizeAfterLastCollect);
        // Theoretically, we shouldn't ever scan more memory than the heap size we planned to have.
        // But we are sloppy, so we have to defend against the overflow.
        m_maxEdenSize = currentHeapSize > m_maxHeapSize ? 0 : m_maxHeapSize - currentHeapSize;
        if (verbose)
            dataLog("Eden: maxEdenSize = ", m_maxEdenSize, "\n");
        m_sizeAfterLastEdenCollect = currentHeapSize;
        if (verbose)
            dataLog("Eden: sizeAfterLastEdenCollect = ", currentHeapSize, "\n");
        double edenToOldGenerationRatio = (double)m_maxEdenSize / (double)m_maxHeapSize;
        double minEdenToOldGenerationRatio = 1.0 / 3.0;
        if (edenToOldGenerationRatio < minEdenToOldGenerationRatio)
            m_shouldDoFullCollection = true;
        // This seems suspect at first, but what it does is ensure that the nursery size is fixed.
        m_maxHeapSize += currentHeapSize - m_sizeAfterLastCollect;
        if (verbose)
            dataLog("Eden: maxHeapSize = ", m_maxHeapSize, "\n");
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        if (verbose)
            dataLog("Eden: maxEdenSize = ", m_maxEdenSize, "\n");
        if (m_fullActivityCallback) {
            ASSERT(currentHeapSize >= m_sizeAfterLastFullCollect);
            m_fullActivityCallback->didAllocate(*this, currentHeapSize - m_sizeAfterLastFullCollect);
        }
    }

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
    // Get critical memory threshold for next cycle.
    overCriticalMemoryThreshold(MemoryThresholdCallType::Direct);
#endif

    m_sizeAfterLastCollect = currentHeapSize;
    if (verbose)
        dataLog("sizeAfterLastCollect = ", m_sizeAfterLastCollect, "\n");
    m_bytesAllocatedThisCycle = 0;

    dataLogIf(Options::logGC(), "=> ", currentHeapSize / 1024, "kb, ");
}

void Heap::didFinishCollection()
{
    m_afterGC = MonotonicTime::now();
    CollectionScope scope = *m_collectionScope;
    if (scope == CollectionScope::Full)
        m_lastFullGCLength = m_afterGC - m_beforeGC;
    else
        m_lastEdenGCLength = m_afterGC - m_beforeGC;

#if ENABLE(RESOURCE_USAGE)
    ASSERT(externalMemorySize() <= extraMemorySize());
#endif

    if (HeapProfiler* heapProfiler = m_vm.heapProfiler()) {
        gatherExtraHeapData(*heapProfiler);
        removeDeadHeapSnapshotNodes(*heapProfiler);
    }

    if (UNLIKELY(m_verifier))
        m_verifier->endGC();

    RELEASE_ASSERT(m_collectionScope);
    m_lastCollectionScope = m_collectionScope;
    m_collectionScope = WTF::nullopt;

    for (auto* observer : m_observers)
        observer->didGarbageCollect(scope);
}

void Heap::resumeCompilerThreads()
{
#if ENABLE(DFG_JIT)
    if (!Options::useJIT())
        return;
    for (unsigned i = DFG::numberOfWorklists(); i--;)
        DFG::existingWorklistForIndex(i).resumeAllThreads();
#endif
}

GCActivityCallback* Heap::fullActivityCallback()
{
    return m_fullActivityCallback.get();
}

GCActivityCallback* Heap::edenActivityCallback()
{
    return m_edenActivityCallback.get();
}

IncrementalSweeper& Heap::sweeper()
{
    return m_sweeper.get();
}

void Heap::setGarbageCollectionTimerEnabled(bool enable)
{
    if (m_fullActivityCallback)
        m_fullActivityCallback->setEnabled(enable);
    if (m_edenActivityCallback)
        m_edenActivityCallback->setEnabled(enable);
}

void Heap::didAllocate(size_t bytes)
{
    if (m_edenActivityCallback)
        m_edenActivityCallback->didAllocate(*this, m_bytesAllocatedThisCycle + m_bytesAbandonedSinceLastFullCollect);
    m_bytesAllocatedThisCycle += bytes;
    performIncrement(bytes);
}

bool Heap::isValidAllocation(size_t)
{
    if (!isValidThreadState(m_vm))
        return false;

    if (isCurrentThreadBusy())
        return false;
    
    return true;
}

void Heap::addFinalizer(JSCell* cell, CFinalizer finalizer)
{
    WeakSet::allocate(cell, &m_cFinalizerOwner, bitwise_cast<void*>(finalizer)); // Balanced by CFinalizerOwner::finalize().
}

void Heap::addFinalizer(JSCell* cell, LambdaFinalizer function)
{
    WeakSet::allocate(cell, &m_lambdaFinalizerOwner, function.leakImpl()); // Balanced by LambdaFinalizerOwner::finalize().
}

void Heap::CFinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    HandleSlot slot = handle.slot();
    CFinalizer finalizer = bitwise_cast<CFinalizer>(context);
    finalizer(slot->asCell());
    WeakSet::deallocate(WeakImpl::asWeakImpl(slot));
}

void Heap::LambdaFinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    LambdaFinalizer::Impl* impl = bitwise_cast<LambdaFinalizer::Impl*>(context);
    LambdaFinalizer finalizer(impl);
    HandleSlot slot = handle.slot();
    finalizer(slot->asCell());
    WeakSet::deallocate(WeakImpl::asWeakImpl(slot));
}

void Heap::collectNowFullIfNotDoneRecently(Synchronousness synchronousness)
{
    if (!m_fullActivityCallback) {
        collectNow(synchronousness, CollectionScope::Full);
        return;
    }

    if (m_fullActivityCallback->didGCRecently()) {
        // A synchronous GC was already requested recently so we merely accelerate next collection.
        reportAbandonedObjectGraph();
        return;
    }

    m_fullActivityCallback->setDidGCRecently();
    collectNow(synchronousness, CollectionScope::Full);
}

bool Heap::useGenerationalGC()
{
    return Options::useGenerationalGC() && !VM::isInMiniMode();
}

bool Heap::shouldSweepSynchronously()
{
    return Options::sweepSynchronously() || VM::isInMiniMode();
}

bool Heap::shouldDoFullCollection()
{
    if (!useGenerationalGC())
        return true;

    if (!m_currentRequest.scope)
        return m_shouldDoFullCollection || overCriticalMemoryThreshold();
    return *m_currentRequest.scope == CollectionScope::Full;
}

void Heap::addLogicallyEmptyWeakBlock(WeakBlock* block)
{
    m_logicallyEmptyWeakBlocks.append(block);
}

void Heap::sweepAllLogicallyEmptyWeakBlocks()
{
    if (m_logicallyEmptyWeakBlocks.isEmpty())
        return;

    m_indexOfNextLogicallyEmptyWeakBlockToSweep = 0;
    while (sweepNextLogicallyEmptyWeakBlock()) { }
}

bool Heap::sweepNextLogicallyEmptyWeakBlock()
{
    if (m_indexOfNextLogicallyEmptyWeakBlockToSweep == WTF::notFound)
        return false;

    WeakBlock* block = m_logicallyEmptyWeakBlocks[m_indexOfNextLogicallyEmptyWeakBlockToSweep];

    block->sweep();
    if (block->isEmpty()) {
        std::swap(m_logicallyEmptyWeakBlocks[m_indexOfNextLogicallyEmptyWeakBlockToSweep], m_logicallyEmptyWeakBlocks.last());
        m_logicallyEmptyWeakBlocks.removeLast();
        WeakBlock::destroy(*this, block);
    } else
        m_indexOfNextLogicallyEmptyWeakBlockToSweep++;

    if (m_indexOfNextLogicallyEmptyWeakBlockToSweep >= m_logicallyEmptyWeakBlocks.size()) {
        m_indexOfNextLogicallyEmptyWeakBlockToSweep = WTF::notFound;
        return false;
    }

    return true;
}

size_t Heap::visitCount()
{
    size_t result = 0;
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            result += visitor.visitCount();
        });
    return result;
}

size_t Heap::bytesVisited()
{
    size_t result = 0;
    forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            result += visitor.bytesVisited();
        });
    return result;
}

void Heap::forEachCodeBlockImpl(const ScopedLambda<void(CodeBlock*)>& func)
{
    // We don't know the full set of CodeBlocks until compilation has terminated.
    completeAllJITPlans();

    return m_codeBlocks->iterate(func);
}

void Heap::forEachCodeBlockIgnoringJITPlansImpl(const AbstractLocker& locker, const ScopedLambda<void(CodeBlock*)>& func)
{
    return m_codeBlocks->iterate(locker, func);
}

void Heap::writeBarrierSlowPath(const JSCell* from)
{
    if (UNLIKELY(mutatorShouldBeFenced())) {
        // In this case, the barrierThreshold is the tautological threshold, so from could still be
        // not black. But we can't know for sure until we fire off a fence.
        WTF::storeLoadFence();
        if (from->cellState() != CellState::PossiblyBlack)
            return;
    }
    
    addToRememberedSet(from);
}

bool Heap::isCurrentThreadBusy()
{
    return Thread::mayBeGCThread() || mutatorState() != MutatorState::Running;
}

void Heap::reportExtraMemoryVisited(size_t size)
{
    size_t* counter = &m_extraMemorySize;
    
    for (;;) {
        size_t oldSize = *counter;
        // FIXME: Change this to use SaturatedArithmetic when available.
        // https://bugs.webkit.org/show_bug.cgi?id=170411
        CheckedSize checkedNewSize = oldSize;
        checkedNewSize += size;
        size_t newSize = UNLIKELY(checkedNewSize.hasOverflowed()) ? std::numeric_limits<size_t>::max() : checkedNewSize.unsafeGet();
        if (WTF::atomicCompareExchangeWeakRelaxed(counter, oldSize, newSize))
            return;
    }
}

#if ENABLE(RESOURCE_USAGE)
void Heap::reportExternalMemoryVisited(size_t size)
{
    size_t* counter = &m_externalMemorySize;

    for (;;) {
        size_t oldSize = *counter;
        if (WTF::atomicCompareExchangeWeakRelaxed(counter, oldSize, oldSize + size))
            return;
    }
}
#endif

void Heap::collectIfNecessaryOrDefer(GCDeferralContext* deferralContext)
{
    ASSERT(deferralContext || isDeferred() || !DisallowGC::isInEffectOnCurrentThread());
    if constexpr (validateDFGDoesGC)
        verifyCanGC();

    if (!m_isSafeToCollect)
        return;

    switch (mutatorState()) {
    case MutatorState::Running:
    case MutatorState::Allocating:
        break;
    case MutatorState::Sweeping:
    case MutatorState::Collecting:
        return;
    }
    if (!Options::useGC())
        return;
    
    if (mayNeedToStop()) {
        if (deferralContext)
            deferralContext->m_shouldGC = true;
        else if (isDeferred())
            m_didDeferGCWork = true;
        else
            stopIfNecessary();
    }
    
    if (UNLIKELY(Options::gcMaxHeapSize())) {
        if (m_bytesAllocatedThisCycle <= Options::gcMaxHeapSize())
            return;
    } else {
        size_t bytesAllowedThisCycle = m_maxEdenSize;

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
        if (overCriticalMemoryThreshold())
            bytesAllowedThisCycle = std::min(m_maxEdenSizeWhenCritical, bytesAllowedThisCycle);
#endif

        if (m_bytesAllocatedThisCycle <= bytesAllowedThisCycle)
            return;
    }

    if (deferralContext)
        deferralContext->m_shouldGC = true;
    else if (isDeferred())
        m_didDeferGCWork = true;
    else {
        collectAsync();
        stopIfNecessary(); // This will immediately start the collection if we have the conn.
    }
}

void Heap::decrementDeferralDepthAndGCIfNeededSlow()
{
    // Can't do anything if we're still deferred.
    if (m_deferralDepth)
        return;
    
    ASSERT(!isDeferred());
    
    m_didDeferGCWork = false;
    // FIXME: Bring back something like the DeferGCProbability mode.
    // https://bugs.webkit.org/show_bug.cgi?id=166627
    collectIfNecessaryOrDefer();
}

void Heap::registerWeakGCMap(WeakGCMapBase* weakGCMap)
{
    m_weakGCMaps.add(weakGCMap);
}

void Heap::unregisterWeakGCMap(WeakGCMapBase* weakGCMap)
{
    m_weakGCMaps.remove(weakGCMap);
}

void Heap::didAllocateBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated += capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

void Heap::didFreeBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE)
    m_blockBytesAllocated -= capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

void Heap::addCoreConstraints()
{
    m_constraintSet->add(
        "Cs", "Conservative Scan",
        [this, lastVersion = static_cast<uint64_t>(0)] (SlotVisitor& slotVisitor) mutable {
            bool shouldNotProduceWork = lastVersion == m_phaseVersion;
            if (shouldNotProduceWork)
                return;
            
            TimingScope preConvergenceTimingScope(*this, "Constraint: conservative scan");
            m_objectSpace.prepareForConservativeScan();
            m_jitStubRoutines->prepareForConservativeScan();

            {
                ConservativeRoots conservativeRoots(*this);
                SuperSamplerScope superSamplerScope(false);

                gatherStackRoots(conservativeRoots);
                gatherJSStackRoots(conservativeRoots);
                gatherScratchBufferRoots(conservativeRoots);

                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::ConservativeScan);
                slotVisitor.append(conservativeRoots);
            }
            if (Options::useJIT()) {
                // JITStubRoutines must be visited after scanning ConservativeRoots since JITStubRoutines depend on the hook executed during gathering ConservativeRoots.
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::JITStubRoutines);
                m_jitStubRoutines->traceMarkedStubRoutines(slotVisitor);
            }
            
            lastVersion = m_phaseVersion;
        },
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Msr", "Misc Small Roots",
        [this] (SlotVisitor& slotVisitor) {

#if JSC_OBJC_API_ENABLED
            scanExternalRememberedSet(m_vm, slotVisitor);
#endif
            if (m_vm.smallStrings.needsToBeVisited(*m_collectionScope)) {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::StrongReferences);
                m_vm.smallStrings.visitStrongReferences(slotVisitor);
            }
            
            {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::ProtectedValues);
                for (auto& pair : m_protectedValues)
                    slotVisitor.appendUnbarriered(pair.key);
            }
            
            if (m_markListSet && m_markListSet->size()) {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::ConservativeScan);
                MarkedArgumentBuffer::markLists(slotVisitor, *m_markListSet);
            }

            m_markedJSValueRefArrays.forEach([&] (MarkedJSValueRefArray* array) {
                array->visitAggregate(slotVisitor);
            });

            {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::VMExceptions);
                slotVisitor.appendUnbarriered(m_vm.exception());
                slotVisitor.appendUnbarriered(m_vm.lastException());
            }
        },
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Sh", "Strong Handles",
        [this] (SlotVisitor& slotVisitor) {
            SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::StrongHandles);
            m_handleSet.visitStrongHandles(slotVisitor);
        },
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "D", "Debugger",
        [this] (SlotVisitor& slotVisitor) {
            SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::Debugger);

#if ENABLE(SAMPLING_PROFILER)
            if (SamplingProfiler* samplingProfiler = m_vm.samplingProfiler()) {
                auto locker = holdLock(samplingProfiler->getLock());
                samplingProfiler->processUnverifiedStackTraces(locker);
                samplingProfiler->visit(slotVisitor);
                if (Options::logGC() == GCLogging::Verbose)
                    dataLog("Sampling Profiler data:\n", slotVisitor);
            }
#endif // ENABLE(SAMPLING_PROFILER)

            if (m_vm.typeProfiler())
                m_vm.typeProfilerLog()->visit(slotVisitor);
            
            if (auto* shadowChicken = m_vm.shadowChicken())
                shadowChicken->visitChildren(slotVisitor);
        },
        ConstraintVolatility::GreyedByExecution);
    
    m_constraintSet->add(
        "Ws", "Weak Sets",
        [this] (SlotVisitor& slotVisitor) {
            SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::WeakSets);
            m_objectSpace.visitWeakSets(slotVisitor);
        },
        ConstraintVolatility::GreyedByMarking);
    
    m_constraintSet->add(
        "O", "Output",
        [] (SlotVisitor& slotVisitor) {
            VM& vm = slotVisitor.vm();

            auto callOutputConstraint = [] (SlotVisitor& slotVisitor, HeapCell* heapCell, HeapCell::Kind) {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::Output);
                VM& vm = slotVisitor.vm();
                JSCell* cell = static_cast<JSCell*>(heapCell);
                cell->methodTable(vm)->visitOutputConstraints(cell, slotVisitor);
            };
            
            auto add = [&] (auto& set) {
                slotVisitor.addParallelConstraintTask(set.forEachMarkedCellInParallel(callOutputConstraint));
            };
            
            add(vm.executableToCodeBlockEdgesWithConstraints);
            if (vm.m_weakMapSpace)
                add(*vm.m_weakMapSpace);
        },
        ConstraintVolatility::GreyedByMarking,
        ConstraintParallelism::Parallel);
    
#if ENABLE(DFG_JIT)
    if (Options::useJIT()) {
        m_constraintSet->add(
            "Dw", "DFG Worklists",
            [this] (SlotVisitor& slotVisitor) {
                SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::DFGWorkLists);

                for (unsigned i = DFG::numberOfWorklists(); i--;)
                    DFG::existingWorklistForIndex(i).visitWeakReferences(slotVisitor);
                
                // FIXME: This is almost certainly unnecessary.
                // https://bugs.webkit.org/show_bug.cgi?id=166829
                DFG::iterateCodeBlocksForGC(
                    m_vm,
                    [&] (CodeBlock* codeBlock) {
                        slotVisitor.appendUnbarriered(codeBlock);
                    });
                
                if (Options::logGC() == GCLogging::Verbose)
                    dataLog("DFG Worklists:\n", slotVisitor);
            },
            ConstraintVolatility::GreyedByMarking);
    }
#endif
    
    m_constraintSet->add(
        "Cb", "CodeBlocks",
        [this] (SlotVisitor& slotVisitor) {
            SetRootMarkReasonScope rootScope(slotVisitor, SlotVisitor::RootMarkReason::CodeBlocks);
            iterateExecutingAndCompilingCodeBlocksWithoutHoldingLocks(
                [&] (CodeBlock* codeBlock) {
                    // Visit the CodeBlock as a constraint only if it's black.
                    if (isMarked(codeBlock)
                        && codeBlock->cellState() == CellState::PossiblyBlack)
                        slotVisitor.visitAsConstraint(codeBlock);
                });
        },
        ConstraintVolatility::SeldomGreyed);
    
    m_constraintSet->add(makeUnique<MarkStackMergingConstraint>(*this));
}

void Heap::addMarkingConstraint(std::unique_ptr<MarkingConstraint> constraint)
{
    PreventCollectionScope preventCollectionScope(*this);
    m_constraintSet->add(WTFMove(constraint));
}

void Heap::notifyIsSafeToCollect()
{
    MonotonicTime before;
    if (UNLIKELY(Options::logGC())) {
        before = MonotonicTime::now();
        dataLog("[GC<", RawPointer(this), ">: starting ");
    }
    
    addCoreConstraints();
    
    m_isSafeToCollect = true;
    
    if (Options::collectContinuously()) {
        m_collectContinuouslyThread = Thread::create(
            "JSC DEBUG Continuous GC",
            [this] () {
                MonotonicTime initialTime = MonotonicTime::now();
                Seconds period = Seconds::fromMilliseconds(Options::collectContinuouslyPeriodMS());
                while (true) {
                    LockHolder locker(m_collectContinuouslyLock);
                    {
                        LockHolder locker(*m_threadLock);
                        if (m_requests.isEmpty()) {
                            m_requests.append(WTF::nullopt);
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
    
    dataLogIf(Options::logGC(), (MonotonicTime::now() - before).milliseconds(), "ms]\n");
}

void Heap::preventCollection()
{
    if (!m_isSafeToCollect)
        return;
    
    // This prevents the collectContinuously thread from starting a collection.
    m_collectContinuouslyLock.lock();
    
    // Wait for all collections to finish.
    waitForCollector(
        [&] (const AbstractLocker&) -> bool {
            ASSERT(m_lastServedTicket <= m_lastGrantedTicket);
            return m_lastServedTicket == m_lastGrantedTicket;
        });
    
    // Now a collection can only start if this thread starts it.
    RELEASE_ASSERT(!m_collectionScope);
}

void Heap::allowCollection()
{
    if (!m_isSafeToCollect)
        return;
    
    m_collectContinuouslyLock.unlock();
}

void Heap::setMutatorShouldBeFenced(bool value)
{
    m_mutatorShouldBeFenced = value;
    m_barrierThreshold = value ? tautologicalThreshold : blackThreshold;
}

void Heap::performIncrement(size_t bytes)
{
    if (!m_objectSpace.isMarking())
        return;

    if (isDeferred())
        return;

    m_incrementBalance += bytes * Options::gcIncrementScale();

    // Save ourselves from crazy. Since this is an optimization, it's OK to go back to any consistent
    // state when the double goes wild.
    if (std::isnan(m_incrementBalance) || std::isinf(m_incrementBalance))
        m_incrementBalance = 0;
    
    if (m_incrementBalance < static_cast<double>(Options::gcIncrementBytes()))
        return;

    double targetBytes = m_incrementBalance;
    if (targetBytes <= 0)
        return;
    targetBytes = std::min(targetBytes, Options::gcIncrementMaxBytes());

    SlotVisitor& slotVisitor = *m_mutatorSlotVisitor;
    ParallelModeEnabler parallelModeEnabler(slotVisitor);
    size_t bytesVisited = slotVisitor.performIncrementOfDraining(static_cast<size_t>(targetBytes));
    // incrementBalance may go negative here because it'll remember how many bytes we overshot.
    m_incrementBalance -= bytesVisited;
}

void Heap::addHeapFinalizerCallback(const HeapFinalizerCallback& callback)
{
    m_heapFinalizerCallbacks.append(callback);
}

void Heap::removeHeapFinalizerCallback(const HeapFinalizerCallback& callback)
{
    m_heapFinalizerCallbacks.removeFirst(callback);
}

void Heap::setBonusVisitorTask(RefPtr<SharedTask<void(SlotVisitor&)>> task)
{
    auto locker = holdLock(m_markingMutex);
    m_bonusVisitorTask = task;
    m_markingConditionVariable.notifyAll();
}


void Heap::addMarkedJSValueRefArray(MarkedJSValueRefArray* array)
{
    m_markedJSValueRefArrays.append(array);
}

void Heap::runTaskInParallel(RefPtr<SharedTask<void(SlotVisitor&)>> task)
{
    unsigned initialRefCount = task->refCount();
    setBonusVisitorTask(task);
    task->run(*m_collectorSlotVisitor);
    setBonusVisitorTask(nullptr);
    // The constraint solver expects return of this function to imply termination of the task in all
    // threads. This ensures that property.
    {
        auto locker = holdLock(m_markingMutex);
        while (task->refCount() > initialRefCount)
            m_markingConditionVariable.wait(m_markingMutex);
    }
}

} // namespace JSC
