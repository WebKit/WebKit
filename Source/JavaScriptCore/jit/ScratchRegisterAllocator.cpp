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
    ASSERT(Reg::fromIndex(reg).isGPR());
    m_lockedRegisters.add(reg, IgnoreVectors);
}

void ScratchRegisterAllocator::lock(FPRReg reg)
{
    if (reg == InvalidFPRReg)
        return;
    ASSERT(Reg::fromIndex(reg).isFPR());
    m_lockedRegisters.add(reg, IgnoreVectors);
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
        auto reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.contains(reg, IgnoreVectors)
            && !m_usedRegisters.contains(reg, IgnoreVectors)
            && !m_scratchRegisters.contains(reg, IgnoreVectors)) {
            m_scratchRegisters.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
            return reg;
        }
    }
    
    // Since that failed, try to allocate a register that is not yet
    // locked or used for scratch.
    for (unsigned i = 0; i < BankInfo::numberOfRegisters; ++i) {
        auto reg = BankInfo::toRegister(i);
        if (!m_lockedRegisters.contains(reg, IgnoreVectors) && !m_scratchRegisters.contains(reg, IgnoreVectors)) {
            m_scratchRegisters.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
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

    JIT_COMMENT(jit, "preserveReusedRegistersByPushing");

    RegisterSet registersToSpill;
    for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
        FPRReg reg = FPRInfo::toRegister(i);
        ASSERT(reg != InvalidFPRReg);
        if (m_scratchRegisters.contains(reg, IgnoreVectors) && m_usedRegisters.contains(reg, IgnoreVectors))
            registersToSpill.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
    }
    for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
        GPRReg reg = GPRInfo::toRegister(i);
        ASSERT(reg != InvalidGPRReg);
        if (m_scratchRegisters.contains(reg, IgnoreVectors) && m_usedRegisters.contains(reg, IgnoreVectors))
            registersToSpill.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
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
    
    JIT_COMMENT(jit, "restoreReusedRegistersByPopping");

    RegisterSet registersToFill;
    for (unsigned i = GPRInfo::numberOfRegisters; i--;) {
        GPRReg reg = GPRInfo::toRegister(i);
        ASSERT(reg != InvalidGPRReg);
        if (m_scratchRegisters.contains(reg, IgnoreVectors) && m_usedRegisters.contains(reg, IgnoreVectors))
            registersToFill.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
    }
    for (unsigned i = FPRInfo::numberOfRegisters; i--;) {
        FPRReg reg = FPRInfo::toRegister(i);
        ASSERT(reg != InvalidFPRReg);
        if (m_scratchRegisters.contains(reg, IgnoreVectors) && m_usedRegisters.contains(reg, IgnoreVectors))
            registersToFill.add(reg, Options::useWebAssemblySIMD() ? conservativeWidth(reg) : conservativeWidthWithoutVectors(reg));
    }

    unsigned extraStackBytesAtTopOfStack =
        preservedState.extraStackSpaceRequirement == ExtraStackSpace::SpaceForCCall ? maxFrameExtentForSlowPathCall : 0;
    ScratchRegisterAllocator::restoreRegistersFromStackForCall(jit, registersToFill, { },
        preservedState.numberOfBytesPreserved, extraStackBytesAtTopOfStack);
}

