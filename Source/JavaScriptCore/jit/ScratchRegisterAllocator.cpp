/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "VM.h"

namespace JSC {

ScratchRegisterAllocator::ScratchRegisterAllocator(const TempRegisterSet& usedRegisters)
    : m_usedRegisters(usedRegisters)
    , m_numberOfReusedRegisters(0)
{
}

ScratchRegisterAllocator::~ScratchRegisterAllocator() { }

void ScratchRegisterAllocator::lock(GPRReg reg)
{
    unsigned index = GPRInfo::toIndex(reg);
    if (index == GPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setGPRByIndex(index);
}

void ScratchRegisterAllocator::lock(FPRReg reg)
{
    unsigned index = FPRInfo::toIndex(reg);
    if (index == FPRInfo::InvalidIndex)
        return;
    m_lockedRegisters.setFPRByIndex(index);
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

void ScratchRegisterAllocator::preserveReusedRegistersByPushing(MacroAssembler& jit)
{
    if (!didReuseRegisters())
        return;
        
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.getFPRByIndex(i))
            jit.pushToSave(FPRInfo::toRegister(i));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.getGPRByIndex(i))
            jit.pushToSave(GPRInfo::toRegister(i));
    }
}

void ScratchRegisterAllocator::restoreReusedRegistersByPopping(MacroAssembler& jit)
{
    if (!didReuseRegisters())
        return;
        
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        if (m_scratchRegisters.getGPRByIndex(i) && m_usedRegisters.getGPRByIndex(i))
            jit.popToRestore(GPRInfo::toRegister(i));
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        if (m_scratchRegisters.getFPRByIndex(i) && m_usedRegisters.getFPRByIndex(i))
            jit.popToRestore(FPRInfo::toRegister(i));
    }
}

unsigned ScratchRegisterAllocator::desiredScratchBufferSize() const
{
    return m_usedRegisters.numberOfSetRegisters() * sizeof(JSValue);
}

void ScratchRegisterAllocator::preserveUsedRegistersToScratchBuffer(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR)
{
    unsigned count = 0;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        if (m_usedRegisters.getGPRByIndex(i)) {
#if USE(JSVALUE64)
            jit.store64(GPRInfo::toRegister(i), static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++));
#else
            jit.store32(GPRInfo::toRegister(i), static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++));
#endif
        }
        if (scratchGPR == InvalidGPRReg && !m_lockedRegisters.getGPRByIndex(i) && !m_scratchRegisters.getGPRByIndex(i))
            scratchGPR = GPRInfo::toRegister(i);
    }
    RELEASE_ASSERT(scratchGPR != InvalidGPRReg);
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        if (m_usedRegisters.getFPRByIndex(i)) {
            jit.move(MacroAssembler::TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++)), scratchGPR);
            jit.storeDouble(FPRInfo::toRegister(i), scratchGPR);
        }
    }
    RELEASE_ASSERT(count * sizeof(JSValue) == desiredScratchBufferSize());
    
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratchGPR);
    jit.storePtr(MacroAssembler::TrustedImmPtr(static_cast<size_t>(count * sizeof(JSValue))), scratchGPR);
}

void ScratchRegisterAllocator::restoreUsedRegistersFromScratchBuffer(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR)
{
    if (scratchGPR == InvalidGPRReg) {
        // Find a scratch register.
        for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
            if (m_lockedRegisters.getGPRByIndex(i) || m_scratchRegisters.getGPRByIndex(i))
                continue;
            scratchGPR = GPRInfo::toRegister(i);
            break;
        }
    }
    RELEASE_ASSERT(scratchGPR != InvalidGPRReg);
        
    jit.move(MacroAssembler::TrustedImmPtr(scratchBuffer->activeLengthPtr()), scratchGPR);
    jit.storePtr(MacroAssembler::TrustedImmPtr(0), scratchGPR);

    // Restore double registers first.
    unsigned count = m_usedRegisters.numberOfSetGPRs();
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        if (m_usedRegisters.getFPRByIndex(i)) {
            jit.move(MacroAssembler::TrustedImmPtr(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++)), scratchGPR);
            jit.loadDouble(scratchGPR, FPRInfo::toRegister(i));
        }
    }
        
    count = 0;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        if (m_usedRegisters.getGPRByIndex(i)) {
#if USE(JSVALUE64)
            jit.load64(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++), GPRInfo::toRegister(i));
#else
            jit.load32(static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer()) + (count++), GPRInfo::toRegister(i));
#endif
        }
    }
}

} // namespace JSC

#endif // ENABLE(JIT)
