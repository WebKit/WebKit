/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "FPRInfo.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "RegisterAtOffsetList.h"

namespace JSC {

RegisterAtOffsetList* RegisterSet::vmCalleeSaveRegisterOffsets()
{
    static RegisterAtOffsetList* result;
    static std::once_flag calleeSavesFlag;
    std::call_once(calleeSavesFlag, [] () {
        result = new RegisterAtOffsetList(vmCalleeSaveRegisters(), RegisterAtOffsetList::ZeroBased);
#if USE(JSVALUE64)
        ASSERT(result->registerCount() == result->sizeOfAreaInBytes() / sizeof(CPURegister));
#endif
    });
    return result;
}

WholeRegisterSet RegisterSet::stackRegisters()
{
    return RegisterSet(
        MacroAssembler::stackPointerRegister,
        MacroAssembler::framePointerRegister).whole();
}

WholeRegisterSet RegisterSet::reservedHardwareRegisters()
{
    WholeRegisterSet result;

#define SET_IF_RESERVED(id, name, isReserved, isCalleeSaved)    \
    if (isReserved)                                             \
        result.includeRegister(RegisterNames::id, Width128);
    FOR_EACH_GP_REGISTER(SET_IF_RESERVED)
    FOR_EACH_FP_REGISTER(SET_IF_RESERVED)
#undef SET_IF_RESERVED

    ASSERT(!result.numberOfSetFPRs());

    return result;
}

WholeRegisterSet RegisterSet::runtimeTagRegisters()
{
#if USE(JSVALUE64)
    return RegisterSet(GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister).whole();
#else
    return { };
#endif
}

WholeRegisterSet RegisterSet::specialRegisters()
{
    return RegisterSet(
        RegisterSet::stackRegisters(),
        RegisterSet::reservedHardwareRegisters(),
        runtimeTagRegisters()).whole();
}

WholeRegisterSet RegisterSet::stubUnavailableRegisters()
{
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    return RegisterSet(specialRegisters(), vmCalleeSaveRegisters()).whole();
}

WholeRegisterSet RegisterSet::macroClobberedRegisters()
{
#if CPU(X86_64)
    return RegisterSet(MacroAssembler::s_scratchRegister).whole();
#elif CPU(ARM64) || CPU(RISCV64)
    return RegisterSet(MacroAssembler::dataTempRegister, MacroAssembler::memoryTempRegister).whole();
#elif CPU(ARM_THUMB2)
    WholeRegisterSet result;
    result.includeRegister(MacroAssembler::dataTempRegister, Width64);
    result.includeRegister(MacroAssembler::addressTempRegister, Width64);
    result.includeRegister(MacroAssembler::fpTempRegister, Width64);
    return result;
#elif CPU(MIPS)
    WholeRegisterSet result;
    result.includeRegister(MacroAssembler::immTempRegister, Width64);
    result.includeRegister(MacroAssembler::dataTempRegister, Width64);
    result.includeRegister(MacroAssembler::addrTempRegister, Width64);
    result.includeRegister(MacroAssembler::cmpTempRegister, Width64);
    return result;
#else
    return { };
#endif
}

WholeRegisterSet RegisterSet::calleeSaveRegisters()
{
    WholeRegisterSet result;

#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.includeRegister(RegisterNames::id, Width64);
    FOR_EACH_GP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED
#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.includeRegister(RegisterNames::id, Width64);
    FOR_EACH_FP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED

    return result;
}

WholeRegisterSet RegisterSet::vmCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86_64)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
#if OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
#endif
#elif CPU(ARM64)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
#elif CPU(ARM_THUMB2)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
#elif CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
#elif CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
    result.includeRegister(GPRInfo::regCS10, Width64);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
    result.includeRegister(FPRInfo::fpRegCS8, Width64);
    result.includeRegister(FPRInfo::fpRegCS9, Width64);
    result.includeRegister(FPRInfo::fpRegCS10, Width64);
    result.includeRegister(FPRInfo::fpRegCS11, Width64);
#endif
    return result;
}

