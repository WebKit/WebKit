/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
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
#include "GCActivityCallback.h"
#include "HeapRootVisitor.h"
#include "Interpreter.h"
#include "JSGlobalData.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "Tracing.h"
#include <algorithm>
#include <wtf/CurrentTime.h>


using namespace std;
using namespace JSC;

namespace JSC {

namespace { 

static const size_t largeHeapSize = 16 * 1024 * 1024;
static const size_t smallHeapSize = 512 * 1024;

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
        printf("%s: %.2lfms (avg. %.2lf, min. %.2lf, max. %.2lf)\n", m_name, m_time * 1000, m_time * 1000 / m_count, m_min*1000, m_max*1000);
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
        , m_start(WTF::currentTime())
    {
    }
    ~GCTimerScope()
    {
        double delta = WTF::currentTime() - m_start;
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
        printf("%s: %zu values (avg. %zu, min. %zu, max. %zu)\n", m_name, m_total, m_total / m_count, m_min, m_max);
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

static size_t heapSizeForHint(HeapSize heapSize)
{
#if ENABLE(LARGE_HEAP)
    if (heapSize == LargeHeap)
        return largeHeapSize;
    ASSERT(heapSize == SmallHeap);
    return smallHeapSize;
#else
    ASSERT_UNUSED(heapSize, heapSize == LargeHeap || heapSize == SmallHeap);
    return smallHeapSize;
#endif
}

static inline bool isValidSharedInstanceThreadState()
{
    if (!JSLock::lockCount())
        return false;

    if (!JSLock::currentThreadIsHoldingLock())
        return false;

    return true;
}

static inline bool isValidThreadState(JSGlobalData* globalData)
{
    if (globalData->identifierTable != wtfThreadData().currentIdentifierTable())
        return false;

    if (globalData->isSharedInstance() && !isValidSharedInstanceThreadState())
        return false;

    return true;
}

class CountFunctor {
public:
    typedef size_t ReturnType;

    CountFunctor();
    void count(size_t);
    ReturnType returnValue();

private:
    ReturnType m_count;
};

inline CountFunctor::CountFunctor()
    : m_count(0)
{
}

inline void CountFunctor::count(size_t count)
{
    m_count += count;
}

inline CountFunctor::ReturnType CountFunctor::returnValue()
{
    return m_count;
}

struct ClearMarks : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock*);
};

inline void ClearMarks::operator()(MarkedBlock* block)
{
    block->clearMarks();
}

struct Sweep : MarkedBlock::VoidFunctor {
    void operator()(MarkedBlock*);
};

inline void Sweep::operator()(MarkedBlock* block)
{
    block->sweep();
}

struct MarkCount : CountFunctor {
    void operator()(MarkedBlock*);
};

inline void MarkCount::operator()(MarkedBlock* block)
{
    count(block->markCount());
}

struct Size : CountFunctor {
    void operator()(MarkedBlock*);
};

inline void Size::operator()(MarkedBlock* block)
{
    count(block->markCount() * block->cellSize());
}

struct Capacity : CountFunctor {
    void operator()(MarkedBlock*);
};

inline void Capacity::operator()(MarkedBlock* block)
{
    count(block->capacity());
}

struct Count : public CountFunctor {
    void operator()(JSCell*);
};

inline void Count::operator()(JSCell*)
{
    count(1);
}

struct CountIfGlobalObject : CountFunctor {
    void operator()(JSCell*);
};

inline void CountIfGlobalObject::operator()(JSCell* cell)
{
    if (!cell->isObject())
        return;
    if (!asObject(cell)->isGlobalObject())
        return;
    count(1);
}

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

Heap::Heap(JSGlobalData* globalData, HeapSize heapSize)
    : m_heapSize(heapSize)
    , m_minBytesPerCycle(heapSizeForHint(heapSize))
    , m_lastFullGCSize(0)
    , m_operationInProgress(NoOperation)
    , m_objectSpace(this)
    , m_blockFreeingThreadShouldQuit(false)
    , m_extraCost(0)
    , m_markListSet(0)
    , m_activityCallback(DefaultGCActivityCallback::create(this))
    , m_machineThreads(this)
    , m_sharedData(globalData)
    , m_slotVisitor(m_sharedData, globalData->jsArrayVPtr, globalData->jsFinalObjectVPtr, globalData->jsStringVPtr)
    , m_handleHeap(globalData)
    , m_isSafeToCollect(false)
    , m_globalData(globalData)
{
    m_objectSpace.setHighWaterMark(m_minBytesPerCycle);
    (*m_activityCallback)();
    m_numberOfFreeBlocks = 0;
    m_blockFreeingThread = createThread(blockFreeingThreadStartFunc, this, "JavaScriptCore::BlockFree");
    
    ASSERT(m_blockFreeingThread);
}

Heap::~Heap()
{
    // Destroy our block freeing thread.
    {
        MutexLocker locker(m_freeBlockLock);
        m_blockFreeingThreadShouldQuit = true;
        m_freeBlockCondition.broadcast();
    }
    waitForThreadCompletion(m_blockFreeingThread, 0);

    // The destroy function must already have been called, so assert this.
    ASSERT(!m_globalData);
}

void Heap::destroy()
{
    JSLock lock(SilenceAssertionsOnly);

    if (!m_globalData)
        return;

    ASSERT(!m_globalData->dynamicGlobalObject);
    ASSERT(m_operationInProgress == NoOperation);
    
    // The global object is not GC protected at this point, so sweeping may delete it
    // (and thus the global data) before other objects that may use the global data.
    RefPtr<JSGlobalData> protect(m_globalData);

#if ENABLE(JIT)
    m_globalData->jitStubs->clearHostFunctionStubs();
#endif

    delete m_markListSet;
    m_markListSet = 0;

    canonicalizeCellLivenessData();
    clearMarks();

    m_handleHeap.finalizeWeakHandles();
    m_globalData->smallStrings.finalizeSmallStrings();
    shrink();
    ASSERT(!size());
    
#if ENABLE(SIMPLE_HEAP_PROFILING)
    m_slotVisitor.m_visitedTypeCounts.dump(stderr, "Visited Type Counts");
    m_destroyedTypeCounts.dump(stderr, "Destroyed Type Counts");
#endif
    
    releaseFreeBlocks();

    m_globalData = 0;
}

void Heap::waitForRelativeTimeWhileHoldingLock(double relative)
{
    if (m_blockFreeingThreadShouldQuit)
        return;
    m_freeBlockCondition.timedWait(m_freeBlockLock, currentTime() + relative);
}

void Heap::waitForRelativeTime(double relative)
{
    // If this returns early, that's fine, so long as it doesn't do it too
    // frequently. It would only be a bug if this function failed to return
    // when it was asked to do so.
    
    MutexLocker locker(m_freeBlockLock);
    waitForRelativeTimeWhileHoldingLock(relative);
}

void* Heap::blockFreeingThreadStartFunc(void* heap)
{
    static_cast<Heap*>(heap)->blockFreeingThreadMain();
    return 0;
}

void Heap::blockFreeingThreadMain()
{
    while (!m_blockFreeingThreadShouldQuit) {
        // Generally wait for one second before scavenging free blocks. This
        // may return early, particularly when we're being asked to quit.
        waitForRelativeTime(1.0);
        if (m_blockFreeingThreadShouldQuit)
            break;
        
        // Now process the list of free blocks. Keep freeing until half of the
        // blocks that are currently on the list are gone. Assume that a size_t
        // field can be accessed atomically.
        size_t currentNumberOfFreeBlocks = m_numberOfFreeBlocks;
        if (!currentNumberOfFreeBlocks)
            continue;
        
        size_t desiredNumberOfFreeBlocks = currentNumberOfFreeBlocks / 2;
        
        while (!m_blockFreeingThreadShouldQuit) {
            MarkedBlock* block;
            {
                MutexLocker locker(m_freeBlockLock);
                if (m_numberOfFreeBlocks <= desiredNumberOfFreeBlocks)
                    block = 0;
                else {
                    block = m_freeBlocks.removeHead();
                    ASSERT(block);
                    m_numberOfFreeBlocks--;
                }
            }
            
            if (!block)
                break;
            
            MarkedBlock::destroy(block);
        }
    }
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

    if (m_extraCost > maxExtraCost && m_extraCost > m_objectSpace.highWaterMark() / 2)
        collectAllGarbage();
    m_extraCost += cost;
}

void Heap::protect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance());

    if (!k.isCell())
        return;

    m_protectedValues.add(k.asCell());
}

