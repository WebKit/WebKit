/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Subspace.h>
#include <JavaScriptCore/SubspaceAccess.h>

namespace JSC {

// Unlike other Subspaces, PreciseSubspace doesn't support LocalAllocators as it's meant for large, rarely allocated objects
// that wouldn't be profitable to allocate directly in JIT code.
class PreciseSubspace : public Subspace {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(PreciseSubspace, JS_EXPORT_PRIVATE);
public:
    JS_EXPORT_PRIVATE PreciseSubspace(CString name, Heap&, const HeapCellType&, AlignedMemoryAllocator*);
    JS_EXPORT_PRIVATE ~PreciseSubspace() override;

    void* tryAllocate(size_t cellSize);
    void* allocate(VM&, size_t, GCDeferralContext*, AllocationFailureMode);

private:
    void didResizeBits(unsigned newSize) override;
    void didRemoveBlock(unsigned blockIndex) override;
    void didBeginSweepingToFreeList(MarkedBlock::Handle*) override;
};

namespace GCClient {
// This doesn't do anything interesting right now but we keep the GCClient namespace for consistency/templates.
using PreciseSubspace = JSC::PreciseSubspace;
}

} // namespace JSC

