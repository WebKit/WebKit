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

#include "MarkStack.h"
#include "MarkedSpace.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>

#define ASSERT_CLASS_FITS_IN_CELL(class) COMPILE_ASSERT(sizeof(class) <= CELL_SIZE, class_fits_in_cell)

namespace JSC {

    class JSValue;
    class UString;
    class GCActivityCallback;
    class JSCell;
    class JSGlobalData;
    class JSValue;
    class LiveObjectIterator;
    class MarkedArgumentBuffer;
    class MarkStack;
    class WeakGCHandlePool;

    typedef std::pair<JSValue, UString> ValueStringPair;

    enum OperationInProgress { NoOperation, Allocation, Collection };

    class Heap {
        WTF_MAKE_NONCOPYABLE(Heap);
    public:
        void destroy();

        void* allocate(size_t);

        bool isBusy(); // true if an allocation or collection is in progress
        void collectAllGarbage();

        GCActivityCallback* activityCallback();
        void setActivityCallback(PassOwnPtr<GCActivityCallback>);

        static const size_t minExtraCost = 256;
        static const size_t maxExtraCost = 1024 * 1024;

        void reportExtraMemoryCost(size_t cost);

        size_t objectCount() const;
        MarkedSpace::Statistics statistics() const;
        size_t size() const;

        void protect(JSValue);
        // Returns true if the value is no longer protected by any protect pointers
        // (though it may still be alive due to heap/stack references).
        bool unprotect(JSValue);

        static Heap* heap(JSValue); // 0 for immediate values
        static Heap* heap(JSCell*);

        size_t globalObjectCount();
        size_t protectedObjectCount();
        size_t protectedGlobalObjectCount();
        HashCountedSet<const char*>* protectedObjectTypeCounts();
        HashCountedSet<const char*>* objectTypeCounts();

        static bool isCellMarked(const JSCell*);
        static bool checkMarkCell(const JSCell*);
        static void markCell(JSCell*);

        WeakGCHandle* addWeakGCHandle(JSCell*);

        void markConservatively(ConservativeSet&, void* start, void* end);

        void pushTempSortVector(WTF::Vector<ValueStringPair>*);
        void popTempSortVector(WTF::Vector<ValueStringPair>*);        

        HashSet<MarkedArgumentBuffer*>& markListSet() { if (!m_markListSet) m_markListSet = new HashSet<MarkedArgumentBuffer*>; return *m_markListSet; }

        JSGlobalData* globalData() const { return m_globalData; }
        
        LiveObjectIterator primaryHeapBegin();
        LiveObjectIterator primaryHeapEnd();
        
        MachineStackMarker& machineStackMarker() { return m_machineStackMarker; }

        MarkedSpace& markedSpace() { return m_markedSpace; }

    private:
        friend class JSGlobalData;
        Heap(JSGlobalData*);
        ~Heap();

        void recordExtraCost(size_t);

        void markRoots();
        void markProtectedObjects(MarkStack&);
        void markTempSortVectors(MarkStack&);

        void updateWeakGCHandles();
        WeakGCHandlePool* weakGCHandlePool(size_t index);

        MarkedSpace m_markedSpace;
        OperationInProgress m_operationInProgress;

        ProtectCountSet m_protectedValues;
        WTF::Vector<PageAllocationAligned> m_weakGCHandlePools;
        WTF::Vector<WTF::Vector<ValueStringPair>* > m_tempSortingVectors;

        HashSet<MarkedArgumentBuffer*>* m_markListSet;

        OwnPtr<GCActivityCallback> m_activityCallback;

        JSGlobalData* m_globalData;
        
        MachineStackMarker m_machineStackMarker;
        MarkStack m_markStack;
        
        size_t m_extraCost;
    };

    inline bool Heap::isCellMarked(const JSCell* cell)
    {
        return MarkedSpace::isCellMarked(cell);
    }

    inline bool Heap::checkMarkCell(const JSCell* cell)
    {
        return MarkedSpace::checkMarkCell(cell);
    }

    inline void Heap::markCell(JSCell* cell)
    {
        MarkedSpace::markCell(cell);
    }

    inline void Heap::reportExtraMemoryCost(size_t cost)
    {
        if (cost > minExtraCost) 
            recordExtraCost(cost);
    }
    
    inline WeakGCHandlePool* Heap::weakGCHandlePool(size_t index)
    {
        return static_cast<WeakGCHandlePool*>(m_weakGCHandlePools[index].base());
    }

} // namespace JSC

#endif // Heap_h
