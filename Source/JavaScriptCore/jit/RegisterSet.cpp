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

RegisterSet RegisterSet::stackRegisters()
{
    RegisterSet result;
    result.add(MacroAssembler::stackPointerRegister);
    result.add(MacroAssembler::framePointerRegister);
    return result;
}

RegisterSet RegisterSet::reservedHardwareRegisters()
{
    RegisterSet result;

#define SET_IF_RESERVED(id, name, isReserved, isCalleeSaved)    \
    if (isReserved)                                             \
        result.add(RegisterNames::id, IgnoreVectors);
    FOR_EACH_GP_REGISTER(SET_IF_RESERVED)
    FOR_EACH_FP_REGISTER(SET_IF_RESERVED)
#undef SET_IF_RESERVED

    ASSERT(!result.numberOfSetFPRs());

    return result;
}

RegisterSet RegisterSet::runtimeTagRegisters()
{
#if USE(JSVALUE64)
    RegisterSet result;
    result.add(GPRInfo::numberTagRegister);
    result.add(GPRInfo::notCellMaskRegister);
    return result;
#else
    return { };
#endif
}

RegisterSet RegisterSet::specialRegisters()
{
    RegisterSet result;
    result.merge(stackRegisters());
    result.merge(reservedHardwareRegisters());
    result.merge(runtimeTagRegisters());
    return result;
}

RegisterSet RegisterSet::stubUnavailableRegisters()
{
    // FIXME: This is overly conservative. We could subtract out those callee-saves that we
    // actually saved.
    // https://bugs.webkit.org/show_bug.cgi?id=185686
    RegisterSet result;
    result.merge(specialRegisters());
    result.merge(vmCalleeSaveRegisters());
    return result;
}

RegisterSet RegisterSet::macroClobberedGPRs()
{
    RegisterSet result;
#if CPU(X86_64)
    result.add(MacroAssembler::s_scratchRegister);
#elif CPU(ARM64) || CPU(RISCV64)
    result.add(MacroAssembler::dataTempRegister);
    result.add(MacroAssembler::memoryTempRegister);
#elif CPU(ARM_THUMB2)
    result.add(MacroAssembler::dataTempRegister);
    result.add(MacroAssembler::addressTempRegister);
#endif
    return result;
}

RegisterSet RegisterSet::macroClobberedFPRs()
{
    RegisterSet result;
#if CPU(X86_64) || CPU(ARM64) || CPU(ARM_THUMB2)
    result.add(MacroAssembler::fpTempRegister, IgnoreVectors);
#elif CPU(RISCV64)
    result.add(MacroAssembler::fpTempRegister, IgnoreVectors);
    result.add(MacroAssembler::fpTempRegister2, IgnoreVectors);
#endif
    return result;
}

RegisterSet RegisterSet::calleeSaveRegisters()
{
    RegisterSet result;

#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.add(RegisterNames::id, IgnoreVectors);
    FOR_EACH_GP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED
#define SET_IF_CALLEESAVED(id, name, isReserved, isCalleeSaved)        \
    if (isCalleeSaved)                                                 \
        result.add(RegisterNames::id, Width64);
    FOR_EACH_FP_REGISTER(SET_IF_CALLEESAVED)
#undef SET_IF_CALLEESAVED

    return result;
}

RegisterSet RegisterSet::vmCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86_64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
#elif CPU(ARM64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
    result.add(GPRInfo::regCS5);
    result.add(GPRInfo::regCS6);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
    result.add(FPRInfo::fpRegCS0, Width64);
    result.add(FPRInfo::fpRegCS1, Width64);
    result.add(FPRInfo::fpRegCS2, Width64);
    result.add(FPRInfo::fpRegCS3, Width64);
    result.add(FPRInfo::fpRegCS4, Width64);
    result.add(FPRInfo::fpRegCS5, Width64);
    result.add(FPRInfo::fpRegCS6, Width64);
    result.add(FPRInfo::fpRegCS7, Width64);
