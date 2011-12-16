/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "MarkStack.h"

#include "ConservativeRoots.h"
#include "Heap.h"
#include "Options.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSObject.h"
#include "ScopeChain.h"
#include "Structure.h"
#include "WriteBarrier.h"
#include <wtf/MainThread.h>

namespace JSC {

MarkStackSegmentAllocator::MarkStackSegmentAllocator()
    : m_nextFreeSegment(0)
{
}

MarkStackSegmentAllocator::~MarkStackSegmentAllocator()
{
    shrinkReserve();
}

MarkStackSegment* MarkStackSegmentAllocator::allocate()
{
    {
        MutexLocker locker(m_lock);
        if (m_nextFreeSegment) {
            MarkStackSegment* result = m_nextFreeSegment;
            m_nextFreeSegment = result->m_previous;
            return result;
        }
    }

    return static_cast<MarkStackSegment*>(OSAllocator::reserveAndCommit(Options::gcMarkStackSegmentSize));
}

void MarkStackSegmentAllocator::release(MarkStackSegment* segment)
{
    MutexLocker locker(m_lock);
    segment->m_previous = m_nextFreeSegment;
    m_nextFreeSegment = segment;
}

void MarkStackSegmentAllocator::shrinkReserve()
{
    MarkStackSegment* segments;
    {
        MutexLocker locker(m_lock);
        segments = m_nextFreeSegment;
        m_nextFreeSegment = 0;
    }
    while (segments) {
        MarkStackSegment* toFree = segments;
        segments = segments->m_previous;
        OSAllocator::decommitAndRelease(toFree, Options::gcMarkStackSegmentSize);
    }
}

MarkStackArray::MarkStackArray(MarkStackSegmentAllocator& allocator)
    : m_allocator(allocator)
    , m_segmentCapacity(MarkStackSegment::capacityFromSize(Options::gcMarkStackSegmentSize))
    , m_top(0)
    , m_numberOfPreviousSegments(0)
{
    m_topSegment = m_allocator.allocate();
#if !ASSERT_DISABLED
    m_topSegment->m_top = 0;
#endif
    m_topSegment->m_previous = 0;
}

MarkStackArray::~MarkStackArray()
{
    ASSERT(!m_topSegment->m_previous);
    m_allocator.release(m_topSegment);
}

void MarkStackArray::expand()
{
    ASSERT(m_topSegment->m_top == m_segmentCapacity);
    
    m_numberOfPreviousSegments++;
    
    MarkStackSegment* nextSegment = m_allocator.allocate();
#if !ASSERT_DISABLED
    nextSegment->m_top = 0;
#endif
    nextSegment->m_previous = m_topSegment;
    m_topSegment = nextSegment;
    setTopForEmptySegment();
    validatePrevious();
}

bool MarkStackArray::refill()
{
    validatePrevious();
    if (top())
        return true;
    MarkStackSegment* toFree = m_topSegment;
    MarkStackSegment* previous = m_topSegment->m_previous;
    if (!previous)
        return false;
    ASSERT(m_numberOfPreviousSegments);
    m_numberOfPreviousSegments--;
    m_topSegment = previous;
    m_allocator.release(toFree);
    setTopForFullSegment();
    validatePrevious();
    return true;
}

bool MarkStackArray::donateSomeCellsTo(MarkStackArray& other)
{
    ASSERT(m_segmentCapacity == other.m_segmentCapacity);
    validatePrevious();
    other.validatePrevious();
        
    // Fast check: see if the other mark stack already has enough segments.
    if (other.m_numberOfPreviousSegments + 1 >= Options::maximumNumberOfSharedSegments)
        return false;
        
    size_t numberOfCellsToKeep = Options::minimumNumberOfCellsToKeep;
    ASSERT(m_top > numberOfCellsToKeep || m_topSegment->m_previous);
        
    // Looks like we should donate! Give the other mark stack all of our
    // previous segments, and then top it off.
    MarkStackSegment* previous = m_topSegment->m_previous;
    while (previous) {
        ASSERT(m_numberOfPreviousSegments);

        MarkStackSegment* current = previous;
        previous = current->m_previous;
            
        current->m_previous = other.m_topSegment->m_previous;
        other.m_topSegment->m_previous = current;
            
        m_numberOfPreviousSegments--;
        other.m_numberOfPreviousSegments++;
    }
    ASSERT(!m_numberOfPreviousSegments);
    m_topSegment->m_previous = 0;
    validatePrevious();
    other.validatePrevious();
        
    // Now top off. We want to keep at a minimum numberOfCellsToKeep, but if
    // we really have a lot of work, we give up half.
    if (m_top > numberOfCellsToKeep * 2)
        numberOfCellsToKeep = m_top / 2;
    while (m_top > numberOfCellsToKeep)
        other.append(removeLast());
        
    return true;
}

void MarkStackArray::stealSomeCellsFrom(MarkStackArray& other)
{
    ASSERT(m_segmentCapacity == other.m_segmentCapacity);
    validatePrevious();
    other.validatePrevious();
        
    // If other has an entire segment, steal it and return.
    if (other.m_topSegment->m_previous) {
        ASSERT(other.m_topSegment->m_previous->m_top == m_segmentCapacity);
            
        // First remove a segment from other.
        MarkStackSegment* current = other.m_topSegment->m_previous;
        other.m_topSegment->m_previous = current->m_previous;
        other.m_numberOfPreviousSegments--;
            
        ASSERT(!!other.m_numberOfPreviousSegments == !!other.m_topSegment->m_previous);
            
        // Now add it to this.
        current->m_previous = m_topSegment->m_previous;
        m_topSegment->m_previous = current;
        m_numberOfPreviousSegments++;
            
        validatePrevious();
        other.validatePrevious();
        return;
    }
        
    // Otherwise drain 1/Nth of the shared array where N is the number of
    // workers, or Options::minimumNumberOfCellsToKeep, whichever is bigger.
    size_t numberOfCellsToSteal = std::max((size_t)Options::minimumNumberOfCellsToKeep, other.size() / Options::numberOfGCMarkers);
    while (numberOfCellsToSteal-- > 0 && other.canRemoveLast())
        append(other.removeLast());
}

#if ENABLE(PARALLEL_GC)
void MarkStackThreadSharedData::markingThreadMain()
{
    WTF::registerGCThread();
    SlotVisitor slotVisitor(*this);
    ParallelModeEnabler enabler(slotVisitor);
    slotVisitor.drainFromShared(SlotVisitor::SlaveDrain);
}

void* MarkStackThreadSharedData::markingThreadStartFunc(void* shared)
{
    static_cast<MarkStackThreadSharedData*>(shared)->markingThreadMain();
    return 0;
}
#endif

MarkStackThreadSharedData::MarkStackThreadSharedData(JSGlobalData* globalData)
    : m_globalData(globalData)
    , m_sharedMarkStack(m_segmentAllocator)
    , m_numberOfActiveParallelMarkers(0)
    , m_parallelMarkersShouldExit(false)
{
#if ENABLE(PARALLEL_GC)
    for (unsigned i = 1; i < Options::numberOfGCMarkers; ++i) {
        m_markingThreads.append(createThread(markingThreadStartFunc, this, "JavaScriptCore::Marking"));
        ASSERT(m_markingThreads.last());
    }
#endif
}

MarkStackThreadSharedData::~MarkStackThreadSharedData()
{
#if ENABLE(PARALLEL_GC)    
    // Destroy our marking threads.
    {
        MutexLocker locker(m_markingLock);
        m_parallelMarkersShouldExit = true;
        m_markingCondition.broadcast();
    }
    for (unsigned i = 0; i < m_markingThreads.size(); ++i)
        waitForThreadCompletion(m_markingThreads[i], 0);
#endif
}
    
void MarkStackThreadSharedData::reset()
{
    ASSERT(!m_numberOfActiveParallelMarkers);
    ASSERT(!m_parallelMarkersShouldExit);
    ASSERT(m_sharedMarkStack.isEmpty());
    
#if ENABLE(PARALLEL_GC)
    m_segmentAllocator.shrinkReserve();
    m_opaqueRoots.clear();
#else
    ASSERT(m_opaqueRoots.isEmpty());
#endif
    
    m_weakReferenceHarvesters.removeAll();
}

void MarkStack::reset()
{
    m_visitCount = 0;
    ASSERT(m_stack.isEmpty());
#if ENABLE(PARALLEL_GC)
    ASSERT(m_opaqueRoots.isEmpty()); // Should have merged by now.
#else
    m_opaqueRoots.clear();
#endif
}

void MarkStack::append(ConservativeRoots& conservativeRoots)
{
    JSCell** roots = conservativeRoots.roots();
    size_t size = conservativeRoots.size();
    for (size_t i = 0; i < size; ++i)
        internalAppend(roots[i]);
}

ALWAYS_INLINE static void visitChildren(SlotVisitor& visitor, const JSCell* cell)
{
#if ENABLE(SIMPLE_HEAP_PROFILING)
    m_visitedTypeCounts.count(cell);
#endif

    ASSERT(Heap::isMarked(cell));

    if (isJSString(cell)) {
        JSString::visitChildren(const_cast<JSCell*>(cell), visitor);
        return;
    }

    if (isJSFinalObject(cell)) {
        JSObject::visitChildren(const_cast<JSCell*>(cell), visitor);
        return;
    }

    if (isJSArray(cell)) {
        JSArray::visitChildren(const_cast<JSCell*>(cell), visitor);
        return;
    }

    cell->methodTable()->visitChildren(const_cast<JSCell*>(cell), visitor);
}

void SlotVisitor::donateSlow()
{
    // Refuse to donate if shared has more entries than I do.
    if (m_shared.m_sharedMarkStack.size() > m_stack.size())
        return;
    MutexLocker locker(m_shared.m_markingLock);
    if (m_stack.donateSomeCellsTo(m_shared.m_sharedMarkStack)) {
        // Only wake up threads if the shared stack is big enough; otherwise assume that
        // it's more profitable for us to just scan this ourselves later.
        if (m_shared.m_sharedMarkStack.size() >= Options::sharedStackWakeupThreshold)
            m_shared.m_markingCondition.broadcast();
    }
}

void SlotVisitor::drain()
{
    ASSERT(m_isInParallelMode);
    
#if ENABLE(PARALLEL_GC)
    if (Options::numberOfGCMarkers > 1) {
        while (!m_stack.isEmpty()) {
            m_stack.refill();
            for (unsigned countdown = Options::minimumNumberOfScansBetweenRebalance; m_stack.canRemoveLast() && countdown--;)
                visitChildren(*this, m_stack.removeLast());
            donateKnownParallel();
        }
        
        mergeOpaqueRootsIfNecessary();
        return;
    }
#endif
    
    while (!m_stack.isEmpty()) {
        m_stack.refill();
        while (m_stack.canRemoveLast())
            visitChildren(*this, m_stack.removeLast());
    }
}

void SlotVisitor::drainFromShared(SharedDrainMode sharedDrainMode)
{
    ASSERT(m_isInParallelMode);
    
    ASSERT(Options::numberOfGCMarkers);
    
    bool shouldBeParallel;

#if ENABLE(PARALLEL_GC)
    shouldBeParallel = Options::numberOfGCMarkers > 1;
#else
    ASSERT(Options::numberOfGCMarkers == 1);
    shouldBeParallel = false;
#endif
    
    if (!shouldBeParallel) {
        // This call should be a no-op.
        ASSERT_UNUSED(sharedDrainMode, sharedDrainMode == MasterDrain);
        ASSERT(m_stack.isEmpty());
        ASSERT(m_shared.m_sharedMarkStack.isEmpty());
        return;
    }
    
#if ENABLE(PARALLEL_GC)
    {
        MutexLocker locker(m_shared.m_markingLock);
        m_shared.m_numberOfActiveParallelMarkers++;
    }
    while (true) {
        {
            MutexLocker locker(m_shared.m_markingLock);
            m_shared.m_numberOfActiveParallelMarkers--;

            // How we wait differs depending on drain mode.
            if (sharedDrainMode == MasterDrain) {
                // Wait until either termination is reached, or until there is some work
                // for us to do.
                while (true) {
                    // Did we reach termination?
                    if (!m_shared.m_numberOfActiveParallelMarkers && m_shared.m_sharedMarkStack.isEmpty())
                        return;
                    
                    // Is there work to be done?
                    if (!m_shared.m_sharedMarkStack.isEmpty())
                        break;
                    
                    // Otherwise wait.
                    m_shared.m_markingCondition.wait(m_shared.m_markingLock);
                }
            } else {
                ASSERT(sharedDrainMode == SlaveDrain);
                
                // Did we detect termination? If so, let the master know.
                if (!m_shared.m_numberOfActiveParallelMarkers && m_shared.m_sharedMarkStack.isEmpty())
                    m_shared.m_markingCondition.broadcast();
                
                while (m_shared.m_sharedMarkStack.isEmpty() && !m_shared.m_parallelMarkersShouldExit)
                    m_shared.m_markingCondition.wait(m_shared.m_markingLock);
                
                // Is the VM exiting? If so, exit this thread.
                if (m_shared.m_parallelMarkersShouldExit)
                    return;
            }
            
            m_stack.stealSomeCellsFrom(m_shared.m_sharedMarkStack);
            m_shared.m_numberOfActiveParallelMarkers++;
        }
        
        drain();
    }
#endif
}

void MarkStack::mergeOpaqueRoots()
{
    ASSERT(!m_opaqueRoots.isEmpty()); // Should only be called when opaque roots are non-empty.
    {
        MutexLocker locker(m_shared.m_opaqueRootsLock);
        HashSet<void*>::iterator begin = m_opaqueRoots.begin();
        HashSet<void*>::iterator end = m_opaqueRoots.end();
        for (HashSet<void*>::iterator iter = begin; iter != end; ++iter)
            m_shared.m_opaqueRoots.add(*iter);
    }
    m_opaqueRoots.clear();
}

void SlotVisitor::harvestWeakReferences()
{
    for (WeakReferenceHarvester* current = m_shared.m_weakReferenceHarvesters.head(); current; current = current->next())
        current->visitWeakReferences(*this);
}

void SlotVisitor::finalizeUnconditionalFinalizers()
{
    while (m_shared.m_unconditionalFinalizers.hasNext())
        m_shared.m_unconditionalFinalizers.removeNext()->finalizeUnconditionally();
}

#if ENABLE(GC_VALIDATION)
void MarkStack::validate(JSCell* cell)
{
    if (!cell)
        CRASH();

    if (!cell->structure())
        CRASH();

    // Both the cell's structure, and the cell's structure's structure should be the Structure Structure.
    // I hate this sentence.
    if (cell->structure()->structure()->JSCell::classInfo() != cell->structure()->JSCell::classInfo())
        CRASH();
}
#else
void MarkStack::validate(JSCell*)
{
}
#endif

} // namespace JSC
