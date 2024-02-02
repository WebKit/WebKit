/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "CCallHelpers.h"
#include "DisallowMacroScratchRegisterUsage.h"
#include "LinkBuffer.h"
#include "WasmCallingConvention.h"
#include "WasmInstance.h"

namespace JSC { namespace Wasm {

using JIT = CCallHelpers;

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> wasmToWasm(unsigned importIndex)
{
    // FIXME: Consider uniquify the stubs based on signature + index to see if this saves memory.
    // https://bugs.webkit.org/show_bug.cgi?id=184157
    JIT jit;

    GPRReg scratch = wasmCallingConvention().prologueScratchGPRs[0];
    ASSERT(scratch != GPRReg::InvalidGPRReg);
    ASSERT(noOverlap(scratch, GPRInfo::wasmContextInstancePointer));

    JIT_COMMENT(jit, "Store Callee's wasm callee for import function ", importIndex);
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfBoxedTargetCalleeLoadLocation(importIndex)), scratch);
    jit.loadPtr(JIT::Address(scratch), scratch);
    // We are halfway between being the caller and the callee: we have already made the call, but not yet completed the prologue.
    // On ARM64 this doesn't really matter, but on intel we need to worry about the pushed pc.
    jit.storeWasmCalleeCallee(scratch, safeCast<int>(sizeof(CallerFrameAndPC)) - safeCast<int>(prologueStackPointerDelta()));

    // B3's call codegen ensures that the JSCell is a WebAssemblyFunction.
    // While we're accessing that cacheline, also get the wasm entrypoint so we can tail call to it below.
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfWasmEntrypointLoadLocation(importIndex)), scratch);
    // Get the callee's Wasm::Instance and set it as WasmContext's instance. The caller will take care of restoring its own Instance.
    // This switches the current instance.
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, Instance::offsetOfTargetInstance(importIndex)), GPRInfo::wasmContextInstancePointer); // Instance*.

#if !CPU(ARM) // ARM has no pinned registers for Wasm Memory, so no need to set them up
    // FIXME the following code assumes that all Wasm::Instance have the same pinned registers. https://bugs.webkit.org/show_bug.cgi?id=162952
    // Set up the callee's baseMemoryPointer register as well as the memory size registers.
    {
        jit.loadPairPtr(GPRInfo::wasmContextInstancePointer, CCallHelpers::TrustedImm32(Wasm::Instance::offsetOfCachedMemory()), GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister);
        jit.cageConditionally(Gigacage::Primitive, GPRInfo::wasmBaseMemoryPointer, GPRInfo::wasmBoundsCheckingSizeRegister, wasmCallingConvention().prologueScratchGPRs[1]);
    }
#endif

    // Tail call into the callee WebAssembly function.
    jit.loadPtr(JIT::Address(scratch), scratch);
    jit.farJump(scratch, WasmEntryPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationCanFail);
    if (UNLIKELY(patchBuffer.didFailToAllocate()))
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_WASM_CODE(patchBuffer, WasmEntryPtrTag, "WebAssembly->WebAssembly import[%i]", importIndex);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
