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
#include "DelayedReleaseScope.h"
#include "GCActivityCallback.h"
#include "GCIncomingRefCountedSetInlines.h"
#include "HeapIterationScope.h"
#include "HeapRootVisitor.h"
#include "HeapStatistics.h"
#include "IncrementalSweeper.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "JSCInlines.h"
#include "RecursiveAllocationScope.h"
#include "Tracing.h"
#include "UnlinkedCodeBlock.h"
#include "VM.h"
#include "WeakSetInlines.h"
#include <algorithm>
#include <wtf/RAMSize.h>
#include <wtf/CurrentTime.h>

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
        : m_time(0)
        , m_min(100000000)
        , m_max(0)
        , m_count(0)
        , m_name(name)
    {
    }
    ~GCTimer()
    {
        dataLogF("%s: %.2lfms (avg. %.2lf, min. %.2lf, max. %.2lf)\n", m_name, m_time * 1000, m_time * 1000 / m_count, m_min*1000, m_max*1000);
    }
    double m_time;
    double m_min;
    double m_max;
    size_t m_count;
    const char* m_name;
};

struct GCTimerScope {
    GCTimerScope(GCTimer* timer)
        : m_timer(timer)
        , m_start(WTF::monotonicallyIncreasingTime())
    {
    }
    ~GCTimerScope()
    {
        double delta = WTF::monotonicallyIncreasingTime() - m_start;
        if (delta < m_timer->m_min)
            m_timer->m_min = delta;
        if (delta > m_timer->m_max)
            m_timer->m_max = delta;
        m_timer->m_count++;
        m_timer->m_time += delta;
    }
    GCTimer* m_timer;
    double m_start;
};

struct GCCounter {
    GCCounter(const char* name)
        : m_name(name)
        , m_count(0)
        , m_total(0)
        , m_min(10000000)
        , m_max(0)
    {
    }
    
    void count(size_t amount)
    {
        m_count++;
        m_total += amount;
        if (amount < m_min)
            m_min = amount;
        if (amount > m_max)
            m_max = amount;
    }
    ~GCCounter()
    {
        dataLogF("%s: %zu values (avg. %zu, min. %zu, max. %zu)\n", m_name, m_total, m_total / m_count, m_min, m_max);
    }
    const char* m_name;
    size_t m_count;
    size_t m_total;
    size_t m_min;
    size_t m_max;
};

#define GCPHASE(name) DEFINE_GC_LOGGING_GLOBAL(GCTimer, name##Timer, (#name)); GCTimerScope name##TimerScope(&name##Timer)
#define COND_GCPHASE(cond, name1, name2) DEFINE_GC_LOGGING_GLOBAL(GCTimer, name1##Timer, (#name1)); DEFINE_GC_LOGGING_GLOBAL(GCTimer, name2##Timer, (#name2)); GCTimerScope name1##CondTimerScope(cond ? &name1##Timer : &name2##Timer)
#define GCCOUNTER(name, value) do { DEFINE_GC_LOGGING_GLOBAL(GCCounter, name##Counter, (#name)); name##Counter.count(value); } while (false)
    
#else

#define GCPHASE(name) do { } while (false)
#define COND_GCPHASE(cond, name1, name2) do { } while (false)
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
    if (vm->identifierTable != wtfThreadData().currentIdentifierTable())
        return false;

    if (vm->isSharedInstance() && !isValidSharedInstanceThreadState(vm))
        return false;

    return true;
}

struct MarkObject : public MarkedBlock::VoidFunctor {
    void operator()(JSCell* cell)
    {
        if (cell->isZapped())
            return;
        Heap::heap(cell)->setMarked(cell);
    }
};

struct Count : public MarkedBlock::CountFunctor {
    void operator()(JSCell*) { count(1); }
};

struct CountIfGlobalObject : MarkedBlock::CountFunctor {
    void operator()(JSCell* cell) {
        if (!cell->isObject())
            return;
        if (!asObject(cell)->isGlobalObject())
            return;
        count(1);
    }
};

class RecordType {
public:
    typedef PassOwnPtr<TypeCountSet> ReturnType;

