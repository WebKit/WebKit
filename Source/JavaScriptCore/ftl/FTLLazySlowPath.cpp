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
#include "FTLLazySlowPath.h"

#if ENABLE(FTL_JIT)

#include "FTLSlowPathCall.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

LazySlowPath::LazySlowPath(
#if FTL_USES_B3
    CodeLocationJump patchableJump, CodeLocationLabel done,
#else // FTL_USES_B3
    CodeLocationLabel patchpoint,
#endif // FTL_USES_B3
    CodeLocationLabel exceptionTarget,
    const RegisterSet& usedRegisters, CallSiteIndex callSiteIndex, RefPtr<Generator> generator
#if !FTL_USES_B3
    , GPRReg newZeroReg, ScratchRegisterAllocator scratchRegisterAllocator
#endif // !FTL_USES_B3
    )
#if FTL_USES_B3
    : m_patchableJump(patchableJump)
    , m_done(done)
#else // FTL_USES_B3
    : m_patchpoint(patchpoint)
#endif // FTL_USES_B3
    , m_exceptionTarget(exceptionTarget)
    , m_usedRegisters(usedRegisters)
    , m_callSiteIndex(callSiteIndex)
    , m_generator(generator)
#if !FTL_USES_B3
    , m_newZeroValueRegister(newZeroReg)
    , m_scratchRegisterAllocator(scratchRegisterAllocator)
#endif // !FTL_USES_B3
{
}

LazySlowPath::~LazySlowPath()
{
}

void LazySlowPath::generate(CodeBlock* codeBlock)
{
    RELEASE_ASSERT(!m_stub);

    VM& vm = *codeBlock->vm();

    CCallHelpers jit(&vm, codeBlock);
    GenerationParams params;
    CCallHelpers::JumpList exceptionJumps;
    params.exceptionJumps = m_exceptionTarget ? &exceptionJumps : nullptr;
    params.lazySlowPath = this;

#if !FTL_USES_B3
    ScratchRegisterAllocator::PreservedState preservedState =
        m_scratchRegisterAllocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
    // This is needed because LLVM may create a stackmap location that is the register SP.
    // But on arm64, SP is also the same register number as ZR, so LLVM is telling us that it has
    // proven something is zero. Our MASM isn't universally compatible with arm64's context dependent
    // notion of SP meaning ZR. We just make things easier by ensuring we do the necessary move of zero
    // into a non-SP register.
    if (m_newZeroValueRegister != InvalidGPRReg)
        jit.move(CCallHelpers::TrustedImm32(0), m_newZeroValueRegister);
#endif // !FTL_USES_B3

    m_generator->run(jit, params);

#if !FTL_USES_B3
    CCallHelpers::Label doneLabel;
    CCallHelpers::Jump jumpToEndOfPatchpoint;
    if (preservedState.numberOfBytesPreserved) {
        doneLabel = jit.label();
        m_scratchRegisterAllocator.restoreReusedRegistersByPopping(jit, preservedState);
        jumpToEndOfPatchpoint = jit.jump();
    }
#endif // !FTL_USES_B3

    LinkBuffer linkBuffer(vm, jit, codeBlock, JITCompilationMustSucceed);
#if FTL_USES_B3
    linkBuffer.link(params.doneJumps, m_done);
#else // FTL_USES_B3
    if (preservedState.numberOfBytesPreserved) {
        linkBuffer.link(params.doneJumps, linkBuffer.locationOf(doneLabel));
        linkBuffer.link(jumpToEndOfPatchpoint, m_patchpoint.labelAtOffset(MacroAssembler::maxJumpReplacementSize()));
    } else
        linkBuffer.link(params.doneJumps, m_patchpoint.labelAtOffset(MacroAssembler::maxJumpReplacementSize()));
#endif // FTL_USES_B3
    if (m_exceptionTarget)
        linkBuffer.link(exceptionJumps, m_exceptionTarget);
    m_stub = FINALIZE_CODE_FOR(codeBlock, linkBuffer, ("Lazy slow path call stub"));

#if FTL_USES_B3
    MacroAssembler::repatchJump(m_patchableJump, CodeLocationLabel(m_stub.code()));
#else // FTL_USES_B3
    MacroAssembler::replaceWithJump(m_patchpoint, CodeLocationLabel(m_stub.code()));
#endif // FTL_USES_B3
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