bool Heap::unprotect(JSValue k)
{
    ASSERT(k);
    ASSERT(JSLock::currentThreadIsHoldingLock() || !m_globalData->isSharedInstance());

    if (!k.isCell())
        return false;

    return m_protectedValues.remove(k.asCell());
}

void Heap::jettisonDFGCodeBlock(PassOwnPtr<CodeBlock> codeBlock)
{
    m_dfgCodeBlocks.jettison(codeBlock);
}

void Heap::markProtectedObjects(HeapRootVisitor& heapRootVisitor)
{
    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        heapRootVisitor.visit(&it->first);
}

void Heap::pushTempSortVector(Vector<ValueStringPair>* tempVector)
{
    m_tempSortingVectors.append(tempVector);
}

void Heap::popTempSortVector(Vector<ValueStringPair>* tempVector)
{
    ASSERT_UNUSED(tempVector, tempVector == m_tempSortingVectors.last());
    m_tempSortingVectors.removeLast();
}
    
void Heap::markTempSortVectors(HeapRootVisitor& heapRootVisitor)
{
    typedef Vector<Vector<ValueStringPair>* > VectorOfValueStringVectors;

    VectorOfValueStringVectors::iterator end = m_tempSortingVectors.end();
    for (VectorOfValueStringVectors::iterator it = m_tempSortingVectors.begin(); it != end; ++it) {
        Vector<ValueStringPair>* tempSortingVector = *it;

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

inline RegisterFile& Heap::registerFile()
{
    return m_globalData->interpreter->registerFile();
}

void Heap::getConservativeRegisterRoots(HashSet<JSCell*>& roots)
{
    ASSERT(isValidThreadState(m_globalData));
    if (m_operationInProgress != NoOperation)
        CRASH();
    m_operationInProgress = Collection;
    ConservativeRoots registerFileRoots(&m_objectSpace.blocks());
    registerFile().gatherConservativeRoots(registerFileRoots);
    size_t registerFileRootCount = registerFileRoots.size();
    JSCell** registerRoots = registerFileRoots.roots();
    for (size_t i = 0; i < registerFileRootCount; i++) {
        setMarked(registerRoots[i]);
        roots.add(registerRoots[i]);
    }
    m_operationInProgress = NoOperation;
}

void Heap::markRoots(bool fullGC)
{
    SamplingRegion samplingRegion("Garbage Collection: Tracing");

    COND_GCPHASE(fullGC, MarkFullRoots, MarkYoungRoots);
    UNUSED_PARAM(fullGC);
    ASSERT(isValidThreadState(m_globalData));
    if (m_operationInProgress != NoOperation)
        CRASH();
    m_operationInProgress = Collection;

    void* dummy;
    
    // We gather conservative roots before clearing mark bits because conservative
    // gathering uses the mark bits to determine whether a reference is valid.
    ConservativeRoots machineThreadRoots(&m_objectSpace.blocks());
    {
        GCPHASE(GatherConservativeRoots);
        m_machineThreads.gatherConservativeRoots(machineThreadRoots, &dummy);
    }

    ConservativeRoots registerFileRoots(&m_objectSpace.blocks());
    m_dfgCodeBlocks.clearMarks();
    {
        GCPHASE(GatherRegisterFileRoots);
        registerFile().gatherConservativeRoots(registerFileRoots, m_dfgCodeBlocks);
    }
#if ENABLE(GGC)
    MarkedBlock::DirtyCellVector dirtyCells;
    if (!fullGC) {
        GCPHASE(GatheringDirtyCells);
        m_objectSpace.gatherDirtyCells(dirtyCells);
    } else
#endif
    {
        GCPHASE(clearMarks);
        clearMarks();
    }

    SlotVisitor& visitor = m_slotVisitor;
    HeapRootVisitor heapRootVisitor(visitor);

    {
        ParallelModeEnabler enabler(visitor);
#if ENABLE(GGC)
        {
            size_t dirtyCellCount = dirtyCells.size();
            GCPHASE(VisitDirtyCells);
            GCCOUNTER(DirtyCellCount, dirtyCellCount);
            for (size_t i = 0; i < dirtyCellCount; i++) {
                heapRootVisitor.visitChildren(dirtyCells[i]);
                visitor.donateAndDrain();
            }
        }
#endif
    
        if (m_globalData->codeBlocksBeingCompiled.size()) {
            GCPHASE(VisitActiveCodeBlock);
            for (size_t i = 0; i < m_globalData->codeBlocksBeingCompiled.size(); i++)
                m_globalData->codeBlocksBeingCompiled[i]->visitAggregate(visitor);
        }
    
        {
            GCPHASE(VisitMachineRoots);
            visitor.append(machineThreadRoots);
            visitor.donateAndDrain();
        }
        {
            GCPHASE(VisitRegisterFileRoots);
            visitor.append(registerFileRoots);
            visitor.donateAndDrain();
        }
        {
            GCPHASE(VisitProtectedObjects);
            markProtectedObjects(heapRootVisitor);
            visitor.donateAndDrain();
        }
        {
            GCPHASE(VisitTempSortVectors);
            markTempSortVectors(heapRootVisitor);
            visitor.donateAndDrain();
        }

        {
            GCPHASE(MarkingArgumentBuffers);
            if (m_markListSet && m_markListSet->size()) {
                MarkedArgumentBuffer::markLists(heapRootVisitor, *m_markListSet);
                visitor.donateAndDrain();
            }
        }
        if (m_globalData->exception) {
            GCPHASE(MarkingException);
            heapRootVisitor.visit(&m_globalData->exception);
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(VisitStrongHandles);
            m_handleHeap.visitStrongHandles(heapRootVisitor);
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(HandleStack);
            m_handleStack.visit(heapRootVisitor);
            visitor.donateAndDrain();
        }
    
        {
            GCPHASE(TraceCodeBlocks);
            m_dfgCodeBlocks.traceMarkedCodeBlocks(visitor);
            visitor.donateAndDrain();
        }
    
#if ENABLE(PARALLEL_GC)
        {
            GCPHASE(Convergence);
            visitor.drainFromShared(SlotVisitor::MasterDrain);
        }
#endif
    }

    // Weak handles must be marked last, because their owners use the set of
    // opaque roots to determine reachability.
    {
        GCPHASE(VisitingWeakHandles);
        while (true) {
            m_handleHeap.visitWeakHandles(heapRootVisitor);
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
    GCCOUNTER(VisitedValueCount, visitor.visitCount());

    visitor.reset();
    m_sharedData.reset();

    m_operationInProgress = NoOperation;
}

void Heap::clearMarks()
{
    m_objectSpace.forEachBlock<ClearMarks>();
}

void Heap::sweep()
{
    m_objectSpace.forEachBlock<Sweep>();
}

size_t Heap::objectCount()
{
    return m_objectSpace.forEachBlock<MarkCount>();
}

size_t Heap::size()
{
    return m_objectSpace.forEachBlock<Size>();
}

size_t Heap::capacity()
{
    return m_objectSpace.forEachBlock<Capacity>();
}

size_t Heap::protectedGlobalObjectCount()
{
    return forEachProtectedCell<CountIfGlobalObject>();
}

size_t Heap::globalObjectCount()
{
    return m_objectSpace.forEachCell<CountIfGlobalObject>();
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
    return m_objectSpace.forEachCell<RecordType>();
}

void Heap::collectAllGarbage()
{
    if (!m_isSafeToCollect)
        return;
    if (!m_globalData->dynamicGlobalObject)
        m_globalData->recompileAllJSFunctions();

    collect(DoSweep);
}

void Heap::collect(SweepToggle sweepToggle)
{
    SamplingRegion samplingRegion("Garbage Collection");
    
    GCPHASE(Collect);
    ASSERT(globalData()->identifierTable == wtfThreadData().currentIdentifierTable());
    ASSERT(m_isSafeToCollect);
    JAVASCRIPTCORE_GC_BEGIN();
#if ENABLE(GGC)
    bool fullGC = sweepToggle == DoSweep;
    if (!fullGC)
        fullGC = (capacity() > 4 * m_lastFullGCSize);  
#else
    bool fullGC = true;
#endif
    {
        GCPHASE(Canonicalize);
        canonicalizeCellLivenessData();
    }

    markRoots(fullGC);
    
    {
        GCPHASE(FinalizeUnconditionalFinalizers);
        finalizeUnconditionalFinalizers();
    }
        
    {
        GCPHASE(FinalizeWeakHandles);
        m_handleHeap.finalizeWeakHandles();
        m_globalData->smallStrings.finalizeSmallStrings();
    }
    
    JAVASCRIPTCORE_GC_MARKED();

    {
        GCPHASE(ResetAllocator);
        resetAllocator();
    }
    
    {
        GCPHASE(DeleteCodeBlocks);
        m_dfgCodeBlocks.deleteUnmarkedJettisonedCodeBlocks();
    }

    if (sweepToggle == DoSweep) {
        SamplingRegion samplingRegion("Garbage Collection: Sweeping");
        GCPHASE(Sweeping);
        sweep();
        shrink();
    }

    // To avoid pathological GC churn in large heaps, we set the allocation high
    // water mark to be proportional to the current size of the heap. The exact
    // proportion is a bit arbitrary. A 2X multiplier gives a 1:1 (heap size :
    // new bytes allocated) proportion, and seems to work well in benchmarks.
    size_t newSize = size();
    size_t proportionalBytes = 2 * newSize;
    if (fullGC) {
        m_lastFullGCSize = newSize;
        m_objectSpace.setHighWaterMark(max(proportionalBytes, m_minBytesPerCycle));
    }
    JAVASCRIPTCORE_GC_END();

    (*m_activityCallback)();
}

void Heap::canonicalizeCellLivenessData()
{
    m_objectSpace.canonicalizeCellLivenessData();
}

void Heap::resetAllocator()
{
    m_extraCost = 0;
    m_objectSpace.resetAllocator();
}

void Heap::setActivityCallback(PassOwnPtr<GCActivityCallback> activityCallback)
{
    m_activityCallback = activityCallback;
}

GCActivityCallback* Heap::activityCallback()
{
    return m_activityCallback.get();
}

bool Heap::isValidAllocation(size_t bytes)
{
    if (!isValidThreadState(m_globalData))
        return false;

    if (bytes > MarkedSpace::maxCellSize)
        return false;

    if (m_operationInProgress != NoOperation)
        return false;
    
    return true;
}

void Heap::freeBlocks(MarkedBlock* head)
{
    m_objectSpace.freeBlocks(head);
}

void Heap::shrink()
{
    m_objectSpace.shrink();
}

void Heap::releaseFreeBlocks()
{
    while (true) {
        MarkedBlock* block;
        {
            MutexLocker locker(m_freeBlockLock);
            if (!m_numberOfFreeBlocks)
                block = 0;
            else {
                block = m_freeBlocks.removeHead();
                ASSERT(block);
                m_numberOfFreeBlocks--;
            }
        }
        
        if (!block)
            break;
        
        MarkedBlock::destroy(block);
    }
}

void Heap::addFinalizer(JSCell* cell, Finalizer finalizer)
{
    Weak<JSCell> weak(*globalData(), cell, &m_finalizerOwner, reinterpret_cast<void*>(finalizer));
    weak.leakHandle(); // Balanced by FinalizerOwner::finalize().
}

void Heap::FinalizerOwner::finalize(Handle<Unknown> handle, void* context)
{
    Weak<JSCell> weak(Weak<JSCell>::Adopt, handle);
    Finalizer finalizer = reinterpret_cast<Finalizer>(context);
    finalizer(weak.get());
}

} // namespace JSC
