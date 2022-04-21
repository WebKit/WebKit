/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "ScratchRegisterAllocator.h"

#if ENABLE(JIT)

#include "AssemblyHelpersSpoolers.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "VM.h"

namespace JSC {

ScratchRegisterAllocator::ScratchRegisterAllocator(const RegisterSet& usedRegisters)
    : m_usedRegisters(usedRegisters)
    , m_numberOfReusedRegisters(0)
{
}

ScratchRegisterAllocator::~ScratchRegisterAllocator() { }

void ScratchRegisterAllocator::lock(GPRReg reg)
{
    if (reg == InvalidGPRReg)
        return;
    unsigned index = GPRInfo::toIndex(reg);
    if (index == GPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setGPRByIndex(index);
}

void ScratchRegisterAllocator::lock(FPRReg reg)
{
    if (reg == InvalidFPRReg)
        return;
    unsigned index = FPRInfo::toIndex(reg);
    if (index == FPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setFPRByIndex(index);
}

void ScratchRegisterAllocator::lock(JSValueRegs regs)
{
    lock(regs.tagGPR());
    lock(regs.payloadGPR());
}

template<typename BankInfo>
typename BankInfo::RegisterType ScratchRegisterAllocator::allocateScratch()
{
    // First try to allocate a register that is totally free.
    for (unsigned i = 0; i < BankInfo::numberOfRegisters; ++i) {
        typename BankInfo::RegisterType reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.get(reg)
            && !m_usedRegisters.get(reg)
            && !m_scratchRegisters.get(reg)) {
            m_scratchRegisters.set(reg);
            return reg;
        }
    }
    
    // Since that failed, try to allocate a register that is not yet
    // locked or used for scratch.
    for (unsigned i = 0; i < BankInfo::numberOfRegisters; ++i) {
        typename BankInfo::RegisterType reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.get(reg) && !m_scratchRegisters.get(reg)) {
            m_scratchRegisters.set(reg);
            m_numberOfReusedRegisters++;
            return reg;
        }
    }
        
    // We failed.
    CRASH();
    // Make some silly compilers happy.
    return static_cast<typename BankInfo::RegisterType>(-1);
}

GPRReg ScratchRegisterAllocator::allocateScratchGPR() { return allocateScratch<GPRInfo>(); }
FPRReg ScratchRegisterAllocator::allocateScratchFPR() { return allocateScratch<FPRInfo>(); }

ScratchRegisterAllocator::PreservedState ScratchRegisterAllocator::preserveReusedRegistersByPushing(AssemblyHelpers& jit, ExtraStackSpace extraStackSpace)
{
    if (!didReuseRegisters())
        return PreservedState(0, extraStackSpace);

    RegisterSet registersToSpill;
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg reg = FPRInfo::toRegister(i);
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.get(reg))
            registersToSpill.set(reg);
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.get(reg))
            registersToSpill.set(reg);
    }

    unsigned extraStackBytesAtTopOfStack = extraStackSpace == ExtraStackSpace::SpaceForCCall ? maxFrameExtentForSlowPathCall : 0;
    unsigned stackAdjustmentSize = ScratchRegisterAllocator::preserveRegistersToStackForCall(jit, registersToSpill, extraStackBytesAtTopOfStack);

    return PreservedState(stackAdjustmentSize, extraStackSpace);
}

void ScratchRegisterAllocator::restoreReusedRegistersByPopping(AssemblyHelpers& jit, const ScratchRegisterAllocator::PreservedState& preservedState)
{
    RELEASE_ASSERT(preservedState);
    if (!didReuseRegisters())
        return;

    RegisterSet registersToFill;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg reg = GPRInfo::toRegister(i);
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.get(reg))
            registersToFill.set(reg);
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        FPRReg reg = FPRInfo::toRegister(i);
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.get(reg))
            registersToFill.set(reg);
    }

    unsigned extraStackBytesAtTopOfStack =
        preservedState.extraStackSpaceRequirement == ExtraStackSpace::SpaceForCCall ? maxFrameExtentForSlowPathCall : 0;
    RegisterSet dontRestore; // Empty set. We want to restore everything.
    ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToFill, dontRestore,
        preservedState.numberOfBytesPreserved, extraStackBytesAtTopOfStack);
}

RegisterSet ScratchRegisterAllocator::usedRegistersForCall() const
{
    RegisterSet result = m_usedRegisters;
    result.exclude(RegisterSet::registersToNotSaveForJSCall());
    return result;
}

unsigned ScratchRegisterAllocator::desiredScratchBufferSizeForCall() const
{
    return usedRegistersForCall().numberOfSetRegisters() * sizeof(JSValue);
}

unsigned ScratchRegisterAllocator::preserveRegistersToStackForCall(AssemblyHelpers& jit, const RegisterSet& usedRegisters, unsigned extraBytesAtTopOfStack)
{
    RELEASE_ASSERT(extraBytesAtTopOfStack % sizeof(void*) == 0);
    if (!usedRegisters.numberOfSetRegisters())
        return 0;
    
    unsigned stackOffset = (usedRegisters.numberOfSetRegisters()) * sizeof(EncodedJSValue);
    stackOffset += extraBytesAtTopOfStack;
    stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset);
    jit.subPtr(
        MacroAssembler::TrustedImm32(stackOffset),
        MacroAssembler::stackPointerRegister);

    AssemblyHelpers::StoreRegSpooler spooler(jit, MacroAssembler::stackPointerRegister);

    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.get(reg)) {
            spooler.storeGPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (count * sizeof(EncodedJSValue))) });
            count++;
        }
    }
    spooler.finalizeGPR();

    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (usedRegisters.get(reg)) {
            spooler.storeFPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (count * sizeof(EncodedJSValue))) });
            count++;
        }
    }
    spooler.finalizeFPR();

    RELEASE_ASSERT(count == usedRegisters.numberOfSetRegisters());

    return stackOffset;
}

void ScratchRegisterAllocator::restoreRegistersFromStackForCall(AssemblyHelpers& jit, const RegisterSet& usedRegisters, const RegisterSet& ignore, unsigned numberOfStackBytesUsedForRegisterPreservation, unsigned extraBytesAtTopOfStack)
{
    RELEASE_ASSERT(extraBytesAtTopOfStack % sizeof(void*) == 0);
    if (!usedRegisters.numberOfSetRegisters()) {
        RELEASE_ASSERT(numberOfStackBytesUsedForRegisterPreservation == 0);
        return;
    }

    AssemblyHelpers::LoadRegSpooler spooler(jit, MacroAssembler::stackPointerRegister);

    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.get(reg)) {
            if (!ignore.get(reg))
                spooler.loadGPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (sizeof(EncodedJSValue) * count)) });
            count++;
        }
    }
    spooler.finalizeGPR();

    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (usedRegisters.get(reg)) {
            if (!ignore.get(reg))
                spooler.loadFPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (sizeof(EncodedJSValue) * count)) });
            count++;
        }
    }
    spooler.finalizeFPR();

    unsigned stackOffset = (usedRegisters.numberOfSetRegisters()) * sizeof(EncodedJSValue);
    stackOffset += extraBytesAtTopOfStack;
    stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset);

    RELEASE_ASSERT(count == usedRegisters.numberOfSetRegisters());
    RELEASE_ASSERT(stackOffset == numberOfStackBytesUsedForRegisterPreservation);

    jit.addPtr(
        MacroAssembler::TrustedImm32(stackOffset),
        MacroAssembler::stackPointerRegister);
}

} // namespace JSC

#endif // ENABLE(JIT)
