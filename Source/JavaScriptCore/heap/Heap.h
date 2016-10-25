/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2009, 2013-2016 Apple Inc. All rights reserved.
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

#include "ArrayBuffer.h"
#include "CollectionScope.h"
#include "GCIncomingRefCountedSet.h"
#include "HandleSet.h"
#include "HandleStack.h"
#include "HeapObserver.h"
#include "ListableHandler.h"
#include "MachineStackMarker.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "MarkedSpace.h"
#include "MutatorState.h"
#include "Options.h"
#include "SlotVisitor.h"
#include "StructureIDTable.h"
#include "TinyBloomFilter.h"
#include "UnconditionalFinalizer.h"
#include "WeakHandleOwner.h"
#include "WeakReferenceHarvester.h"
#include "WriteBarrierBuffer.h"
#include "WriteBarrierSupport.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/ParallelHelperPool.h>

namespace JSC {

class CodeBlock;
class CodeBlockSet;
class GCDeferralContext;
class EdenGCActivityCallback;
class ExecutableBase;
class FullGCActivityCallback;
class GCActivityCallback;
class GCAwareJITStubRoutine;
class Heap;
class HeapProfiler;
class HeapRootVisitor;
class HeapVerifier;
class HelpingGCScope;
class IncrementalSweeper;
class JITStubRoutine;
class JITStubRoutineSet;
class JSCell;
class JSValue;
class LLIntOffsetsExtractor;
class MarkedArgumentBuffer;
class VM;

namespace DFG {
class SpeculativeJIT;
class Worklist;
}

static void* const zombifiedBits = reinterpret_cast<void*>(static_cast<uintptr_t>(0xdeadbeef));

typedef HashCountedSet<JSCell*> ProtectCountSet;
typedef HashCountedSet<const char*> TypeCountSet;

enum HeapType { SmallHeap, LargeHeap };

class HeapUtil;

class Heap {
    WTF_MAKE_NONCOPYABLE(Heap);
public:
    friend class JIT;
    friend class DFG::SpeculativeJIT;
    static Heap* heap(const JSValue); // 0 for immediate values
    static Heap* heap(const HeapCell*);

    // This constant determines how many blocks we iterate between checks of our 
    // deadline when calling Heap::isPagedOut. Decreasing it will cause us to detect 
    // overstepping our deadline more quickly, while increasing it will cause 
    // our scan to run faster. 
    static const unsigned s_timeCheckResolution = 16;

    static bool isMarked(const void*);
    static bool isMarkedConcurrently(const void*);
    static bool testAndSetMarked(HeapVersion, const void*);
    
    static size_t cellSize(const void*);

    void writeBarrier(const JSCell* from);
    void writeBarrier(const JSCell* from, JSValue to);
    void writeBarrier(const JSCell* from, JSCell* to);
    
    void writeBarrierWithoutFence(const JSCell* from);
    
    // Take this if you know that from->cellState() < barrierThreshold.
    JS_EXPORT_PRIVATE void writeBarrierSlowPath(const JSCell* from);

    WriteBarrierBuffer& writeBarrierBuffer() { return m_writeBarrierBuffer; }
    void flushWriteBarrierBuffer(JSCell*);

    Heap(VM*, HeapType);
    ~Heap();
    void lastChanceToFinalize();
    void releaseDelayedReleasedObjects();

    VM* vm() const { return m_vm; }
    MarkedSpace& objectSpace() { return m_objectSpace; }
    MachineThreads& machineThreads() { return m_machineThreads; }

    const SlotVisitor& slotVisitor() const { return m_slotVisitor; }

    JS_EXPORT_PRIVATE GCActivityCallback* fullActivityCallback();
    JS_EXPORT_PRIVATE GCActivityCallback* edenActivityCallback();
    JS_EXPORT_PRIVATE void setFullActivityCallback(PassRefPtr<FullGCActivityCallback>);
    JS_EXPORT_PRIVATE void setEdenActivityCallback(PassRefPtr<EdenGCActivityCallback>);
    JS_EXPORT_PRIVATE void setGarbageCollectionTimerEnabled(bool);

    JS_EXPORT_PRIVATE IncrementalSweeper* sweeper();
    JS_EXPORT_PRIVATE void setIncrementalSweeper(std::unique_ptr<IncrementalSweeper>);

