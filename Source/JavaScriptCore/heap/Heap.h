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

        JS_EXPORT_PRIVATE void addToRememberedSet(const JSCell*);
        bool isInRememberedSet(const JSCell* cell) const
        {
            ASSERT(cell);
            ASSERT(!Options::enableConcurrentJIT() || !isCompilationThread());
            return MarkedBlock::blockFor(cell)->isRemembered(cell);
        }
        static bool isWriteBarrierEnabled();
        JS_EXPORT_PRIVATE static void writeBarrier(const JSCell*);
        static void writeBarrier(const JSCell*, JSValue);
        static void writeBarrier(const JSCell*, JSCell*);

        WriteBarrierBuffer& writeBarrierBuffer() { return m_writeBarrierBuffer; }
        void flushWriteBarrierBuffer(JSCell*);

        Heap(VM*, HeapType);
        ~Heap();
        JS_EXPORT_PRIVATE void lastChanceToFinalize();

        VM* vm() const { return m_vm; }
        MarkedSpace& objectSpace() { return m_objectSpace; }
        MachineThreads& machineThreads() { return m_machineThreads; }

        JS_EXPORT_PRIVATE GCActivityCallback* activityCallback();
        JS_EXPORT_PRIVATE void setActivityCallback(PassOwnPtr<GCActivityCallback>);
        JS_EXPORT_PRIVATE void setGarbageCollectionTimerEnabled(bool);

        JS_EXPORT_PRIVATE IncrementalSweeper* sweeper();
        JS_EXPORT_PRIVATE void setIncrementalSweeper(PassOwnPtr<IncrementalSweeper>);

        // true if collection is in progress
        inline bool isCollecting();
        inline HeapOperation operationInProgress() { return m_operationInProgress; }
        // true if an allocation or collection is in progress
        inline bool isBusy();
        
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
        void collect();
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
    
        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = adoptPtr(new HashSet<MarkedArgumentBuffer*>); return *m_markListSet; }
        
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell(Functor&);
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell();
        template<typename Functor> inline void forEachCodeBlock(Functor&);

        HandleSet* handleSet() { return &m_handleSet; }
        HandleStack* handleStack() { return &m_handleStack; }

        void willStartIterating();
        void didFinishIterating();
        void getConservativeRegisterRoots(HashSet<JSCell*>& roots);

        double lastGCLength() { return m_lastGCLength; }
        void increaseLastGCLength(double amount) { m_lastGCLength += amount; }

        JS_EXPORT_PRIVATE void deleteAllCompiledCode();

        void didAllocate(size_t);
        void didAbandon(size_t);

        bool isPagedOut(double deadline);
        
        const JITStubRoutineSet& jitStubRoutines() { return m_jitStubRoutines; }
        
        void addReference(JSCell*, ArrayBuffer*);
        
        bool isDeferred() const { return !!m_deferralDepth || Options::disableGC(); }

#if USE(CF)
        template<typename T> void releaseSoon(RetainPtr<T>&&);
