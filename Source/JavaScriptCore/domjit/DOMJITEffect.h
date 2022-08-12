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

#include "DFGAbstractHeap.h"
#include "DOMJITHeapRange.h"

namespace JSC { namespace DOMJIT {

struct Effect {
    constexpr static Effect forWrite(HeapRange writeRange)
    {
        return { HeapRange::none(), writeRange };
    }

    constexpr static Effect forRead(HeapRange readRange)
    {
        return { readRange, HeapRange::none() };
    }

    template<uint8_t N>
    constexpr static Effect forReadDFG(const DFG::AbstractHeapKind* kinds)
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), kinds, N };
    }

    template<uint8_t N>
    constexpr static Effect forWriteDFG(const DFG::AbstractHeapKind* kinds)
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), nullptr, 0, kinds, N };
    }

    template<uint8_t N, uint8_t M>
    constexpr static Effect forReadWriteDFG(const DFG::AbstractHeapKind* reads, const DFG::AbstractHeapKind* writes)
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), reads, N, writes, M };
    }

    constexpr static Effect forReadWrite(HeapRange readRange, HeapRange writeRange)
    {
        return { readRange, writeRange };
    }

    constexpr static Effect forPure()
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none() };
    }

    constexpr static Effect forDef(HeapRange def)
    {
        return { def, HeapRange::none(), def };
    }

    constexpr static Effect forDef(HeapRange def, HeapRange readRange, HeapRange writeRange)
    {
        return { readRange, writeRange, def };
    }

    constexpr bool mustGenerate() const
    {
        return !!writes;
    }

    HeapRange reads { HeapRange::top() };
    HeapRange writes { HeapRange::top() };
    HeapRange def { HeapRange::top() };
    const DFG::AbstractHeapKind* readsKind { nullptr };
    uint8_t readsLen { 0 };
    const DFG::AbstractHeapKind* writesKind { nullptr };
    uint8_t writesLen { 0 };
};

} }
