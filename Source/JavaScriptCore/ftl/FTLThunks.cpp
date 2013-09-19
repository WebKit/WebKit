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

#include "FPRInfo.h"
#include "FTLOSRExitCompiler.h"
#include "GPRInfo.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"

namespace JSC { namespace FTL {

using namespace DFG;

MacroAssemblerCodeRef osrExitGenerationThunkGenerator(VM* vm)
{
    MacroAssembler jit;
    
    // Pretend that we're a C call frame.
    jit.push(MacroAssembler::framePointerRegister);
    jit.move(MacroAssembler::stackPointerRegister, MacroAssembler::framePointerRegister);
    jit.push(GPRInfo::regT0);
    jit.push(GPRInfo::regT0);
    
    size_t scratchSize = sizeof(EncodedJSValue) * (GPRInfo::numberOfArgumentRegisters + FPRInfo::numberOfArgumentRegisters);
    ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());
    
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; ++i)
        jit.store64(GPRInfo::toArgumentRegister(i), buffer + i);
    for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfArgumentRegisters + i), GPRInfo::nonArgGPR1);
        jit.storeDouble(FPRInfo::toArgumentRegister(i), GPRInfo::nonArgGPR1);
    }
    
    // Tell GC mark phase how much of the scratch buffer is active during call.
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::nonArgGPR1);
    jit.storePtr(MacroAssembler::TrustedImmPtr(scratchSize), GPRInfo::nonArgGPR1);

    // argument 0 is already the call frame.
    jit.move(GPRInfo::nonArgGPR0, GPRInfo::argumentGPR1);
    MacroAssembler::Call functionCall = jit.call();
    
    // Make sure that we're not using the return register if it's an argument register.
    jit.move(GPRInfo::returnValueGPR, GPRInfo::nonArgGPR0);
    
    // Prepare for tail call.
    jit.pop(GPRInfo::nonArgGPR1);
    jit.pop(GPRInfo::nonArgGPR1);
    jit.pop(MacroAssembler::framePointerRegister);
    
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::nonArgGPR1);
    jit.storePtr(MacroAssembler::TrustedImmPtr(0), GPRInfo::nonArgGPR1);
    
    for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfArgumentRegisters + i), GPRInfo::nonArgGPR1);
        jit.loadDouble(GPRInfo::nonArgGPR1, FPRInfo::toArgumentRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; ++i)
        jit.load64(buffer + i, GPRInfo::toArgumentRegister(i));
    
    jit.jump(GPRInfo::nonArgGPR0);
    
    LinkBuffer patchBuffer(*vm, &jit, GLOBAL_THUNK_ID);
    patchBuffer.link(functionCall, compileFTLOSRExit);
    return FINALIZE_CODE(patchBuffer, ("FTL OSR exit generation thunk"));
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