    RecordType();
    void operator()(JSCell*);
    ReturnType returnValue();

private:
    const char* typeName(JSCell*);
    OwnPtr<TypeCountSet> m_typeCountSet;
};

inline RecordType::RecordType()
    : m_typeCountSet(adoptPtr(new TypeCountSet))
{
}

inline const char* RecordType::typeName(JSCell* cell)
{
    const ClassInfo* info = cell->classInfo();
    if (!info || !info->className)
        return "[unknown]";
    return info->className;
}

inline void RecordType::operator()(JSCell* cell)
{
    m_typeCountSet->add(typeName(cell));
}

inline PassOwnPtr<TypeCountSet> RecordType::returnValue()
{
    return m_typeCountSet.release();
}

} // anonymous namespace

Heap::Heap(VM* vm, HeapType heapType)
    : m_heapType(heapType)
    , m_ramSize(ramSize())
    , m_minBytesPerCycle(minHeapSize(m_heapType, m_ramSize))
    , m_sizeAfterLastCollect(0)
    , m_bytesAllocatedThisCycle(0)
    , m_bytesAbandonedThisCycle(0)
    , m_maxEdenSize(m_minBytesPerCycle)
    , m_maxHeapSize(m_minBytesPerCycle)
    , m_shouldDoFullCollection(false)
    , m_totalBytesVisited(0)
    , m_totalBytesCopied(0)
    , m_operationInProgress(NoOperation)
    , m_blockAllocator()
    , m_objectSpace(this)
    , m_storageSpace(this)
    , m_extraMemoryUsage(0)
    , m_machineThreads(this)
    , m_sharedData(vm)
    , m_slotVisitor(m_sharedData)
    , m_copyVisitor(m_sharedData)
    , m_handleSet(vm)
    , m_codeBlocks(m_blockAllocator)
    , m_isSafeToCollect(false)
    , m_writeBarrierBuffer(256)
    , m_vm(vm)
    , m_lastGCLength(0)
    , m_lastCodeDiscardTime(WTF::monotonicallyIncreasingTime())
    , m_activityCallback(DefaultGCActivityCallback::create(this))
    , m_sweeper(IncrementalSweeper::create(this))
    , m_deferralDepth(0)
{
    m_storageSpace.init();
}

Heap::~Heap()
{
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
}

void Heap::reportExtraMemoryCostSlowCase(size_t cost)
{
    // Our frequency of garbage collection tries to balance memory use against speed
    // by collecting based on the number of newly created values. However, for values
    // that hold on to a great deal of memory that's not in the form of other JS values,
    // that is not good enough - in some cases a lot of those objects can pile up and
    // use crazy amounts of memory without a GC happening. So we track these extra
    // memory costs. Only unusually large objects are noted, and we only keep track
    // of this extra cost until the next GC. In garbage collected languages, most values
    // are either very short lived temporaries, or have extremely long lifetimes. So
    // if a large value survives one garbage collection, there is not much point to
    // collecting more frequently as long as it stays alive.

    didAllocate(cost);
    collectIfNecessaryOrDefer();
}

void Heap::reportAbandonedObjectGraph()
{
    // Our clients don't know exactly how much memory they
    // are abandoning so we just guess for them.
    double abandonedBytes = 0.10 * m_sizeAfterLastCollect;

    // We want to accelerate the next collection. Because memory has just 
    // been abandoned, the next collection has the potential to 
    // be more profitable. Since allocation is the trigger for collection, 
    // we hasten the next collection by pretending that we've allocated more memory. 
    didAbandon(abandonedBytes);
}

