/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
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
#include "BlockAllocator.h"
#include "CodeBlockSet.h"
#include "CopyVisitor.h"
#include "GCIncomingRefCountedSet.h"
#include "GCThreadSharedData.h"
#include "HandleSet.h"
#include "HandleStack.h"
#include "HeapOperation.h"
#include "JITStubRoutineSet.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "MarkedSpace.h"
#include "Options.h"
#include "SlotVisitor.h"
#include "StructureIDTable.h"
#include "WeakHandleOwner.h"
#include "WriteBarrierBuffer.h"
#include "WriteBarrierSupport.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>

#define COLLECT_ON_EVERY_ALLOCATION 0

namespace JSC {

class CopiedSpace;
class CodeBlock;
class ExecutableBase;
class EdenGCActivityCallback;
class FullGCActivityCallback;
class GCActivityCallback;
class GCAwareJITStubRoutine;
class GlobalCodeBlock;
class Heap;
class HeapRootVisitor;
class IncrementalSweeper;
class JITStubRoutine;
class JSCell;
class VM;
class JSStack;
class JSValue;
class LiveObjectIterator;
class LLIntOffsetsExtractor;
class MarkedArgumentBuffer;
class WeakGCHandlePool;
class SlotVisitor;

typedef std::pair<JSValue, WTF::String> ValueStringPair;
typedef HashCountedSet<JSCell*> ProtectCountSet;
typedef HashCountedSet<const char*> TypeCountSet;

enum HeapType { SmallHeap, LargeHeap };

class Heap {
    WTF_MAKE_NONCOPYABLE(Heap);
public:
    friend class JIT;
    friend class DFG::SpeculativeJIT;
    friend class GCThreadSharedData;
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
    static bool isRemembered(const void*);

    JS_EXPORT_PRIVATE void addToRememberedSet(const JSCell*);
    bool isInRememberedSet(const JSCell*) const;
    static bool isWriteBarrierEnabled();
    JS_EXPORT_PRIVATE void writeBarrier(const JSCell*);
    void writeBarrier(const JSCell*, JSValue);
    void writeBarrier(const JSCell*, JSCell*);

    WriteBarrierBuffer& writeBarrierBuffer() { return m_writeBarrierBuffer; }
    void flushWriteBarrierBuffer(JSCell*);

    Heap(VM*, HeapType);
    ~Heap();
    JS_EXPORT_PRIVATE void lastChanceToFinalize();

    VM* vm() const { return m_vm; }
    MarkedSpace& objectSpace() { return m_objectSpace; }
    MachineThreads& machineThreads() { return m_machineThreads; }

    JS_EXPORT_PRIVATE GCActivityCallback* fullActivityCallback();
    JS_EXPORT_PRIVATE GCActivityCallback* edenActivityCallback();
    JS_EXPORT_PRIVATE void setFullActivityCallback(PassRefPtr<FullGCActivityCallback>);
    JS_EXPORT_PRIVATE void setEdenActivityCallback(PassRefPtr<EdenGCActivityCallback>);
    JS_EXPORT_PRIVATE void setGarbageCollectionTimerEnabled(bool);

    JS_EXPORT_PRIVATE IncrementalSweeper* sweeper();
    JS_EXPORT_PRIVATE void setIncrementalSweeper(PassOwnPtr<IncrementalSweeper>);

    // true if collection is in progress
    bool isCollecting();
    HeapOperation operationInProgress() { return m_operationInProgress; }
    // true if an allocation or collection is in progress
    bool isBusy();
    
    MarkedAllocator& allocatorForObjectWithoutDestructor(size_t bytes) { return m_objectSpace.allocatorFor(bytes); }
    MarkedAllocator& allocatorForObjectWithNormalDestructor(size_t bytes) { return m_objectSpace.normalDestructorAllocatorFor(bytes); }
    MarkedAllocator& allocatorForObjectWithImmortalStructureDestructor(size_t bytes) { return m_objectSpace.immortalStructureDestructorAllocatorFor(bytes); }
    CopiedAllocator& storageAllocator() { return m_storageSpace.allocator(); }
    CheckedBoolean tryAllocateStorage(JSCell* intendedOwner, size_t, void**);
    CheckedBoolean tryReallocateStorage(JSCell* intendedOwner, void**, size_t, size_t);
    void ascribeOwner(JSCell* intendedOwner, void*);

