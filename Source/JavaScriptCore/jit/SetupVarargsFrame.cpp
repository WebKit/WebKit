/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "Interpreter.h"
#include "JSArray.h"
#include "JSCJSValueInlines.h"
#include "JSCellButterfly.h"
#include "StackAlignment.h"

namespace JSC {

void emitSetVarargsFrame(CCallHelpers& jit, GPRReg lengthGPR, bool lengthIncludesThis, GPRReg numUsedSlotsGPR, GPRReg resultGPR)
{
    // We really want to make sure the size of the new call frame is a multiple of
    // stackAlignmentRegisters(), however it is easier to accomplish this by
    // rounding numUsedSlotsGPR to the next multiple of stackAlignmentRegisters().
    // Together with the rounding below, we will assure that the new call frame is
    // located on a stackAlignmentRegisters() boundary and a multiple of
    // stackAlignmentRegisters() in size.
    jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentRegisters() - 1), numUsedSlotsGPR, resultGPR);
    jit.andPtr(CCallHelpers::TrustedImm32(~(stackAlignmentRegisters() - 1)), resultGPR);

    // resultGPR now has the required frame size in Register units
    // Round resultGPR to next multiple of stackAlignmentRegisters()
    jit.addPtr(lengthGPR, resultGPR);
    jit.addPtr(CCallHelpers::TrustedImm32(CallFrame::headerSizeInRegisters + (lengthIncludesThis ? 0 : 1) + (stackAlignmentRegisters() - 1)), resultGPR);
    jit.andPtr(CCallHelpers::TrustedImm32(~(stackAlignmentRegisters() - 1)), resultGPR);
    
    // Now resultGPR has the right stack frame offset in Register units.
    jit.negPtr(resultGPR);
    jit.getEffectiveAddress(CCallHelpers::BaseIndex(GPRInfo::callFrameRegister, resultGPR, CCallHelpers::TimesEight), resultGPR);
}

static void emitSkipFirstVarArgs(CCallHelpers& jit, GPRReg scratchGPR1, unsigned firstVarArgOffset)
{
    if (firstVarArgOffset) {
        CCallHelpers::Jump sufficientArguments = jit.branch32(CCallHelpers::GreaterThan, scratchGPR1, CCallHelpers::TrustedImm32(firstVarArgOffset + 1));
        jit.move(CCallHelpers::TrustedImm32(1), scratchGPR1);
        CCallHelpers::Jump endVarArgs = jit.jump();
        sufficientArguments.link(&jit);
        jit.sub32(CCallHelpers::TrustedImm32(firstVarArgOffset), scratchGPR1);
        endVarArgs.link(&jit);
    }
}

static void emitSizeVarargsFrameFastCase(VM& vm, CCallHelpers& jit, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, CCallHelpers::JumpList& slowCase)
{
    emitSetVarargsFrame(jit, scratchGPR1, true, numUsedSlotsGPR, scratchGPR2);

    slowCase.append(jit.branchPtr(CCallHelpers::GreaterThan, CCallHelpers::AbsoluteAddress(vm.addressOfSoftStackLimit()), scratchGPR2));

    // Before touching stack values, we should update the stack pointer to protect them from signal stack.
    jit.addPtr(CCallHelpers::TrustedImm32(sizeof(CallerFrameAndPC)), scratchGPR2, CCallHelpers::stackPointerRegister);

    // Initialize ArgumentCount.
    jit.store32(scratchGPR1, CCallHelpers::Address(scratchGPR2, CallFrameSlot::argumentCountIncludingThis * static_cast<int>(sizeof(Register)) + LowWordOffset));

    jit.signExtend32ToPtr(scratchGPR1, scratchGPR1);
}

static void emitLoadVarargs(CCallHelpers& jit, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, CCallHelpers::Address firstArgument, unsigned firstVarArgOffset)
{
    // Copy arguments.
    CCallHelpers::Jump done = jit.branchSubPtr(CCallHelpers::Zero, CCallHelpers::TrustedImm32(1), scratchGPR1);
    // scratchGPR1: argumentCount

    CCallHelpers::Label copyLoop = jit.label();
    int argOffset = firstArgument.offset + (static_cast<int>(firstVarArgOffset) - 1) * static_cast<int>(sizeof(Register));
    jit.load64(CCallHelpers::BaseIndex(firstArgument.base, scratchGPR1, CCallHelpers::TimesEight, argOffset), scratchGPR3);
    jit.store64(scratchGPR3, CCallHelpers::BaseIndex(scratchGPR2, scratchGPR1, CCallHelpers::TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))));
    jit.branchSubPtr(CCallHelpers::NonZero, CCallHelpers::TrustedImm32(1), scratchGPR1).linkTo(copyLoop, &jit);
    
    done.link(&jit);
}

