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

#include "CellContainer.h"
#include "JSCell.h"
#include "LargeAllocation.h"
#include "MarkedBlock.h"

namespace JSC {

inline bool CellContainer::isMarked() const
{
    if (isLargeAllocation())
        return true;
    return markedBlock().handle().isMarked();
}

inline bool CellContainer::isMarked(HeapCell* cell) const
{
    if (isLargeAllocation())
        return largeAllocation().isMarked();
    return markedBlock().isMarked(cell);
}

inline bool CellContainer::isMarkedOrNewlyAllocated(HeapCell* cell) const
{
    if (isLargeAllocation())
        return largeAllocation().isMarkedOrNewlyAllocated();
    return markedBlock().isMarkedOrNewlyAllocated(cell);
}

inline void CellContainer::noteMarked()
{
    if (!isLargeAllocation())
        markedBlock().noteMarked();
}

inline size_t CellContainer::cellSize() const
{
    if (isLargeAllocation())
        return largeAllocation().cellSize();
    return markedBlock().cellSize();
}

inline WeakSet& CellContainer::weakSet() const
{
    if (isLargeAllocation())
        return largeAllocation().weakSet();
    return markedBlock().weakSet();
}

inline void CellContainer::flipIfNecessary(HeapVersion heapVersion)
{
    if (!isLargeAllocation())
        markedBlock().flipIfNecessary(heapVersion);
}

inline void CellContainer::flipIfNecessary()
{
    if (!isLargeAllocation())
        markedBlock().flipIfNecessary();
}

} // namespace JSC

