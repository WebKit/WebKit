/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "RegisterSet.h"

#if ENABLE(ASSEMBLER)

#include "GPRInfo.h"
#include "JSCInlines.h"
#include "MacroAssembler.h"
#include "RegisterAtOffsetList.h"
#include "assembler/RegisterInfo.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

RegisterSet RegisterSet::stackRegisters()
{
    return RegisterSet(
        MacroAssembler::stackPointerRegister,
        MacroAssembler::framePointerRegister);
}

RegisterSet RegisterSet::reservedHardwareRegisters()
{
    RegisterSet result;

#define SET_IF_RESERVED(id, name, isReserved, isCalleeSaved)    \
    if (isReserved)                                             \
        result.set(RegisterNames::id);
    FOR_EACH_GP_REGISTER(SET_IF_RESERVED)
    FOR_EACH_FP_REGISTER(SET_IF_RESERVED)
#undef SET_IF_RESERVED

    return result;
}

RegisterSet RegisterSet::runtimeTagRegisters()
{
#if USE(JSVALUE64)
    return RegisterSet(GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
#else
    return { };
#endif
}

RegisterSet RegisterSet::specialRegisters()
{
    return RegisterSet(
        stackRegisters(), reservedHardwareRegisters(), runtimeTagRegisters());
}

RegisterSet RegisterSet::volatileRegistersForJSCall()
{
    RegisterSet volatileRegisters = allRegisters();
    volatileRegisters.exclude(RegisterSet::stackRegisters());
    volatileRegisters.exclude(RegisterSet::reservedHardwareRegisters());
    volatileRegisters.exclude(RegisterSet::vmCalleeSaveRegisters());
    return volatileRegisters;
}

RegisterSet RegisterSet::stubUnavailableRegisters()
{
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    return RegisterSet(specialRegisters(), vmCalleeSaveRegisters());
}

RegisterSet RegisterSet::macroScratchRegisters()
{
#if CPU(X86_64)
    return RegisterSet(MacroAssembler::s_scratchRegister);
#elif CPU(ARM64)
    return RegisterSet(MacroAssembler::dataTempRegister, MacroAssembler::memoryTempRegister);
#elif CPU(MIPS)
    RegisterSet result;
    result.set(MacroAssembler::immTempRegister);
    result.set(MacroAssembler::dataTempRegister);
    result.set(MacroAssembler::addrTempRegister);
    result.set(MacroAssembler::cmpTempRegister);
    return result;
#else
    return { };
#endif
}

RegisterSet RegisterSet::calleeSaveRegisters()
{
    RegisterSet result;

#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.set(RegisterNames::id);
    FOR_EACH_GP_REGISTER(SET_IF_CALLEESAVED)
    FOR_EACH_FP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED
        
    return result;
}

RegisterSet RegisterSet::vmCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86_64)
    result.set(GPRInfo::regCS0);
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
#if OS(WINDOWS)
    result.set(GPRInfo::regCS5);
    result.set(GPRInfo::regCS6);
#endif
#elif CPU(ARM64)
    result.set(GPRInfo::regCS0);
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
    result.set(GPRInfo::regCS5);
    result.set(GPRInfo::regCS6);
    result.set(GPRInfo::regCS7);
    result.set(GPRInfo::regCS8);
    result.set(GPRInfo::regCS9);
    result.set(FPRInfo::fpRegCS0);
    result.set(FPRInfo::fpRegCS1);
    result.set(FPRInfo::fpRegCS2);
    result.set(FPRInfo::fpRegCS3);
    result.set(FPRInfo::fpRegCS4);
    result.set(FPRInfo::fpRegCS5);
    result.set(FPRInfo::fpRegCS6);
    result.set(FPRInfo::fpRegCS7);
#elif CPU(ARM_THUMB2) || CPU(MIPS)
    result.set(GPRInfo::regCS0);
#endif
    return result;
}

RegisterAtOffsetList* RegisterSet::vmCalleeSaveRegisterOffsets()
{
    static RegisterAtOffsetList* result;
    static std::once_flag calleeSavesFlag;
    std::call_once(calleeSavesFlag, [] () {
        result = new RegisterAtOffsetList(vmCalleeSaveRegisters(), RegisterAtOffsetList::ZeroBased);
    });
    return result;
}

