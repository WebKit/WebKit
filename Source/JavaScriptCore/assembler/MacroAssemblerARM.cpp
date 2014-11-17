/*
 * Copyright (C) 2013, 2014 Apple Inc.
 * Copyright (C) 2009 University of Szeged
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)

#include "MacroAssemblerARM.h"

#if OS(LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include <asm/hwcap.h>
#endif

namespace JSC {

static bool isVFPPresent()
{
#if OS(LINUX)
    int fd = open("/proc/self/auxv", O_RDONLY);
    if (fd != -1) {
        Elf32_auxv_t aux;
        while (read(fd, &aux, sizeof(Elf32_auxv_t))) {
            if (aux.a_type == AT_HWCAP) {
                close(fd);
                return aux.a_un.a_val & HWCAP_VFP;
            }
        }
        close(fd);
    }
#endif // OS(LINUX)

#if (COMPILER(GCC) && defined(__VFP_FP__))
    return true;
#else
    return false;
#endif
}

const bool MacroAssemblerARM::s_isVFPPresent = isVFPPresent();

#if CPU(ARMV5_OR_LOWER)
/* On ARMv5 and below, natural alignment is required. */
void MacroAssemblerARM::load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
{
    ARMWord op2;

    ASSERT(address.scale >= 0 && address.scale <= 3);
    op2 = m_assembler.lsl(address.index, static_cast<int>(address.scale));

    if (address.offset >= 0 && address.offset + 0x2 <= 0xff) {
        m_assembler.add(ARMRegisters::S0, address.base, op2);
        m_assembler.halfDtrUp(ARMAssembler::LoadUint16, dest, ARMRegisters::S0, ARMAssembler::getOp2Half(address.offset));
        m_assembler.halfDtrUp(ARMAssembler::LoadUint16, ARMRegisters::S0, ARMRegisters::S0, ARMAssembler::getOp2Half(address.offset + 0x2));
    } else if (address.offset < 0 && address.offset >= -0xff) {
        m_assembler.add(ARMRegisters::S0, address.base, op2);
        m_assembler.halfDtrDown(ARMAssembler::LoadUint16, dest, ARMRegisters::S0, ARMAssembler::getOp2Half(-address.offset));
        m_assembler.halfDtrDown(ARMAssembler::LoadUint16, ARMRegisters::S0, ARMRegisters::S0, ARMAssembler::getOp2Half(-address.offset - 0x2));
    } else {
        m_assembler.moveImm(address.offset, ARMRegisters::S0);
        m_assembler.add(ARMRegisters::S0, ARMRegisters::S0, op2);
        m_assembler.halfDtrUpRegister(ARMAssembler::LoadUint16, dest, address.base, ARMRegisters::S0);
        m_assembler.add(ARMRegisters::S0, ARMRegisters::S0, ARMAssembler::Op2Immediate | 0x2);
        m_assembler.halfDtrUpRegister(ARMAssembler::LoadUint16, ARMRegisters::S0, address.base, ARMRegisters::S0);
    }
    m_assembler.orr(dest, dest, m_assembler.lsl(ARMRegisters::S0, 16));
}
#endif // CPU(ARMV5_OR_LOWER)

#if ENABLE(MASM_PROBE)

#define INDENT printIndent(indentation)

void MacroAssemblerARM::printCPURegisters(CPUState& cpu, int indentation)
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

void MacroAssemblerARM::printRegister(MacroAssemblerARM::CPUState& cpu, RegisterID regID)
{
    const char* name = CPUState::registerName(regID);
    union {
        void* voidPtr;
        intptr_t intptrValue;
    } u;
    u.voidPtr = cpu.registerValue(regID);
    dataLogF("%s:<%p %ld>", name, u.voidPtr, u.intptrValue);
}

void MacroAssemblerARM::printRegister(MacroAssemblerARM::CPUState& cpu, FPRegisterID regID)
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

void MacroAssemblerARM::probe(MacroAssemblerARM::ProbeFunction function, void* arg1, void* arg2)
{
    push(RegisterID::sp);
    push(RegisterID::lr);
    push(RegisterID::ip);
    push(RegisterID::S0);
    // The following uses RegisterID::S0. So, they must come after we push S0 above.
    push(trustedImm32FromPtr(arg2));
    push(trustedImm32FromPtr(arg1));
    push(trustedImm32FromPtr(function));

    move(trustedImm32FromPtr(ctiMasmProbeTrampoline), RegisterID::S0);
    m_assembler.blx(RegisterID::S0);

}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)
