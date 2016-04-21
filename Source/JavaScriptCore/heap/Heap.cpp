/*
 *  Copyright (C) 2003-2009, 2011, 2013-2016 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "ConservativeRoots.h"
#include "CopiedSpace.h"
#include "CopiedSpaceInlines.h"
#include "CopyVisitorInlines.h"
#include "DFGWorklist.h"
#include "EdenGCActivityCallback.h"
#include "FullGCActivityCallback.h"
#include "GCActivityCallback.h"
#include "GCIncomingRefCountedSetInlines.h"
#include "HeapHelperPool.h"
#include "HeapIterationScope.h"
#include "HeapProfiler.h"
#include "HeapRootVisitor.h"
#include "HeapSnapshot.h"
#include "HeapStatistics.h"
#include "HeapVerifier.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSVirtualMachineInternal.h"
#include "SamplingProfiler.h"
#include "ShadowChicken.h"
#include "Tracing.h"
#include "TypeProfilerLog.h"
#include "UnlinkedCodeBlock.h"
#include "VM.h"
#include "WeakSetInlines.h"
#include <algorithm>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/ParallelVectorIterator.h>
#include <wtf/ProcessID.h>
#include <wtf/RAMSize.h>

#if __has_include(<objc/objc-internal.h>)
#include <objc/objc-internal.h>
#else
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *context);
#endif

using namespace std;

namespace JSC {

namespace {

static const size_t largeHeapSize = 32 * MB; // About 1.5X the average webpage.
static const size_t smallHeapSize = 1 * MB; // Matches the FastMalloc per-thread cache.

#define ENABLE_GC_LOGGING 0

#if ENABLE(GC_LOGGING)
#if COMPILER(CLANG)
#define DEFINE_GC_LOGGING_GLOBAL(type, name, arguments) \
_Pragma("clang diagnostic push") \
_Pragma("clang diagnostic ignored \"-Wglobal-constructors\"") \
_Pragma("clang diagnostic ignored \"-Wexit-time-destructors\"") \
static type name arguments; \
_Pragma("clang diagnostic pop")
#else
#define DEFINE_GC_LOGGING_GLOBAL(type, name, arguments) \
static type name arguments;
#endif // COMPILER(CLANG)

struct GCTimer {
    GCTimer(const char* name)
        : name(name)
    {
    }
    ~GCTimer()
    {
        logData(allCollectionData, "(All)");
        logData(edenCollectionData, "(Eden)");
        logData(fullCollectionData, "(Full)");
    }

    struct TimeRecord {
        TimeRecord()
            : time(0)
            , min(std::numeric_limits<double>::infinity())
            , max(0)
            , count(0)
        {
        }

        double time;
        double min;
        double max;
        size_t count;
    };

    void logData(const TimeRecord& data, const char* extra)
    {
        dataLogF("[%d] %s (Parent: %s) %s: %.2lfms (avg. %.2lf, min. %.2lf, max. %.2lf, count %lu)\n", 
            getCurrentProcessID(),
            name,
            parent ? parent->name : "nullptr",
            extra, 
            data.time * 1000, 
            data.time * 1000 / data.count, 
            data.min * 1000, 
            data.max * 1000,
            data.count);
    }

    void updateData(TimeRecord& data, double duration)
    {
        if (duration < data.min)
            data.min = duration;
        if (duration > data.max)
            data.max = duration;
        data.count++;
        data.time += duration;
    }

    void didFinishPhase(HeapOperation collectionType, double duration)
    {
        TimeRecord& data = collectionType == EdenCollection ? edenCollectionData : fullCollectionData;
        updateData(data, duration);
        updateData(allCollectionData, duration);
    }

    static GCTimer* s_currentGlobalTimer;

    TimeRecord allCollectionData;
    TimeRecord fullCollectionData;
    TimeRecord edenCollectionData;
    const char* name;
    GCTimer* parent { nullptr };
};

GCTimer* GCTimer::s_currentGlobalTimer = nullptr;

struct GCTimerScope {
    GCTimerScope(GCTimer& timer, HeapOperation collectionType)
        : timer(timer)
        , start(WTF::monotonicallyIncreasingTime())
        , collectionType(collectionType)
    {
        timer.parent = GCTimer::s_currentGlobalTimer;
        GCTimer::s_currentGlobalTimer = &timer;
    }
    ~GCTimerScope()
    {
        double delta = WTF::monotonicallyIncreasingTime() - start;
        timer.didFinishPhase(collectionType, delta);
        GCTimer::s_currentGlobalTimer = timer.parent;
    }
    GCTimer& timer;
    double start;
    HeapOperation collectionType;
};

struct GCCounter {
    GCCounter(const char* name)
        : name(name)
        , count(0)
        , total(0)
        , min(10000000)
        , max(0)
    {
    }
    
    void add(size_t amount)
    {
        count++;
        total += amount;
        if (amount < min)
            min = amount;
        if (amount > max)
            max = amount;
    }
    ~GCCounter()
    {
        dataLogF("[%d] %s: %zu values (avg. %zu, min. %zu, max. %zu)\n", getCurrentProcessID(), name, total, total / count, min, max);
    }
    const char* name;
    size_t count;
    size_t total;
    size_t min;
    size_t max;
};

#define GCPHASE(name) DEFINE_GC_LOGGING_GLOBAL(GCTimer, name##Timer, (#name)); GCTimerScope name##TimerScope(name##Timer, m_operationInProgress)
#define GCCOUNTER(name, value) do { DEFINE_GC_LOGGING_GLOBAL(GCCounter, name##Counter, (#name)); name##Counter.add(value); } while (false)
    
#else

#define GCPHASE(name) do { } while (false)
#define GCCOUNTER(name, value) do { } while (false)
#endif

static inline size_t minHeapSize(HeapType heapType, size_t ramSize)
{
    if (heapType == LargeHeap)
        return min(largeHeapSize, ramSize / 4);
    return smallHeapSize;
}

static inline size_t proportionalHeapSize(size_t heapSize, size_t ramSize)
{
    // Try to stay under 1/2 RAM size to leave room for the DOM, rendering, networking, etc.
    if (heapSize < ramSize / 4)
        return 2 * heapSize;
    if (heapSize < ramSize / 2)
        return 1.5 * heapSize;
    return 1.25 * heapSize;
}

static inline bool isValidSharedInstanceThreadState(VM* vm)
{
    return vm->currentThreadIsHoldingAPILock();
}

static inline bool isValidThreadState(VM* vm)
{
    if (vm->atomicStringTable() != wtfThreadData().atomicStringTable())
        return false;

    if (vm->isSharedInstance() && !isValidSharedInstanceThreadState(vm))
        return false;

    return true;
}

struct MarkObject : public MarkedBlock::VoidFunctor {
    inline void visit(JSCell* cell)
    {
        if (cell->isZapped())
            return;
        Heap::heap(cell)->setMarked(cell);
    }
    IterationStatus operator()(JSCell* cell)
    {
        visit(cell);
        return IterationStatus::Continue;
    }
};

struct Count : public MarkedBlock::CountFunctor {
    void operator()(JSCell*) { count(1); }
};

struct CountIfGlobalObject : MarkedBlock::CountFunctor {
    inline void visit(JSCell* cell)
    {
        if (!cell->isObject())
            return;
        if (!asObject(cell)->isGlobalObject())
            return;
        count(1);
    }
    IterationStatus operator()(JSCell* cell)
    {
        visit(cell);
        return IterationStatus::Continue;
    }
};

class RecordType {
public:
    typedef std::unique_ptr<TypeCountSet> ReturnType;

    RecordType();
    IterationStatus operator()(JSCell*);
    ReturnType returnValue();

private:
    const char* typeName(JSCell*);
    std::unique_ptr<TypeCountSet> m_typeCountSet;
};

inline RecordType::RecordType()
    : m_typeCountSet(std::make_unique<TypeCountSet>())
{
}

inline const char* RecordType::typeName(JSCell* cell)
{
    const ClassInfo* info = cell->classInfo();
    if (!info || !info->className)
        return "[unknown]";
    return info->className;
}

inline IterationStatus RecordType::operator()(JSCell* cell)
{
    m_typeCountSet->add(typeName(cell));
    return IterationStatus::Continue;
}

inline std::unique_ptr<TypeCountSet> RecordType::returnValue()
{
    return WTFMove(m_typeCountSet);
}

} // anonymous namespace

Heap::Heap(VM* vm, HeapType heapType)
    : m_heapType(heapType)
    , m_ramSize(Options::forceRAMSize() ? Options::forceRAMSize() : ramSize())
    , m_minBytesPerCycle(minHeapSize(m_heapType, m_ramSize))
    , m_sizeAfterLastCollect(0)
    , m_sizeAfterLastFullCollect(0)
    , m_sizeBeforeLastFullCollect(0)
    , m_sizeAfterLastEdenCollect(0)
    , m_sizeBeforeLastEdenCollect(0)
    , m_bytesAllocatedThisCycle(0)
    , m_bytesAbandonedSinceLastFullCollect(0)
    , m_maxEdenSize(m_minBytesPerCycle)
    , m_maxHeapSize(m_minBytesPerCycle)
    , m_shouldDoFullCollection(false)
    , m_totalBytesVisited(0)
    , m_totalBytesCopied(0)
    , m_operationInProgress(NoOperation)
    , m_objectSpace(this)
    , m_storageSpace(this)
    , m_extraMemorySize(0)
    , m_deprecatedExtraMemorySize(0)
    , m_machineThreads(this)
    , m_slotVisitor(*this)
    , m_handleSet(vm)
    , m_isSafeToCollect(false)
    , m_writeBarrierBuffer(256)
    , m_vm(vm)
    // We seed with 10ms so that GCActivityCallback::didAllocate doesn't continuously 
    // schedule the timer if we've never done a collection.
    , m_lastFullGCLength(0.01)
    , m_lastEdenGCLength(0.01)
    , m_fullActivityCallback(GCActivityCallback::createFullTimer(this))
    , m_edenActivityCallback(GCActivityCallback::createEdenTimer(this))
#if USE(CF)
    , m_sweeper(std::make_unique<IncrementalSweeper>(this, CFRunLoopGetCurrent()))
#else
    , m_sweeper(std::make_unique<IncrementalSweeper>(this))
#endif
    , m_deferralDepth(0)
#if USE(FOUNDATION)
    , m_delayedReleaseRecursionCount(0)
#endif
    , m_helperClient(&heapHelperPool())
{
    m_storageSpace.init();
    if (Options::verifyHeap())
        m_verifier = std::make_unique<HeapVerifier>(this, Options::numberOfGCCyclesToRecordForVerification());
}

Heap::~Heap()
{
    for (WeakBlock* block : m_logicallyEmptyWeakBlocks)
        WeakBlock::destroy(*this, block);
}

bool Heap::isPagedOut(double deadline)
{
    return m_objectSpace.isPagedOut(deadline) || m_storageSpace.isPagedOut(deadline);
}

// The VM is being destroyed and the collector will never run again.
// Run all pending finalizers now because we won't get another chance.
void Heap::lastChanceToFinalize()
{
    RELEASE_ASSERT(!m_vm->entryScope);
    RELEASE_ASSERT(m_operationInProgress == NoOperation);

    m_arrayBuffers.lastChanceToFinalize();
    m_codeBlocks.lastChanceToFinalize();
    m_objectSpace.lastChanceToFinalize();
    releaseDelayedReleasedObjects();

    sweepAllLogicallyEmptyWeakBlocks();
}

void Heap::releaseDelayedReleasedObjects()
{
#if USE(FOUNDATION)
    // We need to guard against the case that releasing an object can create more objects due to the
    // release calling into JS. When those JS call(s) exit and all locks are being dropped we end up
    // back here and could try to recursively release objects. We guard that with a recursive entry
    // count. Only the initial call will release objects, recursive calls simple return and let the
    // the initial call to the function take care of any objects created during release time.
    // This also means that we need to loop until there are no objects in m_delayedReleaseObjects
    // and use a temp Vector for the actual releasing.
    if (!m_delayedReleaseRecursionCount++) {
        while (!m_delayedReleaseObjects.isEmpty()) {
            ASSERT(m_vm->currentThreadIsHoldingAPILock());

            Vector<RetainPtr<CFTypeRef>> objectsToRelease = WTFMove(m_delayedReleaseObjects);

            {
                // We need to drop locks before calling out to arbitrary code.
                JSLock::DropAllLocks dropAllLocks(m_vm);

                void* context = objc_autoreleasePoolPush();
                objectsToRelease.clear();
                objc_autoreleasePoolPop(context);
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
    m_deprecatedExtraMemorySize += size;
    reportExtraMemoryAllocatedSlowCase(size);
}

void Heap::reportAbandonedObjectGraph()
{
    // Our clients don't know exactly how much memory they
    // are abandoning so we just guess for them.
    double abandonedBytes = 0.1 * m_sizeAfterLastCollect;

    // We want to accelerate the next collection. Because memory has just 
    // been abandoned, the next collection has the potential to 
    // be more profitable. Since allocation is the trigger for collection, 
    // we hasten the next collection by pretending that we've allocated more memory. 
    didAbandon(abandonedBytes);
}

void Heap::didAbandon(size_t bytes)
{
    if (m_fullActivityCallback) {
        m_fullActivityCallback->didAllocate(
            m_sizeAfterLastCollect - m_sizeAfterLastFullCollect + m_bytesAllocatedThisCycle + m_bytesAbandonedSinceLastFullCollect);
    }
    m_bytesAbandonedSinceLastFullCollect += bytes;
}

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(m_vm->currentThreadIsHoldingAPILock());

    if (!k.isCell())
        return;

    m_protectedValues.add(k.asCell());
}

bool Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(m_vm->currentThreadIsHoldingAPILock());

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

void Heap::harvestWeakReferences()
{
    m_slotVisitor.harvestWeakReferences();
}

void Heap::finalizeUnconditionalFinalizers()
{
    GCPHASE(FinalizeUnconditionalFinalizers);
    m_slotVisitor.finalizeUnconditionalFinalizers();
}

inline JSStack& Heap::stack()
{
    return m_vm->interpreter->stack();
}

void Heap::willStartIterating()
{
    m_objectSpace.willStartIterating();
}

void Heap::didFinishIterating()
{
    m_objectSpace.didFinishIterating();
}

void Heap::completeAllDFGPlans()
{
#if ENABLE(DFG_JIT)
    DFG::completeAllPlansForVM(*m_vm);
#endif
}

void Heap::markRoots(double gcStartTime, void* stackOrigin, void* stackTop, MachineThreads::RegisterState& calleeSavedRegisters)
{
    GCPHASE(MarkRoots);
    ASSERT(isValidThreadState(m_vm));

    // We gather conservative roots before clearing mark bits because conservative
    // gathering uses the mark bits to determine whether a reference is valid.
    ConservativeRoots conservativeRoots(&m_objectSpace.blocks(), &m_storageSpace);
    gatherStackRoots(conservativeRoots, stackOrigin, stackTop, calleeSavedRegisters);
    gatherJSStackRoots(conservativeRoots);
    gatherScratchBufferRoots(conservativeRoots);

#if ENABLE(DFG_JIT)
    DFG::rememberCodeBlocks(*m_vm);
#endif

#if ENABLE(SAMPLING_PROFILER)
    if (SamplingProfiler* samplingProfiler = m_vm->samplingProfiler()) {
        // Note that we need to own the lock from now until we're done
        // marking the SamplingProfiler's data because once we verify the
        // SamplingProfiler's stack traces, we don't want it to accumulate
        // more stack traces before we get the chance to mark it.
        // This lock is released inside visitSamplingProfiler().
        samplingProfiler->getLock().lock();
        samplingProfiler->processUnverifiedStackTraces();
    }
#endif // ENABLE(SAMPLING_PROFILER)

    if (m_operationInProgress == FullCollection) {
        m_opaqueRoots.clear();
        m_slotVisitor.clearMarkStack();
    }

    clearLivenessData();

    m_parallelMarkersShouldExit = false;

    m_helperClient.setFunction(
        [this] () {
            SlotVisitor* slotVisitor;
            {
                LockHolder locker(m_parallelSlotVisitorLock);
                if (m_availableParallelSlotVisitors.isEmpty()) {
                    std::unique_ptr<SlotVisitor> newVisitor =
                        std::make_unique<SlotVisitor>(*this);
                    slotVisitor = newVisitor.get();
                    m_parallelSlotVisitors.append(WTFMove(newVisitor));
                } else
                    slotVisitor = m_availableParallelSlotVisitors.takeLast();
            }

            WTF::registerGCThread();

            {
                ParallelModeEnabler parallelModeEnabler(*slotVisitor);
                slotVisitor->didStartMarking();
                slotVisitor->drainFromShared(SlotVisitor::SlaveDrain);
            }

            {
                LockHolder locker(m_parallelSlotVisitorLock);
                m_availableParallelSlotVisitors.append(slotVisitor);
            }
        });

    m_slotVisitor.didStartMarking();
    
    HeapRootVisitor heapRootVisitor(m_slotVisitor);

    {
        ParallelModeEnabler enabler(m_slotVisitor);

        m_slotVisitor.donateAndDrain();
        visitExternalRememberedSet();
        visitSmallStrings();
        visitConservativeRoots(conservativeRoots);
        visitProtectedObjects(heapRootVisitor);
        visitArgumentBuffers(heapRootVisitor);
        visitException(heapRootVisitor);
        visitStrongHandles(heapRootVisitor);
        visitHandleStack(heapRootVisitor);
        visitSamplingProfiler();
        visitShadowChicken();
        traceCodeBlocksAndJITStubRoutines();
        converge();
    }

    // Weak references must be marked last because their liveness depends on
    // the liveness of the rest of the object graph.
    visitWeakHandles(heapRootVisitor);

    {
        std::lock_guard<Lock> lock(m_markingMutex);
        m_parallelMarkersShouldExit = true;
        m_markingConditionVariable.notifyAll();
    }
    m_helperClient.finish();
    updateObjectCounts(gcStartTime);
    resetVisitors();
}

void Heap::copyBackingStores()
{
    GCPHASE(CopyBackingStores);
    if (m_operationInProgress == EdenCollection)
        m_storageSpace.startedCopying<EdenCollection>();
    else {
        ASSERT(m_operationInProgress == FullCollection);
        m_storageSpace.startedCopying<FullCollection>();
    }

    if (m_storageSpace.shouldDoCopyPhase()) {
        if (m_operationInProgress == EdenCollection) {
            // Reset the vector to be empty, but don't throw away the backing store.
            m_blocksToCopy.shrink(0);
            for (CopiedBlock* block = m_storageSpace.m_newGen.fromSpace->head(); block; block = block->next())
                m_blocksToCopy.append(block);
        } else {
            ASSERT(m_operationInProgress == FullCollection);
            WTF::copyToVector(m_storageSpace.m_blockSet, m_blocksToCopy);
        }

        ParallelVectorIterator<Vector<CopiedBlock*>> iterator(
            m_blocksToCopy, s_blockFragmentLength);

        // Note that it's safe to use the [&] capture list here, even though we're creating a task
        // that other threads run. That's because after runFunctionInParallel() returns, the task
        // we have created is not going to be running anymore. Hence, everything on the stack here
        // outlives the task.
        m_helperClient.runFunctionInParallel(
            [&] () {
                CopyVisitor copyVisitor(*this);
                
                iterator.iterate(
                    [&] (CopiedBlock* block) {
                        if (!block->hasWorkList())
                            return;
                        
                        CopyWorkList& workList = block->workList();
                        for (CopyWorklistItem item : workList) {
                            if (item.token() == ButterflyCopyToken) {
                                JSObject::copyBackingStore(
                                    item.cell(), copyVisitor, ButterflyCopyToken);
                                continue;
                            }
                            
                            item.cell()->methodTable()->copyBackingStore(
                                item.cell(), copyVisitor, item.token());
                        }
                        
                        ASSERT(!block->liveBytes());
                        m_storageSpace.recycleEvacuatedBlock(block, m_operationInProgress);
                    });
            });
    }
    
    m_storageSpace.doneCopying();
}

void Heap::gatherStackRoots(ConservativeRoots& roots, void* stackOrigin, void* stackTop, MachineThreads::RegisterState& calleeSavedRegisters)
{
    GCPHASE(GatherStackRoots);
    m_jitStubRoutines.clearMarks();
    m_machineThreads.gatherConservativeRoots(roots, m_jitStubRoutines, m_codeBlocks, stackOrigin, stackTop, calleeSavedRegisters);
}

void Heap::gatherJSStackRoots(ConservativeRoots& roots)
{
#if !ENABLE(JIT)
    GCPHASE(GatherJSStackRoots);
    stack().gatherConservativeRoots(roots, m_jitStubRoutines, m_codeBlocks);
#else
    UNUSED_PARAM(roots);
#endif
}

void Heap::gatherScratchBufferRoots(ConservativeRoots& roots)
{
#if ENABLE(DFG_JIT)
    GCPHASE(GatherScratchBufferRoots);
    m_vm->gatherConservativeRoots(roots);
#else
    UNUSED_PARAM(roots);
#endif
}

void Heap::clearLivenessData()
{
    GCPHASE(ClearLivenessData);
    if (m_operationInProgress == FullCollection)
        m_codeBlocks.clearMarksForFullCollection();

    m_objectSpace.clearNewlyAllocated();
    m_objectSpace.clearMarks();
}

void Heap::visitExternalRememberedSet()
{
#if JSC_OBJC_API_ENABLED
    scanExternalRememberedSet(*m_vm, m_slotVisitor);
#endif
}

void Heap::visitSmallStrings()
{
    GCPHASE(VisitSmallStrings);
    if (!m_vm->smallStrings.needsToBeVisited(m_operationInProgress))
        return;

    m_vm->smallStrings.visitStrongReferences(m_slotVisitor);
    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Small strings:\n", m_slotVisitor);
    m_slotVisitor.donateAndDrain();
}

void Heap::visitConservativeRoots(ConservativeRoots& roots)
{
    GCPHASE(VisitConservativeRoots);
    m_slotVisitor.append(roots);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Conservative Roots:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitCompilerWorklistWeakReferences()
{
#if ENABLE(DFG_JIT)
    for (auto worklist : m_suspendedCompilerWorklists)
        worklist->visitWeakReferences(m_slotVisitor);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("DFG Worklists:\n", m_slotVisitor);
#endif
}

void Heap::removeDeadCompilerWorklistEntries()
{
#if ENABLE(DFG_JIT)
    GCPHASE(FinalizeDFGWorklists);
    for (auto worklist : m_suspendedCompilerWorklists)
        worklist->removeDeadPlans(*m_vm);
#endif
}

bool Heap::isHeapSnapshotting() const
{
    HeapProfiler* heapProfiler = m_vm->heapProfiler();
    if (UNLIKELY(heapProfiler))
        return heapProfiler->activeSnapshotBuilder();
    return false;
}

struct GatherHeapSnapshotData : MarkedBlock::CountFunctor {
    GatherHeapSnapshotData(HeapSnapshotBuilder& builder)
        : m_builder(builder)
    {
    }

    IterationStatus operator()(JSCell* cell)
    {
        cell->methodTable()->heapSnapshot(cell, m_builder);
        return IterationStatus::Continue;
    }

    HeapSnapshotBuilder& m_builder;
};

void Heap::gatherExtraHeapSnapshotData(HeapProfiler& heapProfiler)
{
    GCPHASE(GatherExtraHeapSnapshotData);
    if (HeapSnapshotBuilder* builder = heapProfiler.activeSnapshotBuilder()) {
        HeapIterationScope heapIterationScope(*this);
        GatherHeapSnapshotData functor(*builder);
        m_objectSpace.forEachLiveCell(heapIterationScope, functor);
    }
}

struct RemoveDeadHeapSnapshotNodes : MarkedBlock::CountFunctor {
    RemoveDeadHeapSnapshotNodes(HeapSnapshot& snapshot)
        : m_snapshot(snapshot)
    {
    }

    IterationStatus operator()(JSCell* cell)
    {
        m_snapshot.sweepCell(cell);
        return IterationStatus::Continue;
    }

    HeapSnapshot& m_snapshot;
};

void Heap::removeDeadHeapSnapshotNodes(HeapProfiler& heapProfiler)
{
    GCPHASE(RemoveDeadHeapSnapshotNodes);
    if (HeapSnapshot* snapshot = heapProfiler.mostRecentSnapshot()) {
        HeapIterationScope heapIterationScope(*this);
        RemoveDeadHeapSnapshotNodes functor(*snapshot);
        m_objectSpace.forEachDeadCell(heapIterationScope, functor);
        snapshot->shrinkToFit();
    }
}

void Heap::visitProtectedObjects(HeapRootVisitor& heapRootVisitor)
{
    GCPHASE(VisitProtectedObjects);

    for (auto& pair : m_protectedValues)
        heapRootVisitor.visit(&pair.key);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Protected Objects:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitArgumentBuffers(HeapRootVisitor& visitor)
{
    GCPHASE(MarkingArgumentBuffers);
    if (!m_markListSet || !m_markListSet->size())
        return;

    MarkedArgumentBuffer::markLists(visitor, *m_markListSet);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Argument Buffers:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitException(HeapRootVisitor& visitor)
{
    GCPHASE(MarkingException);
    if (!m_vm->exception() && !m_vm->lastException())
        return;

    visitor.visit(m_vm->addressOfException());
    visitor.visit(m_vm->addressOfLastException());

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Exceptions:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitStrongHandles(HeapRootVisitor& visitor)
{
    GCPHASE(VisitStrongHandles);
    m_handleSet.visitStrongHandles(visitor);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Strong Handles:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitHandleStack(HeapRootVisitor& visitor)
{
    GCPHASE(VisitHandleStack);
    m_handleStack.visit(visitor);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Handle Stack:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::visitSamplingProfiler()
{
#if ENABLE(SAMPLING_PROFILER)
    if (SamplingProfiler* samplingProfiler = m_vm->samplingProfiler()) {
        ASSERT(samplingProfiler->getLock().isLocked());
        GCPHASE(VisitSamplingProfiler);
        samplingProfiler->visit(m_slotVisitor);
        if (Options::logGC() == GCLogging::Verbose)
            dataLog("Sampling Profiler data:\n", m_slotVisitor);

        m_slotVisitor.donateAndDrain();
        samplingProfiler->getLock().unlock();
    }
#endif // ENABLE(SAMPLING_PROFILER)
}

void Heap::visitShadowChicken()
{
    m_vm->shadowChicken().visitChildren(m_slotVisitor);
}

void Heap::traceCodeBlocksAndJITStubRoutines()
{
    GCPHASE(TraceCodeBlocksAndJITStubRoutines);
    m_jitStubRoutines.traceMarkedStubRoutines(m_slotVisitor);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Code Blocks and JIT Stub Routines:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::converge()
{
    GCPHASE(Convergence);
    m_slotVisitor.drainFromShared(SlotVisitor::MasterDrain);
}

void Heap::visitWeakHandles(HeapRootVisitor& visitor)
{
    GCPHASE(VisitingLiveWeakHandles);
    while (true) {
        m_objectSpace.visitWeakSets(visitor);
        harvestWeakReferences();
        visitCompilerWorklistWeakReferences();
        if (m_slotVisitor.isEmpty())
            break;

        if (Options::logGC() == GCLogging::Verbose)
            dataLog("Live Weak Handles:\n", m_slotVisitor);

        {
            ParallelModeEnabler enabler(m_slotVisitor);
            m_slotVisitor.donateAndDrain();
            m_slotVisitor.drainFromShared(SlotVisitor::MasterDrain);
        }
    }
}

void Heap::updateObjectCounts(double gcStartTime)
{
    GCCOUNTER(VisitedValueCount, m_slotVisitor.visitCount() + threadVisitCount());

    if (Options::logGC() == GCLogging::Verbose) {
        size_t visitCount = m_slotVisitor.visitCount();
        visitCount += threadVisitCount();
        dataLogF("\nNumber of live Objects after GC %lu, took %.6f secs\n", static_cast<unsigned long>(visitCount), WTF::monotonicallyIncreasingTime() - gcStartTime);
    }
    
    size_t bytesRemovedFromOldSpaceDueToReallocation =
        m_storageSpace.takeBytesRemovedFromOldSpaceDueToReallocation();
    
    if (m_operationInProgress == FullCollection) {
        m_totalBytesVisited = 0;
        m_totalBytesCopied = 0;
    } else
        m_totalBytesCopied -= bytesRemovedFromOldSpaceDueToReallocation;

    m_totalBytesVisitedThisCycle = m_slotVisitor.bytesVisited() + threadBytesVisited();
    m_totalBytesCopiedThisCycle = m_slotVisitor.bytesCopied() + threadBytesCopied();
    
    m_totalBytesVisited += m_totalBytesVisitedThisCycle;
    m_totalBytesCopied += m_totalBytesCopiedThisCycle;
}

void Heap::resetVisitors()
{
    m_slotVisitor.reset();

    for (auto& parallelVisitor : m_parallelSlotVisitors)
        parallelVisitor->reset();

    ASSERT(m_sharedMarkStack.isEmpty());
    m_weakReferenceHarvesters.removeAll();
}

size_t Heap::objectCount()
{
    return m_objectSpace.objectCount();
}

size_t Heap::extraMemorySize()
{
    return m_extraMemorySize + m_deprecatedExtraMemorySize + m_arrayBuffers.size();
}

size_t Heap::size()
{
    return m_objectSpace.size() + m_storageSpace.size() + extraMemorySize();
}

size_t Heap::capacity()
{
    return m_objectSpace.capacity() + m_storageSpace.capacity() + extraMemorySize();
}

size_t Heap::protectedGlobalObjectCount()
{
    return forEachProtectedCell<CountIfGlobalObject>();
}

size_t Heap::globalObjectCount()
{
    HeapIterationScope iterationScope(*this);
    return m_objectSpace.forEachLiveCell<CountIfGlobalObject>(iterationScope);
}

size_t Heap::protectedObjectCount()
{
    return forEachProtectedCell<Count>();
}

std::unique_ptr<TypeCountSet> Heap::protectedObjectTypeCounts()
{
    return forEachProtectedCell<RecordType>();
}

std::unique_ptr<TypeCountSet> Heap::objectTypeCounts()
{
    HeapIterationScope iterationScope(*this);
    return m_objectSpace.forEachLiveCell<RecordType>(iterationScope);
}

void Heap::deleteAllCodeBlocks()
{
    // If JavaScript is running, it's not safe to delete all JavaScript code, since
    // we'll end up returning to deleted code.
    RELEASE_ASSERT(!m_vm->entryScope);
    ASSERT(m_operationInProgress == NoOperation);

    completeAllDFGPlans();

    for (ExecutableBase* executable : m_executables)
        executable->clearCode();
}

void Heap::deleteAllUnlinkedCodeBlocks()
{
    for (ExecutableBase* current : m_executables) {
        if (!current->isFunctionExecutable())
            continue;
        static_cast<FunctionExecutable*>(current)->unlinkedExecutable()->clearCode();
    }
}

void Heap::clearUnmarkedExecutables()
{
    GCPHASE(ClearUnmarkedExecutables);
    for (unsigned i = m_executables.size(); i--;) {
        ExecutableBase* current = m_executables[i];
        if (isMarked(current))
            continue;

        // Eagerly dereference the Executable's JITCode in order to run watchpoint
        // destructors. Otherwise, watchpoints might fire for deleted CodeBlocks.
        current->clearCode();
        std::swap(m_executables[i], m_executables.last());
        m_executables.removeLast();
    }

    m_executables.shrinkToFit();
}

void Heap::deleteUnmarkedCompiledCode()
{
    GCPHASE(DeleteCodeBlocks);
    clearUnmarkedExecutables();
    m_codeBlocks.deleteUnmarkedAndUnreferenced(m_operationInProgress);
    m_jitStubRoutines.deleteUnmarkedJettisonedStubRoutines();
}

void Heap::addToRememberedSet(const JSCell* cell)
{
    ASSERT(cell);
    ASSERT(!Options::useConcurrentJIT() || !isCompilationThread());
    ASSERT(cell->cellState() == CellState::OldBlack);
    // Indicate that this object is grey and that it's one of the following:
    // - A re-greyed object during a concurrent collection.
    // - An old remembered object.
    // "OldGrey" doesn't tell us which of these things is true, but we usually treat the two cases the
    // same.
    cell->setCellState(CellState::OldGrey);
    m_slotVisitor.appendToMarkStack(const_cast<JSCell*>(cell));
}

void Heap::collectAndSweep(HeapOperation collectionType)
{
    if (!m_isSafeToCollect)
        return;

    collect(collectionType);

    DeferGCForAWhile deferGC(*this);
    m_objectSpace.sweep();
    m_objectSpace.shrink();

    sweepAllLogicallyEmptyWeakBlocks();
}

NEVER_INLINE void Heap::collect(HeapOperation collectionType)
{
    void* stackTop;
    ALLOCATE_AND_GET_REGISTER_STATE(registers);

    collectImpl(collectionType, wtfThreadData().stack().origin(), &stackTop, registers);

    sanitizeStackForVM(m_vm);
}

NEVER_INLINE void Heap::collectImpl(HeapOperation collectionType, void* stackOrigin, void* stackTop, MachineThreads::RegisterState& calleeSavedRegisters)
{
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC starting collection.\n");
#endif
    
    double before = 0;
    if (Options::logGC()) {
        dataLog("[GC: ", capacity() / 1024, " kb ");
        before = currentTimeMS();
    }
    
    if (vm()->typeProfiler()) {
        DeferGCForAWhile awhile(*this);
        vm()->typeProfilerLog()->processLogEntries(ASCIILiteral("GC"));
    }

    vm()->shadowChicken().update(*vm(), vm()->topCallFrame);

    RELEASE_ASSERT(!m_deferralDepth);
    ASSERT(vm()->currentThreadIsHoldingAPILock());
    RELEASE_ASSERT(vm()->atomicStringTable() == wtfThreadData().atomicStringTable());
    ASSERT(m_isSafeToCollect);
    JAVASCRIPTCORE_GC_BEGIN();
    RELEASE_ASSERT(m_operationInProgress == NoOperation);

    suspendCompilerThreads();
    willStartCollection(collectionType);
    GCPHASE(Collect);

    double gcStartTime = WTF::monotonicallyIncreasingTime();
    if (m_verifier) {
        // Verify that live objects from the last GC cycle haven't been corrupted by
        // mutators before we begin this new GC cycle.
        m_verifier->verify(HeapVerifier::Phase::BeforeGC);

        m_verifier->initializeGCCycle();
        m_verifier->gatherLiveObjects(HeapVerifier::Phase::BeforeMarking);
    }

    flushOldStructureIDTables();
    stopAllocation();
    flushWriteBarrierBuffer();

    markRoots(gcStartTime, stackOrigin, stackTop, calleeSavedRegisters);

    if (m_verifier) {
        m_verifier->gatherLiveObjects(HeapVerifier::Phase::AfterMarking);
        m_verifier->verify(HeapVerifier::Phase::AfterMarking);
    }
    JAVASCRIPTCORE_GC_MARKED();

    if (vm()->typeProfiler())
        vm()->typeProfiler()->invalidateTypeSetCache();

    reapWeakHandles();
    pruneStaleEntriesFromWeakGCMaps();
    sweepArrayBuffers();
    snapshotMarkedSpace();

    copyBackingStores();

    finalizeUnconditionalFinalizers();
    removeDeadCompilerWorklistEntries();
    deleteUnmarkedCompiledCode();
    deleteSourceProviderCaches();

    notifyIncrementalSweeper();
    writeBarrierCurrentlyExecutingCodeBlocks();

    resetAllocators();
    updateAllocationLimits();
    didFinishCollection(gcStartTime);
    resumeCompilerThreads();

    if (m_verifier) {
        m_verifier->trimDeadObjects();
        m_verifier->verify(HeapVerifier::Phase::AfterGC);
    }

    if (Options::logGC()) {
        double after = currentTimeMS();
        dataLog(after - before, " ms]\n");
    }
}

void Heap::suspendCompilerThreads()
{
#if ENABLE(DFG_JIT)
    GCPHASE(SuspendCompilerThreads);
    ASSERT(m_suspendedCompilerWorklists.isEmpty());
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i)) {
            m_suspendedCompilerWorklists.append(worklist);
            worklist->suspendAllThreads();
        }
    }
#endif
}

void Heap::willStartCollection(HeapOperation collectionType)
{
    GCPHASE(StartingCollection);
    
    if (Options::logGC())
        dataLog("=> ");
    
    if (shouldDoFullCollection(collectionType)) {
        m_operationInProgress = FullCollection;
        m_shouldDoFullCollection = false;
        if (Options::logGC())
            dataLog("FullCollection, ");
    } else {
        m_operationInProgress = EdenCollection;
        if (Options::logGC())
            dataLog("EdenCollection, ");
    }
    if (m_operationInProgress == FullCollection) {
        m_sizeBeforeLastFullCollect = m_sizeAfterLastCollect + m_bytesAllocatedThisCycle;
        m_extraMemorySize = 0;
        m_deprecatedExtraMemorySize = 0;

        if (m_fullActivityCallback)
            m_fullActivityCallback->willCollect();
    } else {
        ASSERT(m_operationInProgress == EdenCollection);
        m_sizeBeforeLastEdenCollect = m_sizeAfterLastCollect + m_bytesAllocatedThisCycle;
    }

    if (m_edenActivityCallback)
        m_edenActivityCallback->willCollect();

    for (auto* observer : m_observers)
        observer->willGarbageCollect();
}

void Heap::flushOldStructureIDTables()
{
    GCPHASE(FlushOldStructureIDTables);
    m_structureIDTable.flushOldTables();
}

void Heap::flushWriteBarrierBuffer()
{
    GCPHASE(FlushWriteBarrierBuffer);
    if (m_operationInProgress == EdenCollection) {
        m_writeBarrierBuffer.flush(*this);
        return;
    }
    m_writeBarrierBuffer.reset();
}

void Heap::stopAllocation()
{
    GCPHASE(StopAllocation);
    m_objectSpace.stopAllocating();
    if (m_operationInProgress == FullCollection)
        m_storageSpace.didStartFullCollection();
}

void Heap::reapWeakHandles()
{
    GCPHASE(ReapingWeakHandles);
    m_objectSpace.reapWeakSets();
}

void Heap::pruneStaleEntriesFromWeakGCMaps()
{
    GCPHASE(PruningStaleEntriesFromWeakGCMaps);
    if (m_operationInProgress != FullCollection)
        return;
    for (auto& pruneCallback : m_weakGCMaps.values())
        pruneCallback();
}

void Heap::sweepArrayBuffers()
{
    GCPHASE(SweepingArrayBuffers);
    m_arrayBuffers.sweep();
}

struct MarkedBlockSnapshotFunctor : public MarkedBlock::VoidFunctor {
    MarkedBlockSnapshotFunctor(Vector<MarkedBlock*>& blocks) 
        : m_index(0) 
        , m_blocks(blocks)
    {
    }

    void operator()(MarkedBlock* block) { m_blocks[m_index++] = block; }

    size_t m_index;
    Vector<MarkedBlock*>& m_blocks;
};

void Heap::snapshotMarkedSpace()
{
    GCPHASE(SnapshotMarkedSpace);

    if (m_operationInProgress == EdenCollection) {
        m_blockSnapshot.appendVector(m_objectSpace.blocksWithNewObjects());
        // Sort and deduplicate the block snapshot since we might be appending to an unfinished work list.
        std::sort(m_blockSnapshot.begin(), m_blockSnapshot.end());
        m_blockSnapshot.shrink(std::unique(m_blockSnapshot.begin(), m_blockSnapshot.end()) - m_blockSnapshot.begin());
    } else {
        m_blockSnapshot.resizeToFit(m_objectSpace.blocks().set().size());
        MarkedBlockSnapshotFunctor functor(m_blockSnapshot);
        m_objectSpace.forEachBlock(functor);
    }
}

void Heap::deleteSourceProviderCaches()
{
    GCPHASE(DeleteSourceProviderCaches);
    m_vm->clearSourceProviderCaches();
}

void Heap::notifyIncrementalSweeper()
{
    GCPHASE(NotifyIncrementalSweeper);

    if (m_operationInProgress == FullCollection) {
        if (!m_logicallyEmptyWeakBlocks.isEmpty())
            m_indexOfNextLogicallyEmptyWeakBlockToSweep = 0;
    }

    m_sweeper->startSweeping();
}

void Heap::writeBarrierCurrentlyExecutingCodeBlocks()
{
    GCPHASE(WriteBarrierCurrentlyExecutingCodeBlocks);
    m_codeBlocks.writeBarrierCurrentlyExecutingCodeBlocks(this);
}

void Heap::resetAllocators()
{
    GCPHASE(ResetAllocators);
    m_objectSpace.resetAllocators();
}

void Heap::updateAllocationLimits()
{
    GCPHASE(UpdateAllocationLimits);
    
    // Calculate our current heap size threshold for the purpose of figuring out when we should
    // run another collection. This isn't the same as either size() or capacity(), though it should
    // be somewhere between the two. The key is to match the size calculations involved calls to
    // didAllocate(), while never dangerously underestimating capacity(). In extreme cases of
    // fragmentation, we may have size() much smaller than capacity(). Our collector sometimes
    // temporarily allows very high fragmentation because it doesn't defragment old blocks in copied
    // space.
    size_t currentHeapSize = 0;

    // For marked space, we use the total number of bytes visited. This matches the logic for
    // MarkedAllocator's calls to didAllocate(), which effectively accounts for the total size of
    // objects allocated rather than blocks used. This will underestimate capacity(), and in case
    // of fragmentation, this may be substantial. Fortunately, marked space rarely fragments because
    // cells usually have a narrow range of sizes. So, the underestimation is probably OK.
    currentHeapSize += m_totalBytesVisited;

    // For copied space, we use the capacity of storage space. This is because copied space may get
    // badly fragmented between full collections. This arises when each eden collection evacuates
    // much less than one CopiedBlock's worth of stuff. It can also happen when CopiedBlocks get
    // pinned due to very short-lived objects. In such a case, we want to get to a full collection
    // sooner rather than later. If we used m_totalBytesCopied, then for for each CopiedBlock that an
    // eden allocation promoted, we would only deduct the one object's size from eden size. This
    // would mean that we could "leak" many CopiedBlocks before we did a full collection and
    // defragmented all of them. It would be great to use m_totalBytesCopied, but we'd need to
    // augment it with something that accounts for those fragmented blocks.
    // FIXME: Make it possible to compute heap size using m_totalBytesCopied rather than
    // m_storageSpace.capacity()
    // https://bugs.webkit.org/show_bug.cgi?id=150268
    ASSERT(m_totalBytesCopied <= m_storageSpace.size());
    currentHeapSize += m_storageSpace.capacity();

    // It's up to the user to ensure that extraMemorySize() ends up corresponding to allocation-time
    // extra memory reporting.
    currentHeapSize += extraMemorySize();
    
    if (Options::gcMaxHeapSize() && currentHeapSize > Options::gcMaxHeapSize())
        HeapStatistics::exitWithFailure();

    if (m_operationInProgress == FullCollection) {
        // To avoid pathological GC churn in very small and very large heaps, we set
        // the new allocation limit based on the current size of the heap, with a
        // fixed minimum.
        m_maxHeapSize = max(minHeapSize(m_heapType, m_ramSize), proportionalHeapSize(currentHeapSize, m_ramSize));
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        m_sizeAfterLastFullCollect = currentHeapSize;
        m_bytesAbandonedSinceLastFullCollect = 0;
    } else {
        static const bool verbose = false;
        
        ASSERT(currentHeapSize >= m_sizeAfterLastCollect);
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        m_sizeAfterLastEdenCollect = currentHeapSize;
        if (verbose) {
            dataLog("Max heap size: ", m_maxHeapSize, "\n");
            dataLog("Current heap size: ", currentHeapSize, "\n");
            dataLog("Size after last eden collection: ", m_sizeAfterLastEdenCollect, "\n");
        }
        double edenToOldGenerationRatio = (double)m_maxEdenSize / (double)m_maxHeapSize;
        if (verbose)
            dataLog("Eden to old generation ratio: ", edenToOldGenerationRatio, "\n");
        double minEdenToOldGenerationRatio = 1.0 / 3.0;
        if (edenToOldGenerationRatio < minEdenToOldGenerationRatio)
            m_shouldDoFullCollection = true;
        // This seems suspect at first, but what it does is ensure that the nursery size is fixed.
        m_maxHeapSize += currentHeapSize - m_sizeAfterLastCollect;
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        if (m_fullActivityCallback) {
            ASSERT(currentHeapSize >= m_sizeAfterLastFullCollect);
            m_fullActivityCallback->didAllocate(currentHeapSize - m_sizeAfterLastFullCollect);
        }
    }

    m_sizeAfterLastCollect = currentHeapSize;
    m_bytesAllocatedThisCycle = 0;

    if (Options::logGC())
        dataLog(currentHeapSize / 1024, " kb, ");
}

void Heap::didFinishCollection(double gcStartTime)
{
    GCPHASE(FinishingCollection);
    double gcEndTime = WTF::monotonicallyIncreasingTime();
    HeapOperation operation = m_operationInProgress;
    if (m_operationInProgress == FullCollection)
        m_lastFullGCLength = gcEndTime - gcStartTime;
    else
        m_lastEdenGCLength = gcEndTime - gcStartTime;

    if (Options::recordGCPauseTimes())
        HeapStatistics::recordGCPauseTime(gcStartTime, gcEndTime);

    if (Options::useZombieMode())
        zombifyDeadObjects();

    if (Options::useImmortalObjects())
        markDeadObjects();

    if (Options::dumpObjectStatistics())
        HeapStatistics::dumpObjectStatistics(this);

    if (Options::logGC() == GCLogging::Verbose)
        GCLogging::dumpObjectGraph(this);

    if (HeapProfiler* heapProfiler = m_vm->heapProfiler()) {
        gatherExtraHeapSnapshotData(*heapProfiler);
        removeDeadHeapSnapshotNodes(*heapProfiler);
    }

    RELEASE_ASSERT(m_operationInProgress == EdenCollection || m_operationInProgress == FullCollection);
    m_operationInProgress = NoOperation;
    JAVASCRIPTCORE_GC_END();

    for (auto* observer : m_observers)
        observer->didGarbageCollect(operation);
}

void Heap::resumeCompilerThreads()
{
#if ENABLE(DFG_JIT)
    GCPHASE(ResumeCompilerThreads);
    for (auto worklist : m_suspendedCompilerWorklists)
        worklist->resumeAllThreads();
    m_suspendedCompilerWorklists.clear();
#endif
}

void Heap::markDeadObjects()
{
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachDeadCell<MarkObject>(iterationScope);
}

void Heap::setFullActivityCallback(PassRefPtr<FullGCActivityCallback> activityCallback)
{
    m_fullActivityCallback = activityCallback;
}

void Heap::setEdenActivityCallback(PassRefPtr<EdenGCActivityCallback> activityCallback)
{
    m_edenActivityCallback = activityCallback;
}

GCActivityCallback* Heap::fullActivityCallback()
{
    return m_fullActivityCallback.get();
}

GCActivityCallback* Heap::edenActivityCallback()
{
    return m_edenActivityCallback.get();
}

void Heap::setIncrementalSweeper(std::unique_ptr<IncrementalSweeper> sweeper)
{
    m_sweeper = WTFMove(sweeper);
}

IncrementalSweeper* Heap::sweeper()
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
        m_edenActivityCallback->didAllocate(m_bytesAllocatedThisCycle + m_bytesAbandonedSinceLastFullCollect);
    m_bytesAllocatedThisCycle += bytes;
}

bool Heap::isValidAllocation(size_t)
{
    if (!isValidThreadState(m_vm))
        return false;

    if (m_operationInProgress != NoOperation)
        return false;
    
    return true;
}

void Heap::addFinalizer(JSCell* cell, Finalizer finalizer)
{
    WeakSet::allocate(cell, &m_finalizerOwner, reinterpret_cast<void*>(finalizer)); // Balanced by FinalizerOwner::finalize().
}

void Heap::FinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    HandleSlot slot = handle.slot();
    Finalizer finalizer = reinterpret_cast<Finalizer>(context);
    finalizer(slot->asCell());
    WeakSet::deallocate(WeakImpl::asWeakImpl(slot));
}

void Heap::addExecutable(ExecutableBase* executable)
{
    m_executables.append(executable);
}

void Heap::collectAllGarbageIfNotDoneRecently()
{
    if (!m_fullActivityCallback) {
        collectAllGarbage();
        return;
    }

    if (m_fullActivityCallback->didSyncGCRecently()) {
        // A synchronous GC was already requested recently so we merely accelerate next collection.
        reportAbandonedObjectGraph();
        return;
    }

    m_fullActivityCallback->setDidSyncGCRecently();
    collectAllGarbage();
}

class Zombify : public MarkedBlock::VoidFunctor {
public:
    inline void visit(JSCell* cell)
    {
        void** current = reinterpret_cast<void**>(cell);

        // We want to maintain zapped-ness because that's how we know if we've called 
        // the destructor.
        if (cell->isZapped())
            current++;

        void* limit = static_cast<void*>(reinterpret_cast<char*>(cell) + MarkedBlock::blockFor(cell)->cellSize());
        for (; current < limit; current++)
            *current = zombifiedBits;
    }
    IterationStatus operator()(JSCell* cell)
    {
        visit(cell);
        return IterationStatus::Continue;
    }
};

void Heap::zombifyDeadObjects()
{
    // Sweep now because destructors will crash once we're zombified.
    m_objectSpace.zombifySweep();
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachDeadCell<Zombify>(iterationScope);
}

void Heap::flushWriteBarrierBuffer(JSCell* cell)
{
    m_writeBarrierBuffer.flush(*this);
    m_writeBarrierBuffer.add(cell);
}

bool Heap::shouldDoFullCollection(HeapOperation requestedCollectionType) const
{
    if (!Options::useGenerationalGC())
        return true;

    switch (requestedCollectionType) {
    case EdenCollection:
        return false;
    case FullCollection:
        return true;
    case AnyCollection:
        return m_shouldDoFullCollection;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
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

size_t Heap::threadVisitCount()
{       
    unsigned long result = 0;
    for (auto& parallelVisitor : m_parallelSlotVisitors)
        result += parallelVisitor->visitCount();
    return result;
}

size_t Heap::threadBytesVisited()
{       
    size_t result = 0;
    for (auto& parallelVisitor : m_parallelSlotVisitors)
        result += parallelVisitor->bytesVisited();
    return result;
}

size_t Heap::threadBytesCopied()
{       
    size_t result = 0;
    for (auto& parallelVisitor : m_parallelSlotVisitors)
        result += parallelVisitor->bytesCopied();
    return result;
}

} // namespace JSC
