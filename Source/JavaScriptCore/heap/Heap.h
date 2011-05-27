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

#include "HandleHeap.h"
#include "HandleStack.h"
#include "MarkStack.h"
#include "MarkedSpace.h"
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>

namespace JSC {

    class GCActivityCallback;
    class GlobalCodeBlock;
    class HeapRootVisitor;
    class JSCell;
    class JSGlobalData;
    class JSValue;
    class LiveObjectIterator;
    class MarkStack;
    class MarkedArgumentBuffer;
    class RegisterFile;
    class UString;
    class WeakGCHandlePool;
    typedef MarkStack SlotVisitor;

    typedef std::pair<JSValue, UString> ValueStringPair;
    typedef HashCountedSet<JSCell*> ProtectCountSet;
    typedef HashCountedSet<const char*> TypeCountSet;

    enum OperationInProgress { NoOperation, Allocation, Collection };

    class Heap {
        WTF_MAKE_NONCOPYABLE(Heap);
    public:
        static Heap* heap(JSValue); // 0 for immediate values
        static Heap* heap(JSCell*);

        static bool isMarked(const void*);
        static bool testAndSetMarked(const void*);
        static bool testAndClearMarked(const void*);
        static void setMarked(const void*);

        static void writeBarrier(const JSCell*, JSValue);
        static void writeBarrier(const JSCell*, JSCell*);

        Heap(JSGlobalData*);
        ~Heap();
        void destroy(); // JSGlobalData must call destroy() before ~Heap().

        JSGlobalData* globalData() const { return m_globalData; }
        MarkedSpace& markedSpace() { return m_markedSpace; }
        MachineThreads& machineThreads() { return m_machineThreads; }

        GCActivityCallback* activityCallback();
        void setActivityCallback(PassOwnPtr<GCActivityCallback>);

        // true if an allocation or collection is in progress
        inline bool isBusy();

        void* allocate(size_t);
        void collectAllGarbage();

        void reportExtraMemoryCost(size_t cost);

        void protect(JSValue);
        bool unprotect(JSValue); // True when the protect count drops to 0.

        bool contains(void*);

        size_t size() const;
        size_t capacity() const;
        size_t objectCount() const;
        size_t globalObjectCount();
        size_t protectedObjectCount();
        size_t protectedGlobalObjectCount();
        PassOwnPtr<TypeCountSet> protectedObjectTypeCounts();
        PassOwnPtr<TypeCountSet> objectTypeCounts();

        void pushTempSortVector(Vector<ValueStringPair>*);
        void popTempSortVector(Vector<ValueStringPair>*);
    
        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = new HashSet<MarkedArgumentBuffer*>; return *m_markListSet; }
        
        template <typename Functor> void forEach(Functor&);
        
        HandleSlot allocateGlobalHandle() { return m_handleHeap.allocate(); }
        HandleSlot allocateLocalHandle() { return m_handleStack.push(); }

        HandleStack* handleStack() { return &m_handleStack; }

    private:
        friend class JSGlobalData;

        static const size_t minExtraCost = 256;
        static const size_t maxExtraCost = 1024 * 1024;

        bool isValidAllocation(size_t);
        void* allocateSlowCase(size_t);
        void reportExtraMemoryCostSlowCase(size_t);

        void markRoots();
        void markProtectedObjects(HeapRootVisitor&);
        void markTempSortVectors(HeapRootVisitor&);

        enum SweepToggle { DoNotSweep, DoSweep };
        void reset(SweepToggle);

        RegisterFile& registerFile();

        OperationInProgress m_operationInProgress;
        MarkedSpace m_markedSpace;

        ProtectCountSet m_protectedValues;
        Vector<Vector<ValueStringPair>* > m_tempSortingVectors;

        HashSet<MarkedArgumentBuffer*>* m_markListSet;

        OwnPtr<GCActivityCallback> m_activityCallback;

        JSGlobalData* m_globalData;
        
        MachineThreads m_machineThreads;
        MarkStack m_markStack;
        HandleHeap m_handleHeap;
        HandleStack m_handleStack;

        size_t m_extraCost;
    };

    bool Heap::isBusy()
    {
        return m_operationInProgress != NoOperation;
    }

    inline bool Heap::isMarked(const void* cell)
    {
        return MarkedSpace::isMarked(cell);
    }

    inline bool Heap::testAndSetMarked(const void* cell)
    {
        return MarkedSpace::testAndSetMarked(cell);
    }

    inline bool Heap::testAndClearMarked(const void* cell)
    {
        return MarkedSpace::testAndClearMarked(cell);
    }

    inline void Heap::setMarked(const void* cell)
    {
        MarkedSpace::setMarked(cell);
    }

    inline void Heap::writeBarrier(const JSCell*, JSValue)
    {
    }

    inline void Heap::writeBarrier(const JSCell*, JSCell*)
    {
    }

    inline bool Heap::contains(void* p)
    {
        return m_markedSpace.contains(p);
    }

    inline void Heap::reportExtraMemoryCost(size_t cost)
    {
        if (cost > minExtraCost) 
            reportExtraMemoryCostSlowCase(cost);
    }

    template <typename Functor> inline void Heap::forEach(Functor& functor)
    {
        m_markedSpace.forEach(functor);
    }

    inline void* Heap::allocate(size_t bytes)
    {
        ASSERT(isValidAllocation(bytes));

        m_operationInProgress = Allocation;
        void* result = m_markedSpace.allocate(bytes);
        m_operationInProgress = NoOperation;
        if (result)
            return result;

        return allocateSlowCase(bytes);
    }

    inline Heap* Heap::heap(JSValue v)
    {
        if (!v.isCell())
            return 0;
        return heap(v.asCell());
    }

    inline Heap* Heap::heap(JSCell* c)
    {
        return MarkedSpace::heap(c);
    }

} // namespace JSC

#endif // Heap_h
