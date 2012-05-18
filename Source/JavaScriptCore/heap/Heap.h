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

#include "BlockAllocator.h"
#include "DFGCodeBlocks.h"
#include "HandleSet.h"
#include "HandleStack.h"
#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedBlockSet.h"
#include "MarkedSpace.h"
#include "SlotVisitor.h"
#include "WeakHandleOwner.h"
#include "WeakSet.h"
#include "WriteBarrierSupport.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>

#define COLLECT_ON_EVERY_ALLOCATION 0

namespace JSC {

    class CopiedSpace;
    class CodeBlock;
    class FunctionExecutable;
    class GCActivityCallback;
    class GlobalCodeBlock;
    class Heap;
    class HeapRootVisitor;
    class JSCell;
    class JSGlobalData;
    class JSValue;
    class LiveObjectIterator;
    class LLIntOffsetsExtractor;
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
        friend class MarkStackThreadSharedData;
        static Heap* heap(const JSValue); // 0 for immediate values
        static Heap* heap(const JSCell*);

        // This constant determines how many blocks we iterate between checks of our 
        // deadline when calling Heap::isPagedOut. Decreasing it will cause us to detect 
        // overstepping our deadline more quickly, while increasing it will cause 
        // our scan to run faster. 
        static const unsigned s_timeCheckResolution = 16;

        static bool isMarked(const void*);
        static bool testAndSetMarked(const void*);
        static void setMarked(const void*);

        static void writeBarrier(const JSCell*, JSValue);
        static void writeBarrier(const JSCell*, JSCell*);
        static uint8_t* addressOfCardFor(JSCell*);

        Heap(JSGlobalData*, HeapSize);
        ~Heap();
        JS_EXPORT_PRIVATE void lastChanceToFinalize();

        JSGlobalData* globalData() const { return m_globalData; }
        MarkedSpace& objectSpace() { return m_objectSpace; }
        MachineThreads& machineThreads() { return m_machineThreads; }

        JS_EXPORT_PRIVATE GCActivityCallback* activityCallback();
        JS_EXPORT_PRIVATE void setActivityCallback(PassOwnPtr<GCActivityCallback>);
        JS_EXPORT_PRIVATE void setGarbageCollectionTimerEnabled(bool);

        // true if an allocation or collection is in progress
        inline bool isBusy();
        
        MarkedAllocator& firstAllocatorWithoutDestructors() { return m_objectSpace.firstAllocator(); }
        MarkedAllocator& allocatorForObjectWithoutDestructor(size_t bytes) { return m_objectSpace.allocatorFor(bytes); }
        MarkedAllocator& allocatorForObjectWithDestructor(size_t bytes) { return m_objectSpace.destructorAllocatorFor(bytes); }
        CopiedAllocator& storageAllocator() { return m_storageSpace.allocator(); }
        CheckedBoolean tryAllocateStorage(size_t, void**);
        CheckedBoolean tryReallocateStorage(void**, size_t, size_t);

        typedef void (*Finalizer)(JSCell*);
        JS_EXPORT_PRIVATE void addFinalizer(JSCell*, Finalizer);
        void addFunctionExecutable(FunctionExecutable*);
        void removeFunctionExecutable(FunctionExecutable*);

        void notifyIsSafeToCollect() { m_isSafeToCollect = true; }

        JS_EXPORT_PRIVATE void collectAllGarbage();
        enum SweepToggle { DoNotSweep, DoSweep };
        bool shouldCollect();
        void collect(SweepToggle);

        void reportExtraMemoryCost(size_t cost);
        JS_EXPORT_PRIVATE void reportAbandonedObjectGraph();

        JS_EXPORT_PRIVATE void protect(JSValue);
        JS_EXPORT_PRIVATE bool unprotect(JSValue); // True when the protect count drops to 0.
        
        void jettisonDFGCodeBlock(PassOwnPtr<CodeBlock>);

        JS_EXPORT_PRIVATE size_t size();
        JS_EXPORT_PRIVATE size_t capacity();
        JS_EXPORT_PRIVATE size_t objectCount();
        JS_EXPORT_PRIVATE size_t globalObjectCount();
        JS_EXPORT_PRIVATE size_t protectedObjectCount();
        JS_EXPORT_PRIVATE size_t protectedGlobalObjectCount();
        JS_EXPORT_PRIVATE PassOwnPtr<TypeCountSet> protectedObjectTypeCounts();
        JS_EXPORT_PRIVATE PassOwnPtr<TypeCountSet> objectTypeCounts();

