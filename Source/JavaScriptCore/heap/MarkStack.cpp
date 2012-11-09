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
#include "MarkStackInlines.h"

#include "ConservativeRoots.h"
#include "CopiedSpace.h"
#include "CopiedSpaceInlines.h"
#include "Heap.h"
#include "Options.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSObject.h"

#include "SlotVisitorInlines.h"
#include "Structure.h"
#include "WriteBarrier.h"
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/MainThread.h>

namespace JSC {

MarkStackSegmentAllocator::MarkStackSegmentAllocator()
    : m_nextFreeSegment(0)
{
    m_lock.Init();
}

MarkStackSegmentAllocator::~MarkStackSegmentAllocator()
{
    shrinkReserve();
}

MarkStackSegment* MarkStackSegmentAllocator::allocate()
{
    {
        SpinLockHolder locker(&m_lock);
        if (m_nextFreeSegment) {
            MarkStackSegment* result = m_nextFreeSegment;
            m_nextFreeSegment = result->m_previous;
            return result;
        }
    }

    return static_cast<MarkStackSegment*>(OSAllocator::reserveAndCommit(Options::gcMarkStackSegmentSize()));
}

void MarkStackSegmentAllocator::release(MarkStackSegment* segment)
{
    SpinLockHolder locker(&m_lock);
    segment->m_previous = m_nextFreeSegment;
    m_nextFreeSegment = segment;
}

void MarkStackSegmentAllocator::shrinkReserve()
{
    MarkStackSegment* segments;
    {
        SpinLockHolder locker(&m_lock);
        segments = m_nextFreeSegment;
        m_nextFreeSegment = 0;
    }
    while (segments) {
        MarkStackSegment* toFree = segments;
        segments = segments->m_previous;
        OSAllocator::decommitAndRelease(toFree, Options::gcMarkStackSegmentSize());
    }
}

MarkStackArray::MarkStackArray(MarkStackSegmentAllocator& allocator)
    : m_allocator(allocator)
    , m_segmentCapacity(MarkStackSegment::capacityFromSize(Options::gcMarkStackSegmentSize()))
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

void MarkStackArray::donateSomeCellsTo(MarkStackArray& other)
{
    // Try to donate about 1 / 2 of our cells. To reduce copying costs,
    // we prefer donating whole segments over donating individual cells,
    // even if this skews away from our 1 / 2 target.

    ASSERT(m_segmentCapacity == other.m_segmentCapacity);

    size_t segmentsToDonate = (m_numberOfPreviousSegments + 2 - 1) / 2; // Round up to donate 1 / 1 previous segments.

    if (!segmentsToDonate) {
        size_t cellsToDonate = m_top / 2; // Round down to donate 0 / 1 cells.
        while (cellsToDonate--) {
            ASSERT(m_top);
            other.append(removeLast());
        }
        return;
    }

    validatePrevious();
    other.validatePrevious();

    MarkStackSegment* previous = m_topSegment->m_previous;
    while (segmentsToDonate--) {
        ASSERT(previous);
        ASSERT(m_numberOfPreviousSegments);

        MarkStackSegment* current = previous;
        previous = current->m_previous;
            
        current->m_previous = other.m_topSegment->m_previous;
        other.m_topSegment->m_previous = current;
            
        m_numberOfPreviousSegments--;
        other.m_numberOfPreviousSegments++;
    }
    m_topSegment->m_previous = previous;

    validatePrevious();
    other.validatePrevious();
}

void MarkStackArray::stealSomeCellsFrom(MarkStackArray& other, size_t idleThreadCount)
{
    // Try to steal 1 / Nth of the shared array, where N is the number of idle threads.
    // To reduce copying costs, we prefer stealing a whole segment over stealing
    // individual cells, even if this skews away from our 1 / N target.

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

    size_t numberOfCellsToSteal = (other.size() + idleThreadCount - 1) / idleThreadCount; // Round up to steal 1 / 1.
    while (numberOfCellsToSteal-- > 0 && other.canRemoveLast())
        append(other.removeLast());
}

} // namespace JSC
