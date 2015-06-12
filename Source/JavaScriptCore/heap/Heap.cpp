/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2013, 2014 Apple Inc. All rights reserved.
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
#include "HeapIterationScope.h"
#include "HeapRootVisitor.h"
#include "HeapStatistics.h"
#include "HeapVerifier.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "JSCInlines.h"
#include "JSVirtualMachineInternal.h"
#include "RecursiveAllocationScope.h"
#include "Tracing.h"
#include "TypeProfilerLog.h"
#include "UnlinkedCodeBlock.h"
#include "VM.h"
#include "WeakSetInlines.h"
#include <algorithm>
#include <wtf/RAMSize.h>
#include <wtf/CurrentTime.h>
#include <wtf/ProcessID.h>

using namespace std;
using namespace JSC;

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
    return WTF::move(m_typeCountSet);
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
    , m_sharedData(vm)
    , m_slotVisitor(m_sharedData)
    , m_copyVisitor(m_sharedData)
    , m_handleSet(vm)
    , m_isSafeToCollect(false)
    , m_writeBarrierBuffer(256)
    , m_vm(vm)
    // We seed with 10ms so that GCActivityCallback::didAllocate doesn't continuously 
    // schedule the timer if we've never done a collection.
    , m_lastFullGCLength(0.01)
    , m_lastEdenGCLength(0.01)
    , m_lastCodeDiscardTime(WTF::monotonicallyIncreasingTime())
    , m_fullActivityCallback(GCActivityCallback::createFullTimer(this))
#if ENABLE(GGC)
    , m_edenActivityCallback(GCActivityCallback::createEdenTimer(this))
#else
    , m_edenActivityCallback(m_fullActivityCallback)
#endif
#if USE(CF)
    , m_sweeper(std::make_unique<IncrementalSweeper>(this, CFRunLoopGetCurrent()))
#else
    , m_sweeper(std::make_unique<IncrementalSweeper>(this->vm()))
#endif
    , m_deferralDepth(0)
#if USE(CF)
    , m_delayedReleaseRecursionCount(0)
#endif
{
    m_storageSpace.init();
    if (Options::verifyHeap())
        m_verifier = std::make_unique<HeapVerifier>(this, Options::numberOfGCCyclesToRecordForVerification());
}

Heap::~Heap()
{
    for (WeakBlock* block : m_logicallyEmptyWeakBlocks)
        WeakBlock::destroy(block);
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

    m_objectSpace.lastChanceToFinalize();
    releaseDelayedReleasedObjects();

    sweepAllLogicallyEmptyWeakBlocks();
}