void Heap::didAbandon(size_t bytes)
{
    if (m_activityCallback)
        m_activityCallback->didAllocate(m_bytesAllocatedThisCycle + m_bytesAbandonedThisCycle);
    m_bytesAbandonedThisCycle += bytes;
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

void Heap::markProtectedObjects(HeapRootVisitor& heapRootVisitor)
{
    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        heapRootVisitor.visit(&it->key);
}

void Heap::pushTempSortVector(Vector<ValueStringPair, 0, UnsafeVectorOverflow>* tempVector)
{
    m_tempSortingVectors.append(tempVector);
}

void Heap::popTempSortVector(Vector<ValueStringPair, 0, UnsafeVectorOverflow>* tempVector)
{
    ASSERT_UNUSED(tempVector, tempVector == m_tempSortingVectors.last());
    m_tempSortingVectors.removeLast();
}

void Heap::markTempSortVectors(HeapRootVisitor& heapRootVisitor)
{
    typedef Vector<Vector<ValueStringPair, 0, UnsafeVectorOverflow>* > VectorOfValueStringVectors;

    VectorOfValueStringVectors::iterator end = m_tempSortingVectors.end();
    for (VectorOfValueStringVectors::iterator it = m_tempSortingVectors.begin(); it != end; ++it) {
        Vector<ValueStringPair, 0, UnsafeVectorOverflow>* tempSortingVector = *it;

        Vector<ValueStringPair>::iterator vectorEnd = tempSortingVector->end();
        for (Vector<ValueStringPair>::iterator vectorIt = tempSortingVector->begin(); vectorIt != vectorEnd; ++vectorIt) {
            if (vectorIt->first)
                heapRootVisitor.visit(&vectorIt->first);
        }
    }
}

void Heap::harvestWeakReferences()
{
    m_slotVisitor.harvestWeakReferences();
}

void Heap::finalizeUnconditionalFinalizers()
{
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
        roots.add(registerRoots[i]);
    }
}

void Heap::markRoots()
{
    SamplingRegion samplingRegion("Garbage Collection: Tracing");

    GCPHASE(MarkRoots);
    ASSERT(isValidThreadState(m_vm));

#if ENABLE(OBJECT_MARK_LOGGING)
    double gcStartTime = WTF::monotonicallyIncreasingTime();
#endif

    m_codeBlocks.clearMarks();
    void* dummy;

    // We gather conservative roots before clearing mark bits because conservative
    // gathering uses the mark bits to determine whether a reference is valid.
    ConservativeRoots machineThreadRoots(&m_objectSpace.blocks(), &m_storageSpace);
    m_jitStubRoutines.clearMarks();
    {
        GCPHASE(GatherConservativeRoots);
        m_machineThreads.gatherConservativeRoots(machineThreadRoots, m_jitStubRoutines, m_codeBlocks, &dummy);
    }

#if ENABLE(LLINT_C_LOOP)
    ConservativeRoots stackRoots(&m_objectSpace.blocks(), &m_storageSpace);
    {
        GCPHASE(GatherStackRoots);
        stack().gatherConservativeRoots(stackRoots, m_jitStubRoutines, m_codeBlocks);
    }
#endif

    sanitizeStackForVM(m_vm);

#if ENABLE(DFG_JIT)
    ConservativeRoots scratchBufferRoots(&m_objectSpace.blocks(), &m_storageSpace);
    {
        GCPHASE(GatherScratchBufferRoots);
        m_vm->gatherConservativeRoots(scratchBufferRoots);
    }
#endif

    {
        GCPHASE(ClearLivenessData);
        m_objectSpace.clearNewlyAllocated();
        m_objectSpace.clearMarks();
    }

    m_sharedData.didStartMarking();
    SlotVisitor& visitor = m_slotVisitor;
    visitor.didStartMarking();
    HeapRootVisitor heapRootVisitor(visitor);

#if ENABLE(GGC)
    Vector<const JSCell*> rememberedSet(m_slotVisitor.markStack().size());
    m_slotVisitor.markStack().fillVector(rememberedSet);
#endif

    {
        ParallelModeEnabler enabler(visitor);

        m_vm->smallStrings.visitStrongReferences(visitor);

        {
            GCPHASE(VisitMachineRoots);
            MARK_LOG_ROOT(visitor, "C++ Stack");
            visitor.append(machineThreadRoots);
            visitor.donateAndDrain();
        }
#if ENABLE(LLINT_C_LOOP)
        {
            GCPHASE(VisitStackRoots);
            MARK_LOG_ROOT(visitor, "Stack");
            visitor.append(stackRoots);
            visitor.donateAndDrain();
        }
#endif
#if ENABLE(DFG_JIT)
        {
            GCPHASE(VisitScratchBufferRoots);
            MARK_LOG_ROOT(visitor, "Scratch Buffers");
            visitor.append(scratchBufferRoots);
            visitor.donateAndDrain();
        }
        {
            GCPHASE(VisitDFGWorklists);
            MARK_LOG_ROOT(visitor, "DFG Worklists");
            for (unsigned i = DFG::numberOfWorklists(); i--;) {
                if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i))
                    worklist->visitChildren(visitor, m_codeBlocks);
            }
        }
