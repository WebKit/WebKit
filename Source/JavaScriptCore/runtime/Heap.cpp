/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "CollectorHeapIterator.h"
#include "GCActivityCallback.h"
#include "GCHandle.h"
#include "Interpreter.h"
#include "JSGlobalData.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSONObject.h"
#include "Tracing.h"

#define COLLECT_ON_EVERY_ALLOCATION 0

namespace JSC {

Heap::Heap(JSGlobalData* globalData)
    : m_markedSpace(globalData)
    , m_operationInProgress(NoOperation)
    , m_markListSet(0)
    , m_activityCallback(DefaultGCActivityCallback::create(this))
    , m_globalData(globalData)
    , m_machineStackMarker(this)
    , m_markStack(globalData->jsArrayVPtr)
    , m_extraCost(0)
{
    (*m_activityCallback)();
}

Heap::~Heap()
{
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

    delete m_markListSet;
    m_markListSet = 0;

    ProtectCountSet protectedValuesCopy = m_protectedValues;
    m_markedSpace.destroy(protectedValuesCopy);
    ASSERT(!protectedObjectCount());

    m_globalData = 0;
}

void Heap::recordExtraCost(size_t cost)
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

    if (m_extraCost > maxExtraCost && m_extraCost > m_markedSpace.size() / 2) {
        JAVASCRIPTCORE_GC_BEGIN();

        markRoots();

        JAVASCRIPTCORE_GC_MARKED();

        m_markedSpace.reset();
        m_extraCost = 0;

        JAVASCRIPTCORE_GC_END();

        (*m_activityCallback)();
    }
    m_extraCost += cost;
}

void* Heap::allocate(size_t s)
{
    ASSERT(globalData()->identifierTable == wtfThreadData().currentIdentifierTable());
    ASSERT(JSLock::lockCount() > 0);
    ASSERT(JSLock::currentThreadIsHoldingLock());
    ASSERT_UNUSED(s, s <= HeapConstants::cellSize);
    ASSERT(m_operationInProgress == NoOperation);

#if COLLECT_ON_EVERY_ALLOCATION
    collectAllGarbage();
    ASSERT(m_operationInProgress == NoOperation);
#endif

    m_operationInProgress = Allocation;
    void* result = m_markedSpace.allocate(s);
    m_operationInProgress = NoOperation;

    if (!result) {
        JAVASCRIPTCORE_GC_BEGIN();

        markRoots();

        JAVASCRIPTCORE_GC_MARKED();

        m_markedSpace.reset();
        m_extraCost = 0;

        JAVASCRIPTCORE_GC_END();

        (*m_activityCallback)();

        m_operationInProgress = Allocation;
        result = m_markedSpace.allocate(s);
        m_operationInProgress = NoOperation;
    }
    ASSERT(result);
    return result;
}

void Heap::markConservatively(ConservativeSet& conservativeSet, void* start, void* end)
{
    m_markedSpace.markConservatively(conservativeSet, start, end);
}

void Heap::updateWeakGCHandles()
{
    for (unsigned i = 0; i < m_weakGCHandlePools.size(); ++i)
        weakGCHandlePool(i)->update();
}

void WeakGCHandlePool::update()
{
    for (unsigned i = 1; i < WeakGCHandlePool::numPoolEntries; ++i) {
        if (m_entries[i].isValidPtr()) {
            JSCell* cell = m_entries[i].get();
            if (!cell || !Heap::isCellMarked(cell))
                m_entries[i].invalidate();
        }
    }
}

