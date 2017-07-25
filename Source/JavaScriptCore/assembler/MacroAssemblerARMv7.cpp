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

#if ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)
#include "MacroAssembler.h"

#include <wtf/InlineASM.h>

namespace JSC {

#if ENABLE(MASM_PROBE)

extern "C" void ctiMasmProbeTrampoline();

#if COMPILER(GCC_OR_CLANG)

// The following are offsets for ProbeContext fields accessed
// by the ctiMasmProbeTrampoline stub.

#define PTR_SIZE 4
#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET (2 * PTR_SIZE)

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
#define PROBE_ALIGNED_SIZE (PROBE_SIZE)

// These ASSERTs remind you that if you change the layout of ProbeContext,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct ProbeContext, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeContext_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, ProbeContext_arg_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_R0_OFFSET & 0x3), ProbeContext_cpu_r0_offset_should_be_4_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r0]) == PROBE_CPU_R0_OFFSET, ProbeContext_cpu_r0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r1]) == PROBE_CPU_R1_OFFSET, ProbeContext_cpu_r1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r2]) == PROBE_CPU_R2_OFFSET, ProbeContext_cpu_r2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r3]) == PROBE_CPU_R3_OFFSET, ProbeContext_cpu_r3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r4]) == PROBE_CPU_R4_OFFSET, ProbeContext_cpu_r4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r5]) == PROBE_CPU_R5_OFFSET, ProbeContext_cpu_r5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r6]) == PROBE_CPU_R6_OFFSET, ProbeContext_cpu_r6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r7]) == PROBE_CPU_R7_OFFSET, ProbeContext_cpu_r7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r8]) == PROBE_CPU_R8_OFFSET, ProbeContext_cpu_r8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r9]) == PROBE_CPU_R9_OFFSET, ProbeContext_cpu_r9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r10]) == PROBE_CPU_R10_OFFSET, ProbeContext_cpu_r10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r11]) == PROBE_CPU_R11_OFFSET, ProbeContext_cpu_r11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::ip]) == PROBE_CPU_IP_OFFSET, ProbeContext_cpu_ip_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::sp]) == PROBE_CPU_SP_OFFSET, ProbeContext_cpu_sp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::lr]) == PROBE_CPU_LR_OFFSET, ProbeContext_cpu_lr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::pc]) == PROBE_CPU_PC_OFFSET, ProbeContext_cpu_pc_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::apsr]) == PROBE_CPU_APSR_OFFSET, ProbeContext_cpu_apsr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::fpscr]) == PROBE_CPU_FPSCR_OFFSET, ProbeContext_cpu_fpscr_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_D0_OFFSET & 0xf), ProbeContext_cpu_d0_offset_should_be_16_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d0]) == PROBE_CPU_D0_OFFSET, ProbeContext_cpu_d0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d1]) == PROBE_CPU_D1_OFFSET, ProbeContext_cpu_d1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d2]) == PROBE_CPU_D2_OFFSET, ProbeContext_cpu_d2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d3]) == PROBE_CPU_D3_OFFSET, ProbeContext_cpu_d3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d4]) == PROBE_CPU_D4_OFFSET, ProbeContext_cpu_d4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d5]) == PROBE_CPU_D5_OFFSET, ProbeContext_cpu_d5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d6]) == PROBE_CPU_D6_OFFSET, ProbeContext_cpu_d6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d7]) == PROBE_CPU_D7_OFFSET, ProbeContext_cpu_d7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d8]) == PROBE_CPU_D8_OFFSET, ProbeContext_cpu_d8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d9]) == PROBE_CPU_D9_OFFSET, ProbeContext_cpu_d9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d10]) == PROBE_CPU_D10_OFFSET, ProbeContext_cpu_d10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d11]) == PROBE_CPU_D11_OFFSET, ProbeContext_cpu_d11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d12]) == PROBE_CPU_D12_OFFSET, ProbeContext_cpu_d12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d13]) == PROBE_CPU_D13_OFFSET, ProbeContext_cpu_d13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d14]) == PROBE_CPU_D14_OFFSET, ProbeContext_cpu_d14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d15]) == PROBE_CPU_D15_OFFSET, ProbeContext_cpu_d15_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d16]) == PROBE_CPU_D16_OFFSET, ProbeContext_cpu_d16_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d17]) == PROBE_CPU_D17_OFFSET, ProbeContext_cpu_d17_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d18]) == PROBE_CPU_D18_OFFSET, ProbeContext_cpu_d18_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d19]) == PROBE_CPU_D19_OFFSET, ProbeContext_cpu_d19_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d20]) == PROBE_CPU_D20_OFFSET, ProbeContext_cpu_d20_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d21]) == PROBE_CPU_D21_OFFSET, ProbeContext_cpu_d21_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d22]) == PROBE_CPU_D22_OFFSET, ProbeContext_cpu_d22_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d23]) == PROBE_CPU_D23_OFFSET, ProbeContext_cpu_d23_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d24]) == PROBE_CPU_D24_OFFSET, ProbeContext_cpu_d24_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d25]) == PROBE_CPU_D25_OFFSET, ProbeContext_cpu_d25_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d26]) == PROBE_CPU_D26_OFFSET, ProbeContext_cpu_d26_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d27]) == PROBE_CPU_D27_OFFSET, ProbeContext_cpu_d27_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d28]) == PROBE_CPU_D28_OFFSET, ProbeContext_cpu_d28_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d29]) == PROBE_CPU_D29_OFFSET, ProbeContext_cpu_d29_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d30]) == PROBE_CPU_D30_OFFSET, ProbeContext_cpu_d30_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARMRegisters::d31]) == PROBE_CPU_D31_OFFSET, ProbeContext_cpu_d31_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(sizeof(ProbeContext) == PROBE_SIZE, ProbeContext_size_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(!(PROBE_ALIGNED_SIZE & 0xf), ProbeContext_aligned_size_offset_should_be_16_byte_aligned);

#undef PROBE_OFFSETOF
    
asm (
    ".text" "\n"
    ".align 2" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    ".thumb" "\n"
    ".thumb_func " THUMB_FUNC_PARAM(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssemblerARMv7::probe() has already generated code to store some values.
    // The top of stack now looks like this:
    //     esp[0 * ptrSize]: probe handler function
    //     esp[1 * ptrSize]: probe arg
    //     esp[2 * ptrSize]: saved r0
    //     esp[3 * ptrSize]: saved ip
    //     esp[4 * ptrSize]: saved lr
    //     esp[5 * ptrSize]: saved sp

    "mov       ip, sp" "\n"
    "mov       r0, sp" "\n"
    "sub       r0, r0, #" STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) "\n"

    // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "bic       r0, r0, #0xf" "\n"
    "mov       sp, r0" "\n"

    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "add       lr, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R1_OFFSET) "\n"
    "stmia     lr, { r1-r11 }" "\n"
    "mrs       lr, APSR" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "vmrs      lr, FPSCR" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSCR_OFFSET) "]" "\n"