#endif
        {
            GCPHASE(VisitProtectedObjects);
            MARK_LOG_ROOT(visitor, "Protected Objects");
            markProtectedObjects(heapRootVisitor);
            visitor.donateAndDrain();
        }
        {
            GCPHASE(VisitTempSortVectors);
            MARK_LOG_ROOT(visitor, "Temp Sort Vectors");
            markTempSortVectors(heapRootVisitor);
            visitor.donateAndDrain();
        }

        {
            GCPHASE(MarkingArgumentBuffers);
            if (m_markListSet && m_markListSet->size()) {
                MARK_LOG_ROOT(visitor, "Argument Buffers");
                MarkedArgumentBuffer::markLists(heapRootVisitor, *m_markListSet);
                visitor.donateAndDrain();
            }
        }
        if (m_vm->exception()) {
            GCPHASE(MarkingException);
            MARK_LOG_ROOT(visitor, "Exceptions");
            heapRootVisitor.visit(m_vm->addressOfException());
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(VisitStrongHandles);
            MARK_LOG_ROOT(visitor, "Strong Handles");
            m_handleSet.visitStrongHandles(heapRootVisitor);
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(HandleStack);
            MARK_LOG_ROOT(visitor, "Handle Stack");
            m_handleStack.visit(heapRootVisitor);
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(TraceCodeBlocksAndJITStubRoutines);
            MARK_LOG_ROOT(visitor, "Trace Code Blocks and JIT Stub Routines");
            m_codeBlocks.traceMarked(visitor);
            m_jitStubRoutines.traceMarkedStubRoutines(visitor);
            visitor.donateAndDrain();
        }
    
#if ENABLE(PARALLEL_GC)
        {
            GCPHASE(Convergence);
            visitor.drainFromShared(SlotVisitor::MasterDrain);
        }
#endif
    }

    // Weak references must be marked last because their liveness depends on
    // the liveness of the rest of the object graph.
    {
        GCPHASE(VisitingLiveWeakHandles);
        MARK_LOG_ROOT(visitor, "Live Weak Handles");
        while (true) {
            m_objectSpace.visitWeakSets(heapRootVisitor);
            harvestWeakReferences();
            if (visitor.isEmpty())
                break;
            {
                ParallelModeEnabler enabler(visitor);
                visitor.donateAndDrain();
#if ENABLE(PARALLEL_GC)
                visitor.drainFromShared(SlotVisitor::MasterDrain);
#endif
            }
        }
    }

#if ENABLE(GGC)
    {
        GCPHASE(ClearRememberedSet);
        for (unsigned i = 0; i < rememberedSet.size(); ++i) {
            const JSCell* cell = rememberedSet[i];
            MarkedBlock::blockFor(cell)->clearRemembered(cell);
        }
    }
#endif

    GCCOUNTER(VisitedValueCount, visitor.visitCount());

    m_sharedData.didFinishMarking();
#if ENABLE(OBJECT_MARK_LOGGING)
    size_t visitCount = visitor.visitCount();
#if ENABLE(PARALLEL_GC)
    visitCount += m_sharedData.childVisitCount();
#endif
    MARK_LOG_MESSAGE2("\nNumber of live Objects after full GC %lu, took %.6f secs\n", visitCount, WTF::monotonicallyIncreasingTime() - gcStartTime);
#endif

    if (m_operationInProgress == EdenCollection) {
        m_totalBytesVisited += visitor.bytesVisited();
        m_totalBytesCopied += visitor.bytesCopied();
    } else {
        ASSERT(m_operationInProgress == FullCollection);
        m_totalBytesVisited = visitor.bytesVisited();
        m_totalBytesCopied = visitor.bytesCopied();
    }
