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
#include "RegisterPreservationWrapperGenerator.h"

#if ENABLE(JIT)

#include "AssemblyHelpers.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "StackAlignment.h"

namespace JSC {

RegisterSet registersToPreserve()
{
    RegisterSet calleeSaves = RegisterSet::calleeSaveRegisters();
    
    // No need to preserve FP since that always gets preserved anyway.
    calleeSaves.clear(GPRInfo::callFrameRegister);
    
    return calleeSaves;
}

ptrdiff_t registerPreservationOffset()
{
    unsigned numberOfCalleeSaves = registersToPreserve().numberOfSetRegisters();
    
    // Need to preserve the old return PC.
    unsigned numberOfValuesToSave = numberOfCalleeSaves + 1;
    
    // Alignment. Preserve the same alignment invariants that the caller imposed.
    unsigned numberOfNewStackSlots =
        WTF::roundUpToMultipleOf(stackAlignmentRegisters(), numberOfValuesToSave);
    
    return sizeof(Register) * numberOfNewStackSlots;
}

MacroAssemblerCodeRef generateRegisterPreservationWrapper(VM& vm, ExecutableBase* executable, MacroAssemblerCodePtr target)
{
#if ENABLE(FTL_JIT)
    // We shouldn't ever be generating wrappers for native functions.
    RegisterSet toSave = registersToPreserve();
    ptrdiff_t offset = registerPreservationOffset();
    
    AssemblyHelpers jit(&vm, 0);
    
    jit.preserveReturnAddressAfterCall(GPRInfo::regT1);
    jit.load32(
        AssemblyHelpers::Address(
            AssemblyHelpers::stackPointerRegister,
            (JSStack::ArgumentCount - JSStack::CallerFrameAndPCSize) * sizeof(Register) + PayloadOffset),
        GPRInfo::regT2);
    
    // Place the stack pointer where we want it to be.
    jit.subPtr(AssemblyHelpers::TrustedImm32(offset), AssemblyHelpers::stackPointerRegister);
    
    // Compute the number of things we will be copying.
    jit.add32(
        AssemblyHelpers::TrustedImm32(
            JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize),
        GPRInfo::regT2);

    ASSERT(!toSave.get(GPRInfo::regT4));
    jit.move(AssemblyHelpers::stackPointerRegister, GPRInfo::regT4);
    
    AssemblyHelpers::Label loop = jit.label();
    jit.sub32(AssemblyHelpers::TrustedImm32(1), GPRInfo::regT2);
    jit.load64(AssemblyHelpers::Address(GPRInfo::regT4, offset), GPRInfo::regT0);
    jit.store64(GPRInfo::regT0, GPRInfo::regT4);
    jit.addPtr(AssemblyHelpers::TrustedImm32(sizeof(Register)), GPRInfo::regT4);
    jit.branchTest32(AssemblyHelpers::NonZero, GPRInfo::regT2).linkTo(loop, &jit);

    // At this point regT4 + offset points to where we save things.
    ptrdiff_t currentOffset = 0;
    jit.storePtr(GPRInfo::regT1, AssemblyHelpers::Address(GPRInfo::regT4, currentOffset));
    
    for (GPRReg gpr = AssemblyHelpers::firstRegister(); gpr <= AssemblyHelpers::lastRegister(); gpr = static_cast<GPRReg>(gpr + 1)) {
        if (!toSave.get(gpr))
            continue;
        currentOffset += sizeof(Register);
        jit.store64(gpr, AssemblyHelpers::Address(GPRInfo::regT4, currentOffset));
    }
    
    // Assume that there aren't any saved FP registers.
    
    // Restore the tag registers.
    jit.move(AssemblyHelpers::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
    jit.add64(AssemblyHelpers::TrustedImm32(TagMask - TagTypeNumber), GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
    
    jit.move(
        AssemblyHelpers::TrustedImmPtr(
            vm.getCTIStub(registerRestorationThunkGenerator).code().executableAddress()),
        GPRInfo::nonArgGPR0);
    jit.restoreReturnAddressBeforeReturn(GPRInfo::nonArgGPR0);
    AssemblyHelpers::Jump jump = jit.jump();
    
    LinkBuffer linkBuffer(vm, &jit, GLOBAL_THUNK_ID);
    linkBuffer.link(jump, CodeLocationLabel(target));

    if (Options::verboseFTLToJSThunk())
        dataLog("Need a thunk for calls from FTL to non-FTL version of ", *executable, "\n");
    
    return FINALIZE_DFG_CODE(linkBuffer, ("Register preservation wrapper for %s/%s, %p", toCString(executable->hashFor(CodeForCall)).data(), toCString(executable->hashFor(CodeForConstruct)).data(), target.executableAddress()));
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(vm);
    UNUSED_PARAM(executable);
    UNUSED_PARAM(target);
    // We don't support non-FTL builds for two reasons:
    // - It just so happens that currently only the FTL bottoms out in this code.
    // - The code above uses 64-bit instructions. It doesn't necessarily have to; it would be
    //   easy to change it so that it doesn't. But obviously making that change would be a
    //   prerequisite to removing this #if.
    UNREACHABLE_FOR_PLATFORM();
    return MacroAssemblerCodeRef();
#endif // ENABLE(FTL_JIT)
}

static void generateRegisterRestoration(AssemblyHelpers& jit)
{
#if ENABLE(FTL_JIT)
    RegisterSet toSave = registersToPreserve();
    ptrdiff_t offset = registerPreservationOffset();
    
    ASSERT(!toSave.get(GPRInfo::regT4));

    // We need to place the stack pointer back to where the caller thought they left it.
    // But also, in order to recover the registers, we need to figure out how big the
    // arguments area is.
    
    jit.load32(
        AssemblyHelpers::Address(
            AssemblyHelpers::stackPointerRegister,
            (JSStack::ArgumentCount - JSStack::CallerFrameAndPCSize) * sizeof(Register) + PayloadOffset),
        GPRInfo::regT4);
    
    jit.move(GPRInfo::regT4, GPRInfo::regT2);
    jit.lshift32(AssemblyHelpers::TrustedImm32(3), GPRInfo::regT2);
    
    jit.addPtr(AssemblyHelpers::TrustedImm32(offset), AssemblyHelpers::stackPointerRegister);
    jit.addPtr(AssemblyHelpers::stackPointerRegister, GPRInfo::regT2);
    
    // We saved things at:
    //
    //     adjSP + (JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize + NumArgs) * 8
    //
    // Where:
    //
    //     adjSP = origSP - offset
    //
    // regT2 now points at:
    //
    //     origSP + NumArgs * 8
    //   = adjSP + offset + NumArgs * 8
    // 
    // So if we subtract offset and then add JSStack::CallFrameHeaderSize and subtract
    // JSStack::CallerFrameAndPCSize, we'll get the thing we want.
    ptrdiff_t currentOffset = -offset + sizeof(Register) * (
        JSStack::CallFrameHeaderSize - JSStack::CallerFrameAndPCSize);
    jit.loadPtr(AssemblyHelpers::Address(GPRInfo::regT2, currentOffset), GPRInfo::regT1);
    
    for (GPRReg gpr = AssemblyHelpers::firstRegister(); gpr <= AssemblyHelpers::lastRegister(); gpr = static_cast<GPRReg>(gpr + 1)) {
        if (!toSave.get(gpr))
            continue;
        currentOffset += sizeof(Register);
        jit.load64(AssemblyHelpers::Address(GPRInfo::regT2, currentOffset), gpr);
    }
    
    // Thunks like this rely on the ArgumentCount being intact. Pay it forward.
    jit.store32(
        GPRInfo::regT4,
        AssemblyHelpers::Address(
            AssemblyHelpers::stackPointerRegister,
            (JSStack::ArgumentCount - JSStack::CallerFrameAndPCSize) * sizeof(Register) + PayloadOffset));
    
    if (!ASSERT_DISABLED) {
        AssemblyHelpers::Jump ok = jit.branchPtr(
            AssemblyHelpers::Above, GPRInfo::regT1, AssemblyHelpers::TrustedImmPtr(static_cast<size_t>(0x1000)));
        jit.breakpoint();
        ok.link(&jit);
    }
    
    jit.jump(GPRInfo::regT1);
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(jit);
    UNREACHABLE_FOR_PLATFORM();
#endif // ENABLE(FTL_JIT)
}

MacroAssemblerCodeRef registerRestorationThunkGenerator(VM* vm)
{
    AssemblyHelpers jit(vm, 0);
    generateRegisterRestoration(jit);
    LinkBuffer linkBuffer(*vm, &jit, GLOBAL_THUNK_ID);
    return FINALIZE_CODE(linkBuffer, ("Register restoration thunk"));
}

} // namespace JSC

#endif // ENABLE(JIT)