void Heap::releaseDelayedReleasedObjects()
{
#if USE(CF)
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

            Vector<RetainPtr<CFTypeRef>> objectsToRelease = WTF::move(m_delayedReleaseObjects);

            {
                // We need to drop locks before calling out to arbitrary code.
                JSLock::DropAllLocks dropAllLocks(m_vm);

                objectsToRelease.clear();
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

void Heap::getConservativeRegisterRoots(HashSet<JSCell*>& roots)
{
    ASSERT(isValidThreadState(m_vm));
    ConservativeRoots stackRoots(&m_objectSpace.blocks(), &m_storageSpace);
    stack().gatherConservativeRoots(stackRoots);
    size_t stackRootCount = stackRoots.size();
    JSCell** registerRoots = stackRoots.roots();
    for (size_t i = 0; i < stackRootCount; i++) {
        setMarked(registerRoots[i]);
        registerRoots[i]->setMarked();
        roots.add(registerRoots[i]);
    }
}

void Heap::markRoots(double gcStartTime, void* stackOrigin, void* stackTop, MachineThreads::RegisterState& calleeSavedRegisters)
{
    SamplingRegion samplingRegion("Garbage Collection: Marking");

    GCPHASE(MarkRoots);
    ASSERT(isValidThreadState(m_vm));

#if ENABLE(GGC)
    Vector<const JSCell*> rememberedSet(m_slotVisitor.markStack().size());
    m_slotVisitor.markStack().fillVector(rememberedSet);
#else
    Vector<const JSCell*> rememberedSet;
#endif

    if (m_operationInProgress == EdenCollection)
        m_codeBlocks.clearMarksForEdenCollection(rememberedSet);
    else
        m_codeBlocks.clearMarksForFullCollection();

    // We gather conservative roots before clearing mark bits because conservative
    // gathering uses the mark bits to determine whether a reference is valid.
    ConservativeRoots conservativeRoots(&m_objectSpace.blocks(), &m_storageSpace);
    gatherStackRoots(conservativeRoots, stackOrigin, stackTop, calleeSavedRegisters);
    gatherJSStackRoots(conservativeRoots);
    gatherScratchBufferRoots(conservativeRoots);

    clearLivenessData();

    m_sharedData.didStartMarking();
    m_slotVisitor.didStartMarking();
    HeapRootVisitor heapRootVisitor(m_slotVisitor);

    {
        ParallelModeEnabler enabler(m_slotVisitor);

        visitExternalRememberedSet();
        visitSmallStrings();
        visitConservativeRoots(conservativeRoots);
        visitProtectedObjects(heapRootVisitor);
        visitArgumentBuffers(heapRootVisitor);
        visitException(heapRootVisitor);
        visitStrongHandles(heapRootVisitor);
        visitHandleStack(heapRootVisitor);
        traceCodeBlocksAndJITStubRoutines();
        converge();
    }

    // Weak references must be marked last because their liveness depends on
    // the liveness of the rest of the object graph.
    visitWeakHandles(heapRootVisitor);

    clearRememberedSet(rememberedSet);
    m_sharedData.didFinishMarking();
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
        m_sharedData.didStartCopying();
        m_copyVisitor.startCopying();
        m_copyVisitor.copyFromShared();
        m_copyVisitor.doneCopying();
        // We need to wait for everybody to finish and return their CopiedBlocks 
        // before signaling that the phase is complete.
        m_storageSpace.doneCopying();
        m_sharedData.didFinishCopying();
    } else
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
        worklist->visitWeakReferences(m_slotVisitor, m_codeBlocks);

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

void Heap::traceCodeBlocksAndJITStubRoutines()
{
    GCPHASE(TraceCodeBlocksAndJITStubRoutines);
    m_codeBlocks.traceMarked(m_slotVisitor);
    m_jitStubRoutines.traceMarkedStubRoutines(m_slotVisitor);

    if (Options::logGC() == GCLogging::Verbose)
        dataLog("Code Blocks and JIT Stub Routines:\n", m_slotVisitor);

    m_slotVisitor.donateAndDrain();
}

void Heap::converge()
{
#if ENABLE(PARALLEL_GC)
    GCPHASE(Convergence);
    m_slotVisitor.drainFromShared(SlotVisitor::MasterDrain);
#endif
}

void Heap::visitWeakHandles(HeapRootVisitor& visitor)
{
    GCPHASE(VisitingLiveWeakHandles);
    while (true) {
        m_objectSpace.visitWeakSets(visitor);
        harvestWeakReferences();
        visitCompilerWorklistWeakReferences();
        m_codeBlocks.traceMarked(m_slotVisitor); // New "executing" code blocks may be discovered.
        if (m_slotVisitor.isEmpty())
            break;

        if (Options::logGC() == GCLogging::Verbose)
            dataLog("Live Weak Handles:\n", m_slotVisitor);

        {
            ParallelModeEnabler enabler(m_slotVisitor);
            m_slotVisitor.donateAndDrain();
#if ENABLE(PARALLEL_GC)
            m_slotVisitor.drainFromShared(SlotVisitor::MasterDrain);
#endif
        }
    }
}

void Heap::clearRememberedSet(Vector<const JSCell*>& rememberedSet)
{
#if ENABLE(GGC)
    GCPHASE(ClearRememberedSet);
    for (auto* cell : rememberedSet)
        const_cast<JSCell*>(cell)->setRemembered(false);
#else
    UNUSED_PARAM(rememberedSet);
#endif
}

void Heap::updateObjectCounts(double gcStartTime)
{
    GCCOUNTER(VisitedValueCount, m_slotVisitor.visitCount());

    if (Options::logGC() == GCLogging::Verbose) {
        size_t visitCount = m_slotVisitor.visitCount();
#if ENABLE(PARALLEL_GC)
        visitCount += m_sharedData.childVisitCount();
#endif
        dataLogF("\nNumber of live Objects after GC %lu, took %.6f secs\n", static_cast<unsigned long>(visitCount), WTF::monotonicallyIncreasingTime() - gcStartTime);
    }
    
    size_t bytesRemovedFromOldSpaceDueToReallocation =
        m_storageSpace.takeBytesRemovedFromOldSpaceDueToReallocation();
    
    if (m_operationInProgress == FullCollection) {
        m_totalBytesVisited = 0;
        m_totalBytesCopied = 0;
    } else
        m_totalBytesCopied -= bytesRemovedFromOldSpaceDueToReallocation;
    
    m_totalBytesVisited += m_slotVisitor.bytesVisited();
    m_totalBytesCopied += m_slotVisitor.bytesCopied();
#if ENABLE(PARALLEL_GC)
    m_totalBytesVisited += m_sharedData.childBytesVisited();
    m_totalBytesCopied += m_sharedData.childBytesCopied();
#endif
}

void Heap::resetVisitors()
{
    m_slotVisitor.reset();
#if ENABLE(PARALLEL_GC)
    m_sharedData.resetChildren();
#endif
    m_sharedData.reset();
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

size_t Heap::sizeAfterCollect()
{
    // The result here may not agree with the normal Heap::size(). 
    // This is due to the fact that we only count live copied bytes
    // rather than all used (including dead) copied bytes, thus it's 
    // always the case that m_totalBytesCopied <= m_storageSpace.size(). 
    ASSERT(m_totalBytesCopied <= m_storageSpace.size());
    return m_totalBytesVisited + m_totalBytesCopied + extraMemorySize();
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

void Heap::deleteAllCompiledCode()
{
    // If JavaScript is running, it's not safe to delete code, since we'll end
    // up deleting code that is live on the stack.
    if (m_vm->entryScope)
        return;
    
    // If we have things on any worklist, then don't delete code. This is kind of
    // a weird heuristic. It's definitely not safe to throw away code that is on
    // the worklist. But this change was made in a hurry so we just avoid throwing
    // away any code if there is any code on any worklist. I suspect that this
    // might not actually be too dumb: if there is code on worklists then that
    // means that we are running some hot JS code right now. Maybe causing
    // recompilations isn't a good idea.
#if ENABLE(DFG_JIT)
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i)) {
            if (worklist->isActiveForVM(*vm()))
                return;
        }
    }
#endif // ENABLE(DFG_JIT)

    for (ExecutableBase* current : m_compiledCode) {
        if (!current->isFunctionExecutable())
            continue;
        static_cast<FunctionExecutable*>(current)->clearCode();
    }

    ASSERT(m_operationInProgress == FullCollection || m_operationInProgress == NoOperation);
    m_codeBlocks.clearMarksForFullCollection();
    m_codeBlocks.deleteUnmarkedAndUnreferenced(FullCollection);
}

void Heap::deleteAllUnlinkedFunctionCode()
{
    for (ExecutableBase* current : m_compiledCode) {
        if (!current->isFunctionExecutable())
            continue;
        static_cast<FunctionExecutable*>(current)->clearUnlinkedCodeForRecompilation();
    }
}

void Heap::clearUnmarkedExecutables()
{
    GCPHASE(ClearUnmarkedExecutables);
    for (unsigned i = m_compiledCode.size(); i--;) {
        ExecutableBase* current = m_compiledCode[i];
        if (isMarked(current))
            continue;

        // We do this because executable memory is limited on some platforms and because
        // CodeBlock requires eager finalization.
        ExecutableBase::clearCodeVirtual(current);
        std::swap(m_compiledCode[i], m_compiledCode.last());
        m_compiledCode.removeLast();
    }
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
    ASSERT(!Options::enableConcurrentJIT() || !isCompilationThread());
    if (isRemembered(cell))
        return;
    const_cast<JSCell*>(cell)->setRemembered(true);
    m_slotVisitor.unconditionallyAppend(const_cast<JSCell*>(cell));
}

void Heap::collectAndSweep(HeapOperation collectionType)
{
    if (!m_isSafeToCollect)
        return;

    collect(collectionType);

    SamplingRegion samplingRegion("Garbage Collection: Sweeping");

    DeferGCForAWhile deferGC(*this);
    m_objectSpace.sweep();
    m_objectSpace.shrink();

    sweepAllLogicallyEmptyWeakBlocks();
}

static double minute = 60.0;

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
        dataLog("[GC: ");
        before = currentTimeMS();
    }
    
    SamplingRegion samplingRegion("Garbage Collection");
    
    if (vm()->typeProfiler()) {
        DeferGCForAWhile awhile(*this);
        vm()->typeProfilerLog()->processLogEntries(ASCIILiteral("GC"));
    }
    
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

    deleteOldCode(gcStartTime);
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
    rememberCurrentlyExecutingCodeBlocks();

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
    if (shouldDoFullCollection(collectionType)) {
        m_operationInProgress = FullCollection;
        m_slotVisitor.clearMarkStack();
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
}

