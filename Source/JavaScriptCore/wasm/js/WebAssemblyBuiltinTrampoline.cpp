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

#include "config.h"
#include "WebAssemblyBuiltinTrampoline.h"

#if ENABLE(WEBASSEMBLY) && ENABLE(JIT)

#include "JSWebAssemblyInstance.h"
#include "WasmBinding.h"
#include "WasmOperations.h"

namespace JSC {
namespace Wasm {

using JIT = CCallHelpers;

// When JIT is enabled, this generates a function that lives in the JIT arena and serves as a gateway
// into the builtin implementation. When JIT is disabled, the call path instead uses one of the static
// trampolines defined in InPlaceInterpreter.asm.
//
// IMPORTANT: Any changes to the trampoline here should be replicated in its static counterparts in InPlaceInterpreter.asm.

Expected<MacroAssemblerCodeRef<WasmEntryPtrTag>, BindingFailure> generateWasmBuiltinTrampoline(const WebAssemblyBuiltin& builtin)
{
    auto scratch = GPRInfo::regT5;
    size_t arity = builtin.signature().numParams();
    ASSERT(arity <= 4); // r5 is scratch leaving r0-r4 for args, one of which is wasmInstance, so at most 4 for the builtin itself
    auto extraArgGPR = GPRInfo::toArgumentRegister(arity);

    ASSERT(scratch != GPRReg::InvalidGPRReg);
    ASSERT(noOverlap(scratch, GPRInfo::wasmContextInstancePointer));

    JIT jit;
    jit.emitFunctionPrologue();

    // IPInt stores the callee and wasmInstance into the frame but JIT tiers don't, so we must do that here.
    jit.move(GPRInfo::wasmContextInstancePointer, scratch);
    const size_t builtinEntryByteOffset = JSWebAssemblyInstance::offsetOfBuiltinCalleeBits() + builtin.id() * sizeof(CalleeBits);
    jit.loadPtr(JIT::Address(scratch, builtinEntryByteOffset), scratch);
    jit.storePairPtr(GPRInfo::wasmContextInstancePointer, scratch, GPRInfo::callFrameRegister, JIT::TrustedImm32(CallFrameSlot::codeBlock * sizeof(Register)));

    // Set VM topCallFrame to null to not build an unnecessary stack trace if the function throws an exception.
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), scratch);
    jit.storePtr(JIT::TrustedImmPtr(nullptr), JIT::Address(scratch, VM::offsetOfTopCallFrame()));

    // Add wasmInstance as the extra arg
    jit.move(GPRInfo::wasmContextInstancePointer, extraArgGPR);
    CodePtr<OperationPtrTag> entrypointAsOperation = builtin.wasmEntrypoint().template retagged<OperationPtrTag>();
    jit.callOperation<OperationPtrTag>(entrypointAsOperation);

    // Check for an exception and branch if present
    jit.loadPtr(JIT::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), scratch);
    auto handleException = jit.branchTestPtr(JIT::ResultCondition::NonZero, JIT::Address(scratch, VM::exceptionOffset()));

#if CPU(X86)
    // Wasm always expects the return value in a0 which on x86 is not the same as r0
    jit.move(GPRInfo::returnValueGPR, GPRInfo::argumentGPR0);
#endif

    jit.emitFunctionEpilogue();
    jit.ret();

    // Handle the exception
    handleException.link(jit);
    jit.loadPtr(CCallHelpers::Address(GPRInfo::wasmContextInstancePointer, JSWebAssemblyInstance::offsetOfVM()), GPRInfo::argumentGPR0);
    jit.copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRInfo::argumentGPR0);
    jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);
    jit.setupArguments<decltype(operationWasmUnwind)>(GPRInfo::wasmContextInstancePointer);
    jit.callOperation<OperationPtrTag>(operationWasmUnwind);
    jit.farJump(GPRInfo::returnValueGPR, ExceptionHandlerPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::WasmThunk, JITCompilationMustSucceed);
    if (patchBuffer.didFailToAllocate()) [[unlikely]]
        return makeUnexpected(BindingFailure::OutOfMemory);

    return FINALIZE_WASM_CODE(patchBuffer, WasmEntryPtrTag, nullptr, "WebAssemblyBuiltinThunk [%s]", builtin.name());
}


} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY) && ENABLE(JIT)
