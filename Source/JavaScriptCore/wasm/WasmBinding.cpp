/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WasmBinding.h"

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "WasmInstance.h"

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToWasm(unsigned importIndex)
{
    // FIXME: Consider uniquify the stubs based on signature + index to see if this saves memory.
    // https://bugs.webkit.org/show_bug.cgi?id=184157

    const PinnedRegisterInfo& pinnedRegs = PinnedRegisterInfo::get();
    JIT jit;

    GPRReg scratch = GPRInfo::nonPreservedNonArgumentGPR0;
    GPRReg baseMemory = pinnedRegs.baseMemoryPointer;
    ASSERT(baseMemory != scratch);
    const auto& sizeRegs = pinnedRegs.sizeRegisters;
    ASSERT(sizeRegs.size() >= 1);
    ASSERT(sizeRegs[0].sizeRegister != baseMemory);
    ASSERT(sizeRegs[0].sizeRegister != scratch);
    GPRReg sizeRegAsScratch = sizeRegs[0].sizeRegister;

    // B3's call codegen ensures that the JSCell is a WebAssemblyFunction.
    jit.loadWasmContextInstance(sizeRegAsScratch); // Old Instance*
    // Get the callee's Wasm::Instance and set it as WasmContext's instance. The caller will take care of restoring its own Instance.
    jit.loadPtr(JIT::Address(sizeRegAsScratch, Instance::offsetOfTargetInstance(importIndex)), baseMemory); // Instance*.
    // While we're accessing that cacheline, also get the wasm entrypoint so we can tail call to it below.
    jit.loadPtr(JIT::Address(sizeRegAsScratch, Instance::offsetOfWasmEntrypointLoadLocation(importIndex)), scratch);
    jit.storeWasmContextInstance(baseMemory);

    jit.loadPtr(JIT::Address(sizeRegAsScratch, Instance::offsetOfCachedStackLimit()), sizeRegAsScratch);
    jit.storePtr(sizeRegAsScratch, JIT::Address(baseMemory, Instance::offsetOfCachedStackLimit()));

    // FIXME the following code assumes that all Wasm::Instance have the same pinned registers. https://bugs.webkit.org/show_bug.cgi?id=162952
    // Set up the callee's baseMemory register as well as the memory size registers.
    ASSERT(!sizeRegs[0].sizeOffset); // The following code assumes we start at 0, and calculates subsequent size registers relative to 0.
    jit.loadPtr(JIT::Address(baseMemory, Wasm::Instance::offsetOfCachedMemorySize()), sizeRegs[0].sizeRegister); // Memory size.
    jit.loadPtr(JIT::Address(baseMemory, Wasm::Instance::offsetOfCachedMemory()), baseMemory); // Wasm::Memory::void*.
    for (unsigned i = 1; i < sizeRegs.size(); ++i) {
        ASSERT(sizeRegs[i].sizeRegister != baseMemory);
        ASSERT(sizeRegs[i].sizeRegister != scratch);
        jit.add64(JIT::TrustedImm32(-sizeRegs[i].sizeOffset), sizeRegs[0].sizeRegister, sizeRegs[i].sizeRegister);
    }

    // Tail call into the callee WebAssembly function.
    jit.loadPtr(scratch, scratch);
    jit.jump(scratch, WasmEntryPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, JITCompilationCanFail);
    if (UNLIKELY(patchBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_CODE(patchBuffer, WasmEntryPtrTag, "WebAssembly->WebAssembly import[%i]", importIndex);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
