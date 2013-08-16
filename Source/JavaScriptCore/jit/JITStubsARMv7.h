/*
 * Copyright (C) 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 * Copyright (C) Research In Motion Limited 2010, 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JITStubsARMv7_h
#define JITStubsARMv7_h

#if !CPU(ARM_THUMB2)
#error "JITStubsARMv7.h should only be #included if CPU(ARM_THUMB2)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsARMv7.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#define THUNK_RETURN_ADDRESS_OFFSET      0x38
#define PRESERVED_RETURN_ADDRESS_OFFSET  0x3C
#define PRESERVED_R4_OFFSET              0x40
#define PRESERVED_R5_OFFSET              0x44
#define PRESERVED_R6_OFFSET              0x48
#define PRESERVED_R7_OFFSET              0x4C
#define PRESERVED_R8_OFFSET              0x50
#define PRESERVED_R9_OFFSET              0x54
#define PRESERVED_R10_OFFSET             0x58
#define PRESERVED_R11_OFFSET             0x5C
#define REGISTER_FILE_OFFSET             0x60
#define FIRST_STACK_ARGUMENT             0x68

#if COMPILER(GCC)

#if USE(MASM_PROBE)
// The following are offsets for MacroAssembler::ProbeContext fields accessed
// by the ctiMasmProbeTrampoline stub.

#define PTR_SIZE 4
#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG1_OFFSET (1 * PTR_SIZE)
#define PROBE_ARG2_OFFSET (2 * PTR_SIZE)
#define PROBE_JIT_STACK_FRAME_OFFSET (3 * PTR_SIZE)

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

#if CPU(APPLE_ARMV7S)
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
#endif // CPU(APPLE_ARMV7S)


// These ASSERTs remind you that if you change the layout of ProbeContext,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct MacroAssembler::ProbeContext, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeContext_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg1) == PROBE_ARG1_OFFSET, ProbeContext_arg1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg2) == PROBE_ARG2_OFFSET, ProbeContext_arg2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(jitStackFrame) == PROBE_JIT_STACK_FRAME_OFFSET, ProbeContext_jitStackFrame_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r0) == PROBE_CPU_R0_OFFSET, ProbeContext_cpu_r0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r1) == PROBE_CPU_R1_OFFSET, ProbeContext_cpu_r1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r2) == PROBE_CPU_R2_OFFSET, ProbeContext_cpu_r2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r3) == PROBE_CPU_R3_OFFSET, ProbeContext_cpu_r3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r4) == PROBE_CPU_R4_OFFSET, ProbeContext_cpu_r4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r5) == PROBE_CPU_R5_OFFSET, ProbeContext_cpu_r5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r6) == PROBE_CPU_R6_OFFSET, ProbeContext_cpu_r6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r7) == PROBE_CPU_R7_OFFSET, ProbeContext_cpu_r70_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r8) == PROBE_CPU_R8_OFFSET, ProbeContext_cpu_r8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r9) == PROBE_CPU_R9_OFFSET, ProbeContext_cpu_r9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r10) == PROBE_CPU_R10_OFFSET, ProbeContext_cpu_r10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r11) == PROBE_CPU_R11_OFFSET, ProbeContext_cpu_r11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.ip) == PROBE_CPU_IP_OFFSET, ProbeContext_cpu_ip_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sp) == PROBE_CPU_SP_OFFSET, ProbeContext_cpu_sp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.lr) == PROBE_CPU_LR_OFFSET, ProbeContext_cpu_lr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.pc) == PROBE_CPU_PC_OFFSET, ProbeContext_cpu_pc_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.apsr) == PROBE_CPU_APSR_OFFSET, ProbeContext_cpu_apsr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fpscr) == PROBE_CPU_FPSCR_OFFSET, ProbeContext_cpu_fpscr_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d0) == PROBE_CPU_D0_OFFSET, ProbeContext_cpu_d0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d1) == PROBE_CPU_D1_OFFSET, ProbeContext_cpu_d1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d2) == PROBE_CPU_D2_OFFSET, ProbeContext_cpu_d2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d3) == PROBE_CPU_D3_OFFSET, ProbeContext_cpu_d3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d4) == PROBE_CPU_D4_OFFSET, ProbeContext_cpu_d4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d5) == PROBE_CPU_D5_OFFSET, ProbeContext_cpu_d5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d6) == PROBE_CPU_D6_OFFSET, ProbeContext_cpu_d6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d7) == PROBE_CPU_D7_OFFSET, ProbeContext_cpu_d7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d8) == PROBE_CPU_D8_OFFSET, ProbeContext_cpu_d8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d9) == PROBE_CPU_D9_OFFSET, ProbeContext_cpu_d9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d10) == PROBE_CPU_D10_OFFSET, ProbeContext_cpu_d10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d11) == PROBE_CPU_D11_OFFSET, ProbeContext_cpu_d11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d12) == PROBE_CPU_D12_OFFSET, ProbeContext_cpu_d12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d13) == PROBE_CPU_D13_OFFSET, ProbeContext_cpu_d13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d14) == PROBE_CPU_D14_OFFSET, ProbeContext_cpu_d14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d15) == PROBE_CPU_D15_OFFSET, ProbeContext_cpu_d15_offset_matches_ctiMasmProbeTrampoline);

#if CPU(APPLE_ARMV7S)
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d16) == PROBE_CPU_D16_OFFSET, ProbeContext_cpu_d16_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d17) == PROBE_CPU_D17_OFFSET, ProbeContext_cpu_d17_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d18) == PROBE_CPU_D18_OFFSET, ProbeContext_cpu_d18_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d19) == PROBE_CPU_D19_OFFSET, ProbeContext_cpu_d19_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d20) == PROBE_CPU_D20_OFFSET, ProbeContext_cpu_d20_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d21) == PROBE_CPU_D21_OFFSET, ProbeContext_cpu_d21_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d22) == PROBE_CPU_D22_OFFSET, ProbeContext_cpu_d22_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d23) == PROBE_CPU_D23_OFFSET, ProbeContext_cpu_d23_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d24) == PROBE_CPU_D24_OFFSET, ProbeContext_cpu_d24_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d25) == PROBE_CPU_D25_OFFSET, ProbeContext_cpu_d25_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d26) == PROBE_CPU_D26_OFFSET, ProbeContext_cpu_d26_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d27) == PROBE_CPU_D27_OFFSET, ProbeContext_cpu_d27_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d28) == PROBE_CPU_D28_OFFSET, ProbeContext_cpu_d28_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d29) == PROBE_CPU_D29_OFFSET, ProbeContext_cpu_d29_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d30) == PROBE_CPU_D30_OFFSET, ProbeContext_cpu_d30_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.d31) == PROBE_CPU_D31_OFFSET, ProbeContext_cpu_d31_offset_matches_ctiMasmProbeTrampoline);
#endif // CPU(APPLE_ARMV7S)

COMPILE_ASSERT(sizeof(MacroAssembler::ProbeContext) == PROBE_SIZE, ProbeContext_size_matches_ctiMasmProbeTrampoline);

#undef PROBE_OFFSETOF

#endif // USE(MASM_PROBE)


asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "sub sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "str lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "str r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "str r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "str r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "str r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "str r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "str r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "str r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "str r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "str r1, [sp, #" STRINGIZE_VALUE_OF(REGISTER_FILE_OFFSET) "]" "\n"
    "mov r5, r2" "\n"
    "mov r6, #512" "\n"
    "blx r0" "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "mov r0, sp" "\n"
    "bl " LOCAL_REFERENCE(cti_vm_throw) "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiVMHandleException) "\n"
HIDE_SYMBOL(ctiVMHandleException) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiVMHandleException) "\n"
SYMBOL_STRING(ctiVMHandleException) ":" "\n"
    "mov r0, r5" "\n"
    "bl " LOCAL_REFERENCE(cti_vm_handle_exception) "\n"
    // When cti_vm_handle_exception returns, r0 has callFrame and r1 has handler address
    "mov r5, r0" "\n"
    "bx r1" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
);

#if USE(MASM_PROBE)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiMasmProbeTrampoline) "\n"
SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssembler::probe() has already generated code to store some values.
    // The top of stack now looks like this:
    //     esp[0 * ptrSize]: probeFunction
    //     esp[1 * ptrSize]: arg1
    //     esp[2 * ptrSize]: arg2
    //     esp[3 * ptrSize]: saved r0
    //     esp[4 * ptrSize]: saved ip
    //     esp[5 * ptrSize]: saved lr
    //     esp[6 * ptrSize]: saved sp

    "mov       ip, sp" "\n"
    "mov       r0, sp" "\n"
    "sub       r0, r0, #" STRINGIZE_VALUE_OF(PROBE_SIZE) "\n"

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
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_ARG1_OFFSET) "]" "\n"
    "ldr       lr, [ip, #2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_ARG2_OFFSET) "]" "\n"
    "ldr       lr, [ip, #3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "]" "\n"
    "ldr       lr, [ip, #4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "ldr       lr, [ip, #5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       lr, [ip, #6 * " STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_JIT_STACK_FRAME_OFFSET) "]" "\n"

    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D0_OFFSET) "\n"
#if CPU(APPLE_ARMV7S)
    "vstmia.64 ip, { d0-d31 }" "\n"
#else
    "vstmia.64 ip, { d0-d15 }" "\n"
#endif

    "mov       fp, sp" "\n" // Save the ProbeContext*.

    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "mov       r0, sp" "\n" // the ProbeContext* arg.
    "blx       ip" "\n"

    "mov       sp, fp" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the ProbeContext before returning.

#if CPU(APPLE_ARMV7S)
    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D31_OFFSET + FPREG_SIZE) "\n"
    "vldmdb.64 ip!, { d0-d31 }" "\n"
#else
    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D15_OFFSET + FPREG_SIZE) "\n"
    "vldmdb.64 ip!, { d0-d15 }" "\n"
#endif
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
    //    ProbeContext.cpu.pc. And at that moment, we won't have any available
    //    scratch registers to hold the return address (lr needs to hold
    //    ProbeContext.cpu.lr, not the return address).
    //
    //    The solution is to store the return address on the stack and load the
    //     pc from there.
    //
    // 2. Issue 1 means we will need to write to the stack location at
    //    ProbeContext.cpu.sp - 4. But if the user probe function had  modified
    //    the value of ProbeContext.cpu.sp to point in the range between
    //    &ProbeContext.cpu.ip thru &ProbeContext.cpu.aspr, then the action for
    //    Issue 1 may trash the values to be restored before we can restore
    //    them.
    //
    //    The solution is to check if ProbeContext.cpu.sp contains a value in
    //    the undesirable range. If so, we copy the remaining ProbeContext
    //    register data to a safe range (at memory lower than where
    //    ProbeContext.cpu.sp points) first, and restore the remaining register
    //    from this new range.

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "\n"
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "cmp       lr, ip" "\n"
    "it        gt" "\n"
    "bgt     " SYMBOL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    // We get here because the new expected stack pointer location is lower
    // than where it's supposed to be. This means the safe range of stack
    // memory where we'll be copying the remaining register restore values to
    // might be in a region of memory below the sp i.e. unallocated stack
    // memory. This, in turn, makes it vulnerable to interrupts potentially
    // trashing the copied values. To prevent that, we must first allocate the
    // needed stack memory by adjusting the sp before the copying.

    "sub       lr, lr, #(6 * " STRINGIZE_VALUE_OF(PTR_SIZE) 
                         " + " STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) ")" "\n"

    "mov       ip, sp" "\n"
    "mov       sp, lr" "\n"
    "mov       lr, ip" "\n"

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

".thumb_func " THUMB_FUNC_PARAM(ctiMasmProbeTrampolineEnd) "\n"
SYMBOL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "sub       lr, lr, #" STRINGIZE_VALUE_OF(PTR_SIZE) "\n"
    "str       ip, [lr]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "msr       APSR, ip" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "mov       lr, ip" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "ldr       sp, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    "pop       { pc }" "\n"
);
#endif // USE(MASM_PROBE)


#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".text" "\n" \
        ".align 2" "\n" \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        HIDE_SYMBOL(cti_##op) "\n"             \
        ".thumb" "\n" \
        ".thumb_func " THUMB_FUNC_PARAM(cti_##op) "\n" \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        "str lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bl " SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "ldr lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bx lr" "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION) \

#endif // COMPILER(GCC)

#if COMPILER(RVCT)

__asm EncodedJSValue ctiTrampoline(void*, JSStack*, CallFrame*, void* /*unused1*/, void* /*unused2*/, VM*)
{
    PRESERVE8
    sub sp, sp, # FIRST_STACK_ARGUMENT
    str lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    str r4, [sp, # PRESERVED_R4_OFFSET ]
    str r5, [sp, # PRESERVED_R5_OFFSET ]
    str r6, [sp, # PRESERVED_R6_OFFSET ]
    str r7, [sp, # PRESERVED_R7_OFFSET ]
    str r8, [sp, # PRESERVED_R8_OFFSET ]
    str r9, [sp, # PRESERVED_R9_OFFSET ]
    str r10, [sp, # PRESERVED_R10_OFFSET ]
    str r11, [sp, # PRESERVED_R11_OFFSET ]
    str r1, [sp, # REGISTER_FILE_OFFSET ]
    mov r5, r2
    mov r6, #512
    blx r0
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

__asm void ctiVMThrowTrampoline()
{
    PRESERVE8
    mov r0, sp
    bl cti_vm_throw
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

__asm void ctiOpThrowNotCaught()
{
    PRESERVE8
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

#define DEFINE_STUB_FUNCTION(rtype, op) rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

/* The following is a workaround for RVCT toolchain; precompiler macros are not expanded before the code is passed to the assembler */

/* The following section is a template to generate code for GeneratedJITStubs_RVCT.h */
/* The pattern "#xxx#" will be replaced with "xxx" */

/*
RVCT(extern "C" #rtype# JITStubThunked_#op#(STUB_ARGS_DECLARATION);)
RVCT(__asm #rtype# cti_#op#(STUB_ARGS_DECLARATION))
RVCT({)
RVCT(    PRESERVE8)
RVCT(    IMPORT JITStubThunked_#op#)
RVCT(    str lr, [sp, # THUNK_RETURN_ADDRESS_OFFSET])
RVCT(    bl JITStubThunked_#op#)
RVCT(    ldr lr, [sp, # THUNK_RETURN_ADDRESS_OFFSET])
RVCT(    bx lr)
RVCT(})
RVCT()
*/

/* Include the generated file */
#include "GeneratedJITStubs_RVCT.h"

#endif // COMPILER(RVCT)


static void performARMv7JITAssertions()
{
    // Unfortunate the arm compiler does not like the use of offsetof on JITStackFrame (since it contains non POD types),
    // and the OBJECT_OFFSETOF macro does not appear constantish enough for it to be happy with its use in COMPILE_ASSERT
    // macros.
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedReturnAddress) == PRESERVED_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR4) == PRESERVED_R4_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR5) == PRESERVED_R5_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR6) == PRESERVED_R6_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR7) == PRESERVED_R7_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR8) == PRESERVED_R8_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR9) == PRESERVED_R9_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR10) == PRESERVED_R10_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR11) == PRESERVED_R11_OFFSET);

    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, stack) == REGISTER_FILE_OFFSET);
    // The fifth argument is the first item already on the stack.
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, unused1) == FIRST_STACK_ARGUMENT);

    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, thunkReturnAddress) == THUNK_RETURN_ADDRESS_OFFSET);
}

} // namespace JSC

#endif // JITStubsARMv7_h
