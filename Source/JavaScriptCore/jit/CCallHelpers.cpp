/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "CCallHelpers.h"

#if ENABLE(JIT)

#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ShadowChicken.h"

namespace JSC {

void CCallHelpers::logShadowChickenProloguePacket(GPRReg shadowPacket, GPRReg scratch1, GPRReg scope)
{
    storePtr(GPRInfo::callFrameRegister, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, frame)));
    loadPtr(Address(GPRInfo::callFrameRegister, OBJECT_OFFSETOF(CallerFrameAndPC, callerFrame)), scratch1);
    storePtr(scratch1, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, callerFrame)));
    loadPtr(addressFor(CallFrameSlot::callee), scratch1);
    storePtr(scratch1, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, callee)));
    storePtr(scope, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, scope)));
}

void CCallHelpers::ensureShadowChickenPacket(VM& vm, GPRReg shadowPacket, GPRReg scratch1NonArgGPR, GPRReg scratch2)
{
    ShadowChicken* shadowChicken = vm.shadowChicken();
    RELEASE_ASSERT(shadowChicken);
    ASSERT(!RegisterSetBuilder::argumentGPRS().contains(scratch1NonArgGPR, IgnoreVectors));
    move(TrustedImmPtr(shadowChicken->addressOfLogCursor()), scratch1NonArgGPR);
    loadPtr(Address(scratch1NonArgGPR), shadowPacket);
    Jump ok = branchPtr(Below, shadowPacket, TrustedImmPtr(shadowChicken->logEnd()));
    setupArguments<decltype(operationProcessShadowChickenLog)>(TrustedImmPtr(&vm));
    prepareCallOperation(vm);
    move(TrustedImmPtr(tagCFunction<OperationPtrTag>(operationProcessShadowChickenLog)), scratch1NonArgGPR);
    call(scratch1NonArgGPR, OperationPtrTag);
    move(TrustedImmPtr(shadowChicken->addressOfLogCursor()), scratch1NonArgGPR);
    loadPtr(Address(scratch1NonArgGPR), shadowPacket);
    ok.link(this);
    addPtr(TrustedImm32(sizeof(ShadowChicken::Packet)), shadowPacket, scratch2);
    storePtr(scratch2, Address(scratch1NonArgGPR));
}


template <typename CodeBlockType>
void CCallHelpers::logShadowChickenTailPacketImpl(GPRReg shadowPacket, JSValueRegs thisRegs, GPRReg scope, CodeBlockType codeBlock, CallSiteIndex callSiteIndex)
{
    storePtr(GPRInfo::callFrameRegister, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, frame)));
    storePtr(TrustedImmPtr(ShadowChicken::Packet::tailMarker()), Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, callee)));
    storeValue(thisRegs, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, thisValue)));
    storePtr(scope, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, scope)));
    storePtr(codeBlock, Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, codeBlock)));
    store32(TrustedImm32(callSiteIndex.bits()), Address(shadowPacket, OBJECT_OFFSETOF(ShadowChicken::Packet, callSiteIndex)));
}

void CCallHelpers::logShadowChickenTailPacket(GPRReg shadowPacket, JSValueRegs thisRegs, GPRReg scope, GPRReg codeBlock, CallSiteIndex callSiteIndex)
{
    logShadowChickenTailPacketImpl(shadowPacket, thisRegs, scope, codeBlock, callSiteIndex);
}

void CCallHelpers::emitJITCodeOver(CodePtr<JSInternalPtrTag> where, ScopedLambda<void(CCallHelpers&)> emitCode, const char* description)
{
    CCallHelpers jit;
    emitCode(jit);

    constexpr bool needsBranchCompaction = false;
    LinkBuffer linkBuffer(jit, where, jit.m_assembler.buffer().codeSize(), LinkBuffer::Profile::InlineCache, JITCompilationMustSucceed, needsBranchCompaction);
    FINALIZE_CODE(linkBuffer, NoPtrTag, description);
}

static_assert(!((maxFrameExtentForSlowPathCall + 2 * sizeof(CPURegister)) % 16), "Stack must be aligned after CTI thunk entry");

void CCallHelpers::emitCTIThunkPrologue(bool returnAddressAlreadyTagged)
{
    // Stash frame pointer and return address
    if (!returnAddressAlreadyTagged)
        tagReturnAddress();
#if CPU(X86_64)
    push(X86Registers::ebp); // return address pushed by the call instruction
#elif CPU(ARM64) || CPU(ARM_THUMB2) || CPU(RISCV64)
    pushPair(framePointerRegister, linkRegister);
#elif CPU(MIPS)
    pushPair(framePointerRegister, returnAddressRegister);
#else
#   error "Not implemented on platform"
#endif
    // Make enough space on the stack to pass arguments in a call
    if constexpr (!!maxFrameExtentForSlowPathCall)
        subPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
}

void CCallHelpers::emitCTIThunkEpilogue()
{
    // Reset stack
    if constexpr (!!maxFrameExtentForSlowPathCall)
        addPtr(TrustedImm32(maxFrameExtentForSlowPathCall), stackPointerRegister);
    // Restore frame pointer and return address
#if CPU(X86_64)
    pop(X86Registers::ebp); // Return address left on stack
#elif CPU(ARM64) || CPU(ARM_THUMB2) || CPU(RISCV64)
    popPair(framePointerRegister, linkRegister);
#elif CPU(MIPS)
    popPair(framePointerRegister, returnAddressRegister);
#else
#   error "Not implemented on platform"
#endif
}

} // namespace JSC

#endif // ENABLE(JIT)
