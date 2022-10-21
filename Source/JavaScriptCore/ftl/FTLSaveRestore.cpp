/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "FTLSaveRestore.h"

#if ENABLE(FTL_JIT)

#include "AssemblyHelpersSpoolers.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "Reg.h"
#include "RegisterSet.h"

namespace JSC { namespace FTL {

static size_t bytesForGPRs()
{
    return MacroAssembler::numberOfRegisters() * sizeof(int64_t);
}

static size_t bytesForFPRs()
{
    // FIXME: It might be worthwhile saving the full state of the FP registers, at some point.
    // Right now we don't need this since we only do the save/restore just prior to OSR exit, and
    // OSR exit will be guaranteed to only need the double portion of the FP registers.
    return MacroAssembler::numberOfFPRegisters() * sizeof(double);
}

size_t requiredScratchMemorySizeInBytes()
{
    return bytesForGPRs() + bytesForFPRs();
}

size_t offsetOfGPR(GPRReg reg)
{
    return MacroAssembler::registerIndex(reg) * sizeof(int64_t);
}

size_t offsetOfFPR(FPRReg reg)
{
    return bytesForGPRs() + MacroAssembler::fpRegisterIndex(reg) * sizeof(double);
}

size_t offsetOfReg(Reg reg)
{
    if (reg.isGPR())
        return offsetOfGPR(reg.gpr());
    return offsetOfFPR(reg.fpr());
}

namespace {

struct Regs {
    Regs()
    {
        special = RegisterSetBuilder::stackRegisters();
        special.merge(RegisterSetBuilder::reservedHardwareRegisters());

        first = MacroAssembler::firstRegister();
        while (special.contains(first, IgnoreVectors))
            first = MacroAssembler::nextRegister(first);
    }

    GPRReg nextRegister(GPRReg current)
    {
        auto next = MacroAssembler::nextRegister(current);
        for (; next <= MacroAssembler::lastRegister(); next = MacroAssembler::nextRegister(next)) {
            if (!special.contains(next, IgnoreVectors))
                break;
        }
        return next;
    }

    RegisterSet special;
    GPRReg first;
};

} // anonymous namespace

void saveAllRegisters(AssemblyHelpers& jit, char* scratchMemory)
{
    Regs regs;
    
    // Get the first register out of the way, so that we can use it as a pointer.
    GPRReg baseGPR = regs.first;
#if CPU(ARM64)
    GPRReg nextGPR = regs.nextRegister(baseGPR);
    GPRReg firstToSaveGPR = regs.nextRegister(nextGPR);
    ASSERT(baseGPR == ARM64Registers::x0);
    ASSERT(nextGPR == ARM64Registers::x1);
#else
    GPRReg firstToSaveGPR = regs.nextRegister(baseGPR);
#endif
    jit.poke64(baseGPR, 0);
    jit.move(MacroAssembler::TrustedImmPtr(scratchMemory), baseGPR);

    AssemblyHelpers::StoreRegSpooler spooler(jit, baseGPR);

    // Get all of the other GPRs out of the way.
    for (MacroAssembler::RegisterID reg = firstToSaveGPR; reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (regs.special.contains(reg, IgnoreVectors))
            continue;
        spooler.storeGPR({ reg, static_cast<ptrdiff_t>(offsetOfGPR(reg)), conservativeWidthWithoutVectors(reg) });
    }
    spooler.finalizeGPR();
    
    // Restore the first register into the second one and save it.
    jit.peek64(firstToSaveGPR, 0);
#if CPU(ARM64)
    jit.storePair64(firstToSaveGPR, nextGPR, baseGPR, AssemblyHelpers::TrustedImm32(offsetOfGPR(baseGPR)));
#else
    jit.store64(firstToSaveGPR, MacroAssembler::Address(baseGPR, offsetOfGPR(baseGPR)));
#endif
    
    // Finally save all FPR's.
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (regs.special.contains(reg, IgnoreVectors))
            continue;
        spooler.storeFPR({ reg, static_cast<ptrdiff_t>(offsetOfFPR(reg)), conservativeWidthWithoutVectors(reg) });
    }
    spooler.finalizeFPR();
}

void restoreAllRegisters(AssemblyHelpers& jit, char* scratchMemory)
{
    Regs regs;
    
    // Give ourselves a pointer to the scratch memory.
    GPRReg baseGPR = regs.first;
    jit.move(MacroAssembler::TrustedImmPtr(scratchMemory), baseGPR);
    
    AssemblyHelpers::LoadRegSpooler spooler(jit, baseGPR);

    // Restore all FPR's.
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = MacroAssembler::nextFPRegister(reg)) {
        if (regs.special.contains(reg, IgnoreVectors))
            continue;
        spooler.loadFPR({ reg, static_cast<ptrdiff_t>(offsetOfFPR(reg)), conservativeWidthWithoutVectors(reg) });
    }
    spooler.finalizeFPR();
    
#if CPU(ARM64)
    GPRReg nextGPR = regs.nextRegister(baseGPR);
    GPRReg firstToRestoreGPR = regs.nextRegister(nextGPR);
    ASSERT(baseGPR == ARM64Registers::x0);
    ASSERT(nextGPR == ARM64Registers::x1);
#else
    GPRReg firstToRestoreGPR = regs.nextRegister(baseGPR);
#endif
    for (MacroAssembler::RegisterID reg = firstToRestoreGPR; reg <= MacroAssembler::lastRegister(); reg = MacroAssembler::nextRegister(reg)) {
        if (regs.special.contains(reg, IgnoreVectors))
            continue;
        spooler.loadGPR({ reg, static_cast<ptrdiff_t>(offsetOfGPR(reg)), conservativeWidthWithoutVectors(reg) });
    }
    spooler.finalizeGPR();

#if CPU(ARM64)
    jit.loadPair64(baseGPR, AssemblyHelpers::TrustedImm32(offsetOfGPR(baseGPR)), baseGPR, nextGPR);
#else
    jit.load64(MacroAssembler::Address(baseGPR, offsetOfGPR(baseGPR)), baseGPR);
#endif
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