    typedef void (*Finalizer)(JSCell*);
    JS_EXPORT_PRIVATE void addFinalizer(JSCell*, Finalizer);
    void addCompiledCode(ExecutableBase*);

    void notifyIsSafeToCollect() { m_isSafeToCollect = true; }
    bool isSafeToCollect() const { return m_isSafeToCollect; }

    JS_EXPORT_PRIVATE void collectAllGarbage();
    bool shouldCollect();
    JS_EXPORT_PRIVATE void collect(HeapOperation collectionType = AnyCollection);
    bool collectIfNecessaryOrDefer(); // Returns true if it did collect.

    void reportExtraMemoryCost(size_t cost);
    JS_EXPORT_PRIVATE void reportAbandonedObjectGraph();

    JS_EXPORT_PRIVATE void protect(JSValue);
    JS_EXPORT_PRIVATE bool unprotect(JSValue); // True when the protect count drops to 0.
    
    size_t extraSize(); // extra memory usage outside of pages allocated by the heap
    JS_EXPORT_PRIVATE size_t size();
    JS_EXPORT_PRIVATE size_t capacity();
    JS_EXPORT_PRIVATE size_t objectCount();
    JS_EXPORT_PRIVATE size_t globalObjectCount();
    JS_EXPORT_PRIVATE size_t protectedObjectCount();
    JS_EXPORT_PRIVATE size_t protectedGlobalObjectCount();
    JS_EXPORT_PRIVATE PassOwnPtr<TypeCountSet> protectedObjectTypeCounts();
    JS_EXPORT_PRIVATE PassOwnPtr<TypeCountSet> objectTypeCounts();
    void showStatistics();

    void pushTempSortVector(Vector<ValueStringPair, 0, UnsafeVectorOverflow>*);
    void popTempSortVector(Vector<ValueStringPair, 0, UnsafeVectorOverflow>*);

    HashSet<MarkedArgumentBuffer*>& markListSet();
    
    template<typename Functor> typename Functor::ReturnType forEachProtectedCell(Functor&);
    template<typename Functor> typename Functor::ReturnType forEachProtectedCell();
    template<typename Functor> void forEachCodeBlock(Functor&);

    HandleSet* handleSet() { return &m_handleSet; }
    HandleStack* handleStack() { return &m_handleStack; }

    void willStartIterating();
    void didFinishIterating();
    void getConservativeRegisterRoots(HashSet<JSCell*>& roots);

    double lastFullGCLength() const { return m_lastFullGCLength; }
    double lastEdenGCLength() const { return m_lastEdenGCLength; }
    void increaseLastFullGCLength(double amount) { m_lastFullGCLength += amount; }

    size_t sizeBeforeLastEdenCollection() const { return m_sizeBeforeLastEdenCollect; }
    size_t sizeAfterLastEdenCollection() const { return m_sizeAfterLastEdenCollect; }
    size_t sizeBeforeLastFullCollection() const { return m_sizeBeforeLastFullCollect; }
    size_t sizeAfterLastFullCollection() const { return m_sizeAfterLastFullCollect; }

    JS_EXPORT_PRIVATE void deleteAllCompiledCode();
    void deleteAllUnlinkedFunctionCode();

    void didAllocate(size_t);
    void didAbandon(size_t);

    bool isPagedOut(double deadline);
    
    const JITStubRoutineSet& jitStubRoutines() { return m_jitStubRoutines; }
    
    void addReference(JSCell*, ArrayBuffer*);
    
    bool isDeferred() const { return !!m_deferralDepth || Options::disableGC(); }

    BlockAllocator& blockAllocator();
    StructureIDTable& structureIDTable() { return m_structureIDTable; }

#if USE(CF)
        template<typename T> void releaseSoon(RetainPtr<T>&&);
#endif

    void removeCodeBlock(CodeBlock* cb) { m_codeBlocks.remove(cb); }

private:
    friend class CodeBlock;
    friend class CopiedBlock;
    friend class DeferGC;
    friend class DeferGCForAWhile;
    friend class DelayedReleaseScope;
    friend class GCAwareJITStubRoutine;
    friend class HandleSet;
    friend class JITStubRoutine;
    friend class LLIntOffsetsExtractor;
    friend class MarkedSpace;
    friend class MarkedAllocator;
    friend class MarkedBlock;
    friend class CopiedSpace;
    friend class CopyVisitor;
    friend class RecursiveAllocationScope;
    friend class SlotVisitor;
    friend class SuperRegion;
    friend class IncrementalSweeper;
    friend class HeapStatistics;
    friend class VM;
    friend class WeakSet;
    template<typename T> friend void* allocateCell(Heap&);
    template<typename T> friend void* allocateCell(Heap&, size_t);