#if ENABLE(PARALLEL_GC)
    m_totalBytesVisited += m_sharedData.childBytesVisited();
    m_totalBytesCopied += m_sharedData.childBytesCopied();
#endif

    visitor.reset();
#if ENABLE(PARALLEL_GC)
    m_sharedData.resetChildren();
#endif
    m_sharedData.reset();
}

template <HeapOperation collectionType>
void Heap::copyBackingStores()
{
    m_storageSpace.startedCopying<collectionType>();
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

size_t Heap::objectCount()
{
    return m_objectSpace.objectCount();
}

size_t Heap::extraSize()
{
    return m_extraMemoryUsage + m_arrayBuffers.size();
}

size_t Heap::size()
{
    return m_objectSpace.size() + m_storageSpace.size() + extraSize();
}

size_t Heap::capacity()
{
    return m_objectSpace.capacity() + m_storageSpace.capacity() + extraSize();
}

size_t Heap::sizeAfterCollect()
{
    // The result here may not agree with the normal Heap::size(). 
    // This is due to the fact that we only count live copied bytes
    // rather than all used (including dead) copied bytes, thus it's 
    // always the case that m_totalBytesCopied <= m_storageSpace.size(). 
    ASSERT(m_totalBytesCopied <= m_storageSpace.size());
    return m_totalBytesVisited + m_totalBytesCopied + extraSize();
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

PassOwnPtr<TypeCountSet> Heap::protectedObjectTypeCounts()
{
    return forEachProtectedCell<RecordType>();
}

PassOwnPtr<TypeCountSet> Heap::objectTypeCounts()
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
            if (worklist->isActive())
                return;
        }
    }
#endif // ENABLE(DFG_JIT)

    for (ExecutableBase* current = m_compiledCode.head(); current; current = current->next()) {
        if (!current->isFunctionExecutable())
            continue;
        static_cast<FunctionExecutable*>(current)->clearCodeIfNotCompiling();
    }

    m_codeBlocks.clearMarks();
    m_codeBlocks.deleteUnmarkedAndUnreferenced();
}

void Heap::deleteUnmarkedCompiledCode()
{
    ExecutableBase* next;
    for (ExecutableBase* current = m_compiledCode.head(); current; current = next) {
        next = current->next();
        if (isMarked(current))
            continue;

        // We do this because executable memory is limited on some platforms and because
        // CodeBlock requires eager finalization.
        ExecutableBase::clearCodeVirtual(current);
        m_compiledCode.remove(current);
    }

    m_codeBlocks.deleteUnmarkedAndUnreferenced();
    m_jitStubRoutines.deleteUnmarkedJettisonedStubRoutines();
}

void Heap::addToRememberedSet(const JSCell* cell)
{
    ASSERT(cell);
    ASSERT(!Options::enableConcurrentJIT() || !isCompilationThread());
    if (isInRememberedSet(cell))
        return;
    MarkedBlock::blockFor(cell)->setRemembered(cell);
    m_slotVisitor.unconditionallyAppend(const_cast<JSCell*>(cell));
}

void Heap::collectAllGarbage()
{
    if (!m_isSafeToCollect)
        return;

    m_shouldDoFullCollection = true;
    collect();

    SamplingRegion samplingRegion("Garbage Collection: Sweeping");
    DelayedReleaseScope delayedReleaseScope(m_objectSpace);
    m_objectSpace.sweep();
    m_objectSpace.shrink();
}

static double minute = 60.0;

void Heap::collect()
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
    
    RELEASE_ASSERT(!m_deferralDepth);
    GCPHASE(Collect);
    ASSERT(vm()->currentThreadIsHoldingAPILock());
    RELEASE_ASSERT(vm()->identifierTable == wtfThreadData().currentIdentifierTable());
    ASSERT(m_isSafeToCollect);
    JAVASCRIPTCORE_GC_BEGIN();
    RELEASE_ASSERT(m_operationInProgress == NoOperation);
    
