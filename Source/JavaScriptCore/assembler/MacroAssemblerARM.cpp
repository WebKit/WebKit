/*
 * Copyright (C) 2013-2017 Apple Inc.
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
#include "MacroAssembler.h"

#include <wtf/InlineASM.h>

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

#if (COMPILER(GCC_OR_CLANG) && defined(__VFP_FP__))
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

extern "C" void ctiMasmProbeTrampoline();

#if COMPILER(GCC_OR_CLANG)

// The following are offsets for Probe::State fields accessed
// by the ctiMasmProbeTrampoline stub.

#define PTR_SIZE 4
#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INIT_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INIT_STACK_ARG_OFFSET (3 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET (4 * PTR_SIZE)

#define GPREG_SIZE 4
#define PROBE_CPU_R0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (0 * GPREG_SIZE))
#define PROBE_CPU_R1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (1 * GPREG_SIZE))
#define PROBE_CPU_R2_OFFSET (PROBE_FIRST_GPREG_OFFSET + (2 * GPREG_SIZE))
#define PROBE_CPU_R3_OFFSET (PROBE_FIRST_GPREG_OFFSET + (3 * GPREG_SIZE))
#define PROBE_CPU_R4_OFFSET (PROBE_FIRST_GPREG_OFFSET + (4 * GPREG_SIZE))
#define PROBE_CPU_R5_OFFSET (PROBE_FIRST_GPREG_OFFSET + (5 * GPREG_SIZE))
#define PROBE_CPU_R6_OFFSET (PROBE_FIRST_GPREG_OFFSET + (6 * GPREG_SIZE))
#define PROBE_CPU_R7_OFFSET (PROBE_FIRST_GPREG_OFFSET + (7 * GPREG_SIZE))
#define PROBE_CPU_R8_OFFSET (PROBE_FIRST_GPREG_OFFSET + (8 * GPREG_SIZE))
#define PROBE_CPU_R9_OFFSET (PROBE_FIRST_GPREG_OFFSET + (9 * GPREG_SIZE))
#define PROBE_CPU_R10_OFFSET (PROBE_FIRST_GPREG_OFFSET + (10 * GPREG_SIZE))
#define PROBE_CPU_R11_OFFSET (PROBE_FIRST_GPREG_OFFSET + (11 * GPREG_SIZE))
#define PROBE_CPU_IP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (12 * GPREG_SIZE))
#define PROBE_CPU_SP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (13 * GPREG_SIZE))
#define PROBE_CPU_LR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (14 * GPREG_SIZE))
#define PROBE_CPU_PC_OFFSET (PROBE_FIRST_GPREG_OFFSET + (15 * GPREG_SIZE))

#define PROBE_CPU_APSR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (16 * GPREG_SIZE))
#define PROBE_CPU_FPSCR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (17 * GPREG_SIZE))

#define PROBE_FIRST_FPREG_OFFSET (PROBE_FIRST_GPREG_OFFSET + (18 * GPREG_SIZE))

#define FPREG_SIZE 8
#define PROBE_CPU_D0_OFFSET (PROBE_FIRST_FPREG_OFFSET + (0 * FPREG_SIZE))
#define PROBE_CPU_D1_OFFSET (PROBE_FIRST_FPREG_OFFSET + (1 * FPREG_SIZE))
#define PROBE_CPU_D2_OFFSET (PROBE_FIRST_FPREG_OFFSET + (2 * FPREG_SIZE))
#define PROBE_CPU_D3_OFFSET (PROBE_FIRST_FPREG_OFFSET + (3 * FPREG_SIZE))
#define PROBE_CPU_D4_OFFSET (PROBE_FIRST_FPREG_OFFSET + (4 * FPREG_SIZE))
#define PROBE_CPU_D5_OFFSET (PROBE_FIRST_FPREG_OFFSET + (5 * FPREG_SIZE))
#define PROBE_CPU_D6_OFFSET (PROBE_FIRST_FPREG_OFFSET + (6 * FPREG_SIZE))
#define PROBE_CPU_D7_OFFSET (PROBE_FIRST_FPREG_OFFSET + (7 * FPREG_SIZE))
#define PROBE_CPU_D8_OFFSET (PROBE_FIRST_FPREG_OFFSET + (8 * FPREG_SIZE))
#define PROBE_CPU_D9_OFFSET (PROBE_FIRST_FPREG_OFFSET + (9 * FPREG_SIZE))
#define PROBE_CPU_D10_OFFSET (PROBE_FIRST_FPREG_OFFSET + (10 * FPREG_SIZE))
#define PROBE_CPU_D11_OFFSET (PROBE_FIRST_FPREG_OFFSET + (11 * FPREG_SIZE))
#define PROBE_CPU_D12_OFFSET (PROBE_FIRST_FPREG_OFFSET + (12 * FPREG_SIZE))
#define PROBE_CPU_D13_OFFSET (PROBE_FIRST_FPREG_OFFSET + (13 * FPREG_SIZE))
#define PROBE_CPU_D14_OFFSET (PROBE_FIRST_FPREG_OFFSET + (14 * FPREG_SIZE))
#define PROBE_CPU_D15_OFFSET (PROBE_FIRST_FPREG_OFFSET + (15 * FPREG_SIZE))

#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
#define PROBE_CPU_D16_OFFSET (PROBE_FIRST_FPREG_OFFSET + (16 * FPREG_SIZE))
#define PROBE_CPU_D17_OFFSET (PROBE_FIRST_FPREG_OFFSET + (17 * FPREG_SIZE))
#define PROBE_CPU_D18_OFFSET (PROBE_FIRST_FPREG_OFFSET + (18 * FPREG_SIZE))
#define PROBE_CPU_D19_OFFSET (PROBE_FIRST_FPREG_OFFSET + (19 * FPREG_SIZE))
#define PROBE_CPU_D20_OFFSET (PROBE_FIRST_FPREG_OFFSET + (20 * FPREG_SIZE))
#define PROBE_CPU_D21_OFFSET (PROBE_FIRST_FPREG_OFFSET + (21 * FPREG_SIZE))
#define PROBE_CPU_D22_OFFSET (PROBE_FIRST_FPREG_OFFSET + (22 * FPREG_SIZE))
#define PROBE_CPU_D23_OFFSET (PROBE_FIRST_FPREG_OFFSET + (23 * FPREG_SIZE))
#define PROBE_CPU_D24_OFFSET (PROBE_FIRST_FPREG_OFFSET + (24 * FPREG_SIZE))
#define PROBE_CPU_D25_OFFSET (PROBE_FIRST_FPREG_OFFSET + (25 * FPREG_SIZE))
#define PROBE_CPU_D26_OFFSET (PROBE_FIRST_FPREG_OFFSET + (26 * FPREG_SIZE))
#define PROBE_CPU_D27_OFFSET (PROBE_FIRST_FPREG_OFFSET + (27 * FPREG_SIZE))
#define PROBE_CPU_D28_OFFSET (PROBE_FIRST_FPREG_OFFSET + (28 * FPREG_SIZE))
#define PROBE_CPU_D29_OFFSET (PROBE_FIRST_FPREG_OFFSET + (29 * FPREG_SIZE))
#define PROBE_CPU_D30_OFFSET (PROBE_FIRST_FPREG_OFFSET + (30 * FPREG_SIZE))
#define PROBE_CPU_D31_OFFSET (PROBE_FIRST_FPREG_OFFSET + (31 * FPREG_SIZE))

#define PROBE_SIZE (PROBE_FIRST_FPREG_OFFSET + (32 * FPREG_SIZE))
#else
#define PROBE_SIZE (PROBE_FIRST_FPREG_OFFSET + (16 * FPREG_SIZE))
#endif // CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)

#define OUT_SIZE GPREG_SIZE

// These ASSERTs remind you that if you change the layout of Probe::State,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct Probe::State, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeState_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, ProbeState_arg_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INIT_STACK_FUNCTION_OFFSET, ProbeState_initializeStackFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(initializeStackArg) == PROBE_INIT_STACK_ARG_OFFSET, ProbeState_initializeStackArg_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_R0_OFFSET & 0x3), ProbeState_cpu_r0_offset_should_be_4_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r0]) == PROBE_CPU_R0_OFFSET, ProbeState_cpu_r0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r1]) == PROBE_CPU_R1_OFFSET, ProbeState_cpu_r1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r2]) == PROBE_CPU_R2_OFFSET, ProbeState_cpu_r2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r3]) == PROBE_CPU_R3_OFFSET, ProbeState_cpu_r3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r4]) == PROBE_CPU_R4_OFFSET, ProbeState_cpu_r4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r5]) == PROBE_CPU_R5_OFFSET, ProbeState_cpu_r5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r6]) == PROBE_CPU_R6_OFFSET, ProbeState_cpu_r6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r7]) == PROBE_CPU_R7_OFFSET, ProbeState_cpu_r7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r8]) == PROBE_CPU_R8_OFFSET, ProbeState_cpu_r8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r9]) == PROBE_CPU_R9_OFFSET, ProbeState_cpu_r9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r10]) == PROBE_CPU_R10_OFFSET, ProbeState_cpu_r10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r11]) == PROBE_CPU_R11_OFFSET, ProbeState_cpu_r11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::ip]) == PROBE_CPU_IP_OFFSET, ProbeState_cpu_ip_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::sp]) == PROBE_CPU_SP_OFFSET, ProbeState_cpu_sp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::lr]) == PROBE_CPU_LR_OFFSET, ProbeState_cpu_lr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::pc]) == PROBE_CPU_PC_OFFSET, ProbeState_cpu_pc_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::apsr]) == PROBE_CPU_APSR_OFFSET, ProbeState_cpu_apsr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::fpscr]) == PROBE_CPU_FPSCR_OFFSET, ProbeState_cpu_fpscr_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_D0_OFFSET & 0x7), ProbeState_cpu_d0_offset_should_be_8_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d0]) == PROBE_CPU_D0_OFFSET, ProbeState_cpu_d0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d1]) == PROBE_CPU_D1_OFFSET, ProbeState_cpu_d1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d2]) == PROBE_CPU_D2_OFFSET, ProbeState_cpu_d2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d3]) == PROBE_CPU_D3_OFFSET, ProbeState_cpu_d3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d4]) == PROBE_CPU_D4_OFFSET, ProbeState_cpu_d4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d5]) == PROBE_CPU_D5_OFFSET, ProbeState_cpu_d5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d6]) == PROBE_CPU_D6_OFFSET, ProbeState_cpu_d6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d7]) == PROBE_CPU_D7_OFFSET, ProbeState_cpu_d7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d8]) == PROBE_CPU_D8_OFFSET, ProbeState_cpu_d8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d9]) == PROBE_CPU_D9_OFFSET, ProbeState_cpu_d9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d10]) == PROBE_CPU_D10_OFFSET, ProbeState_cpu_d10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d11]) == PROBE_CPU_D11_OFFSET, ProbeState_cpu_d11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d12]) == PROBE_CPU_D12_OFFSET, ProbeState_cpu_d12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d13]) == PROBE_CPU_D13_OFFSET, ProbeState_cpu_d13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d14]) == PROBE_CPU_D14_OFFSET, ProbeState_cpu_d14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d15]) == PROBE_CPU_D15_OFFSET, ProbeState_cpu_d15_offset_matches_ctiMasmProbeTrampoline);

#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d16]) == PROBE_CPU_D16_OFFSET, ProbeState_cpu_d16_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d17]) == PROBE_CPU_D17_OFFSET, ProbeState_cpu_d17_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d18]) == PROBE_CPU_D18_OFFSET, ProbeState_cpu_d18_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d19]) == PROBE_CPU_D19_OFFSET, ProbeState_cpu_d19_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d20]) == PROBE_CPU_D20_OFFSET, ProbeState_cpu_d20_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d21]) == PROBE_CPU_D21_OFFSET, ProbeState_cpu_d21_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d22]) == PROBE_CPU_D22_OFFSET, ProbeState_cpu_d22_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d23]) == PROBE_CPU_D23_OFFSET, ProbeState_cpu_d23_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d24]) == PROBE_CPU_D24_OFFSET, ProbeState_cpu_d24_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d25]) == PROBE_CPU_D25_OFFSET, ProbeState_cpu_d25_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d26]) == PROBE_CPU_D26_OFFSET, ProbeState_cpu_d26_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d27]) == PROBE_CPU_D27_OFFSET, ProbeState_cpu_d27_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d28]) == PROBE_CPU_D28_OFFSET, ProbeState_cpu_d28_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d29]) == PROBE_CPU_D29_OFFSET, ProbeState_cpu_d29_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d30]) == PROBE_CPU_D30_OFFSET, ProbeState_cpu_d30_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d31]) == PROBE_CPU_D31_OFFSET, ProbeState_cpu_d31_offset_matches_ctiMasmProbeTrampoline);
#endif // CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)

COMPILE_ASSERT(sizeof(Probe::State) == PROBE_SIZE, ProbeState_size_matches_ctiMasmProbeTrampoline);
#undef PROBE_OFFSETOF

asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    INLINE_ARM_FUNCTION(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssemblerARM::probe() has already generated code to store some values.
    // The top of stack now looks like this:
    //     esp[0 * ptrSize]: probe handler function
    //     esp[1 * ptrSize]: probe arg
    //     esp[2 * ptrSize]: saved r3 / S0
    //     esp[3 * ptrSize]: saved ip
    //     esp[4 * ptrSize]: saved lr
    //     esp[5 * ptrSize]: saved sp

    "mov       ip, sp" "\n"
    "mov       r3, sp" "\n"
    "sub       r3, r3, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n"

    // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "bic       r3, r3, #0xf" "\n"
    "mov       sp, r3" "\n" // Set the sp to protect the Probe::State from interrupts before we initialize it.

    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "add       lr, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "\n"
    "stmia     lr, { r0-r11 }" "\n"
    "mrs       lr, APSR" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "vmrs      lr, FPSCR" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSCR_OFFSET) "]" "\n"

    "ldr       lr, [ip, #0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "ldr       lr, [ip, #1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "]" "\n"
    "ldr       lr, [ip, #2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R3_OFFSET) "]" "\n"
    "ldr       lr, [ip, #3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "ldr       lr, [ip, #4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       lr, [ip, #5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D0_OFFSET) "\n"
    "vstmia.64 ip!, { d0-d15 }" "\n"
#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
    "vstmia.64 ip!, { d16-d31 }" "\n"
#endif
    "mov       fp, sp" "\n" // Save the Probe::State*.

    // Initialize Probe::State::initializeStackFunction to zero.
    "mov       r0, #0" "\n"
    "str       r0, [fp, #" STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "]" "\n"

    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "mov       r0, sp" "\n" // the Probe::State* arg.
    "blx       ip" "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "ldr       r1, [fp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // Result sp.
    "add       r2, fp, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n" // End of ProveContext + buffer.
    "cmp       r1, r2" "\n"
    "bge     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "sub       r1, r1, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n"
    "bic       r1, r1, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "mov       sp, r1" "\n" // Set the new sp to protect that memory from interrupts before we copy the Probe::State.

    // Copy the Probe::State to the safe place.
    // Note: we have to copy from low address to higher address because we're moving the
    // Probe::State to a lower address.
    "mov       r5, fp" "\n"
    "mov       r6, r1" "\n"
    "add       r7, fp, #" STRINGIZE_VALUE_OF(PROBE_SIZE) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "ldr       r3, [r5], #4" "\n"
    "ldr       r4, [r5], #4" "\n"
    "str       r3, [r6], #4" "\n"
    "str       r4, [r6], #4" "\n"
    "cmp       r5, r7" "\n"
    "blt     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "mov       fp, r1" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "ldr       r2, [fp, #" STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "]" "\n"
    "cmp       r2, #0" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"

    "mov       r0, fp" "\n" // Set the Probe::State* arg.
    "blx       r2" "\n" // Call the initializeStackFunction (loaded into r2 above).

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    "mov       sp, fp" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning.

#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D31_OFFSET + FPREG_SIZE) "\n"
    "vldmdb.64 ip!, { d16-d31 }" "\n"
    "vldmdb.64 ip!, { d0-d15 }" "\n"
#else
    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D15_OFFSET + FPREG_SIZE) "\n"
    "vldmdb.64 ip!, { d0-d15 }" "\n"
#endif

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET + GPREG_SIZE) "\n"
    "ldmdb     ip, { r0-r11 }" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSCR_OFFSET) "]" "\n"
    "vmsr      FPSCR, ip" "\n"

    // There are 5 more registers left to restore: ip, sp, lr, pc, and apsr.

    // Set up the restore area for sp and pc.
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    // Push the pc on to the restore area.
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "sub       lr, lr, #" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"
    "str       ip, [lr]" "\n"
    // Point sp to the restore area.
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    // All done with math i.e. won't trash the status register (apsr) and don't need
    // scratch registers (lr and ip) anymore. Restore apsr, lr, and ip.
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "msr       APSR, ip" "\n"
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"

    // Restore the sp and pc.
    "ldr       sp, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "pop       { pc }" "\n"
);
#endif // COMPILER(GCC_OR_CLANG)

void MacroAssembler::probe(Probe::Function function, void* arg)
{
    push(RegisterID::sp);
    push(RegisterID::lr);
    push(RegisterID::ip);
    push(RegisterID::S0);
    // The following uses RegisterID::S0. So, they must come after we push S0 above.
    push(trustedImm32FromPtr(arg));
    push(trustedImm32FromPtr(function));

    move(trustedImm32FromPtr(ctiMasmProbeTrampoline), RegisterID::S0);
    m_assembler.blx(RegisterID::S0);

}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)
