/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "DFGJITCode.h"
#include "DFGOSRExit.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"
#include "JSCInlines.h"
#include "DFGOSRExitCompilerCommon.h"

namespace JSC { namespace DFG {

MacroAssemblerCodeRef<JITThunkPtrTag> osrExitGenerationThunkGenerator(VM& vm)
{
    CCallHelpers jit(nullptr);

    // This needs to happen before we use the scratch buffer because this function also uses the scratch buffer.
    adjustFrameAndStackInOSRExitCompilerThunk<DFG::JITCode>(jit, vm, JITType::DFGJIT);
    
    size_t scratchSize = sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);
    ScratchBuffer* scratchBuffer = vm.scratchBufferForSize(scratchSize);
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
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->addressOfActiveLength()), GPRInfo::regT0);
    jit.storePtr(MacroAssembler::TrustedImmPtr(scratchSize), MacroAssembler::Address(GPRInfo::regT0));

    // Set up one argument.
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
    jit.prepareCallOperation(vm);

    MacroAssembler::Call functionCall = jit.call(OperationPtrTag);

    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->addressOfActiveLength()), GPRInfo::regT0);
    jit.storePtr(MacroAssembler::TrustedImmPtr(nullptr), MacroAssembler::Address(GPRInfo::regT0));

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

    jit.farJump(MacroAssembler::AbsoluteAddress(&vm.osrExitJumpDestination), OSRExitPtrTag);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID);
    
    patchBuffer.link(functionCall, FunctionPtr<OperationPtrTag>(operationCompileOSRExit));

    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "DFG OSR exit generation thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> osrEntryThunkGenerator(VM& vm)
{
    AssemblyHelpers jit(nullptr);

    // We get passed the address of a scratch buffer in GPRInfo::returnValueGPR2.
    // The first 8-byte slot of the buffer is the frame size. The second 8-byte slot
    // is the pointer to where we are supposed to jump. The remaining bytes are
    // the new call frame header followed by the locals.
    
    ptrdiff_t offsetOfFrameSize = 0; // This is the DFG frame count.
    ptrdiff_t offsetOfTargetPC = offsetOfFrameSize + sizeof(EncodedJSValue);
    ptrdiff_t offsetOfPayload = offsetOfTargetPC + sizeof(EncodedJSValue);
    ptrdiff_t offsetOfLocals = offsetOfPayload + sizeof(Register) * CallFrame::headerSizeInRegisters;
    
    jit.move(GPRInfo::returnValueGPR2, GPRInfo::regT0);
    jit.loadPtr(MacroAssembler::Address(GPRInfo::regT0, offsetOfFrameSize), GPRInfo::regT1); // Load the frame size.
    jit.negPtr(GPRInfo::regT1, GPRInfo::regT2);
    jit.getEffectiveAddress(MacroAssembler::BaseIndex(GPRInfo::callFrameRegister, GPRInfo::regT2, MacroAssembler::TimesEight), MacroAssembler::stackPointerRegister);
    
    MacroAssembler::Label loop = jit.label();
    jit.subPtr(MacroAssembler::TrustedImm32(1), GPRInfo::regT1);
    jit.negPtr(GPRInfo::regT1, GPRInfo::regT4);
    jit.load32(MacroAssembler::BaseIndex(GPRInfo::regT0, GPRInfo::regT1, MacroAssembler::TimesEight, offsetOfLocals), GPRInfo::regT2);
    jit.load32(MacroAssembler::BaseIndex(GPRInfo::regT0, GPRInfo::regT1, MacroAssembler::TimesEight, offsetOfLocals + sizeof(int32_t)), GPRInfo::regT3);
    jit.store32(GPRInfo::regT2, MacroAssembler::BaseIndex(GPRInfo::callFrameRegister, GPRInfo::regT4, MacroAssembler::TimesEight, -static_cast<intptr_t>(sizeof(Register))));
    jit.store32(GPRInfo::regT3, MacroAssembler::BaseIndex(GPRInfo::callFrameRegister, GPRInfo::regT4, MacroAssembler::TimesEight, -static_cast<intptr_t>(sizeof(Register)) + static_cast<intptr_t>(sizeof(int32_t))));
    jit.branchPtr(MacroAssembler::NotEqual, GPRInfo::regT1, MacroAssembler::TrustedImmPtr(bitwise_cast<void*>(-static_cast<intptr_t>(CallFrame::headerSizeInRegisters)))).linkTo(loop, &jit);
    
    jit.loadPtr(MacroAssembler::Address(GPRInfo::regT0, offsetOfTargetPC), GPRInfo::regT1);
    MacroAssembler::Jump ok = jit.branchPtr(MacroAssembler::Above, GPRInfo::regT1, MacroAssembler::TrustedImmPtr(bitwise_cast<void*>(static_cast<intptr_t>(1000))));
    jit.abortWithReason(DFGUnreasonableOSREntryJumpDestination);

    ok.link(&jit);
    jit.restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm.topEntryFrame);
    jit.emitMaterializeTagCheckRegisters();

    jit.farJump(GPRInfo::regT1, GPRInfo::callFrameRegister);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "DFG OSR entry thunk");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