#if ENABLE(DFG_JIT)
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i))
            worklist->suspendAllThreads();
    }
#endif

    bool isFullCollection = m_shouldDoFullCollection;
    if (isFullCollection) {
        m_operationInProgress = FullCollection;
        m_slotVisitor.clearMarkStack();
        m_shouldDoFullCollection = false;
        if (Options::logGC())
            dataLog("FullCollection, ");
    } else {
#if ENABLE(GGC)
        m_operationInProgress = EdenCollection;
        if (Options::logGC())
            dataLog("EdenCollection, ");
#else
        m_operationInProgress = FullCollection;
        m_slotVisitor.clearMarkStack();
        if (Options::logGC())
            dataLog("FullCollection, ");
#endif
    }
    if (m_operationInProgress == FullCollection)
        m_extraMemoryUsage = 0;

    if (m_activityCallback)
        m_activityCallback->willCollect();

    double lastGCStartTime = WTF::monotonicallyIncreasingTime();
    if (lastGCStartTime - m_lastCodeDiscardTime > minute) {
        deleteAllCompiledCode();
        m_lastCodeDiscardTime = WTF::monotonicallyIncreasingTime();
    }

    {
        GCPHASE(StopAllocation);
        m_objectSpace.stopAllocating();
        if (m_operationInProgress == FullCollection)
            m_storageSpace.didStartFullCollection();
    }

    {
        GCPHASE(FlushWriteBarrierBuffer);
        if (m_operationInProgress == EdenCollection)
            m_writeBarrierBuffer.flush(*this);
        else
            m_writeBarrierBuffer.reset();
    }

    markRoots();
    
    {
        GCPHASE(ReapingWeakHandles);
        m_objectSpace.reapWeakSets();
    }

    JAVASCRIPTCORE_GC_MARKED();
    
    {
        GCPHASE(SweepingArrayBuffers);
        m_arrayBuffers.sweep();
    }

    if (m_operationInProgress == FullCollection) {
        m_blockSnapshot.resize(m_objectSpace.blocks().set().size());
        MarkedBlockSnapshotFunctor functor(m_blockSnapshot);
        m_objectSpace.forEachBlock(functor);
    }

    if (m_operationInProgress == FullCollection)
        copyBackingStores<FullCollection>();
    else
        copyBackingStores<EdenCollection>();

    {
        GCPHASE(FinalizeUnconditionalFinalizers);
        finalizeUnconditionalFinalizers();
    }

    {
        GCPHASE(DeleteCodeBlocks);
        deleteUnmarkedCompiledCode();
    }

    {
        GCPHASE(DeleteSourceProviderCaches);
        m_vm->clearSourceProviderCaches();
    }

    if (m_operationInProgress == FullCollection)
        m_sweeper->startSweeping(m_blockSnapshot);

    {
        GCPHASE(AddCurrentlyExecutingCodeBlocksToRememberedSet);
        m_codeBlocks.rememberCurrentlyExecutingCodeBlocks(this);
    }

    m_bytesAbandonedThisCycle = 0;

    {
        GCPHASE(ResetAllocators);
        m_objectSpace.resetAllocators();
    }
    
    size_t currentHeapSize = sizeAfterCollect();
    if (Options::gcMaxHeapSize() && currentHeapSize > Options::gcMaxHeapSize())
        HeapStatistics::exitWithFailure();

    if (m_operationInProgress == FullCollection) {
        // To avoid pathological GC churn in very small and very large heaps, we set
        // the new allocation limit based on the current size of the heap, with a
        // fixed minimum.
        m_maxHeapSize = max(minHeapSize(m_heapType, m_ramSize), proportionalHeapSize(currentHeapSize, m_ramSize));
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
    } else {
        ASSERT(currentHeapSize >= m_sizeAfterLastCollect);
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
        double edenToOldGenerationRatio = (double)m_maxEdenSize / (double)m_maxHeapSize;
        double minEdenToOldGenerationRatio = 1.0 / 3.0;
        if (edenToOldGenerationRatio < minEdenToOldGenerationRatio)
            m_shouldDoFullCollection = true;
        m_maxHeapSize += currentHeapSize - m_sizeAfterLastCollect;
        m_maxEdenSize = m_maxHeapSize - currentHeapSize;
    }

    m_sizeAfterLastCollect = currentHeapSize;

    m_bytesAllocatedThisCycle = 0;
    double lastGCEndTime = WTF::monotonicallyIncreasingTime();
    m_lastGCLength = lastGCEndTime - lastGCStartTime;

    if (Options::recordGCPauseTimes())
        HeapStatistics::recordGCPauseTime(lastGCStartTime, lastGCEndTime);
    RELEASE_ASSERT(m_operationInProgress == EdenCollection || m_operationInProgress == FullCollection);

    m_operationInProgress = NoOperation;
    JAVASCRIPTCORE_GC_END();

    if (Options::useZombieMode())
        zombifyDeadObjects();

    if (Options::objectsAreImmortal())
        markDeadObjects();

    if (Options::showObjectStatistics())
        HeapStatistics::showObjectStatistics(this);
    