static void emitSetupVarargsFrameFastCase(VM& vm, CCallHelpers& jit, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, ValueRecovery argCountRecovery, VirtualRegister firstArgumentReg, unsigned firstVarArgOffset, CCallHelpers::JumpList& slowCase)
{
    if (argCountRecovery.isConstant()) {
        // FIXME: We could constant-fold a lot of the computation below in this case.
        // https://bugs.webkit.org/show_bug.cgi?id=141486
        jit.move(CCallHelpers::TrustedImm32(argCountRecovery.constant().asInt32()), scratchGPR1);
    } else
        jit.load32(CCallHelpers::lowWordFor(argCountRecovery.virtualRegister()), scratchGPR1);
    emitSkipFirstVarArgs(jit, scratchGPR1, firstVarArgOffset);
    slowCase.append(jit.branch32(CCallHelpers::Above, scratchGPR1, CCallHelpers::TrustedImm32(JSC::maxArguments + 1)));

    emitSizeVarargsFrameFastCase(vm, jit, numUsedSlotsGPR, scratchGPR1, scratchGPR2, slowCase);
    emitLoadVarargs(jit, scratchGPR1, scratchGPR2, scratchGPR3, CCallHelpers::addressFor(firstArgumentReg), firstVarArgOffset);
}

void emitSetupVarargsFrameFastCase(VM& vm, CCallHelpers& jit, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, InlineCallFrame* inlineCallFrame, unsigned firstVarArgOffset, CCallHelpers::JumpList& slowCase)
{
    ValueRecovery argumentCountRecovery;
    VirtualRegister firstArgumentReg;
    if (inlineCallFrame) {
        if (inlineCallFrame->isVarargs()) {
            argumentCountRecovery = ValueRecovery::displacedInJSStack(
                inlineCallFrame->argumentCountRegister, DataFormatInt32);
        } else {
            argumentCountRecovery = ValueRecovery::constant(jsNumber(inlineCallFrame->argumentCountIncludingThis));
        }
        if (inlineCallFrame->m_argumentsWithFixup.size() > 1)
            firstArgumentReg = inlineCallFrame->m_argumentsWithFixup[1].virtualRegister();
        else
            firstArgumentReg = VirtualRegister(0);
    } else {
        argumentCountRecovery = ValueRecovery::displacedInJSStack(
            CallFrameSlot::argumentCountIncludingThis, DataFormatInt32);
        firstArgumentReg = VirtualRegister(CallFrame::argumentOffset(0));
    }
    emitSetupVarargsFrameFastCase(vm, jit, numUsedSlotsGPR, scratchGPR1, scratchGPR2, scratchGPR3, argumentCountRecovery, firstArgumentReg, firstVarArgOffset, slowCase);
}

void emitSetupVarargsFrameFromCellButterfly(VM& vm, CCallHelpers& jit, GPRReg cellButterflyGPR, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, unsigned firstVarArgOffset, CCallHelpers::JumpList& slowCase)
{
    jit.load32(CCallHelpers::Address(cellButterflyGPR, JSCellButterfly::offsetOfPublicLength()), scratchGPR1);
    slowCase.append(jit.branch32(CCallHelpers::Above, scratchGPR1, CCallHelpers::TrustedImm32(JSC::maxArguments)));
    jit.add32(CCallHelpers::TrustedImm32(1), scratchGPR1);
    emitSkipFirstVarArgs(jit, scratchGPR1, firstVarArgOffset);

    emitSizeVarargsFrameFastCase(vm, jit, numUsedSlotsGPR, scratchGPR1, scratchGPR2, slowCase);
    emitLoadVarargs(jit, scratchGPR1, scratchGPR2, scratchGPR3, CCallHelpers::Address(cellButterflyGPR, JSCellButterfly::offsetOfData()), firstVarArgOffset);
}

