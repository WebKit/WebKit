/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2009, 2013-2015 Apple Inc. All rights reserved.
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

#ifndef Heap_h
#define Heap_h

#include "ArrayBuffer.h"
#include "CodeBlockSet.h"
#include "CopyVisitor.h"
#include "GCIncomingRefCountedSet.h"
#include "HandleSet.h"
#include "HandleStack.h"
#include "HeapObserver.h"
#include "HeapOperation.h"
#include "JITStubRoutineSet.h"
#include "ListableHandler.h"
#include "MachineStackMarker.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "MarkedSpace.h"
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
class CopiedSpace;
class EdenGCActivityCallback;
class ExecutableBase;
class FullGCActivityCallback;
class GCActivityCallback;
class GCAwareJITStubRoutine;
class Heap;
class HeapRootVisitor;
class HeapVerifier;
class IncrementalSweeper;
class JITStubRoutine;
class JSCell;
class JSStack;
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

class Heap {
    WTF_MAKE_NONCOPYABLE(Heap);
public:
    friend class JIT;
    friend class DFG::SpeculativeJIT;
    static Heap* heap(const JSValue); // 0 for immediate values
    static Heap* heap(const JSCell*);

    // This constant determines how many blocks we iterate between checks of our 
    // deadline when calling Heap::isPagedOut. Decreasing it will cause us to detect 
    // overstepping our deadline more quickly, while increasing it will cause 
    // our scan to run faster. 
    static const unsigned s_timeCheckResolution = 16;

    static bool isLive(const void*);
    static bool isMarked(const void*);
    static bool testAndSetMarked(const void*);
    static void setMarked(const void*);

    // This function must be run after stopAllocation() is called and 
    // before liveness data is cleared to be accurate.
    static bool isPointerGCObject(TinyBloomFilter, MarkedBlockSet&, void* pointer);
    static bool isValueGCObject(TinyBloomFilter, MarkedBlockSet&, JSValue);

    void writeBarrier(const JSCell*);
    void writeBarrier(const JSCell*, JSValue);
    void writeBarrier(const JSCell*, JSCell*);

    JS_EXPORT_PRIVATE static void* copyBarrier(const JSCell* owner, void*& copiedSpacePointer);

    WriteBarrierBuffer& writeBarrierBuffer() { return m_writeBarrierBuffer; }
    void flushWriteBarrierBuffer(JSCell*);

    Heap(VM*, HeapType);
    ~Heap();
    void lastChanceToFinalize();
    void releaseDelayedReleasedObjects();

    VM* vm() const { return m_vm; }
    MarkedSpace& objectSpace() { return m_objectSpace; }
    CopiedSpace& storageSpace() { return m_storageSpace; }
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

    // true if collection is in progress
    bool isCollecting();
    HeapOperation operationInProgress() { return m_operationInProgress; }
    // true if an allocation or collection is in progress
    bool isBusy();
    MarkedSpace::Subspace& subspaceForObjectWithoutDestructor() { return m_objectSpace.subspaceForObjectsWithoutDestructor(); }
    MarkedSpace::Subspace& subspaceForObjectDestructor() { return m_objectSpace.subspaceForObjectsWithDestructor(); }
    template<typename ClassType> MarkedSpace::Subspace& subspaceForObjectOfType();
    MarkedAllocator& allocatorForObjectWithoutDestructor(size_t bytes) { return m_objectSpace.allocatorFor(bytes); }
    MarkedAllocator& allocatorForObjectWithDestructor(size_t bytes) { return m_objectSpace.destructorAllocatorFor(bytes); }
    template<typename ClassType> MarkedAllocator& allocatorForObjectOfType(size_t bytes);
    CopiedAllocator& storageAllocator() { return m_storageSpace.allocator(); }
    CheckedBoolean tryAllocateStorage(JSCell* intendedOwner, size_t, void**);
    CheckedBoolean tryReallocateStorage(JSCell* intendedOwner, void**, size_t, size_t);
    void ascribeOwner(JSCell* intendedOwner, void*);

