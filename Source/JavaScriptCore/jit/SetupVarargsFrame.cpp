/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SetupVarargsFrame.h"

#if ENABLE(JIT)

#include "Arguments.h"
#include "JSCInlines.h"
#include "StackAlignment.h"

namespace JSC {

void emitSetVarargsFrame(CCallHelpers& jit, GPRReg lengthGPR, bool lengthIncludesThis, GPRReg numUsedSlotsGPR, GPRReg resultGPR)
{
    jit.move(numUsedSlotsGPR, resultGPR);
    jit.addPtr(lengthGPR, resultGPR);
    jit.addPtr(CCallHelpers::TrustedImm32(JSStack::CallFrameHeaderSize + (lengthIncludesThis? 0 : 1)), resultGPR);
    
    // resultGPR now has the required frame size in Register units
    // Round resultGPR to next multiple of stackAlignmentRegisters()
    jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentRegisters() - 1), resultGPR);
    jit.andPtr(CCallHelpers::TrustedImm32(~(stackAlignmentRegisters() - 1)), resultGPR);
    
    // Now resultGPR has the right stack frame offset in Register units.
    jit.negPtr(resultGPR);
    jit.lshiftPtr(CCallHelpers::Imm32(3), resultGPR);
    jit.addPtr(GPRInfo::callFrameRegister, resultGPR);
}

void emitSetupVarargsFrameFastCase(CCallHelpers& jit, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, int inlineStackOffset, unsigned firstVarArgOffset, CCallHelpers::JumpList& slowCase)
{
    CCallHelpers::JumpList end;
    
    jit.load32(CCallHelpers::Address(GPRInfo::callFrameRegister, (inlineStackOffset + JSStack::ArgumentCount) * sizeof(Register) + PayloadOffset), scratchGPR1);
    if (firstVarArgOffset) {
        CCallHelpers::Jump sufficientArguments = jit.branch32(CCallHelpers::GreaterThan, scratchGPR1, CCallHelpers::TrustedImm32(firstVarArgOffset + 1));
        jit.move(CCallHelpers::TrustedImm32(1), scratchGPR1);
        CCallHelpers::Jump endVarArgs = jit.jump();
        sufficientArguments.link(&jit);
        jit.sub32(CCallHelpers::TrustedImm32(firstVarArgOffset), scratchGPR1);
        endVarArgs.link(&jit);
    }
    slowCase.append(jit.branch32(CCallHelpers::Above, scratchGPR1, CCallHelpers::TrustedImm32(Arguments::MaxArguments + 1)));
    
    emitSetVarargsFrame(jit, scratchGPR1, true, numUsedSlotsGPR, scratchGPR2);

    slowCase.append(jit.branchPtr(CCallHelpers::Above, CCallHelpers::AbsoluteAddress(jit.vm()->addressOfStackLimit()), scratchGPR2));

    // Initialize ArgumentCount.
    jit.store32(scratchGPR1, CCallHelpers::Address(scratchGPR2, JSStack::ArgumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset));

    // Copy arguments.
    jit.signExtend32ToPtr(scratchGPR1, scratchGPR1);
    CCallHelpers::Jump done = jit.branchSubPtr(CCallHelpers::Zero, CCallHelpers::TrustedImm32(1), scratchGPR1);
    // scratchGPR1: argumentCount

    CCallHelpers::Label copyLoop = jit.label();
#if USE(JSVALUE64)
    jit.load64(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, scratchGPR1, CCallHelpers::TimesEight, (CallFrame::thisArgumentOffset() + inlineStackOffset + firstVarArgOffset) * static_cast<int>(sizeof(Register))), scratchGPR3);
    jit.store64(scratchGPR3, CCallHelpers::BaseIndex(scratchGPR2, scratchGPR1, CCallHelpers::TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));
#else // USE(JSVALUE64), so this begins the 32-bit case
    jit.load32(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, scratchGPR1, CCallHelpers::TimesEight, (CallFrame::thisArgumentOffset() + inlineStackOffset + firstVarArgOffset) * static_cast<int>(sizeof(Register)) + TagOffset), scratchGPR3);
    jit.store32(scratchGPR3, CCallHelpers::BaseIndex(scratchGPR2, scratchGPR1, CCallHelpers::TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register)) + TagOffset));
    jit.load32(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, scratchGPR1, CCallHelpers::TimesEight, (CallFrame::thisArgumentOffset() + inlineStackOffset + firstVarArgOffset) * static_cast<int>(sizeof(Register)) + PayloadOffset), scratchGPR3);
    jit.store32(scratchGPR3, CCallHelpers::BaseIndex(scratchGPR2, scratchGPR1, CCallHelpers::TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register)) + PayloadOffset));
#endif // USE(JSVALUE64), end of 32-bit case
    jit.branchSubPtr(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(1), scratchGPR1).linkTo(copyLoop, &jit);
    
    done.link(&jit);
}

} // namespace JSC

#endif // ENABLE(JIT)

