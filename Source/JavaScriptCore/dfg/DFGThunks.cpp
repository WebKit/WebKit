/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGThunks.h"

#include "CCallHelpers.h"
#include "DFGOSRExitCompiler.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"
#include "Operations.h"

namespace JSC { namespace DFG {

MacroAssemblerCodeRef osrExitGenerationThunkGenerator(VM* vm)
{
    MacroAssembler jit;
    
    size_t scratchSize = sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);
    ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(scratchSize);
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());
    
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        jit.store64(GPRInfo::toRegister(i), buffer + i);
#else
        jit.store32(GPRInfo::toRegister(i), buffer + i);
#endif
    }
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        jit.storeDouble(FPRInfo::toRegister(i), MacroAssembler::Address(GPRInfo::regT0));
    }
    
    // Tell GC mark phase how much of the scratch buffer is active during call.
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::regT0);
    jit.storePtr(MacroAssembler::TrustedImmPtr(scratchSize), MacroAssembler::Address(GPRInfo::regT0));

    // Set up one argument.
#if CPU(X86)
    jit.poke(GPRInfo::callFrameRegister, 0);
#else
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
#endif

    MacroAssembler::Call functionCall = jit.call();

    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::regT0);
    jit.storePtr(MacroAssembler::TrustedImmPtr(0), MacroAssembler::Address(GPRInfo::regT0));

    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        jit.loadDouble(MacroAssembler::Address(GPRInfo::regT0), FPRInfo::toRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
        jit.load64(buffer + i, GPRInfo::toRegister(i));
#else
        jit.load32(buffer + i, GPRInfo::toRegister(i));
#endif
    }
    
    jit.jump(MacroAssembler::AbsoluteAddress(&vm->osrExitJumpDestination));
    
    LinkBuffer patchBuffer(*vm, &jit, GLOBAL_THUNK_ID);
    
    patchBuffer.link(functionCall, compileOSRExit);
    
    return FINALIZE_CODE(patchBuffer, ("DFG OSR exit generation thunk"));
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