WeakGCHandle* Heap::addWeakGCHandle(JSCell* ptr)
{
    for (unsigned i = 0; i < m_weakGCHandlePools.size(); ++i)
        if (!weakGCHandlePool(i)->isFull())
            return weakGCHandlePool(i)->allocate(ptr);

    PageAllocationAligned allocation = PageAllocationAligned::allocate(WeakGCHandlePool::poolSize, WeakGCHandlePool::poolSize, OSAllocator::JSGCHeapPages);
    m_weakGCHandlePools.append(allocation);

    WeakGCHandlePool* pool = new (allocation.base()) WeakGCHandlePool();
    return pool->allocate(ptr);
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

void Heap::markProtectedObjects(MarkStack& markStack)
{
    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        markStack.append(it->first);
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
    
void Heap::markTempSortVectors(MarkStack& markStack)
{
    typedef Vector<Vector<ValueStringPair>* > VectorOfValueStringVectors;

    VectorOfValueStringVectors::iterator end = m_tempSortingVectors.end();
    for (VectorOfValueStringVectors::iterator it = m_tempSortingVectors.begin(); it != end; ++it) {
        Vector<ValueStringPair>* tempSortingVector = *it;

        Vector<ValueStringPair>::iterator vectorEnd = tempSortingVector->end();
        for (Vector<ValueStringPair>::iterator vectorIt = tempSortingVector->begin(); vectorIt != vectorEnd; ++vectorIt) {
            if (vectorIt->first)
                markStack.append(vectorIt->first);
        }
    }
}
    
void Heap::markRoots()
{
#ifndef NDEBUG
    if (m_globalData->isSharedInstance()) {
        ASSERT(JSLock::lockCount() > 0);
        ASSERT(JSLock::currentThreadIsHoldingLock());
    }
#endif

    ASSERT(m_operationInProgress == NoOperation);
    if (m_operationInProgress != NoOperation)
        CRASH();

    m_operationInProgress = Collection;

    // We gather the conservative set before clearing mark bits, because
    // conservative gathering uses the mark bits from our last mark pass to
    // determine whether a reference is valid.
    ConservativeSet conservativeSet;
    m_machineStackMarker.markMachineStackConservatively(conservativeSet);
    m_globalData->interpreter->registerFile().markCallFrames(conservativeSet, this);

    // Reset mark bits.
    m_markedSpace.clearMarkBits();

    MarkStack& markStack = m_markStack;
    conservativeSet.mark(markStack);
    markStack.drain();

    // Mark explicitly registered roots.
    markProtectedObjects(markStack);
    markStack.drain();
    
    // Mark temporary vector for Array sorting
    markTempSortVectors(markStack);
    markStack.drain();

    // Mark misc. other roots.
    if (m_markListSet && m_markListSet->size())
        MarkedArgumentBuffer::markLists(markStack, *m_markListSet);
    if (m_globalData->exception)
        markStack.append(m_globalData->exception);
    if (m_globalData->firstStringifierToMark)
        JSONObject::markStringifiers(markStack, m_globalData->firstStringifierToMark);
    markStack.drain();

    // Mark the small strings cache last, since it will clear itself if nothing
    // else has marked it.
    m_globalData->smallStrings.markChildren(markStack);

    markStack.drain();
    markStack.compact();

    updateWeakGCHandles();

    m_operationInProgress = NoOperation;
}

size_t Heap::objectCount() const
{
    return m_markedSpace.objectCount();
}

MarkedSpace::Statistics Heap::statistics() const
{
    return m_markedSpace.statistics();
}

size_t Heap::size() const
{
    return m_markedSpace.size();
}

size_t Heap::globalObjectCount()
{
    size_t count = 0;
    if (JSGlobalObject* head = m_globalData->head) {
        JSGlobalObject* o = head;
        do {
            ++count;
            o = o->next();
        } while (o != head);
    }
    return count;
}

size_t Heap::protectedGlobalObjectCount()
{
    size_t count = 0;
    if (JSGlobalObject* head = m_globalData->head) {
        JSGlobalObject* o = head;
        do {
            if (m_protectedValues.contains(o))
                ++count;
            o = o->next();
        } while (o != head);
    }

    return count;
}

size_t Heap::protectedObjectCount()
{
    return m_protectedValues.size();
}

static const char* typeName(JSCell* cell)
{
    if (cell->isString())
        return "string";
    if (cell->isGetterSetter())
        return "Getter-Setter";
    if (cell->isAPIValueWrapper())
        return "API wrapper";
    if (cell->isPropertyNameIterator())
        return "For-in iterator";
    if (!cell->isObject())
        return "[empty cell]";
    const ClassInfo* info = cell->classInfo();
    return info ? info->className : "Object";
}

HashCountedSet<const char*>* Heap::protectedObjectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    ProtectCountSet::iterator end = m_protectedValues.end();
    for (ProtectCountSet::iterator it = m_protectedValues.begin(); it != end; ++it)
        counts->add(typeName(it->first));

    return counts;
}

HashCountedSet<const char*>* Heap::objectTypeCounts()
{
    HashCountedSet<const char*>* counts = new HashCountedSet<const char*>;

    LiveObjectIterator it = primaryHeapBegin();
    LiveObjectIterator heapEnd = primaryHeapEnd();
    for ( ; it != heapEnd; ++it)
        counts->add(typeName(*it));

    return counts;
}

bool Heap::isBusy()
{
    return m_operationInProgress != NoOperation;
}

void Heap::collectAllGarbage()
{
    ASSERT(globalData()->identifierTable == wtfThreadData().currentIdentifierTable());
    JAVASCRIPTCORE_GC_BEGIN();

    markRoots();

    JAVASCRIPTCORE_GC_MARKED();

    m_markedSpace.reset();
    m_markedSpace.sweep();
    m_extraCost = 0;

    JAVASCRIPTCORE_GC_END();

    (*m_activityCallback)();
}

LiveObjectIterator Heap::primaryHeapBegin()
{
    return m_markedSpace.primaryHeapBegin();
}

LiveObjectIterator Heap::primaryHeapEnd()
{
    return m_markedSpace.primaryHeapEnd();
}

void Heap::setActivityCallback(PassOwnPtr<GCActivityCallback> activityCallback)
{
    m_activityCallback = activityCallback;
}

GCActivityCallback* Heap::activityCallback()
{
    return m_activityCallback.get();
}

} // namespace JSC
