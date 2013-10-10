/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "FTLThunks.h"

#if ENABLE(FTL_JIT)

#include "AssemblyHelpers.h"
#include "FPRInfo.h"
#include "FTLOSRExitCompiler.h"
#include "FTLSaveRestore.h"
#include "GPRInfo.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

MacroAssemblerCodeRef osrExitGenerationWithoutStackMapThunkGenerator(VM* vm)
{
    AssemblyHelpers jit(vm, 0);
    
    // Pretend that we're a C call frame.
    jit.push(MacroAssembler::framePointerRegister);
    jit.move(MacroAssembler::stackPointerRegister, MacroAssembler::framePointerRegister);
    jit.push(GPRInfo::regT0);
    jit.push(GPRInfo::regT0);
    
    jit.poke(GPRInfo::nonArgGPR0, 1);
    
    ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(requiredScratchMemorySizeInBytes());
    char* buffer = static_cast<char*>(scratchBuffer->dataBuffer());
    
    saveAllRegisters(jit, buffer);
    
    // Tell GC mark phase how much of the scratch buffer is active during call.
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::nonArgGPR1);
    jit.storePtr(MacroAssembler::TrustedImmPtr(requiredScratchMemorySizeInBytes()), GPRInfo::nonArgGPR1);

    // argument 0 is already the call frame.
    jit.peek(GPRInfo::argumentGPR1, 1);
    MacroAssembler::Call functionCall = jit.call();
    
    // At this point we want to make a tail call to what was returned to us in the
    // returnValueGPR. But at the same time as we do this, we must restore all registers.
    // The way we will accomplish this is by arranging to have the tail call target in the
    // return address "slot" (be it a register or the stack).
    
    jit.move(GPRInfo::returnValueGPR, GPRInfo::regT0);
    
    // Prepare for tail call.
    jit.pop(GPRInfo::regT1);
    jit.pop(GPRInfo::regT1);
    jit.pop(MacroAssembler::framePointerRegister);
    
    // At this point we're sitting on the return address - so if we did a jump right now, the
    // tail-callee would be happy. Instead we'll stash the callee in the return address and then
    // restore all registers.
    
    jit.restoreReturnAddressBeforeReturn(GPRInfo::regT0);
    
    restoreAllRegisters(jit, buffer);

    jit.ret();
    
    LinkBuffer patchBuffer(*vm, &jit, GLOBAL_THUNK_ID);
    patchBuffer.link(functionCall, compileFTLOSRExit);
    return FINALIZE_CODE(patchBuffer, ("FTL OSR exit generation thunk"));
}

MacroAssemblerCodeRef osrExitGenerationWithStackMapThunkGenerator(
    VM& vm, const Location& location)
{
    AssemblyHelpers jit(&vm, 0);
    
    // Note that the "return address" will be the OSR exit ID.
    
    // Pretend that we're a C call frame.
    jit.push(MacroAssembler::framePointerRegister);
    jit.move(MacroAssembler::stackPointerRegister, MacroAssembler::framePointerRegister);
    jit.push(GPRInfo::regT0);
    jit.push(GPRInfo::regT0);
    
    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(requiredScratchMemorySizeInBytes());
    char* buffer = static_cast<char*>(scratchBuffer->dataBuffer());
    
    saveAllRegisters(jit, buffer);
    
    // Tell GC mark phase how much of the scratch buffer is active during call.
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::nonArgGPR1);
    jit.storePtr(MacroAssembler::TrustedImmPtr(requiredScratchMemorySizeInBytes()), GPRInfo::nonArgGPR1);

    location.restoreInto(jit, buffer, GPRInfo::argumentGPR0);
    jit.peek(GPRInfo::argumentGPR1, 3);
    MacroAssembler::Call functionCall = jit.call();
    
    // At this point we want to make a tail call to what was returned to us in the
    // returnValueGPR. But at the same time as we do this, we must restore all registers.
    // The way we will accomplish this is by arranging to have the tail call target in the
    // return address "slot" (be it a register or the stack).
    
    jit.move(GPRInfo::returnValueGPR, GPRInfo::regT0);
    
    // Prepare for tail call.
    jit.pop(GPRInfo::regT1);
    jit.pop(GPRInfo::regT1);
    jit.pop(MacroAssembler::framePointerRegister);
    
    // At this point we're sitting on the return address - so if we did a jump right now, the
    // tail-callee would be happy. Instead we'll stash the callee in the return address and then
    // restore all registers.
    
    jit.restoreReturnAddressBeforeReturn(GPRInfo::regT0);
    
    restoreAllRegisters(jit, buffer);

    jit.ret();
    
    LinkBuffer patchBuffer(vm, &jit, GLOBAL_THUNK_ID);
    patchBuffer.link(functionCall, compileFTLOSRExit);
    return FINALIZE_CODE(patchBuffer, ("FTL OSR exit generation thunk for callFrame at %s", toCString(location).data()));
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

