/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "SlowPathCall.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "JITInlines.h"
#include "JITThunks.h"
#include "ThunkGenerators.h"
#include "VM.h"

namespace JSC {

namespace {
    constexpr GPRReg bytecodeOffsetGPR = JIT::argumentGPR3;
}

void JITSlowPathCall::call()
{
    VM& vm = m_jit->vm();
    uint32_t bytecodeOffset = m_jit->m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_jit->m_bytecodeIndex);

    m_jit->move(JIT::TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    m_jit->nearCallThunk(CodeLocationLabel { vm.jitStubs->ctiSlowPathFunctionStub(vm, m_slowPathFunction).retaggedCode<NoPtrTag>() });
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITSlowPathCall::generateThunk(VM& vm, SlowPathFunction slowPathFunction)
{
    CCallHelpers jit;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);

#if OS(WINDOWS) && CPU(X86_64)
    // On Windows, return values larger than 8 bytes are retuened via an implicit pointer passed as
    // the first argument, and remaining arguments are shifted to the right. Make space for this.
    static_assert(sizeof(UGPRPair) == 16, "Assumed by generated call site below");
    jit.subPtr(MacroAssembler::TrustedImm32(16), MacroAssembler::stackPointerRegister);
    jit.move(MacroAssembler::stackPointerRegister, GPRInfo::argumentGPR0);
    constexpr GPRReg callFrameArgGPR = GPRInfo::argumentGPR1;
    constexpr GPRReg pcArgGPR = GPRInfo::argumentGPR2;
    static_assert(noOverlap(GPRInfo::argumentGPR0, callFrameArgGPR, pcArgGPR, bytecodeOffsetGPR));
#else
    constexpr GPRReg callFrameArgGPR = GPRInfo::argumentGPR0;
    constexpr GPRReg pcArgGPR = GPRInfo::argumentGPR1;
    static_assert(noOverlap(callFrameArgGPR, pcArgGPR, bytecodeOffsetGPR));
#endif
    jit.move(GPRInfo::callFrameRegister, callFrameArgGPR);
    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), pcArgGPR);
    jit.loadPtr(CCallHelpers::Address(pcArgGPR, CodeBlock::offsetOfInstructionsRawPointer()), pcArgGPR);
    jit.addPtr(bytecodeOffsetGPR, pcArgGPR);

    jit.callOperation<OperationPtrTag>(slowPathFunction);

#if OS(WINDOWS) && CPU(X86_64)
    jit.pop(GPRInfo::returnValueGPR); // pc
    jit.pop(GPRInfo::returnValueGPR2); // callFrame
#endif

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    jit.jumpThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::CheckException).retaggedCode<NoPtrTag>()));

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "SlowPathCall");
}

} // namespace JSC

#endif // ENABLE(JIT)