#elif CPU(ARM_THUMB2)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(FPRInfo::fpRegCS0, IgnoreVectors);
    result.add(FPRInfo::fpRegCS1, IgnoreVectors);
    result.add(FPRInfo::fpRegCS2, IgnoreVectors);
    result.add(FPRInfo::fpRegCS3, IgnoreVectors);
    result.add(FPRInfo::fpRegCS4, IgnoreVectors);
    result.add(FPRInfo::fpRegCS5, IgnoreVectors);
#elif CPU(RISCV64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
    result.add(GPRInfo::regCS5);
    result.add(GPRInfo::regCS6);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
    result.add(GPRInfo::regCS10);
    result.add(FPRInfo::fpRegCS0, IgnoreVectors);
    result.add(FPRInfo::fpRegCS1, IgnoreVectors);
    result.add(FPRInfo::fpRegCS2, IgnoreVectors);
    result.add(FPRInfo::fpRegCS3, IgnoreVectors);
    result.add(FPRInfo::fpRegCS4, IgnoreVectors);
    result.add(FPRInfo::fpRegCS5, IgnoreVectors);
    result.add(FPRInfo::fpRegCS6, IgnoreVectors);
    result.add(FPRInfo::fpRegCS7, IgnoreVectors);
    result.add(FPRInfo::fpRegCS8, IgnoreVectors);
    result.add(FPRInfo::fpRegCS9, IgnoreVectors);
    result.add(FPRInfo::fpRegCS10, IgnoreVectors);
    result.add(FPRInfo::fpRegCS11, IgnoreVectors);
#endif
    return result;
}

RegisterSet RegisterSet::llintBaselineCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86_64)
    result.add(GPRInfo::regCS1);
    static_assert(GPRInfo::regCS2 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
#elif CPU(ARM_THUMB2)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
#elif CPU(ARM64) || CPU(RISCV64)
    result.add(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

RegisterSet RegisterSet::dfgCalleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86_64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    static_assert(GPRInfo::regCS2 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
#elif CPU(ARM_THUMB2)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
#elif CPU(ARM64) || CPU(RISCV64)
    static_assert(GPRInfo::regCS7 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

RegisterSet RegisterSet::ftlCalleeSaveRegisters()
{
    RegisterSet result;
#if ENABLE(FTL_JIT)
#if CPU(X86_64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    static_assert(GPRInfo::regCS2 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS3 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS4 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
#elif CPU(ARM64)
    // B3 might save and use all ARM64 callee saves specified in the ABI.
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
    result.add(GPRInfo::regCS5);
    result.add(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
    result.add(FPRInfo::fpRegCS0, Width64);
    result.add(FPRInfo::fpRegCS1, Width64);
    result.add(FPRInfo::fpRegCS2, Width64);
    result.add(FPRInfo::fpRegCS3, Width64);
    result.add(FPRInfo::fpRegCS4, Width64);
    result.add(FPRInfo::fpRegCS5, Width64);
    result.add(FPRInfo::fpRegCS6, Width64);
    result.add(FPRInfo::fpRegCS7, Width64);
#elif CPU(RISCV64)
    result.add(GPRInfo::regCS0);
    result.add(GPRInfo::regCS1);
    result.add(GPRInfo::regCS2);
    result.add(GPRInfo::regCS3);
    result.add(GPRInfo::regCS4);
    result.add(GPRInfo::regCS5);
    result.add(GPRInfo::regCS6);
    static_assert(GPRInfo::regCS7 == GPRInfo::jitDataRegister);
    static_assert(GPRInfo::regCS8 == GPRInfo::numberTagRegister);
    static_assert(GPRInfo::regCS9 == GPRInfo::notCellMaskRegister);
    result.add(GPRInfo::regCS7);
    result.add(GPRInfo::regCS8);
    result.add(GPRInfo::regCS9);
    result.add(GPRInfo::regCS10);
    result.add(FPRInfo::fpRegCS0, IgnoreVectors);
    result.add(FPRInfo::fpRegCS1, IgnoreVectors);
    result.add(FPRInfo::fpRegCS2, IgnoreVectors);
    result.add(FPRInfo::fpRegCS3, IgnoreVectors);
    result.add(FPRInfo::fpRegCS4, IgnoreVectors);
    result.add(FPRInfo::fpRegCS5, IgnoreVectors);
    result.add(FPRInfo::fpRegCS6, IgnoreVectors);
    result.add(FPRInfo::fpRegCS7, IgnoreVectors);
    result.add(FPRInfo::fpRegCS8, IgnoreVectors);
    result.add(FPRInfo::fpRegCS9, IgnoreVectors);
    result.add(FPRInfo::fpRegCS10, IgnoreVectors);
    result.add(FPRInfo::fpRegCS11, IgnoreVectors);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
#endif
    return result;
}

RegisterSet RegisterSet::argumentGPRs()
{
    RegisterSet result;
#if NUMBER_OF_ARGUMENT_REGISTERS
    for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; i++)
        result.add(GPRInfo::toArgumentRegister(i));
#endif
    return result;
}

RegisterSet RegisterSet::argumentFPRs()
{
    RegisterSet result;
#if NUMBER_OF_ARGUMENT_REGISTERS
    for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; i++)
        result.add(FPRInfo::toArgumentRegister(i), IgnoreVectors);
#endif
    return result;
}

RegisterSet RegisterSet::registersToSaveForJSCall(RegisterSet liveRegisters)
{
    liveRegisters.exclude(vmCalleeSaveRegisters());
    liveRegisters.exclude(stackRegisters());
    liveRegisters.exclude(reservedHardwareRegisters());
    return liveRegisters;
}

RegisterSet RegisterSet::registersToSaveForCCall(RegisterSet liveRegisters)
{
    liveRegisters.exclude(calleeSaveRegisters());
    liveRegisters.exclude(stackRegisters());
    liveRegisters.exclude(reservedHardwareRegisters());
    return liveRegisters;
}

RegisterSet RegisterSet::allGPRs()
{
    RegisterSet result;
    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1))
        result.add(reg);
    return result;
}

RegisterSet RegisterSet::allFPRs()
{
    RegisterSet result;
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1))
        result.add(reg, conservativeWidth(reg));
    return result;
}