void Heap::deleteOldCode(double gcStartTime)
{
    if (m_operationInProgress == EdenCollection)
        return;

    GCPHASE(DeleteOldCode);
    if (gcStartTime - m_lastCodeDiscardTime > minute) {
        deleteAllCompiledCode();
        m_lastCodeDiscardTime = WTF::monotonicallyIncreasingTime();
    }
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

void Heap::rememberCurrentlyExecutingCodeBlocks()
{
    GCPHASE(RememberCurrentlyExecutingCodeBlocks);
    m_codeBlocks.rememberCurrentlyExecutingCodeBlocks(this);
}

void Heap::resetAllocators()
{
    GCPHASE(ResetAllocators);
    m_objectSpace.resetAllocators();
}

void Heap::updateAllocationLimits()
{
    GCPHASE(UpdateAllocationLimits);
    size_t currentHeapSize = sizeAfterCollect();
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
        ASSERT(currentHeapSize >= m_sizeAfterLastCollect);
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        m_sizeAfterLastEdenCollect = currentHeapSize;
        double edenToOldGenerationRatio = (double)m_maxEdenSize / (double)m_maxHeapSize;
        double minEdenToOldGenerationRatio = 1.0 / 3.0;
        if (edenToOldGenerationRatio < minEdenToOldGenerationRatio)
            m_shouldDoFullCollection = true;
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
    if (m_operationInProgress == FullCollection)
        m_lastFullGCLength = gcEndTime - gcStartTime;
    else
        m_lastEdenGCLength = gcEndTime - gcStartTime;

    if (Options::recordGCPauseTimes())
        HeapStatistics::recordGCPauseTime(gcStartTime, gcEndTime);

    if (Options::useZombieMode())
        zombifyDeadObjects();

    if (Options::objectsAreImmortal())
        markDeadObjects();

    if (Options::showObjectStatistics())
        HeapStatistics::showObjectStatistics(this);

    if (Options::logGC() == GCLogging::Verbose)
        GCLogging::dumpObjectGraph(this);

    RELEASE_ASSERT(m_operationInProgress == EdenCollection || m_operationInProgress == FullCollection);
    m_operationInProgress = NoOperation;
    JAVASCRIPTCORE_GC_END();
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
    m_sweeper = WTF::move(sweeper);
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

void Heap::addCompiledCode(ExecutableBase* executable)
{
    m_compiledCode.append(executable);
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
    {
        SamplingRegion samplingRegion("Garbage Collection: Sweeping");
        m_objectSpace.zombifySweep();
    }
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachDeadCell<Zombify>(iterationScope);
}

void Heap::flushWriteBarrierBuffer(JSCell* cell)
{
#if ENABLE(GGC)
    m_writeBarrierBuffer.flush(*this);
    m_writeBarrierBuffer.add(cell);
#else
    UNUSED_PARAM(cell);
#endif
}

bool Heap::shouldDoFullCollection(HeapOperation requestedCollectionType) const
{
#if ENABLE(GGC)
    if (Options::alwaysDoFullCollection())
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
#else
    UNUSED_PARAM(requestedCollectionType);
    return true;
#endif
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
        WeakBlock::destroy(block);
    } else
        m_indexOfNextLogicallyEmptyWeakBlockToSweep++;

    if (m_indexOfNextLogicallyEmptyWeakBlockToSweep >= m_logicallyEmptyWeakBlocks.size()) {
        m_indexOfNextLogicallyEmptyWeakBlockToSweep = WTF::notFound;
        return false;
    }

    return true;
}

} // namespace JSC
