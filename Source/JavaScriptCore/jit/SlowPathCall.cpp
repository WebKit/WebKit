/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(EXTRA_CTI_THUNKS)

void JITSlowPathCall::call()
{
    VM& vm = m_jit->vm();
    uint32_t bytecodeOffset = m_jit->m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_jit->m_bytecodeIndex);

    UNUSED_VARIABLE(m_pc);
    constexpr GPRReg bytecodeOffsetReg = GPRInfo::argumentGPR1;
    m_jit->move(JIT::TrustedImm32(bytecodeOffset), bytecodeOffsetReg);
    m_jit->emitNakedNearCall(vm.jitStubs->ctiSlowPathFunctionStub(vm, m_slowPathFunction).retaggedCode<NoPtrTag>());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITSlowPathCall::generateThunk(VM& vm, SlowPathFunction slowPathFunction)
{
    CCallHelpers jit;

    constexpr GPRReg bytecodeOffsetReg = JIT::argumentGPR1;

#if CPU(X86_64)
    jit.push(X86Registers::ebp);
#elif CPU(ARM64)
    jit.tagReturnAddress();
    jit.pushPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#endif

    jit.store32(bytecodeOffsetReg, CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    jit.loadPtr(CCallHelpers::addressFor(CallFrameSlot::codeBlock), GPRInfo::argumentGPR0);

    jit.prepareCallOperation(vm);

    jit.loadPtr(CCallHelpers::Address(GPRInfo::argumentGPR0, CodeBlock::offsetOfInstructionsRawPointer()), GPRInfo::argumentGPR0);
    static_assert(JIT::argumentGPR1 == bytecodeOffsetReg);
    jit.addPtr(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1);
    jit.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);

    CCallHelpers::Call call = jit.call(OperationPtrTag);
    CCallHelpers::Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

#if CPU(X86_64)
    jit.pop(X86Registers::ebp);
#elif CPU(ARM64)
    jit.popPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#endif
    jit.ret();

    auto handler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(call, FunctionPtr<OperationPtrTag>(slowPathFunction));
    patchBuffer.link(exceptionCheck, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "SlowPathCall");
}

#endif // ENABLE(EXTRA_CTI_THUNKS)

} // namespace JSC

#endif // ENABLE(JIT)