    void addObserver(HeapObserver* observer) { m_observers.append(observer); }
    void removeObserver(HeapObserver* observer) { m_observers.removeFirst(observer); }

    MutatorState mutatorState() const { return m_mutatorState; }
    Optional<CollectionScope> collectionScope() const { return m_collectionScope; }

    // We're always busy on the collection threads. On the main thread, this returns true if we're
    // helping heap.
    JS_EXPORT_PRIVATE bool isCurrentThreadBusy();
    
    MarkedSpace::Subspace& subspaceForObjectWithoutDestructor() { return m_objectSpace.subspaceForObjectsWithoutDestructor(); }
    MarkedSpace::Subspace& subspaceForObjectDestructor() { return m_objectSpace.subspaceForObjectsWithDestructor(); }
    MarkedSpace::Subspace& subspaceForAuxiliaryData() { return m_objectSpace.subspaceForAuxiliaryData(); }
    template<typename ClassType> MarkedSpace::Subspace& subspaceForObjectOfType();
    MarkedAllocator* allocatorForObjectWithoutDestructor(size_t bytes) { return m_objectSpace.allocatorFor(bytes); }
    MarkedAllocator* allocatorForObjectWithDestructor(size_t bytes) { return m_objectSpace.destructorAllocatorFor(bytes); }
    template<typename ClassType> MarkedAllocator* allocatorForObjectOfType(size_t bytes);
    MarkedAllocator* allocatorForAuxiliaryData(size_t bytes) { return m_objectSpace.auxiliaryAllocatorFor(bytes); }
    void* allocateAuxiliary(JSCell* intendedOwner, size_t);
    void* tryAllocateAuxiliary(JSCell* intendedOwner, size_t);
    void* tryAllocateAuxiliary(GCDeferralContext*, JSCell* intendedOwner, size_t);
    void* tryReallocateAuxiliary(JSCell* intendedOwner, void* oldBase, size_t oldSize, size_t newSize);
    void ascribeOwner(JSCell* intendedOwner, void*);

    typedef void (*Finalizer)(JSCell*);
    JS_EXPORT_PRIVATE void addFinalizer(JSCell*, Finalizer);
    void addExecutable(ExecutableBase*);

    void notifyIsSafeToCollect() { m_isSafeToCollect = true; }
    bool isSafeToCollect() const { return m_isSafeToCollect; }

    JS_EXPORT_PRIVATE bool isHeapSnapshotting() const;

    JS_EXPORT_PRIVATE void collectAllGarbageIfNotDoneRecently();
    JS_EXPORT_PRIVATE void collectAllGarbage();

    bool shouldCollect();
    JS_EXPORT_PRIVATE void collect(Optional<CollectionScope> = Nullopt);
    bool collectIfNecessaryOrDefer(GCDeferralContext* = nullptr); // Returns true if it did collect.
    void collectAccordingToDeferGCProbability();

    void completeAllJITPlans();
    
    // Use this API to report non-GC memory referenced by GC objects. Be sure to
    // call both of these functions: Calling only one may trigger catastropic
    // memory growth.
    void reportExtraMemoryAllocated(size_t);
    JS_EXPORT_PRIVATE void reportExtraMemoryVisited(CellState oldState, size_t);

#if ENABLE(RESOURCE_USAGE)
    // Use this API to report the subset of extra memory that lives outside this process.
    JS_EXPORT_PRIVATE void reportExternalMemoryVisited(CellState oldState, size_t);
    size_t externalMemorySize() { return m_externalMemorySize; }
#endif

    // Use this API to report non-GC memory if you can't use the better API above.
    void deprecatedReportExtraMemory(size_t);

    JS_EXPORT_PRIVATE void reportAbandonedObjectGraph();

    JS_EXPORT_PRIVATE void protect(JSValue);
    JS_EXPORT_PRIVATE bool unprotect(JSValue); // True when the protect count drops to 0.
    
