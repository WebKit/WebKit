/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)
#include "MacroAssemblerARMv7.h"

namespace JSC {

#if ENABLE(MASM_PROBE)

#define INDENT printIndent(indentation)

void MacroAssemblerARMv7::printCPURegisters(CPUState& cpu, int indentation)
{
    #define PRINT_GPREGISTER(_type, _regName) { \
        int32_t value = reinterpret_cast<int32_t>(cpu._regName); \
        INDENT, dataLogF("%5s: 0x%08x  %d\n", #_regName, value, value) ; \
    }
    FOR_EACH_CPU_GPREGISTER(PRINT_GPREGISTER)
    FOR_EACH_CPU_SPECIAL_REGISTER(PRINT_GPREGISTER)
    #undef PRINT_GPREGISTER

    #define PRINT_FPREGISTER(_type, _regName) { \
        uint64_t* u = reinterpret_cast<uint64_t*>(&cpu._regName); \
        double* d = reinterpret_cast<double*>(&cpu._regName); \
        INDENT, dataLogF("%5s: 0x%016llx  %.13g\n", #_regName, *u, *d); \
    }
    FOR_EACH_CPU_FPREGISTER(PRINT_FPREGISTER)
    #undef PRINT_FPREGISTER
}

#undef INDENT

void MacroAssemblerARMv7::printRegister(MacroAssemblerARMv7::CPUState& cpu, RegisterID regID)
{
    const char* name = CPUState::registerName(regID);
    union {
        void* voidPtr;
        intptr_t intptrValue;
    } u;
    u.voidPtr = cpu.registerValue(regID);
    dataLogF("%s:<%p %ld>", name, u.voidPtr, u.intptrValue);
}

void MacroAssemblerARMv7::printRegister(MacroAssemblerARMv7::CPUState& cpu, FPRegisterID regID)
{
    const char* name = CPUState::registerName(regID);
    union {
        double doubleValue;
        uint64_t uint64Value;
    } u;
    u.doubleValue = cpu.registerValue(regID);
    dataLogF("%s:<0x%016llx %.13g>", name, u.uint64Value, u.doubleValue);
}

extern "C" void ctiMasmProbeTrampoline();

// For details on "What code is emitted for the probe?" and "What values are in
// the saved registers?", see comment for MacroAssemblerX86Common::probe() in
// MacroAssemblerX86Common.cpp.

void MacroAssemblerARMv7::probe(MacroAssemblerARMv7::ProbeFunction function, void* arg1, void* arg2)
{
    push(RegisterID::lr);
    push(RegisterID::lr);
    add32(TrustedImm32(8), RegisterID::sp, RegisterID::lr);
    store32(RegisterID::lr, ArmAddress(RegisterID::sp, 4));
    push(RegisterID::ip);
    push(RegisterID::r0);
    // The following uses RegisterID::ip. So, they must come after we push ip above.
    push(trustedImm32FromPtr(arg2));
    push(trustedImm32FromPtr(arg1));
    push(trustedImm32FromPtr(function));

    move(trustedImm32FromPtr(ctiMasmProbeTrampoline), RegisterID::ip);
    m_assembler.blx(RegisterID::ip);
}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

