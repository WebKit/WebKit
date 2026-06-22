/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3CCallValue.h"
#include "B3Opcode.h"
#include "B3StackmapValue.h"
#include "B3Value.h"

namespace JSC { namespace B3 {

inline bool isStoreBarrier(const Value* value)
{
    Opcode op = value->opcode();
    return op == StoreBarrier || op == FencedStoreBarrier;
}

// Returns true if the wasm allocator opcodes WasmStructNew / WasmArrayNew produced this
// value. Their result is a freshly-allocated young object whose cellState is known to be
// in the "no-barrier-needed" range, so the StoreBarrierElisionPhase can elide barriers
// targeting this value.
inline bool isFreshAllocation(const Value* value)
{
    switch (value->opcode()) {
    case WasmStructNew:
    case WasmArrayNew:
        return true;
    default:
        return false;
    }
}

// Returns true if executing this value may trigger garbage collection (any allocator,
// any C call that could allocate or run JS, any patchpoint that exits sideways). The
// StoreBarrierElisionPhase bumps the epoch on each true answer so that allocations
// performed before a GC point lose their "fresh" status. The clustering phase uses the
// same predicate to terminate clusters.
inline bool mayGC(const Value* value)
{
    switch (value->opcode()) {
    case WasmStructNew:
    case WasmArrayNew:
        // Allocators may take the slow path and trigger GC themselves.
        return true;
    case CCall:
        // Conservative: any C call could allocate or invoke runtime code that GCs.
        return true;
    case Patchpoint:
        // Patchpoints with side-exits might call into runtime code.
        return value->effects().exitsSideways;
    default:
        return false;
    }
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