    JS_EXPORT_PRIVATE size_t extraMemorySize(); // Non-GC memory referenced by GC objects.
    JS_EXPORT_PRIVATE size_t size();
    JS_EXPORT_PRIVATE size_t capacity();
    JS_EXPORT_PRIVATE size_t objectCount();
    JS_EXPORT_PRIVATE size_t globalObjectCount();
    JS_EXPORT_PRIVATE size_t protectedObjectCount();
    JS_EXPORT_PRIVATE size_t protectedGlobalObjectCount();
    JS_EXPORT_PRIVATE std::unique_ptr<TypeCountSet> protectedObjectTypeCounts();
    JS_EXPORT_PRIVATE std::unique_ptr<TypeCountSet> objectTypeCounts();

    HashSet<MarkedArgumentBuffer*>& markListSet();
    
    template<typename Functor> void forEachProtectedCell(const Functor&);
    template<typename Functor> void forEachCodeBlock(const Functor&);

    HandleSet* handleSet() { return &m_handleSet; }
    HandleStack* handleStack() { return &m_handleStack; }

    void willStartIterating();
    void didFinishIterating();

    double lastFullGCLength() const { return m_lastFullGCLength; }
    double lastEdenGCLength() const { return m_lastEdenGCLength; }
    void increaseLastFullGCLength(double amount) { m_lastFullGCLength += amount; }

    size_t sizeBeforeLastEdenCollection() const { return m_sizeBeforeLastEdenCollect; }
    size_t sizeAfterLastEdenCollection() const { return m_sizeAfterLastEdenCollect; }
    size_t sizeBeforeLastFullCollection() const { return m_sizeBeforeLastFullCollect; }
    size_t sizeAfterLastFullCollection() const { return m_sizeAfterLastFullCollect; }

    void deleteAllCodeBlocks();
    void deleteAllUnlinkedCodeBlocks();

    void didAllocate(size_t);
    bool isPagedOut(double deadline);
    
    const JITStubRoutineSet& jitStubRoutines() { return *m_jitStubRoutines; }
    
    void addReference(JSCell*, ArrayBuffer*);
    
    bool isDeferred() const { return !!m_deferralDepth || !Options::useGC(); }

    StructureIDTable& structureIDTable() { return m_structureIDTable; }

    CodeBlockSet& codeBlockSet() { return *m_codeBlocks; }

#if USE(FOUNDATION)
    template<typename T> void releaseSoon(RetainPtr<T>&&);
#endif

    static bool isZombified(JSCell* cell) { return *(void**)cell == zombifiedBits; }

    JS_EXPORT_PRIVATE void registerWeakGCMap(void* weakGCMap, std::function<void()> pruningCallback);
    JS_EXPORT_PRIVATE void unregisterWeakGCMap(void* weakGCMap);

    void addLogicallyEmptyWeakBlock(WeakBlock*);

#if ENABLE(RESOURCE_USAGE)
    size_t blockBytesAllocated() const { return m_blockBytesAllocated; }
#endif

    void didAllocateBlock(size_t capacity);
    void didFreeBlock(size_t capacity);
    
    bool barrierShouldBeFenced() const { return m_barrierShouldBeFenced; }
    const bool* addressOfBarrierShouldBeFenced() const { return &m_barrierShouldBeFenced; }
    
    unsigned barrierThreshold() const { return m_barrierThreshold; }
    const unsigned* addressOfBarrierThreshold() const { return &m_barrierThreshold; }

#if USE(CF)
    CFRunLoopRef runLoop() const { return m_runLoop.get(); }
#endif // USE(CF)

private:
    friend class AllocatingScope;
    friend class CodeBlock;
    friend class DeferGC;
    friend class DeferGCForAWhile;
    friend class GCAwareJITStubRoutine;
    friend class GCLogging;
    friend class GCThread;
    friend class HandleSet;
    friend class HeapUtil;
    friend class HeapVerifier;
    friend class HelpingGCScope;
    friend class JITStubRoutine;
    friend class LLIntOffsetsExtractor;
    friend class MarkedSpace;
    friend class MarkedAllocator;
    friend class MarkedBlock;
    friend class SlotVisitor;
    friend class IncrementalSweeper;
    friend class HeapStatistics;
    friend class VM;
    friend class WeakSet;
    template<typename T> friend void* allocateCell(Heap&);
    template<typename T> friend void* allocateCell(Heap&, size_t);
    template<typename T> friend void* allocateCell(Heap&, GCDeferralContext*);
    template<typename T> friend void* allocateCell(Heap&, GCDeferralContext*, size_t);

