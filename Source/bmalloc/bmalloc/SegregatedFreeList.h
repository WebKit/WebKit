/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef SegregatedFreeList_h
#define SegregatedFreeList_h

#include "FreeList.h"
#include <array>

namespace bmalloc {

class SegregatedFreeList {
public:
    SegregatedFreeList(VMState::HasPhysical);

    void insert(const LargeObject&);

    // Returns a reasonable fit for the provided size, or LargeObject() if no fit
    // is found. May return LargeObject() spuriously if searching takes too long.
    // Incrementally removes stale items from the free list while searching.
    // Does not eagerly remove the returned object from the free list.
    LargeObject take(size_t);

    // Returns a reasonable fit for the provided alignment and size, or
    // a reasonable fit for the provided unaligned size, or LargeObject() if no
    // fit is found. May return LargeObject() spuriously if searching takes too
    // long. Incrementally removes stale items from the free list while
    // searching. Does not eagerly remove the returned object from the free list.
    LargeObject take(size_t alignment, size_t, size_t unalignedSize);

    // Returns an unreasonable fit for the provided size, or LargeObject() if no
    // fit is found. Never returns LargeObject() spuriously. Incrementally
    // removes stale items from the free list while searching. Eagerly removes
    // the returned object from the free list.
    LargeObject takeGreedy();

    VMState::HasPhysical hasPhysical() const { return m_hasPhysical; }
private:
    FreeList& select(size_t);

    VMState::HasPhysical m_hasPhysical;
    std::array<FreeList, 15> m_freeLists;
};

} // namespace bmalloc

#endif // SegregatedFreeList_h