    typedef void (*Finalizer)(JSCell*);
    JS_EXPORT_PRIVATE void addFinalizer(JSCell*, Finalizer);
    void addExecutable(ExecutableBase*);

    void notifyIsSafeToCollect() { m_isSafeToCollect = true; }
    bool isSafeToCollect() const { return m_isSafeToCollect; }

    JS_EXPORT_PRIVATE void collectAllGarbageIfNotDoneRecently();
    void collectAllGarbage() { collectAndSweep(FullCollection); }
    JS_EXPORT_PRIVATE void collectAndSweep(HeapOperation collectionType = AnyCollection);
    bool shouldCollect();
    JS_EXPORT_PRIVATE void collect(HeapOperation collectionType = AnyCollection);
    bool collectIfNecessaryOrDefer(); // Returns true if it did collect.

    void completeAllDFGPlans();
    
    // Use this API to report non-GC memory referenced by GC objects. Be sure to
    // call both of these functions: Calling only one may trigger catastropic
    // memory growth.
    void reportExtraMemoryAllocated(size_t);
    void reportExtraMemoryVisited(CellState cellStateBeforeVisiting, size_t);

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
    
    template<typename Functor> typename Functor::ReturnType forEachProtectedCell(Functor&);
    template<typename Functor> typename Functor::ReturnType forEachProtectedCell();
    template<typename Functor> void forEachCodeBlock(Functor&);

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
    void didAbandon(size_t);

    bool isPagedOut(double deadline);
    
    const JITStubRoutineSet& jitStubRoutines() { return m_jitStubRoutines; }
    
    void addReference(JSCell*, ArrayBuffer*);
    
    bool isDeferred() const { return !!m_deferralDepth || !Options::useGC(); }

    StructureIDTable& structureIDTable() { return m_structureIDTable; }

    CodeBlockSet& codeBlockSet() { return m_codeBlocks; }

#if USE(CF)
        template<typename T> void releaseSoon(RetainPtr<T>&&);
#endif

    static bool isZombified(JSCell* cell) { return *(void**)cell == zombifiedBits; }

    void registerWeakGCMap(void* weakGCMap, std::function<void()> pruningCallback);
    void unregisterWeakGCMap(void* weakGCMap);

    void addLogicallyEmptyWeakBlock(WeakBlock*);

#if ENABLE(RESOURCE_USAGE)
    size_t blockBytesAllocated() const { return m_blockBytesAllocated; }
#endif

    void didAllocateBlock(size_t capacity);
    void didFreeBlock(size_t capacity);

private:
    friend class CodeBlock;
    friend class CopiedBlock;
    friend class DeferGC;
    friend class DeferGCForAWhile;
    friend class GCAwareJITStubRoutine;
    friend class GCLogging;
    friend class GCThread;
    friend class HandleSet;
    friend class HeapVerifier;
    friend class JITStubRoutine;
    friend class LLIntOffsetsExtractor;
    friend class MarkedSpace;
    friend class MarkedAllocator;
    friend class MarkedBlock;
    friend class CopiedSpace;
    friend class CopyVisitor;
    friend class SlotVisitor;
    friend class IncrementalSweeper;
    friend class HeapStatistics;
    friend class VM;
    friend class WeakSet;
    template<typename T> friend void* allocateCell(Heap&);
    template<typename T> friend void* allocateCell(Heap&, size_t);

    void* allocateWithDestructor(size_t); // For use with objects with destructors.
    void* allocateWithoutDestructor(size_t); // For use with objects without destructors.
    template<typename ClassType> void* allocateObjectOfType(size_t); // Chooses one of the methods above based on type.

    static const size_t minExtraMemory = 256;
    
    class FinalizerOwner : public WeakHandleOwner {
        virtual void finalize(Handle<Unknown>, void* context) override;
    };

    JS_EXPORT_PRIVATE bool isValidAllocation(size_t);
    JS_EXPORT_PRIVATE void reportExtraMemoryAllocatedSlowCase(size_t);
    JS_EXPORT_PRIVATE void deprecatedReportExtraMemorySlowCase(size_t);