    void collectWithoutAnySweep(Optional<CollectionScope> = Nullopt);

    void* allocateWithDestructor(size_t); // For use with objects with destructors.
    void* allocateWithoutDestructor(size_t); // For use with objects without destructors.
    void* allocateWithDestructor(GCDeferralContext*, size_t);
    void* allocateWithoutDestructor(GCDeferralContext*, size_t);
    template<typename ClassType> void* allocateObjectOfType(size_t); // Chooses one of the methods above based on type.
    template<typename ClassType> void* allocateObjectOfType(GCDeferralContext*, size_t);

    static const size_t minExtraMemory = 256;
    
    class FinalizerOwner : public WeakHandleOwner {
        void finalize(Handle<Unknown>, void* context) override;
    };

    JS_EXPORT_PRIVATE bool isValidAllocation(size_t);
    JS_EXPORT_PRIVATE void reportExtraMemoryAllocatedSlowCase(size_t);
    JS_EXPORT_PRIVATE void deprecatedReportExtraMemorySlowCase(size_t);

    void collectImpl(Optional<CollectionScope>, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);

    void suspendCompilerThreads();
    void willStartCollection(Optional<CollectionScope>);
    void flushOldStructureIDTables();
    void flushWriteBarrierBuffer();
    void stopAllocation();
    void prepareForMarking();
    
    void markRoots(double gcStartTime, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);
    void gatherStackRoots(ConservativeRoots&, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);
    void gatherJSStackRoots(ConservativeRoots&);
    void gatherScratchBufferRoots(ConservativeRoots&);
    void beginMarking();
    void visitExternalRememberedSet();
    void visitSmallStrings();
    void visitConservativeRoots(ConservativeRoots&);
    void visitCompilerWorklistWeakReferences();
    void removeDeadCompilerWorklistEntries();
    void visitProtectedObjects(HeapRootVisitor&);
    void visitArgumentBuffers(HeapRootVisitor&);
    void visitException(HeapRootVisitor&);
    void visitStrongHandles(HeapRootVisitor&);
    void visitHandleStack(HeapRootVisitor&);
    void visitSamplingProfiler();
    void visitShadowChicken();
    void traceCodeBlocksAndJITStubRoutines();
    void visitWeakHandles(HeapRootVisitor&);
    void updateObjectCounts(double gcStartTime);
    void endMarking();

    void reapWeakHandles();
    void pruneStaleEntriesFromWeakGCMaps();
    void sweepArrayBuffers();
    void snapshotUnswept();
    void deleteSourceProviderCaches();
    void notifyIncrementalSweeper();
    void prepareForAllocation();
    void harvestWeakReferences();
    void finalizeUnconditionalFinalizers();
    void clearUnmarkedExecutables();
    void deleteUnmarkedCompiledCode();
    JS_EXPORT_PRIVATE void addToRememberedSet(const JSCell*);
    void updateAllocationLimits();
    void didFinishCollection(double gcStartTime);
    void resumeCompilerThreads();
    void zombifyDeadObjects();
    void gatherExtraHeapSnapshotData(HeapProfiler&);
    void removeDeadHeapSnapshotNodes(HeapProfiler&);
    void sweepLargeAllocations();
    
    void sweepAllLogicallyEmptyWeakBlocks();
    bool sweepNextLogicallyEmptyWeakBlock();

    bool shouldDoFullCollection(Optional<CollectionScope> requestedCollectionScope) const;

    void incrementDeferralDepth();
    void decrementDeferralDepth();
    JS_EXPORT_PRIVATE void decrementDeferralDepthAndGCIfNeeded();

    size_t threadVisitCount();
    size_t threadBytesVisited();
    
    void forEachCodeBlockImpl(const ScopedLambda<bool(CodeBlock*)>&);

    const HeapType m_heapType;
    const size_t m_ramSize;
    const size_t m_minBytesPerCycle;
    size_t m_sizeAfterLastCollect;
    size_t m_sizeAfterLastFullCollect;
    size_t m_sizeBeforeLastFullCollect;
    size_t m_sizeAfterLastEdenCollect;
    size_t m_sizeBeforeLastEdenCollect;