        void pushTempSortVector(Vector<ValueStringPair>*);
        void popTempSortVector(Vector<ValueStringPair>*);
    
        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = new HashSet<MarkedArgumentBuffer*>; return *m_markListSet; }
        
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell(Functor&);
        template<typename Functor> typename Functor::ReturnType forEachProtectedCell();

        WeakSet* weakSet() { return &m_weakSet; }
        HandleSet* handleSet() { return &m_handleSet; }
        HandleStack* handleStack() { return &m_handleStack; }

        void getConservativeRegisterRoots(HashSet<JSCell*>& roots);

        double lastGCLength() { return m_lastGCLength; }
        void increaseLastGCLength(double amount) { m_lastGCLength += amount; }

        JS_EXPORT_PRIVATE void discardAllCompiledCode();

        void didAllocate(size_t);
        void didAbandon(size_t);

        bool isPagedOut(double deadline);

    private:
        friend class CodeBlock;
        friend class LLIntOffsetsExtractor;
        friend class MarkedSpace;
        friend class MarkedAllocator;
        friend class MarkedBlock;
        friend class CopiedSpace;
        friend class SlotVisitor;
        template<typename T> friend void* allocateCell(Heap&);

        void* allocateWithDestructor(size_t);
        void* allocateWithoutDestructor(size_t);

        static const size_t minExtraCost = 256;
        static const size_t maxExtraCost = 1024 * 1024;
        
        class FinalizerOwner : public WeakHandleOwner {
            virtual void finalize(Handle<Unknown>, void* context);
        };

        JS_EXPORT_PRIVATE bool isValidAllocation(size_t);
        JS_EXPORT_PRIVATE void reportExtraMemoryCostSlowCase(size_t);

        void markRoots(bool fullGC);
        void markProtectedObjects(HeapRootVisitor&);
        void markTempSortVectors(HeapRootVisitor&);
        void harvestWeakReferences();
        void finalizeUnconditionalFinalizers();
        
        RegisterFile& registerFile();
        BlockAllocator& blockAllocator();

        const HeapSize m_heapSize;
        const size_t m_minBytesPerCycle;
        size_t m_sizeAfterLastCollect;

        size_t m_bytesAllocatedLimit;
        size_t m_bytesAllocated;
        size_t m_bytesAbandoned;
        
        OperationInProgress m_operationInProgress;
        MarkedSpace m_objectSpace;
        CopiedSpace m_storageSpace;

        BlockAllocator m_blockAllocator;

#if ENABLE(SIMPLE_HEAP_PROFILING)
        VTableSpectrum m_destroyedTypeCounts;
#endif

        ProtectCountSet m_protectedValues;
        Vector<Vector<ValueStringPair>* > m_tempSortingVectors;
        HashSet<MarkedArgumentBuffer*>* m_markListSet;

        OwnPtr<GCActivityCallback> m_activityCallback;
        
        MachineThreads m_machineThreads;
        
        MarkStackThreadSharedData m_sharedData;
        SlotVisitor m_slotVisitor;

        WeakSet m_weakSet;
        HandleSet m_handleSet;
        HandleStack m_handleStack;
        DFGCodeBlocks m_dfgCodeBlocks;
        FinalizerOwner m_finalizerOwner;
        
        bool m_isSafeToCollect;

        JSGlobalData* m_globalData;
        double m_lastGCLength;
        double m_lastCodeDiscardTime;

        DoublyLinkedList<FunctionExecutable> m_functions;
    };

    inline bool Heap::shouldCollect()
    {
#if ENABLE(GGC)
        return m_objectSpace.nurseryWaterMark() >= m_minBytesPerCycle && m_isSafeToCollect;
#else
        return m_bytesAllocated > m_bytesAllocatedLimit && m_isSafeToCollect;
#endif
    }

    bool Heap::isBusy()
    {
        return m_operationInProgress != NoOperation;
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
        m_handleSet.forEachStrongHandle(functor, m_protectedValues);

        return functor.returnValue();
    }

    template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell()
    {
        Functor functor;
        return forEachProtectedCell(functor);
    }

    inline void* Heap::allocateWithDestructor(size_t bytes)
    {
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocateWithDestructor(bytes);
    }
    
    inline void* Heap::allocateWithoutDestructor(size_t bytes)
    {
        ASSERT(isValidAllocation(bytes));
        return m_objectSpace.allocateWithoutDestructor(bytes);
    }
    
    inline CheckedBoolean Heap::tryAllocateStorage(size_t bytes, void** outPtr)
    {
        return m_storageSpace.tryAllocate(bytes, outPtr);
    }
    
    inline CheckedBoolean Heap::tryReallocateStorage(void** ptr, size_t oldSize, size_t newSize)
    {
        return m_storageSpace.tryReallocate(ptr, oldSize, newSize);
    }

    inline BlockAllocator& Heap::blockAllocator()
    {
        return m_blockAllocator;
    }

} // namespace JSC

#endif // Heap_h