WholeRegisterSet RegisterSet::llintBaselineCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
#if !OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS1, Width64);
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
#else
    result.includeRegister(GPRInfo::regCS3, Width64);
    static_assert(GPRInfo::regCS4 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS5 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS6 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
#endif
#elif CPU(ARM_THUMB2) || CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
#elif CPU(ARM64) || CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS6, Width64);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

WholeRegisterSet RegisterSet::dfgCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if CPU(X86)
#elif CPU(X86_64)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
#if !OS(WINDOWS)
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
#else
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    static_assert(GPRInfo::regCS4 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS5 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS6 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
#endif
#elif CPU(ARM_THUMB2) || CPU(MIPS)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
#elif CPU(ARM64) || CPU(RISCV64)
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

WholeRegisterSet RegisterSet::ftlCalleeSaveRegisters()
{
    WholeRegisterSet result;
#if ENABLE(FTL_JIT)
#if CPU(X86_64) && !OS(WINDOWS)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    static_assert(GPRInfo::regCS2 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
#elif CPU(ARM64)
    // B3 might save and use all ARM64 callee saves specified in the ABI.
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
#elif CPU(RISCV64)
    result.includeRegister(GPRInfo::regCS0, Width64);
    result.includeRegister(GPRInfo::regCS1, Width64);
    result.includeRegister(GPRInfo::regCS2, Width64);
    result.includeRegister(GPRInfo::regCS3, Width64);
    result.includeRegister(GPRInfo::regCS4, Width64);
    result.includeRegister(GPRInfo::regCS5, Width64);
    result.includeRegister(GPRInfo::regCS6, Width64);
    static_assert(GPRInfo::regCS7 == GPRInfo::constantsRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.includeRegister(GPRInfo::regCS7, Width64);
    result.includeRegister(GPRInfo::regCS8, Width64);
    result.includeRegister(GPRInfo::regCS9, Width64);
    result.includeRegister(GPRInfo::regCS10, Width64);
    result.includeRegister(FPRInfo::fpRegCS0, Width64);
    result.includeRegister(FPRInfo::fpRegCS1, Width64);
    result.includeRegister(FPRInfo::fpRegCS2, Width64);
    result.includeRegister(FPRInfo::fpRegCS3, Width64);
    result.includeRegister(FPRInfo::fpRegCS4, Width64);
    result.includeRegister(FPRInfo::fpRegCS5, Width64);
    result.includeRegister(FPRInfo::fpRegCS6, Width64);
    result.includeRegister(FPRInfo::fpRegCS7, Width64);
    result.includeRegister(FPRInfo::fpRegCS8, Width64);
    result.includeRegister(FPRInfo::fpRegCS9, Width64);
    result.includeRegister(FPRInfo::fpRegCS10, Width64);
    result.includeRegister(FPRInfo::fpRegCS11, Width64);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
#endif
    return result;
}

WholeRegisterSet RegisterSet::argumentGPRS()
{
    WholeRegisterSet result;
#if NUMBER_OF_ARGUMENT_REGISTERS
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; i++)
        result.includeRegister(GPRInfo::toArgumentRegister(i), Width64);
#endif
    return result;
}

RegisterSet RegisterSet::registersToSaveForJSCall(RegisterSet liveRegisters)
{
    RegisterSet result = liveRegisters;
    result.exclude(RegisterSet::vmCalleeSaveRegisters());
    result.exclude(RegisterSet::stackRegisters());
    result.exclude(RegisterSet::reservedHardwareRegisters());
    return WholeRegisterSet(result);
}

RegisterSet RegisterSet::registersToSaveForCCall(RegisterSet liveRegisters)
{
    RegisterSet result = liveRegisters;
    result.exclude(RegisterSet::calleeSaveRegisters());
    result.exclude(RegisterSet::stackRegisters());
    result.exclude(RegisterSet::reservedHardwareRegisters());
    return WholeRegisterSet(result);
}

WholeRegisterSet RegisterSet::allGPRs()
{
    WholeRegisterSet result;
    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1))
        result.includeRegister(reg, Width64);
    return result;
}

WholeRegisterSet RegisterSet::allFPRs()
{
    WholeRegisterSet result;
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1))
        result.includeRegister(reg, Width128);
    return result;
}

WholeRegisterSet RegisterSet::allRegisters()
{
    WholeRegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    return result;
}

WholeRegisterSet RegisterSet::allScalarRegisters()
{
    WholeRegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    result.m_upperBits.clearAll();
    return result;
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

