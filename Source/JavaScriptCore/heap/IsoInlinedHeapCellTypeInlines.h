/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "IsoInlinedHeapCellType.h"

#include "MarkedBlock.h"

namespace JSC {

template<typename CellType>
inline IsoInlinedHeapCellType<CellType>::IsoInlinedHeapCellType()
    : HeapCellType(CellAttributes(CellType::needsDestruction ? NeedsDestruction : DoesNotNeedDestruction, HeapCell::JSCell))
{
}

template<typename CellType>
ALWAYS_INLINE void IsoInlinedHeapCellType<CellType>::DestroyFunc::operator()(VM&, JSCell* cell) const
{
    CellType::destroy(cell);
}

template<typename CellType>
inline void IsoInlinedHeapCellType<CellType>::finishSweep(MarkedBlock::Handle& handle, FreeList* freeList) const
{
    handle.finishSweepKnowingHeapCellType(freeList, DestroyFunc());
}

template<typename CellType>
inline void IsoInlinedHeapCellType<CellType>::destroy(VM&, JSCell* cell) const
{
    CellType::destroy(cell);
}

} // namespace JSC
