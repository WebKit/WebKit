/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "AllocationSpace.h"
#include "HandleHeap.h"
#include "HandleStack.h"
#include "JettisonedCodeBlocks.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "MarkedSpace.h"
#include "SlotVisitor.h"
#include "WriteBarrierSupport.h"
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>

namespace JSC {

    class GCActivityCallback;
    class GlobalCodeBlock;
    class Heap;
    class HeapRootVisitor;
    class JSCell;
    class JSGlobalData;
    class JSValue;
    class LiveObjectIterator;
    class MarkedArgumentBuffer;
    class RegisterFile;
    class UString;
    class WeakGCHandlePool;
    class SlotVisitor;

    typedef std::pair<JSValue, UString> ValueStringPair;
    typedef HashCountedSet<JSCell*> ProtectCountSet;
    typedef HashCountedSet<const char*> TypeCountSet;

    enum OperationInProgress { NoOperation, Allocation, Collection };
    
    // Heap size hint.
    enum HeapSize { SmallHeap, LargeHeap };

    class Heap {
        WTF_MAKE_NONCOPYABLE(Heap);
    public:
        friend class JIT;
        static Heap* heap(JSValue); // 0 for immediate values
        static Heap* heap(JSCell*);

        static bool isMarked(const void*);
        static bool testAndSetMarked(const void*);
        static void setMarked(const void*);

        static void writeBarrier(const JSCell*, JSValue);
        static void writeBarrier(const JSCell*, JSCell*);
        static uint8_t* addressOfCardFor(JSCell*);

        Heap(JSGlobalData*, HeapSize);
        ~Heap();
        void destroy(); // JSGlobalData must call destroy() before ~Heap().

        JSGlobalData* globalData() const { return m_globalData; }
        AllocationSpace& objectSpace() { return m_objectSpace; }
        MachineThreads& machineThreads() { return m_machineThreads; }

        GCActivityCallback* activityCallback();
        void setActivityCallback(PassOwnPtr<GCActivityCallback>);

        // true if an allocation or collection is in progress
        inline bool isBusy();
        
        MarkedSpace::SizeClass& sizeClassForObject(size_t bytes) { return m_objectSpace.sizeClassFor(bytes); }
        void* allocate(size_t);

        typedef void (*Finalizer)(JSCell*);
        void addFinalizer(JSCell*, Finalizer);

        void notifyIsSafeToCollect() { m_isSafeToCollect = true; }
        void collectAllGarbage();

        void reportExtraMemoryCost(size_t cost);

        void protect(JSValue);
        bool unprotect(JSValue); // True when the protect count drops to 0.
        
        void addJettisonedCodeBlock(PassOwnPtr<CodeBlock>);

        size_t size();
        size_t capacity();
        size_t objectCount();
        size_t globalObjectCount();
        size_t protectedObjectCount();
        size_t protectedGlobalObjectCount();
        PassOwnPtr<TypeCountSet> protectedObjectTypeCounts();
        PassOwnPtr<TypeCountSet> objectTypeCounts();

        void pushTempSortVector(Vector<ValueStringPair>*);
        void popTempSortVector(Vector<ValueStringPair>*);
    
        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = new HashSet<MarkedArgumentBuffer*>; return *m_markListSet; }
        
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell(Functor&);
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell();

        HandleHeap* handleHeap() { return &m_handleHeap; }
        HandleStack* handleStack() { return &m_handleStack; }

        void getConservativeRegisterRoots(HashSet<JSCell*>& roots);

    private:
        friend class MarkedBlock;
        friend class AllocationSpace;
        friend class SlotVisitor;

        static const size_t minExtraCost = 256;
        static const size_t maxExtraCost = 1024 * 1024;
        
        class FinalizerOwner : public WeakHandleOwner {
            virtual void finalize(Handle<Unknown>, void* context);
        };

        bool isValidAllocation(size_t);
        void reportExtraMemoryCostSlowCase(size_t);

        // Call this function before any operation that needs to know which cells
        // in the heap are live. (For example, call this function before
        // conservative marking, eager sweeping, or iterating the cells in a MarkedBlock.)
        void canonicalizeCellLivenessData();