#if ENABLE(DFG_JIT)
    for (unsigned i = DFG::numberOfWorklists(); i--;) {
        if (DFG::Worklist* worklist = DFG::worklistForIndexOrNull(i))
            worklist->resumeAllThreads();
    }
#endif

    if (Options::logGC()) {
        double after = currentTimeMS();
        dataLog(after - before, " ms, ", currentHeapSize / 1024, " kb]\n");
    }
}

bool Heap::collectIfNecessaryOrDefer()
{
    if (isDeferred())
        return false;

    if (!shouldCollect())
        return false;

    collect();
    return true;
}

void Heap::markDeadObjects()
{
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachDeadCell<MarkObject>(iterationScope);
}

void Heap::setActivityCallback(PassOwnPtr<GCActivityCallback> activityCallback)
{
    m_activityCallback = activityCallback;
}

GCActivityCallback* Heap::activityCallback()
{
    return m_activityCallback.get();
}

void Heap::setIncrementalSweeper(PassOwnPtr<IncrementalSweeper> sweeper)
{
    m_sweeper = sweeper;
}

IncrementalSweeper* Heap::sweeper()
{
    return m_sweeper.get();
}

void Heap::setGarbageCollectionTimerEnabled(bool enable)
{
    if (m_activityCallback)
        m_activityCallback->setEnabled(enable);
}

void Heap::didAllocate(size_t bytes)
{
    if (m_activityCallback)
        m_activityCallback->didAllocate(m_bytesAllocatedThisCycle + m_bytesAbandonedThisCycle);
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

class Zombify : public MarkedBlock::VoidFunctor {
public:
    void operator()(JSCell* cell)
    {
        void** current = reinterpret_cast<void**>(cell);

        // We want to maintain zapped-ness because that's how we know if we've called 
        // the destructor.
        if (cell->isZapped())
            current++;

        void* limit = static_cast<void*>(reinterpret_cast<char*>(cell) + MarkedBlock::blockFor(cell)->cellSize());
        for (; current < limit; current++)
            *current = reinterpret_cast<void*>(0xbbadbeef);
    }
};

void Heap::zombifyDeadObjects()
{
    // Sweep now because destructors will crash once we're zombified.
    m_objectSpace.sweep();
    HeapIterationScope iterationScope(*this);
    m_objectSpace.forEachDeadCell<Zombify>(iterationScope);
}

void Heap::incrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth < 100); // Sanity check to make sure this doesn't get ridiculous.
    
    m_deferralDepth++;
}

void Heap::decrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth >= 1);
    
    m_deferralDepth--;
}

void Heap::decrementDeferralDepthAndGCIfNeeded()
{
    decrementDeferralDepth();
    collectIfNecessaryOrDefer();
}

void Heap::writeBarrier(const JSCell* from)
{
#if ENABLE(GGC)
    ASSERT_GC_OBJECT_LOOKS_VALID(const_cast<JSCell*>(from));
    if (!from || !isMarked(from))
        return;
    addToRememberedSet(from);
#else
    UNUSED_PARAM(from);
#endif
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

} // namespace JSC