RegisterSet RegisterSet::llintBaselineCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
#if !OS(WINDOWS)
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
    ASSERT(GPRInfo::regCS3 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS4 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
#else
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
    ASSERT(GPRInfo::regCS5 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS6 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS5);
    result.set(GPRInfo::regCS6);
#endif
#elif CPU(ARM_THUMB2)
    result.set(GPRInfo::regCS0);
#elif CPU(ARM64)
    result.set(GPRInfo::regCS6);
    result.set(GPRInfo::regCS7);
    ASSERT(GPRInfo::regCS8 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS9 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS8);
    result.set(GPRInfo::regCS9);
#elif CPU(MIPS)
    result.set(GPRInfo::regCS0);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

RegisterSet RegisterSet::dfgCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
    result.set(GPRInfo::regCS0);
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
#if !OS(WINDOWS)
    ASSERT(GPRInfo::regCS3 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS4 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
#else
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
    ASSERT(GPRInfo::regCS5 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS6 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS5);
    result.set(GPRInfo::regCS6);
#endif
#elif CPU(ARM_THUMB2)
#elif CPU(ARM64)
    ASSERT(GPRInfo::regCS8 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS9 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS8);
    result.set(GPRInfo::regCS9);
#elif CPU(MIPS)
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

RegisterSet RegisterSet::ftlCalleeSaveRegisters()
{
    RegisterSet result;
#if ENABLE(FTL_JIT)
#if CPU(X86_64) && !OS(WINDOWS)
    result.set(GPRInfo::regCS0);
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
    ASSERT(GPRInfo::regCS3 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS4 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
#elif CPU(ARM64)
    // B3 might save and use all ARM64 callee saves specified in the ABI.
    result.set(GPRInfo::regCS0);
    result.set(GPRInfo::regCS1);
    result.set(GPRInfo::regCS2);
    result.set(GPRInfo::regCS3);
    result.set(GPRInfo::regCS4);
    result.set(GPRInfo::regCS5);
    result.set(GPRInfo::regCS6);
    result.set(GPRInfo::regCS7);
    ASSERT(GPRInfo::regCS8 == GPRInfo::tagTypeNumberRegister);
    ASSERT(GPRInfo::regCS9 == GPRInfo::tagMaskRegister);
    result.set(GPRInfo::regCS8);
    result.set(GPRInfo::regCS9);
    result.set(FPRInfo::fpRegCS0);
    result.set(FPRInfo::fpRegCS1);
    result.set(FPRInfo::fpRegCS2);
    result.set(FPRInfo::fpRegCS3);
    result.set(FPRInfo::fpRegCS4);
    result.set(FPRInfo::fpRegCS5);
    result.set(FPRInfo::fpRegCS6);
    result.set(FPRInfo::fpRegCS7);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
#endif
    return result;
}

RegisterSet RegisterSet::argumentGPRS()
{
    RegisterSet result;
#if NUMBER_OF_ARGUMENT_REGISTERS
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; i++)
        result.set(GPRInfo::toArgumentRegister(i));
#endif
    return result;
}

RegisterSet RegisterSet::registersToNotSaveForJSCall()
{
    return RegisterSet(RegisterSet::vmCalleeSaveRegisters(), RegisterSet::stackRegisters(), RegisterSet::reservedHardwareRegisters());
}

RegisterSet RegisterSet::registersToNotSaveForCCall()
{
    return RegisterSet(RegisterSet::calleeSaveRegisters(), RegisterSet::stackRegisters(), RegisterSet::reservedHardwareRegisters());
}

RegisterSet RegisterSet::allGPRs()
{
    RegisterSet result;
    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1))
        result.set(reg);
    return result;
}

RegisterSet RegisterSet::allFPRs()
{
    RegisterSet result;
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1))
        result.set(reg);
    return result;
}

RegisterSet RegisterSet::allRegisters()
{
    RegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    return result;
}

size_t RegisterSet::numberOfSetGPRs() const
{
    RegisterSet temp = *this;
    temp.filter(allGPRs());
    return temp.numberOfSetRegisters();
}

size_t RegisterSet::numberOfSetFPRs() const
{
    RegisterSet temp = *this;
    temp.filter(allFPRs());
    return temp.numberOfSetRegisters();
}

void RegisterSet::dump(PrintStream& out) const
{
    CommaPrinter comma;
    out.print("[");
    for (Reg reg = Reg::first(); reg <= Reg::last(); reg = reg.next()) {
        if (get(reg))
            out.print(comma, reg);
    }
    out.print("]");
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

