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
#include "DFGThunks.h"

#if ENABLE(DFG_JIT)

#include "DFGFPRInfo.h"
#include "DFGGPRInfo.h"
#include "DFGOSRExitCompiler.h"
#include "MacroAssembler.h"

namespace JSC { namespace DFG {

MacroAssemblerCodeRef osrExitGenerationThunkGenerator(JSGlobalData* globalData)
{
    MacroAssembler jit;
    
    EncodedJSValue* buffer = static_cast<EncodedJSValue*>(globalData->scratchBufferForSize(sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters)));
    
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i)
        jit.storePtr(GPRInfo::toRegister(i), buffer + i);
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        jit.storeDouble(FPRInfo::toRegister(i), GPRInfo::regT0);
    }
    
    // Set up one argument.
#if CPU(X86)
    jit.poke(GPRInfo::callFrameRegister, 0);
#else
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
#endif
    
    MacroAssembler::Call functionCall = jit.call();
    
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        jit.move(MacroAssembler::TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
        jit.loadDouble(GPRInfo::regT0, FPRInfo::toRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i)
        jit.loadPtr(buffer + i, GPRInfo::toRegister(i));
    
    jit.jump(MacroAssembler::AbsoluteAddress(&globalData->osrExitJumpDestination));
    
    LinkBuffer patchBuffer(*globalData, &jit);
    
    patchBuffer.link(functionCall, compileOSRExit);
    
    return patchBuffer.finalizeCode();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