    void* allocateWithImmortalStructureDestructor(size_t); // For use with special objects whose Structures never die.
    void* allocateWithNormalDestructor(size_t); // For use with objects that inherit directly or indirectly from JSDestructibleObject.
    void* allocateWithoutDestructor(size_t); // For use with objects without destructors.

    static const size_t minExtraCost = 256;
    static const size_t maxExtraCost = 1024 * 1024;
    
    class FinalizerOwner : public WeakHandleOwner {
        virtual void finalize(Handle<Unknown>, void* context) override;
    };

    JS_EXPORT_PRIVATE bool isValidAllocation(size_t);
    JS_EXPORT_PRIVATE void reportExtraMemoryCostSlowCase(size_t);

    void suspendCompilerThreads();
    void willStartCollection(HeapOperation collectionType);
    void deleteOldCode(double gcStartTime);
    void flushOldStructureIDTables();
    void flushWriteBarrierBuffer();
    void stopAllocation();

    void markRoots();
    void gatherStackRoots(ConservativeRoots&, void** dummy);
    void gatherJSStackRoots(ConservativeRoots&);
    void gatherScratchBufferRoots(ConservativeRoots&);
    void clearLivenessData();
    void visitSmallStrings();
    void visitConservativeRoots(ConservativeRoots&);
    void visitCompilerWorklists();
    void visitProtectedObjects(HeapRootVisitor&);
    void visitTempSortVectors(HeapRootVisitor&);
    void visitArgumentBuffers(HeapRootVisitor&);
    void visitException(HeapRootVisitor&);
    void visitStrongHandles(HeapRootVisitor&);
    void visitHandleStack(HeapRootVisitor&);
    void traceCodeBlocksAndJITStubRoutines();
    void converge();
    void visitWeakHandles(HeapRootVisitor&);
    void clearRememberedSet(Vector<const JSCell*>&);
    void updateObjectCounts();
    void resetVisitors();

    void reapWeakHandles();
    void sweepArrayBuffers();
    void snapshotMarkedSpace();
    void deleteSourceProviderCaches();
    void notifyIncrementalSweeper();
    void rememberCurrentlyExecutingCodeBlocks();
    void resetAllocators();
    void copyBackingStores();
    void harvestWeakReferences();
    void finalizeUnconditionalFinalizers();
    void deleteUnmarkedCompiledCode();
    void updateAllocationLimits();
    void didFinishCollection(double gcStartTime);
    void resumeCompilerThreads();
    void zombifyDeadObjects();
    void markDeadObjects();

    bool shouldDoFullCollection(HeapOperation requestedCollectionType) const;
    size_t sizeAfterCollect();

    JSStack& stack();
    
    void incrementDeferralDepth();
    void decrementDeferralDepth();
    void decrementDeferralDepthAndGCIfNeeded();

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
    size_t m_totalBytesCopied;
    
    HeapOperation m_operationInProgress;
    BlockAllocator m_blockAllocator;
    StructureIDTable m_structureIDTable;
    MarkedSpace m_objectSpace;
    CopiedSpace m_storageSpace;
    GCIncomingRefCountedSet<ArrayBuffer> m_arrayBuffers;
    size_t m_extraMemoryUsage;

    HashSet<const JSCell*> m_copyingRememberedSet;

    ProtectCountSet m_protectedValues;
    Vector<Vector<ValueStringPair, 0, UnsafeVectorOverflow>*> m_tempSortingVectors;
    OwnPtr<HashSet<MarkedArgumentBuffer*>> m_markListSet;

    MachineThreads m_machineThreads;
    
    GCThreadSharedData m_sharedData;
    SlotVisitor m_slotVisitor;
    CopyVisitor m_copyVisitor;

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
    double m_lastCodeDiscardTime;

    DoublyLinkedList<ExecutableBase> m_compiledCode;
    
    RefPtr<GCActivityCallback> m_fullActivityCallback;
    RefPtr<GCActivityCallback> m_edenActivityCallback;
    OwnPtr<IncrementalSweeper> m_sweeper;
    Vector<MarkedBlock*> m_blockSnapshot;
    
    unsigned m_deferralDepth;
};

} // namespace JSC

#endif // Heap_h