void emitLoadVarargsLengthFromArray(CCallHelpers& jit, GPRReg arrayGPR, GPRReg butterflyGPR, GPRReg indexingShapeGPR, GPRReg lengthGPR, CCallHelpers::JumpList& slowCase)
{
    static_assert(UndecidedShape < Int32Shape && Int32Shape < DoubleShape && DoubleShape < ContiguousShape);
    jit.load8(CCallHelpers::Address(arrayGPR, JSCell::indexingTypeAndMiscOffset()), indexingShapeGPR);
    jit.and32(CCallHelpers::TrustedImm32(IndexingShapeMask), indexingShapeGPR);
    jit.sub32(indexingShapeGPR, CCallHelpers::TrustedImm32(UndecidedShape), lengthGPR);
    slowCase.append(jit.branch32(CCallHelpers::Above, lengthGPR, CCallHelpers::TrustedImm32(ContiguousShape - UndecidedShape)));

    jit.loadPtr(CCallHelpers::Address(arrayGPR, JSObject::butterflyOffset()), butterflyGPR);
    jit.load32(CCallHelpers::Address(butterflyGPR, Butterfly::offsetOfPublicLength()), lengthGPR);
}

void emitLoadVarargsFromArray(CCallHelpers& jit, GPRReg butterflyGPR, GPRReg indexingShapeGPR, GPRReg lengthGPR, FPRReg scratchFPR, unsigned firstVarArgOffset, CCallHelpers::Address destination, CCallHelpers::JumpList& slowCase)
{
    CCallHelpers::BaseIndex source(butterflyGPR, lengthGPR, CCallHelpers::TimesEight, firstVarArgOffset * sizeof(EncodedJSValue));
    CCallHelpers::BaseIndex target(destination.base, lengthGPR, CCallHelpers::TimesEight, destination.offset);

    CCallHelpers::JumpList done;
    done.append(jit.branchTestPtr(CCallHelpers::Zero, lengthGPR));
    CCallHelpers::Jump isDouble = jit.branch32(CCallHelpers::Equal, indexingShapeGPR, CCallHelpers::TrustedImm32(DoubleShape));
    slowCase.append(jit.branch32(CCallHelpers::Equal, indexingShapeGPR, CCallHelpers::TrustedImm32(UndecidedShape)));

    GPRReg valueGPR = indexingShapeGPR;
    CCallHelpers::Label copyLoop = jit.label();
    jit.subPtr(CCallHelpers::TrustedImm32(1), lengthGPR);
    jit.load64(source, valueGPR);
    slowCase.append(jit.branchIfEmpty(valueGPR));
    jit.store64(valueGPR, target);
    jit.branchTestPtr(CCallHelpers::NonZero, lengthGPR).linkTo(copyLoop, &jit);
    done.append(jit.jump());

    isDouble.link(&jit);
    CCallHelpers::Label copyDoubleLoop = jit.label();
    jit.subPtr(CCallHelpers::TrustedImm32(1), lengthGPR);
    jit.loadDouble(source, scratchFPR);
    slowCase.append(jit.branchIfNaN(scratchFPR));
    jit.boxDouble(scratchFPR, valueGPR);
    jit.store64(valueGPR, target);
    jit.branchTestPtr(CCallHelpers::NonZero, lengthGPR).linkTo(copyDoubleLoop, &jit);

    done.link(&jit);
}

void emitSetupVarargsFrameFromArray(VM& vm, CCallHelpers& jit, GPRReg arrayGPR, GPRReg numUsedSlotsGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, GPRReg scratchGPR3, GPRReg scratchGPR4, FPRReg scratchFPR, unsigned firstVarArgOffset, CCallHelpers::JumpList& slowCase)
{
    GPRReg indexingShapeGPR = scratchGPR3;
    GPRReg butterflyGPR = scratchGPR4;

    emitLoadVarargsLengthFromArray(jit, arrayGPR, butterflyGPR, indexingShapeGPR, scratchGPR1, slowCase);
    slowCase.append(jit.branch32(CCallHelpers::Above, scratchGPR1, CCallHelpers::TrustedImm32(JSC::maxArguments)));
    jit.add32(CCallHelpers::TrustedImm32(1), scratchGPR1);
    emitSkipFirstVarArgs(jit, scratchGPR1, firstVarArgOffset);

    emitSizeVarargsFrameFastCase(vm, jit, numUsedSlotsGPR, scratchGPR1, scratchGPR2, slowCase);
    jit.subPtr(CCallHelpers::TrustedImm32(1), scratchGPR1);
    jit.addPtr(CCallHelpers::TrustedImm32(CallFrame::argumentOffset(0) * static_cast<int>(sizeof(Register))), scratchGPR2);
    emitLoadVarargsFromArray(jit, butterflyGPR, indexingShapeGPR, scratchGPR1, scratchFPR, firstVarArgOffset, CCallHelpers::Address(scratchGPR2), slowCase);
}

} // namespace JSC

#endif // ENABLE(JIT)