    void collectImpl(HeapOperation, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);

    void suspendCompilerThreads();
    void willStartCollection(HeapOperation collectionType);
    void flushOldStructureIDTables();
    void flushWriteBarrierBuffer();
    void stopAllocation();
    
    void markRoots(double gcStartTime, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);
    void gatherStackRoots(ConservativeRoots&, void* stackOrigin, void* stackTop, MachineThreads::RegisterState&);
    void gatherJSStackRoots(ConservativeRoots&);
    void gatherScratchBufferRoots(ConservativeRoots&);
    void clearLivenessData();
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
    void traceCodeBlocksAndJITStubRoutines();
    void converge();
    void visitWeakHandles(HeapRootVisitor&);
    void updateObjectCounts(double gcStartTime);
    void resetVisitors();

    void reapWeakHandles();
    void pruneStaleEntriesFromWeakGCMaps();
    void sweepArrayBuffers();
    void snapshotMarkedSpace();
    void deleteSourceProviderCaches();
    void notifyIncrementalSweeper();
    void writeBarrierCurrentlyExecutingCodeBlocks();
    void resetAllocators();
    void copyBackingStores();
    void harvestWeakReferences();
    void finalizeUnconditionalFinalizers();
    void clearUnmarkedExecutables();
    void deleteUnmarkedCompiledCode();
    JS_EXPORT_PRIVATE void addToRememberedSet(const JSCell*);
    void updateAllocationLimits();
    void didFinishCollection(double gcStartTime);
    void resumeCompilerThreads();
    void zombifyDeadObjects();
    void markDeadObjects();

    void sweepAllLogicallyEmptyWeakBlocks();
    bool sweepNextLogicallyEmptyWeakBlock();

    bool shouldDoFullCollection(HeapOperation requestedCollectionType) const;

    JSStack& stack();
    
    void incrementDeferralDepth();
    void decrementDeferralDepth();
    void decrementDeferralDepthAndGCIfNeeded();

    size_t threadVisitCount();
    size_t threadBytesVisited();
    size_t threadBytesCopied();

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
    size_t m_totalBytesCopied;
    size_t m_totalBytesCopiedThisCycle;
    
    HeapOperation m_operationInProgress;
    StructureIDTable m_structureIDTable;
    MarkedSpace m_objectSpace;
    CopiedSpace m_storageSpace;
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
    CodeBlockSet m_codeBlocks;
    JITStubRoutineSet m_jitStubRoutines;
    FinalizerOwner m_finalizerOwner;
    
    bool m_isSafeToCollect;

    WriteBarrierBuffer m_writeBarrierBuffer;

    VM* m_vm;
    double m_lastFullGCLength;
    double m_lastEdenGCLength;

    Vector<ExecutableBase*> m_executables;

    Vector<WeakBlock*> m_logicallyEmptyWeakBlocks;
    size_t m_indexOfNextLogicallyEmptyWeakBlockToSweep { WTF::notFound };
    
    RefPtr<FullGCActivityCallback> m_fullActivityCallback;
    RefPtr<GCActivityCallback> m_edenActivityCallback;
    std::unique_ptr<IncrementalSweeper> m_sweeper;
    Vector<MarkedBlock*> m_blockSnapshot;

    Vector<HeapObserver*> m_observers;

    unsigned m_deferralDepth;
    Vector<DFG::Worklist*> m_suspendedCompilerWorklists;

    std::unique_ptr<HeapVerifier> m_verifier;
#if USE(CF)
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

    Vector<CopiedBlock*> m_blocksToCopy;
    static const size_t s_blockFragmentLength = 32;

    ListableHandler<WeakReferenceHarvester>::List m_weakReferenceHarvesters;
    ListableHandler<UnconditionalFinalizer>::List m_unconditionalFinalizers;

    ParallelHelperClient m_helperClient;

#if ENABLE(RESOURCE_USAGE)
    size_t m_blockBytesAllocated { 0 };
#endif
};

} // namespace JSC

#endif // Heap_h
