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

namespace JSC {
namespace DOMJIT {

struct Effect {
    constexpr static Effect forWrite(HeapRange writeRange)
    {
        return { HeapRange::none(), writeRange };
    }

    constexpr static Effect forRead(HeapRange readRange)
    {
        return { readRange, HeapRange::none() };
    }

    constexpr static Effect forReadKinds(DFG::AbstractHeapKind read1 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind read2 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind read3 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind read4 = DFG::InvalidAbstractHeap)
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), { read1, read2, read3, read4 } };
    }

    constexpr static Effect forWriteKinds(DFG::AbstractHeapKind write1 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind write2 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind write3 = DFG::InvalidAbstractHeap, DFG::AbstractHeapKind write4 = DFG::InvalidAbstractHeap)
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), {}, { write1, write2, write3, write4 } };
    }

    constexpr static Effect forReadWriteKinds(const DFG::AbstractHeapKind reads[4], const DFG::AbstractHeapKind writes[4])
    {
        return { HeapRange::none(), HeapRange::none(), HeapRange::none(), { reads[0], reads[1], reads[2], reads[3] }, { writes[0], writes[1], writes[2], writes[3] } };
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
        return !!domWrites;
    }

    constexpr bool isTop() const
    {
        return domWrites == HeapRange::top() ||
            writes[0] == DFG::Heap ||
            writes[1] == DFG::Heap ||
            writes[2] == DFG::Heap ||
            writes[3] == DFG::Heap;
    }

    HeapRange domReads { HeapRange::top() };
    HeapRange domWrites { HeapRange::top() };
    HeapRange def { HeapRange::top() };
    DFG::AbstractHeapKind reads[4] { DFG::InvalidAbstractHeap };
    DFG::AbstractHeapKind writes[4] { DFG::InvalidAbstractHeap };
};

}
}
