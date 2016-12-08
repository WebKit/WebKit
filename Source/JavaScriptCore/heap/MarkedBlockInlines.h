/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "MarkedAllocator.h"
#include "MarkedBlock.h"
#include "MarkedSpace.h"

namespace JSC {

inline unsigned MarkedBlock::Handle::cellsPerBlock()
{
    return MarkedSpace::blockPayload / cellSize();
}

inline bool MarkedBlock::marksConveyLivenessDuringMarking(HeapVersion markingVersion)
{
    // This returns true if any of these is true:
    // - We just created the block and so the bits are clear already.
    // - This block has objects marked during the last GC, and so its version was up-to-date just
    //   before the current collection did beginMarking(). This means that any objects that have 
    //   their mark bit set are valid objects that were never deleted, and so are candidates for
    //   marking in any conservative scan. Using our jargon, they are "live".
    // - We did ~2^32 collections and rotated the version back to null, so we needed to hard-reset
    //   everything. If the marks had been stale, we would have cleared them. So, we can be sure that
    //   any set mark bit reflects objects marked during last GC, i.e. "live" objects.
    // It would be absurd to use this method when not collecting, since this special "one version
    // back" state only makes sense when we're in a concurrent collection and have to be
    // conservative.
    ASSERT(space()->isMarking());
    return m_markingVersion == MarkedSpace::nullVersion
        || MarkedSpace::nextVersion(m_markingVersion) == markingVersion;
}

inline bool MarkedBlock::Handle::isLive(HeapVersion markingVersion, bool isMarking, const HeapCell* cell)
{
    ASSERT(!isFreeListed());
    
    if (UNLIKELY(hasAnyNewlyAllocated())) {
        if (isNewlyAllocated(cell))
            return true;
    }
    
    if (allocator()->isAllocated(NoLockingNecessary, this))
        return true;
    
    MarkedBlock& block = this->block();
    
    if (block.areMarksStale()) {
        if (!isMarking)
            return false;
        if (!block.marksConveyLivenessDuringMarking(markingVersion))
            return false;
    }

    return block.m_marks.get(block.atomNumber(cell));
}

inline bool MarkedBlock::Handle::isLiveCell(HeapVersion markingVersion, bool isMarking, const void* p)
{
    if (!m_block->isAtom(p))
        return false;
    return isLive(markingVersion, isMarking, static_cast<const HeapCell*>(p));
}

inline bool MarkedBlock::Handle::isNewlyAllocatedStale() const
{
    return m_newlyAllocatedVersion != space()->newlyAllocatedVersion();
}

inline bool MarkedBlock::Handle::hasAnyNewlyAllocated()
{
    return !isNewlyAllocatedStale();
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachLiveCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (!isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template <typename Functor>
inline IterationStatus MarkedBlock::Handle::forEachDeadCell(const Functor& functor)
{
    HeapCell::Kind kind = m_attributes.cellKind;
    for (size_t i = firstAtom(); i < m_endAtom; i += m_atomsPerCell) {
        HeapCell* cell = reinterpret_cast_ptr<HeapCell*>(&m_block->atoms()[i]);
        if (isLive(cell))
            continue;

        if (functor(cell, kind) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

inline Heap* MarkedBlock::heap() const
{
    return &vm()->heap;
}

inline MarkedSpace* MarkedBlock::space() const
{
    return &heap()->objectSpace();
}

inline MarkedSpace* MarkedBlock::Handle::space() const
{
    return &heap()->objectSpace();
}

} // namespace JSC