    size_t m_bytesAllocatedThisCycle;
    size_t m_bytesAbandonedSinceLastFullCollect;
    size_t m_maxEdenSize;
    size_t m_maxHeapSize;
    bool m_shouldDoFullCollection;
    size_t m_totalBytesVisited;
    size_t m_totalBytesVisitedThisCycle;
    
    Optional<CollectionScope> m_collectionScope;
    MutatorState m_mutatorState { MutatorState::Running };
    StructureIDTable m_structureIDTable;
    MarkedSpace m_objectSpace;
    GCIncomingRefCountedSet<ArrayBuffer> m_arrayBuffers;
    size_t m_extraMemorySize;
    size_t m_deprecatedExtraMemorySize;

    HashSet<const JSCell*> m_copyingRememberedSet;

    ProtectCountSet m_protectedValues;
    std::unique_ptr<HashSet<MarkedArgumentBuffer*>> m_markListSet;

    MachineThreads m_machineThreads;
    
    SlotVisitor m_slotVisitor;

    // We pool the slot visitors used by parallel marking threads. It's useful to be able to
    // enumerate over them, and it's useful to have them cache some small amount of memory from
    // one GC to the next. GC marking threads claim these at the start of marking, and return
    // them at the end.
    Vector<std::unique_ptr<SlotVisitor>> m_parallelSlotVisitors;
    Vector<SlotVisitor*> m_availableParallelSlotVisitors;
    Lock m_parallelSlotVisitorLock;

    HandleSet m_handleSet;
    HandleStack m_handleStack;
    std::unique_ptr<CodeBlockSet> m_codeBlocks;
    std::unique_ptr<JITStubRoutineSet> m_jitStubRoutines;
    FinalizerOwner m_finalizerOwner;
    
    bool m_isSafeToCollect;

    WriteBarrierBuffer m_writeBarrierBuffer;
    bool m_barrierShouldBeFenced { Options::forceFencedBarrier() };
    unsigned m_barrierThreshold { Options::forceFencedBarrier() ? tautologicalThreshold : blackThreshold };

    VM* m_vm;
    double m_lastFullGCLength;
    double m_lastEdenGCLength;

    Vector<ExecutableBase*> m_executables;

    Vector<WeakBlock*> m_logicallyEmptyWeakBlocks;
    size_t m_indexOfNextLogicallyEmptyWeakBlockToSweep { WTF::notFound };
    
#if USE(CF)
    RetainPtr<CFRunLoopRef> m_runLoop;
#endif // USE(CF)
    RefPtr<FullGCActivityCallback> m_fullActivityCallback;
    RefPtr<GCActivityCallback> m_edenActivityCallback;
    std::unique_ptr<IncrementalSweeper> m_sweeper;

    Vector<HeapObserver*> m_observers;

    unsigned m_deferralDepth;
    Vector<DFG::Worklist*> m_suspendedCompilerWorklists;

    std::unique_ptr<HeapVerifier> m_verifier;

#if USE(FOUNDATION)
    Vector<RetainPtr<CFTypeRef>> m_delayedReleaseObjects;
    unsigned m_delayedReleaseRecursionCount;
#endif

    HashMap<void*, std::function<void()>> m_weakGCMaps;

    Lock m_markingMutex;
    Condition m_markingConditionVariable;
    MarkStackArray m_sharedMarkStack;
    unsigned m_numberOfActiveParallelMarkers { 0 };
    unsigned m_numberOfWaitingParallelMarkers { 0 };
    bool m_parallelMarkersShouldExit { false };

    Lock m_opaqueRootsMutex;
    HashSet<void*> m_opaqueRoots;

    static const size_t s_blockFragmentLength = 32;

    ListableHandler<WeakReferenceHarvester>::List m_weakReferenceHarvesters;
    ListableHandler<UnconditionalFinalizer>::List m_unconditionalFinalizers;

    ParallelHelperClient m_helperClient;

#if ENABLE(RESOURCE_USAGE)
    size_t m_blockBytesAllocated { 0 };
    size_t m_externalMemorySize { 0 };
#endif
};

} // namespace JSC