RegisterSet RegisterSet::allRegisters()
{
    RegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    return result;
}

RegisterSet RegisterSet::allScalarRegisters()
{
    RegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    result.m_upperBits.clearAll();
    return result;
}

#if ENABLE(WEBASSEMBLY)
RegisterSet RegisterSet::wasmPinnedRegisters()
{
    RegisterSet result;
    if constexpr (GPRInfo::wasmBaseMemoryPointer != InvalidGPRReg)
        result.add(GPRInfo::wasmBaseMemoryPointer);
    if constexpr (GPRInfo::wasmContextInstancePointer != InvalidGPRReg)
        result.add(GPRInfo::wasmContextInstancePointer);
    if constexpr (GPRInfo::wasmBoundsCheckingSizeRegister != InvalidGPRReg)
        result.add(GPRInfo::wasmBoundsCheckingSizeRegister);
    return result;
}

RegisterSet RegisterSet::ipintCalleeSaveRegisters()
{
    RegisterSet registers;
#if CPU(X86_64)
    registers.add(GPRInfo::regCS1); // MC (pointer to metadata)
    registers.add(GPRInfo::regCS2); // PB
#elif CPU(ARM64) || CPU(RISCV64)
    registers.add(GPRInfo::regCS6); // MC
    registers.add(GPRInfo::regCS7); // PB
#elif CPU(ARM)
    registers.add(GPRInfo::regCS0); // MC
    registers.add(GPRInfo::regCS1); // PB
#else
#error Unsupported architecture.
#endif
    return registers;
}

RegisterSet RegisterSet::bbqCalleeSaveRegisters()
{
    RegisterSet registers;
    registers.add(GPRInfo::jitDataRegister);
    ASSERT(!wasmPinnedRegisters().contains(GPRInfo::jitDataRegister, IgnoreVectors));
    return registers;
}
#endif

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