#endif

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

        void markRoots();
        void markProtectedObjects(HeapRootVisitor&);
        void markTempSortVectors(HeapRootVisitor&);
        template <HeapOperation collectionType>
        void copyBackingStores();
        void harvestWeakReferences();
        void finalizeUnconditionalFinalizers();
        void deleteUnmarkedCompiledCode();
        void zombifyDeadObjects();
        void markDeadObjects();

        size_t sizeAfterCollect();

        JSStack& stack();
        BlockAllocator& blockAllocator();
        
        JS_EXPORT_PRIVATE void incrementDeferralDepth();
        void decrementDeferralDepth();
        JS_EXPORT_PRIVATE void decrementDeferralDepthAndGCIfNeeded();

        const HeapType m_heapType;
        const size_t m_ramSize;
        const size_t m_minBytesPerCycle;
        size_t m_sizeAfterLastCollect;

        size_t m_bytesAllocatedThisCycle;
        size_t m_bytesAbandonedThisCycle;
        size_t m_maxEdenSize;
        size_t m_maxHeapSize;
        bool m_shouldDoFullCollection;
        size_t m_totalBytesVisited;
        size_t m_totalBytesCopied;
        
        HeapOperation m_operationInProgress;
        BlockAllocator m_blockAllocator;
        MarkedSpace m_objectSpace;
        CopiedSpace m_storageSpace;
        GCIncomingRefCountedSet<ArrayBuffer> m_arrayBuffers;
        size_t m_extraMemoryUsage;

        HashSet<const JSCell*> m_copyingRememberedSet;

        ProtectCountSet m_protectedValues;
        Vector<Vector<ValueStringPair, 0, UnsafeVectorOverflow>* > m_tempSortingVectors;
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
        double m_lastGCLength;
        double m_lastCodeDiscardTime;

        DoublyLinkedList<ExecutableBase> m_compiledCode;
        
        OwnPtr<GCActivityCallback> m_activityCallback;
        OwnPtr<IncrementalSweeper> m_sweeper;
        Vector<MarkedBlock*> m_blockSnapshot;
        
        unsigned m_deferralDepth;
    };

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

    inline bool Heap::shouldCollect()
    {
        if (isDeferred())
            return false;
        if (Options::gcMaxHeapSize())
            return m_bytesAllocatedThisCycle > Options::gcMaxHeapSize() && m_isSafeToCollect && m_operationInProgress == NoOperation;
        return m_bytesAllocatedThisCycle > m_maxEdenSize && m_isSafeToCollect && m_operationInProgress == NoOperation;
    }

    bool Heap::isBusy()
    {
        return m_operationInProgress != NoOperation;
    }

    bool Heap::isCollecting()
    {
        return m_operationInProgress == FullCollection || m_operationInProgress == EdenCollection;
    }

    inline Heap* Heap::heap(const JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->heap();
    }

    inline Heap* Heap::heap(const JSValue v)
    {
        if (!v.isCell())
            return 0;
        return heap(v.asCell());
    }

    inline bool Heap::isLive(const void* cell)
    {
        return MarkedBlock::blockFor(cell)->isLiveCell(cell);
    }

    inline bool Heap::isMarked(const void* cell)
    {
        return MarkedBlock::blockFor(cell)->isMarked(cell);
    }

    inline bool Heap::testAndSetMarked(const void* cell)
    {
        return MarkedBlock::blockFor(cell)->testAndSetMarked(cell);
    }

    inline void Heap::setMarked(const void* cell)
    {
        MarkedBlock::blockFor(cell)->setMarked(cell);
    }

    inline bool Heap::isWriteBarrierEnabled()
    {
#if ENABLE(WRITE_BARRIER_PROFILING) || ENABLE(GGC)
        return true;
#else
        return false;
#endif
    }

    inline void Heap::writeBarrier(const JSCell* from, JSCell* to)
    {
#if ENABLE(WRITE_BARRIER_PROFILING)
        WriteBarrierCounters::countWriteBarrier();
#endif
        if (!from || !isMarked(from))
            return;
        if (!to || isMarked(to))
            return;
        Heap::heap(from)->addToRememberedSet(from);
    }

    inline void Heap::writeBarrier(const JSCell* from, JSValue to)
    {
#if ENABLE(WRITE_BARRIER_PROFILING)
        WriteBarrierCounters::countWriteBarrier();
#endif
        if (!to.isCell())
            return;
        writeBarrier(from, to.asCell());
    }

    inline void Heap::reportExtraMemoryCost(size_t cost)
    {
        if (cost > minExtraCost) 
            reportExtraMemoryCostSlowCase(cost);
    }

    template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell(Functor& functor)
    {
        ProtectCountSet::iterator end = m_protectedValues.end();
        for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
            functor(it->key);
        m_handleSet.forEachStrongHandle(functor, m_protectedValues);

        return functor.returnValue();
    }

    template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell()
    {
        Functor functor;
        return forEachProtectedCell(functor);
    }

    template<typename Functor> inline void Heap::forEachCodeBlock(Functor& functor)
    {
        return m_codeBlocks.iterate<Functor>(functor);
    }

    inline void* Heap::allocateWithNormalDestructor(size_t bytes)
    {
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC allocating %lu bytes with normal destructor.\n", bytes);
#endif
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocateWithNormalDestructor(bytes);
    }
    
    inline void* Heap::allocateWithImmortalStructureDestructor(size_t bytes)
    {
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC allocating %lu bytes with immortal structure destructor.\n", bytes);
#endif
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocateWithImmortalStructureDestructor(bytes);
    }
    
    inline void* Heap::allocateWithoutDestructor(size_t bytes)
    {
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC allocating %lu bytes without destructor.\n", bytes);
#endif
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocateWithoutDestructor(bytes);
    }
   
    inline CheckedBoolean Heap::tryAllocateStorage(JSCell* intendedOwner, size_t bytes, void** outPtr)
    {
        CheckedBoolean result = m_storageSpace.tryAllocate(bytes, outPtr);
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC allocating %lu bytes of storage for %p: %p.\n", bytes, intendedOwner, *outPtr);
#else
        UNUSED_PARAM(intendedOwner);
#endif
        return result;
    }
    
    inline CheckedBoolean Heap::tryReallocateStorage(JSCell* intendedOwner, void** ptr, size_t oldSize, size_t newSize)
    {
#if ENABLE(ALLOCATION_LOGGING)
        void* oldPtr = *ptr;
#endif
        CheckedBoolean result = m_storageSpace.tryReallocate(ptr, oldSize, newSize);
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC reallocating %lu -> %lu bytes of storage for %p: %p -> %p.\n", oldSize, newSize, intendedOwner, oldPtr, *ptr);
#else
        UNUSED_PARAM(intendedOwner);
#endif
        return result;
    }

    inline void Heap::ascribeOwner(JSCell* intendedOwner, void* storage)
    {
#if ENABLE(ALLOCATION_LOGGING)
        dataLogF("JSC GC ascribing %p as owner of storage %p.\n", intendedOwner, storage);
#else
        UNUSED_PARAM(intendedOwner);
        UNUSED_PARAM(storage);
#endif
    }

    inline BlockAllocator& Heap::blockAllocator()
    {
        return m_blockAllocator;
    }

#if USE(CF)
    template <typename T>
    inline void Heap::releaseSoon(RetainPtr<T>&& object)
    {
        m_objectSpace.releaseSoon(std::move(object));
    }
#endif

} // namespace JSC

#endif // Heap_h
