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

namespace JSC {

inline bool MarkedBlock::Handle::isLive(HeapVersion version, const HeapCell* cell)
{
    ASSERT(!isFreeListed());
    
    if (UNLIKELY(hasAnyNewlyAllocated())) {
        if (isNewlyAllocated(cell))
            return true;
    }
    
    MarkedBlock& block = this->block();
    
    if (allocator()->isAllocated(this))
        return true;
    
    if (block.needsFlip(version))
        return false;

    return block.isMarked(cell);
}

inline bool MarkedBlock::Handle::isLiveCell(HeapVersion version, const void* p)
{
    if (!m_block->isAtom(p))
        return false;
    return isLive(version, static_cast<const HeapCell*>(p));
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

inline void MarkedBlock::resetVersion()
{
    m_version = MarkedSpace::nullVersion;
}

} // namespace JSC

