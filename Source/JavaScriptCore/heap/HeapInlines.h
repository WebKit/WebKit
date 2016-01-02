/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HeapInlines_h
#define HeapInlines_h

#include "CopyBarrier.h"
#include "Heap.h"
#include "JSCell.h"
#include "Structure.h"
#include <type_traits>
#include <wtf/Assertions.h>

namespace JSC {

inline bool Heap::shouldCollect()
{
    if (isDeferred())
        return false;
    if (!m_isSafeToCollect)
        return false;
    if (m_operationInProgress != NoOperation)
        return false;
    if (Options::gcMaxHeapSize())
        return m_bytesAllocatedThisCycle > Options::gcMaxHeapSize();
    return m_bytesAllocatedThisCycle > m_maxEdenSize;
}

inline bool Heap::isBusy()
{
    return m_operationInProgress != NoOperation;
}

inline bool Heap::isCollecting()
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

inline void Heap::writeBarrier(const JSCell* from, JSValue to)
{
#if ENABLE(WRITE_BARRIER_PROFILING)
    WriteBarrierCounters::countWriteBarrier();
#endif
    if (!to.isCell())
        return;
    writeBarrier(from, to.asCell());
}

inline void Heap::writeBarrier(const JSCell* from, JSCell* to)
{
#if ENABLE(WRITE_BARRIER_PROFILING)
    WriteBarrierCounters::countWriteBarrier();
#endif
    if (!from || from->cellState() != CellState::OldBlack)
        return;
    if (!to || to->cellState() != CellState::NewWhite)
        return;
    addToRememberedSet(from);
}

inline void Heap::writeBarrier(const JSCell* from)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(const_cast<JSCell*>(from));
    if (!from || from->cellState() != CellState::OldBlack)
        return;
    addToRememberedSet(from);
}

inline void Heap::reportExtraMemoryAllocated(size_t size)
{
    if (size > minExtraMemory) 
        reportExtraMemoryAllocatedSlowCase(size);
}

inline void Heap::reportExtraMemoryVisited(CellState dataBeforeVisiting, size_t size)
{
    // We don't want to double-count the extra memory that was reported in previous collections.
    if (operationInProgress() == EdenCollection && dataBeforeVisiting == CellState::OldGrey)
        return;

    size_t* counter = &m_extraMemorySize;
    
    for (;;) {
        size_t oldSize = *counter;
        if (WTF::weakCompareAndSwap(counter, oldSize, oldSize + size))
            return;
    }
}

inline void Heap::deprecatedReportExtraMemory(size_t size)
{
    if (size > minExtraMemory) 
        deprecatedReportExtraMemorySlowCase(size);
}

template<typename Functor> inline void Heap::forEachCodeBlock(Functor& functor)
{
    // We don't know the full set of CodeBlocks until compilation has terminated.
    completeAllDFGPlans();

    return m_codeBlocks.iterate<Functor>(functor);
}

template<typename Functor> inline typename Functor::ReturnType Heap::forEachProtectedCell(Functor& functor)
{
    for (auto& pair : m_protectedValues)
        functor(pair.key);
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
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes with normal destructor.\n", bytes);
#endif
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithDestructor(bytes);
}

inline void* Heap::allocateWithoutDestructor(size_t bytes)
{
#if ENABLE(ALLOCATION_LOGGING)
    dataLogF("JSC GC allocating %lu bytes without destructor.\n", bytes);
#endif
    ASSERT(isValidAllocation(bytes));
    return m_objectSpace.allocateWithoutDestructor(bytes);
}

template<typename ClassType>
void* Heap::allocateObjectOfType(size_t bytes)
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));

    if (ClassType::needsDestruction)
        return allocateWithDestructor(bytes);
    return allocateWithoutDestructor(bytes);
}

template<typename ClassType>
MarkedSpace::Subspace& Heap::subspaceForObjectOfType()
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));
    
    if (ClassType::needsDestruction)
        return subspaceForObjectDestructor();
    return subspaceForObjectWithoutDestructor();
}

template<typename ClassType>
MarkedAllocator& Heap::allocatorForObjectOfType(size_t bytes)
{
    // JSCell::classInfo() expects objects allocated with normal destructor to derive from JSDestructibleObject.
    ASSERT((!ClassType::needsDestruction || (ClassType::StructureFlags & StructureIsImmortal) || std::is_convertible<ClassType, JSDestructibleObject>::value));
    
    if (ClassType::needsDestruction)
        return allocatorForObjectWithDestructor(bytes);
    return allocatorForObjectWithoutDestructor(bytes);
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

#if USE(CF)
template <typename T>
inline void Heap::releaseSoon(RetainPtr<T>&& object)
{
    m_delayedReleaseObjects.append(WTFMove(object));
}
#endif

inline void Heap::incrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth < 100); // Sanity check to make sure this doesn't get ridiculous.
    m_deferralDepth++;
}

inline void Heap::decrementDeferralDepth()
{
    RELEASE_ASSERT(m_deferralDepth >= 1);
    m_deferralDepth--;
}

inline bool Heap::collectIfNecessaryOrDefer()
{
    if (!shouldCollect())
        return false;

    collect();
    return true;
}

inline void Heap::decrementDeferralDepthAndGCIfNeeded()
{
    decrementDeferralDepth();
    collectIfNecessaryOrDefer();
}

inline HashSet<MarkedArgumentBuffer*>& Heap::markListSet()
{
    if (!m_markListSet)
        m_markListSet = std::make_unique<HashSet<MarkedArgumentBuffer*>>();
    return *m_markListSet;
}

inline void Heap::registerWeakGCMap(void* weakGCMap, std::function<void()> pruningCallback)
{
    m_weakGCMaps.add(weakGCMap, WTFMove(pruningCallback));
}

inline void Heap::unregisterWeakGCMap(void* weakGCMap)
{
    m_weakGCMaps.remove(weakGCMap);
}

inline void Heap::didAllocateBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE_OVERLAY)
    m_blockBytesAllocated += capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

inline void Heap::didFreeBlock(size_t capacity)
{
#if ENABLE(RESOURCE_USAGE_OVERLAY)
    m_blockBytesAllocated -= capacity;
#else
    UNUSED_PARAM(capacity);
#endif
}

} // namespace JSC

#endif // HeapInlines_h