unsigned ScratchRegisterAllocator::preserveRegistersToStackForCall(AssemblyHelpers& jit, const RegisterSet& usedRegisters, unsigned extraBytesAtTopOfStack)
{
    RELEASE_ASSERT(extraBytesAtTopOfStack % sizeof(void*) == 0);
    if (!usedRegisters.numberOfSetRegisters())
        return 0;
    ASSERT(!usedRegisters.hasAnyWideRegisters() || Options::useWebAssemblySIMD());
    JIT_COMMENT(jit, "Preserve registers to stack for call: ", usedRegisters, "; Extra bytes at top of stack: ", extraBytesAtTopOfStack);

    unsigned stackOffset = usedRegisters.byteSizeOfSetRegisters();
    stackOffset += extraBytesAtTopOfStack;
    stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset);
    jit.subPtr(
        MacroAssembler::TrustedImm32(stackOffset),
        MacroAssembler::stackPointerRegister);

    AssemblyHelpers::StoreRegSpooler spooler(jit, MacroAssembler::stackPointerRegister);

    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.contains(reg, IgnoreVectors)) {
            spooler.storeGPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (count * conservativeRegisterBytesWithoutVectors(reg))), conservativeWidthWithoutVectors(reg) });
            count++;
        }
    }
    spooler.finalizeGPR();

    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (conservativeWidth(reg) == Width128 && usedRegisters.contains(reg, conservativeWidth(reg))) {
            spooler.storeVector({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (count * conservativeRegisterBytesWithoutVectors(reg))), conservativeWidth(reg) });
            count += 2;
        } else if (usedRegisters.contains(reg, IgnoreVectors)) {
            spooler.storeFPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (count * conservativeRegisterBytesWithoutVectors(reg))), conservativeWidthWithoutVectors(reg) });
            count++;
        }
    }
    spooler.finalizeFPR();

#if USE(JSVALUE64)
    ASSERT(count * sizeof(EncodedJSValue) == usedRegisters.byteSizeOfSetRegisters());
#endif
    return stackOffset;
}

void ScratchRegisterAllocator::restoreRegistersFromStackForCall(AssemblyHelpers& jit, const RegisterSet& usedRegisters, const RegisterSet& ignore, unsigned numberOfStackBytesUsedForRegisterPreservation, unsigned extraBytesAtTopOfStack)
{
    RELEASE_ASSERT(extraBytesAtTopOfStack % sizeof(void*) == 0);
    if (!usedRegisters.numberOfSetRegisters()) {
        RELEASE_ASSERT(numberOfStackBytesUsedForRegisterPreservation == 0);
        return;
    }
    ASSERT(!usedRegisters.hasAnyWideRegisters() || Options::useWebAssemblySIMD());
    JIT_COMMENT(jit, "Restore registers from stack for call: ", usedRegisters, "; Extra bytes at top of stack: ", extraBytesAtTopOfStack);

    AssemblyHelpers::LoadRegSpooler spooler(jit, MacroAssembler::stackPointerRegister);

    unsigned count = 0;
    for (GPRReg reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (usedRegisters.contains(reg, IgnoreVectors)) {
            if (!ignore.contains(reg, IgnoreVectors))
                spooler.loadGPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (conservativeRegisterBytesWithoutVectors(reg) * count)), conservativeWidthWithoutVectors(reg) });
            count++;
        }
    }
    spooler.finalizeGPR();

    for (FPRReg reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (usedRegisters.contains(reg, IgnoreVectors)) {
            // You should never have to ignore only part of a register.
            ASSERT(ignore.contains(reg, IgnoreVectors) == ignore.contains(reg, Width128));
            if (conservativeWidth(reg) == Width128 && usedRegisters.contains(reg, conservativeWidth(reg))) {
                if (!ignore.contains(reg, IgnoreVectors)) {
                    spooler.loadVector({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (conservativeRegisterBytesWithoutVectors(reg) * count)), conservativeWidth(reg) });
                    count += 2;
                }
            } else if (usedRegisters.contains(reg, IgnoreVectors)) {
                if (!ignore.contains(reg, IgnoreVectors)) {
                    spooler.loadFPR({ reg, static_cast<ptrdiff_t>(extraBytesAtTopOfStack + (conservativeRegisterBytesWithoutVectors(reg) * count)), conservativeWidthWithoutVectors(reg) });
                    count++;
                }
            }
        }
    }
    spooler.finalizeFPR();

    unsigned stackOffset = usedRegisters.byteSizeOfSetRegisters();
    stackOffset += extraBytesAtTopOfStack;
    stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), stackOffset);

#if USE(JSVALUE64)
    ASSERT(count * sizeof(EncodedJSValue) <= usedRegisters.byteSizeOfSetRegisters());
#endif
    RELEASE_ASSERT(stackOffset == numberOfStackBytesUsedForRegisterPreservation);

    jit.addPtr(
        MacroAssembler::TrustedImm32(stackOffset),
        MacroAssembler::stackPointerRegister);
}

} // namespace JSC

#endif // ENABLE(JIT)
