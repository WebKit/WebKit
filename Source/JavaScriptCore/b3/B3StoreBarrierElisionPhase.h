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

namespace JSC { namespace B3 {

class Procedure;

// Drops StoreBarrier and FencedStoreBarrier nodes whose base is provably a young object
// allocated since the last GC point in the same basic block. Mirrors the analysis half of
// DFGStoreBarrierInsertionPhase, run inverted: the wasm OMG IR generator emits a
// FencedStoreBarrier at every site that could need one, and this phase removes the ones
// that don't.
//
// Each value carries an epoch. The current epoch is bumped at every may-GC point. A fresh
// allocation (WasmStructNew / WasmArrayNew) records its result in the post-bump epoch.
// A barrier whose cell is in the current epoch is redundant and replaced with Nop.
//
// V1 limitation: epoch state is not propagated across basic block boundaries, so cross-
// block elision is not performed. Same-block elision still wins for the common pattern
// of allocate-then-fill on a fresh object.
bool storeBarrierElision(Procedure&);

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