        void resetAllocator();
        void freeBlocks(MarkedBlock*);

        void clearMarks();
        void markRoots(bool fullGC);
        void markProtectedObjects(HeapRootVisitor&);
        void markTempSortVectors(HeapRootVisitor&);
        void harvestWeakReferences();
        void finalizeUnconditionalFinalizers();
        
        enum SweepToggle { DoNotSweep, DoSweep };
        void collect(SweepToggle);
        void shrink();
        void releaseFreeBlocks();
        void sweep();

        RegisterFile& registerFile();

        void waitForRelativeTimeWhileHoldingLock(double relative);
        void waitForRelativeTime(double relative);
        void blockFreeingThreadMain();
        static void* blockFreeingThreadStartFunc(void* heap);
        
        const HeapSize m_heapSize;
        const size_t m_minBytesPerCycle;
        size_t m_lastFullGCSize;
        
        OperationInProgress m_operationInProgress;
        AllocationSpace m_objectSpace;

        DoublyLinkedList<MarkedBlock> m_freeBlocks;
        size_t m_numberOfFreeBlocks;
        
        ThreadIdentifier m_blockFreeingThread;
        Mutex m_freeBlockLock;
        ThreadCondition m_freeBlockCondition;
        bool m_blockFreeingThreadShouldQuit;

#if ENABLE(SIMPLE_HEAP_PROFILING)
        VTableSpectrum m_destroyedTypeCounts;
#endif

        size_t m_extraCost;

        ProtectCountSet m_protectedValues;
        Vector<Vector<ValueStringPair>* > m_tempSortingVectors;
        HashSet<MarkedArgumentBuffer*>* m_markListSet;

        OwnPtr<GCActivityCallback> m_activityCallback;
        
        MachineThreads m_machineThreads;
        
        MarkStackThreadSharedData m_sharedData;
        SlotVisitor m_slotVisitor;

        HandleHeap m_handleHeap;
        HandleStack m_handleStack;
        JettisonedCodeBlocks m_jettisonedCodeBlocks;
        FinalizerOwner m_finalizerOwner;
        
        bool m_isSafeToCollect;

        JSGlobalData* m_globalData;
    };

    bool Heap::isBusy()
    {
        return m_operationInProgress != NoOperation;
    }

    inline Heap* Heap::heap(JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->heap();
    }

    inline Heap* Heap::heap(JSValue v)
    {
        if (!v.isCell())
            return 0;
        return heap(v.asCell());
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

#if ENABLE(GGC)
    inline uint8_t* Heap::addressOfCardFor(JSCell* cell)
    {
        return MarkedBlock::blockFor(cell)->addressOfCardFor(cell);
    }

    inline void Heap::writeBarrier(const JSCell* owner, JSCell*)
    {
        WriteBarrierCounters::countWriteBarrier();
        MarkedBlock* block = MarkedBlock::blockFor(owner);
        if (block->isMarked(owner))
            block->setDirtyObject(owner);
    }

    inline void Heap::writeBarrier(const JSCell* owner, JSValue value)
    {
        if (!value)
            return;
        if (!value.isCell())
            return;
        writeBarrier(owner, value.asCell());
    }
#else

    inline void Heap::writeBarrier(const JSCell*, JSCell*)
    {
        WriteBarrierCounters::countWriteBarrier();
    }

    inline void Heap::writeBarrier(const JSCell*, JSValue)
    {
        WriteBarrierCounters::countWriteBarrier();
    }
#endif

    inline void Heap::reportExtraMemoryCost(size_t cost)
    {
        if (cost > minExtraCost) 
            reportExtraMemoryCostSlowCase(cost);
    }

    template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell(Functor& functor)
    {
        ProtectCountSet::iterator end = m_protectedValues.end();
        for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
            functor(it->first);
        m_handleHeap.forEachStrongHandle(functor, m_protectedValues);

        return functor.returnValue();
    }

    template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell()
    {
        Functor functor;
        return forEachProtectedCell(functor);
    }

    inline void* Heap::allocate(size_t bytes)
    {
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocate(bytes);
    }

} // namespace JSC

#endif // Heap_h