    "ldr       lr, [ip, #0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "ldr       lr, [ip, #1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "]" "\n"
    "ldr       lr, [ip, #2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "]" "\n"
    "ldr       lr, [ip, #3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "ldr       lr, [ip, #4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       lr, [ip, #5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D0_OFFSET) "\n"
    "vstmia.64 ip!, { d0-d15 }" "\n"
    "vstmia.64 ip!, { d16-d31 }" "\n"

    "mov       fp, sp" "\n" // Save the ProbeContext*.

    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "mov       r0, sp" "\n" // the ProbeContext* arg.
    "blx       ip" "\n"

    "mov       sp, fp" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the ProbeContext before returning.

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D31_OFFSET + FPREG_SIZE) "\n"
    "vldmdb.64 ip!, { d16-d31 }" "\n"
    "vldmdb.64 ip!, { d0-d15 }" "\n"

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET + GPREG_SIZE) "\n"
    "ldmdb     ip, { r0-r11 }" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSCR_OFFSET) "]" "\n"
    "vmsr      FPSCR, ip" "\n"

    // There are 5 more registers left to restore: ip, sp, lr, pc, and apsr.
    // There are 2 issues that complicate the restoration of these last few
    // registers:
    //
    // 1. Normal ARM calling convention relies on moving lr to pc to return to
    //    the caller. In our case, the address to return to is specified by
    //    ProbeContext.cpu.gprs[pc]. And at that moment, we won't have any available
    //    scratch registers to hold the return address (lr needs to hold
    //    ProbeContext.cpu.gprs[lr], not the return address).
    //
    //    The solution is to store the return address on the stack and load the
    //    pc from there.
    //
    // 2. Issue 1 means we will need to write to the stack location at
    //    ProbeContext.cpu.gprs[sp] - PTR_SIZE. But if the user probe function had
    //    modified the value of ProbeContext.cpu.gprs[sp] to point in the range between
    //    &ProbeContext.cpu.gprs[ip] thru &ProbeContext.cpu.sprs[aspr], then the action
    //    for Issue 1 may trash the values to be restored before we can restore them.
    //
    //    The solution is to check if ProbeContext.cpu.gprs[sp] contains a value in
    //    the undesirable range. If so, we copy the remaining ProbeContext
    //    register data to a safe area first, and restore the remaining register
    //    from this new safe area.

    // The restore area for the pc will be located at 1 word below the resultant sp.
    // All restore values are located at offset <= PROBE_CPU_APSR_OFFSET. Hence,
    // we need to make sure that resultant sp > offset of apsr + 1.
    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET + PTR_SIZE) "\n"
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "cmp       lr, ip" "\n"
    "it        gt" "\n"
    "bgt     " SYMBOL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    // Getting here means that the restore area will overlap the ProbeContext data
    // that we will need to get the restoration values from. So, let's move that
    // data to a safe place before we start writing into the restore area.
    // Let's locate the "safe area" at 2x sizeof(ProbeContext) below where the
    // restore area. This ensures that:
    // 1. The safe area does not overlap the restore area.
    // 2. The safe area does not overlap the ProbeContext.
    //    This makes it so that we can use memcpy (does not require memmove) semantics
    //    to copy the restore values to the safe area.

    // lr already contains [sp, #STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET)].
    "sub       lr, lr, #(2 * " STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) ")" "\n"
    
    "mov       ip, sp" "\n" // Save the original ProbeContext*.
    
    // Make sure the stack pointer points to the safe area. This ensures that the
    // safe area is protected from interrupt handlers overwriting it.
    "mov       sp, lr" "\n" // sp now points to the new ProbeContext in the safe area.
    
    "mov       lr, ip" "\n" // Use lr as the old ProbeContext*.
    
    // Copy the restore data to the new ProbeContext*.
    "ldr       ip, [lr, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "str       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "ldr       ip, [lr, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "str       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "ldr       ip, [lr, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "str       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       ip, [lr, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "str       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "ldr       ip, [lr, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "str       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"

    // ctiMasmProbeTrampolineEnd expects lr to contain the sp value to be restored.
    // Since we used it as scratch above, let's restore it.
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    ".thumb_func " THUMB_FUNC_PARAM(ctiMasmProbeTrampolineEnd) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Set up the restore area for sp and pc.
    // lr already contains [sp, #STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET)].

    // Push the pc on to the restore area.
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "sub       lr, lr, #" STRINGIZE_VALUE_OF(PTR_SIZE) "\n"
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

void MacroAssembler::probe(ProbeFunction function, void* arg)
{
    push(RegisterID::lr);
    push(RegisterID::lr);
    add32(TrustedImm32(8), RegisterID::sp, RegisterID::lr);
    store32(RegisterID::lr, ArmAddress(RegisterID::sp, 4));
    push(RegisterID::ip);
    push(RegisterID::r0);
    // The following uses RegisterID::ip. So, they must come after we push ip above.
    push(trustedImm32FromPtr(arg));
    push(trustedImm32FromPtr(function));

    move(trustedImm32FromPtr(ctiMasmProbeTrampoline), RegisterID::ip);
    m_assembler.blx(RegisterID::ip);
}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

