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

#if ENABLE(ASSEMBLER) && CPU(ARM64)
#include "MacroAssembler.h"

#include <wtf/InlineASM.h>

namespace JSC {

#if ENABLE(MASM_PROBE)

extern "C" void ctiMasmProbeTrampoline();

using namespace ARM64Registers;

#if COMPILER(GCC_OR_CLANG)

// The following are offsets for ProbeContext fields accessed
// by the ctiMasmProbeTrampoline stub.
#define PTR_SIZE 8
#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET (2 * PTR_SIZE)

#define GPREG_SIZE 8
#define PROBE_CPU_X0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (0 * GPREG_SIZE))
#define PROBE_CPU_X1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (1 * GPREG_SIZE))
#define PROBE_CPU_X2_OFFSET (PROBE_FIRST_GPREG_OFFSET + (2 * GPREG_SIZE))
#define PROBE_CPU_X3_OFFSET (PROBE_FIRST_GPREG_OFFSET + (3 * GPREG_SIZE))
#define PROBE_CPU_X4_OFFSET (PROBE_FIRST_GPREG_OFFSET + (4 * GPREG_SIZE))
#define PROBE_CPU_X5_OFFSET (PROBE_FIRST_GPREG_OFFSET + (5 * GPREG_SIZE))
#define PROBE_CPU_X6_OFFSET (PROBE_FIRST_GPREG_OFFSET + (6 * GPREG_SIZE))
#define PROBE_CPU_X7_OFFSET (PROBE_FIRST_GPREG_OFFSET + (7 * GPREG_SIZE))
#define PROBE_CPU_X8_OFFSET (PROBE_FIRST_GPREG_OFFSET + (8 * GPREG_SIZE))
#define PROBE_CPU_X9_OFFSET (PROBE_FIRST_GPREG_OFFSET + (9 * GPREG_SIZE))
#define PROBE_CPU_X10_OFFSET (PROBE_FIRST_GPREG_OFFSET + (10 * GPREG_SIZE))
#define PROBE_CPU_X11_OFFSET (PROBE_FIRST_GPREG_OFFSET + (11 * GPREG_SIZE))
#define PROBE_CPU_X12_OFFSET (PROBE_FIRST_GPREG_OFFSET + (12 * GPREG_SIZE))
#define PROBE_CPU_X13_OFFSET (PROBE_FIRST_GPREG_OFFSET + (13 * GPREG_SIZE))
#define PROBE_CPU_X14_OFFSET (PROBE_FIRST_GPREG_OFFSET + (14 * GPREG_SIZE))
#define PROBE_CPU_X15_OFFSET (PROBE_FIRST_GPREG_OFFSET + (15 * GPREG_SIZE))
#define PROBE_CPU_X16_OFFSET (PROBE_FIRST_GPREG_OFFSET + (16 * GPREG_SIZE))
#define PROBE_CPU_X17_OFFSET (PROBE_FIRST_GPREG_OFFSET + (17 * GPREG_SIZE))
#define PROBE_CPU_X18_OFFSET (PROBE_FIRST_GPREG_OFFSET + (18 * GPREG_SIZE))
#define PROBE_CPU_X19_OFFSET (PROBE_FIRST_GPREG_OFFSET + (19 * GPREG_SIZE))
#define PROBE_CPU_X20_OFFSET (PROBE_FIRST_GPREG_OFFSET + (20 * GPREG_SIZE))
#define PROBE_CPU_X21_OFFSET (PROBE_FIRST_GPREG_OFFSET + (21 * GPREG_SIZE))
#define PROBE_CPU_X22_OFFSET (PROBE_FIRST_GPREG_OFFSET + (22 * GPREG_SIZE))
#define PROBE_CPU_X23_OFFSET (PROBE_FIRST_GPREG_OFFSET + (23 * GPREG_SIZE))
#define PROBE_CPU_X24_OFFSET (PROBE_FIRST_GPREG_OFFSET + (24 * GPREG_SIZE))
#define PROBE_CPU_X25_OFFSET (PROBE_FIRST_GPREG_OFFSET + (25 * GPREG_SIZE))
#define PROBE_CPU_X26_OFFSET (PROBE_FIRST_GPREG_OFFSET + (26 * GPREG_SIZE))
#define PROBE_CPU_X27_OFFSET (PROBE_FIRST_GPREG_OFFSET + (27 * GPREG_SIZE))
#define PROBE_CPU_X28_OFFSET (PROBE_FIRST_GPREG_OFFSET + (28 * GPREG_SIZE))
#define PROBE_CPU_FP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (29 * GPREG_SIZE))
#define PROBE_CPU_LR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (30 * GPREG_SIZE))
#define PROBE_CPU_SP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (31 * GPREG_SIZE))

#define PROBE_CPU_PC_OFFSET (PROBE_FIRST_GPREG_OFFSET + (32 * GPREG_SIZE))
#define PROBE_CPU_NZCV_OFFSET (PROBE_FIRST_GPREG_OFFSET + (33 * GPREG_SIZE))
#define PROBE_CPU_FPSR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (34 * GPREG_SIZE))

#define PROBE_FIRST_FPREG_OFFSET (PROBE_FIRST_GPREG_OFFSET + (35 * GPREG_SIZE))

#define FPREG_SIZE 8
#define PROBE_CPU_Q0_OFFSET (PROBE_FIRST_FPREG_OFFSET + (0 * FPREG_SIZE))
#define PROBE_CPU_Q1_OFFSET (PROBE_FIRST_FPREG_OFFSET + (1 * FPREG_SIZE))
#define PROBE_CPU_Q2_OFFSET (PROBE_FIRST_FPREG_OFFSET + (2 * FPREG_SIZE))
#define PROBE_CPU_Q3_OFFSET (PROBE_FIRST_FPREG_OFFSET + (3 * FPREG_SIZE))
#define PROBE_CPU_Q4_OFFSET (PROBE_FIRST_FPREG_OFFSET + (4 * FPREG_SIZE))
#define PROBE_CPU_Q5_OFFSET (PROBE_FIRST_FPREG_OFFSET + (5 * FPREG_SIZE))
#define PROBE_CPU_Q6_OFFSET (PROBE_FIRST_FPREG_OFFSET + (6 * FPREG_SIZE))
#define PROBE_CPU_Q7_OFFSET (PROBE_FIRST_FPREG_OFFSET + (7 * FPREG_SIZE))
#define PROBE_CPU_Q8_OFFSET (PROBE_FIRST_FPREG_OFFSET + (8 * FPREG_SIZE))
#define PROBE_CPU_Q9_OFFSET (PROBE_FIRST_FPREG_OFFSET + (9 * FPREG_SIZE))
#define PROBE_CPU_Q10_OFFSET (PROBE_FIRST_FPREG_OFFSET + (10 * FPREG_SIZE))
#define PROBE_CPU_Q11_OFFSET (PROBE_FIRST_FPREG_OFFSET + (11 * FPREG_SIZE))
#define PROBE_CPU_Q12_OFFSET (PROBE_FIRST_FPREG_OFFSET + (12 * FPREG_SIZE))
#define PROBE_CPU_Q13_OFFSET (PROBE_FIRST_FPREG_OFFSET + (13 * FPREG_SIZE))
#define PROBE_CPU_Q14_OFFSET (PROBE_FIRST_FPREG_OFFSET + (14 * FPREG_SIZE))
#define PROBE_CPU_Q15_OFFSET (PROBE_FIRST_FPREG_OFFSET + (15 * FPREG_SIZE))
#define PROBE_CPU_Q16_OFFSET (PROBE_FIRST_FPREG_OFFSET + (16 * FPREG_SIZE))
#define PROBE_CPU_Q17_OFFSET (PROBE_FIRST_FPREG_OFFSET + (17 * FPREG_SIZE))
#define PROBE_CPU_Q18_OFFSET (PROBE_FIRST_FPREG_OFFSET + (18 * FPREG_SIZE))
#define PROBE_CPU_Q19_OFFSET (PROBE_FIRST_FPREG_OFFSET + (19 * FPREG_SIZE))
#define PROBE_CPU_Q20_OFFSET (PROBE_FIRST_FPREG_OFFSET + (20 * FPREG_SIZE))
#define PROBE_CPU_Q21_OFFSET (PROBE_FIRST_FPREG_OFFSET + (21 * FPREG_SIZE))
#define PROBE_CPU_Q22_OFFSET (PROBE_FIRST_FPREG_OFFSET + (22 * FPREG_SIZE))
#define PROBE_CPU_Q23_OFFSET (PROBE_FIRST_FPREG_OFFSET + (23 * FPREG_SIZE))
#define PROBE_CPU_Q24_OFFSET (PROBE_FIRST_FPREG_OFFSET + (24 * FPREG_SIZE))
#define PROBE_CPU_Q25_OFFSET (PROBE_FIRST_FPREG_OFFSET + (25 * FPREG_SIZE))
#define PROBE_CPU_Q26_OFFSET (PROBE_FIRST_FPREG_OFFSET + (26 * FPREG_SIZE))
#define PROBE_CPU_Q27_OFFSET (PROBE_FIRST_FPREG_OFFSET + (27 * FPREG_SIZE))
#define PROBE_CPU_Q28_OFFSET (PROBE_FIRST_FPREG_OFFSET + (28 * FPREG_SIZE))
#define PROBE_CPU_Q29_OFFSET (PROBE_FIRST_FPREG_OFFSET + (29 * FPREG_SIZE))
#define PROBE_CPU_Q30_OFFSET (PROBE_FIRST_FPREG_OFFSET + (30 * FPREG_SIZE))
#define PROBE_CPU_Q31_OFFSET (PROBE_FIRST_FPREG_OFFSET + (31 * FPREG_SIZE))
#define PROBE_SIZE (PROBE_FIRST_FPREG_OFFSET + (32 * FPREG_SIZE))

#define SAVED_PROBE_RETURN_PC_OFFSET        (PROBE_SIZE + (0 * PTR_SIZE))
#define SAVED_PROBE_LR_OFFSET               (PROBE_SIZE + (1 * PTR_SIZE))
#define SAVED_PROBE_ERROR_FUNCTION_OFFSET   (PROBE_SIZE + (2 * PTR_SIZE))
#define PROBE_SIZE_PLUS_EXTRAS              (PROBE_SIZE + (3 * PTR_SIZE))

// These ASSERTs remind you that if you change the layout of ProbeContext,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct ProbeContext, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeContext_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, ProbeContext_arg_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_X0_OFFSET & 0x7), ProbeContext_cpu_r0_offset_should_be_8_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x0]) == PROBE_CPU_X0_OFFSET, ProbeContext_cpu_x0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x1]) == PROBE_CPU_X1_OFFSET, ProbeContext_cpu_x1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x2]) == PROBE_CPU_X2_OFFSET, ProbeContext_cpu_x2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x3]) == PROBE_CPU_X3_OFFSET, ProbeContext_cpu_x3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x4]) == PROBE_CPU_X4_OFFSET, ProbeContext_cpu_x4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x5]) == PROBE_CPU_X5_OFFSET, ProbeContext_cpu_x5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x6]) == PROBE_CPU_X6_OFFSET, ProbeContext_cpu_x6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x7]) == PROBE_CPU_X7_OFFSET, ProbeContext_cpu_x7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x8]) == PROBE_CPU_X8_OFFSET, ProbeContext_cpu_x8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x9]) == PROBE_CPU_X9_OFFSET, ProbeContext_cpu_x9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x10]) == PROBE_CPU_X10_OFFSET, ProbeContext_cpu_x10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x11]) == PROBE_CPU_X11_OFFSET, ProbeContext_cpu_x11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x12]) == PROBE_CPU_X12_OFFSET, ProbeContext_cpu_x12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x13]) == PROBE_CPU_X13_OFFSET, ProbeContext_cpu_x13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x14]) == PROBE_CPU_X14_OFFSET, ProbeContext_cpu_x14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x15]) == PROBE_CPU_X15_OFFSET, ProbeContext_cpu_x15_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x16]) == PROBE_CPU_X16_OFFSET, ProbeContext_cpu_x16_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x17]) == PROBE_CPU_X17_OFFSET, ProbeContext_cpu_x17_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x18]) == PROBE_CPU_X18_OFFSET, ProbeContext_cpu_x18_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x19]) == PROBE_CPU_X19_OFFSET, ProbeContext_cpu_x19_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x20]) == PROBE_CPU_X20_OFFSET, ProbeContext_cpu_x20_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x21]) == PROBE_CPU_X21_OFFSET, ProbeContext_cpu_x21_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x22]) == PROBE_CPU_X22_OFFSET, ProbeContext_cpu_x22_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x23]) == PROBE_CPU_X23_OFFSET, ProbeContext_cpu_x23_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x24]) == PROBE_CPU_X24_OFFSET, ProbeContext_cpu_x24_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x25]) == PROBE_CPU_X25_OFFSET, ProbeContext_cpu_x25_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x26]) == PROBE_CPU_X26_OFFSET, ProbeContext_cpu_x26_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x27]) == PROBE_CPU_X27_OFFSET, ProbeContext_cpu_x27_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x28]) == PROBE_CPU_X28_OFFSET, ProbeContext_cpu_x28_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::fp]) == PROBE_CPU_FP_OFFSET, ProbeContext_cpu_fp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::lr]) == PROBE_CPU_LR_OFFSET, ProbeContext_cpu_lr_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::sp]) == PROBE_CPU_SP_OFFSET, ProbeContext_cpu_sp_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::pc]) == PROBE_CPU_PC_OFFSET, ProbeContext_cpu_pc_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::nzcv]) == PROBE_CPU_NZCV_OFFSET, ProbeContext_cpu_nzcv_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::fpsr]) == PROBE_CPU_FPSR_OFFSET, ProbeContext_cpu_fpsr_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(!(PROBE_CPU_Q0_OFFSET & 0x7), ProbeContext_cpu_q0_offset_should_be_8_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q0]) == PROBE_CPU_Q0_OFFSET, ProbeContext_cpu_q0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q1]) == PROBE_CPU_Q1_OFFSET, ProbeContext_cpu_q1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q2]) == PROBE_CPU_Q2_OFFSET, ProbeContext_cpu_q2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q3]) == PROBE_CPU_Q3_OFFSET, ProbeContext_cpu_q3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q4]) == PROBE_CPU_Q4_OFFSET, ProbeContext_cpu_q4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q5]) == PROBE_CPU_Q5_OFFSET, ProbeContext_cpu_q5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q6]) == PROBE_CPU_Q6_OFFSET, ProbeContext_cpu_q6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q7]) == PROBE_CPU_Q7_OFFSET, ProbeContext_cpu_q7_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q8]) == PROBE_CPU_Q8_OFFSET, ProbeContext_cpu_q8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q9]) == PROBE_CPU_Q9_OFFSET, ProbeContext_cpu_q9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q10]) == PROBE_CPU_Q10_OFFSET, ProbeContext_cpu_q10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q11]) == PROBE_CPU_Q11_OFFSET, ProbeContext_cpu_q11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q12]) == PROBE_CPU_Q12_OFFSET, ProbeContext_cpu_q12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q13]) == PROBE_CPU_Q13_OFFSET, ProbeContext_cpu_q13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q14]) == PROBE_CPU_Q14_OFFSET, ProbeContext_cpu_q14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q15]) == PROBE_CPU_Q15_OFFSET, ProbeContext_cpu_q15_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q16]) == PROBE_CPU_Q16_OFFSET, ProbeContext_cpu_q16_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q17]) == PROBE_CPU_Q17_OFFSET, ProbeContext_cpu_q17_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q18]) == PROBE_CPU_Q18_OFFSET, ProbeContext_cpu_q18_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q19]) == PROBE_CPU_Q19_OFFSET, ProbeContext_cpu_q19_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q20]) == PROBE_CPU_Q20_OFFSET, ProbeContext_cpu_q20_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q21]) == PROBE_CPU_Q21_OFFSET, ProbeContext_cpu_q21_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q22]) == PROBE_CPU_Q22_OFFSET, ProbeContext_cpu_q22_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q23]) == PROBE_CPU_Q23_OFFSET, ProbeContext_cpu_q23_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q24]) == PROBE_CPU_Q24_OFFSET, ProbeContext_cpu_q24_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q25]) == PROBE_CPU_Q25_OFFSET, ProbeContext_cpu_q25_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q26]) == PROBE_CPU_Q26_OFFSET, ProbeContext_cpu_q26_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q27]) == PROBE_CPU_Q27_OFFSET, ProbeContext_cpu_q27_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q28]) == PROBE_CPU_Q28_OFFSET, ProbeContext_cpu_q28_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q29]) == PROBE_CPU_Q29_OFFSET, ProbeContext_cpu_q29_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q30]) == PROBE_CPU_Q30_OFFSET, ProbeContext_cpu_q30_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[ARM64Registers::q31]) == PROBE_CPU_Q31_OFFSET, ProbeContext_cpu_q31_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(sizeof(ProbeContext) == PROBE_SIZE, ProbeContext_size_matches_ctiMasmProbeTrampoline);

// Conditions for using ldp and stp.
static_assert(PROBE_CPU_PC_OFFSET == PROBE_CPU_SP_OFFSET + PTR_SIZE, "PROBE_CPU_SP_OFFSET and PROBE_CPU_PC_OFFSET must be adjacent");

#undef PROBE_OFFSETOF

#define FPR_OFFSET(fpr) (PROBE_CPU_##fpr##_OFFSET - PROBE_CPU_Q0_OFFSET)

struct IncomingProbeRecord {
    uintptr_t probeHandlerFunction;
    uintptr_t probeArg;
    uintptr_t x26;
    uintptr_t x27;
    uintptr_t lr;
    uintptr_t sp;
    uintptr_t probeErrorFunction;
    uintptr_t unused; // Padding for alignment.
};

#define IN_HANDLER_FUNCTION_OFFSET (0 * PTR_SIZE)
#define IN_ARG_OFFSET              (1 * PTR_SIZE)
#define IN_X26_OFFSET              (2 * PTR_SIZE)
#define IN_X27_OFFSET              (3 * PTR_SIZE)
#define IN_LR_OFFSET               (4 * PTR_SIZE)
#define IN_SP_OFFSET               (5 * PTR_SIZE)
#define IN_ERROR_FUNCTION_OFFSET   (6 * PTR_SIZE)

static_assert(IN_HANDLER_FUNCTION_OFFSET == offsetof(IncomingProbeRecord, probeHandlerFunction), "IN_HANDLER_FUNCTION_OFFSET is incorrect");
static_assert(IN_ARG_OFFSET == offsetof(IncomingProbeRecord, probeArg), "IN_ARG_OFFSET is incorrect");
static_assert(IN_X26_OFFSET == offsetof(IncomingProbeRecord, x26), "IN_X26_OFFSET is incorrect");
static_assert(IN_X27_OFFSET == offsetof(IncomingProbeRecord, x27), "IN_X27_OFFSET is incorrect");
static_assert(IN_LR_OFFSET == offsetof(IncomingProbeRecord, lr), "IN_LR_OFFSET is incorrect");
static_assert(IN_SP_OFFSET == offsetof(IncomingProbeRecord, sp), "IN_SP_OFFSET is incorrect");
static_assert(IN_ERROR_FUNCTION_OFFSET == offsetof(IncomingProbeRecord, probeErrorFunction), "IN_ERROR_FUNCTION_OFFSET is incorrect");
static_assert(!(sizeof(IncomingProbeRecord) & 0xf), "IncomingProbeStack must be 16-byte aligned");

struct OutgoingProbeRecord {
    uintptr_t nzcv;
    uintptr_t fpsr;
    uintptr_t x27;
    uintptr_t x28;
    uintptr_t fp;
    uintptr_t lr;
};

#define OUT_NZCV_OFFSET (0 * PTR_SIZE)
#define OUT_FPSR_OFFSET (1 * PTR_SIZE)
#define OUT_X27_OFFSET  (2 * PTR_SIZE)
#define OUT_X28_OFFSET  (3 * PTR_SIZE)
#define OUT_FP_OFFSET   (4 * PTR_SIZE)
#define OUT_LR_OFFSET   (5 * PTR_SIZE)
#define OUT_SIZE        (6 * PTR_SIZE)

static_assert(OUT_NZCV_OFFSET == offsetof(OutgoingProbeRecord, nzcv), "OUT_NZCV_OFFSET is incorrect");
static_assert(OUT_FPSR_OFFSET == offsetof(OutgoingProbeRecord, fpsr), "OUT_FPSR_OFFSET is incorrect");
static_assert(OUT_X27_OFFSET == offsetof(OutgoingProbeRecord, x27), "OUT_X27_OFFSET is incorrect");
static_assert(OUT_X28_OFFSET == offsetof(OutgoingProbeRecord, x28), "OUT_X28_OFFSET is incorrect");
static_assert(OUT_FP_OFFSET == offsetof(OutgoingProbeRecord, fp), "OUT_FP_OFFSET is incorrect");
static_assert(OUT_LR_OFFSET == offsetof(OutgoingProbeRecord, lr), "OUT_LR_OFFSET is incorrect");
static_assert(OUT_SIZE == sizeof(OutgoingProbeRecord), "OUT_SIZE is incorrect");
static_assert(!(sizeof(OutgoingProbeRecord) & 0xf), "IncomingProbeStack must be 16-byte aligned");

#define STATE_PC_NOT_CHANGED 0
#define STATE_PC_CHANGED 1
static_assert(STATE_PC_NOT_CHANGED != STATE_PC_CHANGED, "STATE_PC_NOT_CHANGED and STATE_PC_CHANGED should not be equal");

asm (
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssemblerARM64::probe() has already generated code to store some values in an
    // IncomingProbeRecord. sp points to the IncomingProbeRecord.

    "mov       x26, sp" "\n"
    "mov       x27, sp" "\n"

    "sub       x27, x27, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS) "\n"
    "bic       x27, x27, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "mov       sp, x27" "\n" // Make sure interrupts don't over-write our data on the stack.

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n"
    "mrs       x0, nzcv" "\n" // Preload nzcv.
    "stp       x2, x3, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "]" "\n"
    "stp       x4, x5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X4_OFFSET) "]" "\n"
    "mrs       x1, fpsr" "\n" // Preload fpsr.
    "stp       x6, x7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "]" "\n"
    "stp       x8, x9, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "]" "\n"

    "ldp       x2, x3, [x26, #" STRINGIZE_VALUE_OF(IN_HANDLER_FUNCTION_OFFSET) "]" "\n" // Preload probe handler function and probe arg.
    "ldp       x4, x5, [x26, #" STRINGIZE_VALUE_OF(IN_X26_OFFSET) "]" "\n" // Preload saved r26 and r27.
    "ldp       x6, x7, [x26, #" STRINGIZE_VALUE_OF(IN_LR_OFFSET) "]" "\n" // Preload saved lr and sp.
    "ldr       x8, [x26, #" STRINGIZE_VALUE_OF(IN_ERROR_FUNCTION_OFFSET) "]" "\n" // Preload probe error function.

    "stp       x10, x11, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "]" "\n"
    "stp       x12, x13, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "]" "\n"
    "stp       x14, x15, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "]" "\n"
    "stp       x16, x17, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "]" "\n"
    "stp       x18, x19, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X18_OFFSET) "]" "\n"
    "stp       x20, x21, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X20_OFFSET) "]" "\n"
    "stp       x22, x23, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X22_OFFSET) "]" "\n"
    "stp       x24, x25, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X24_OFFSET) "]" "\n"
    "stp       x4, x5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X26_OFFSET) "]" "\n" // Store saved r26 and r27 (preloaded into x4 and x5 above).
    "stp       x28, fp, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X28_OFFSET) "]" "\n"
    "stp       x6, x7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n" // Save values lr and sp (preloaded into x6 and x7 above).

    "str       x6, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_LR_OFFSET) "]" "\n" // Save a duplicate copy of lr (in x6).
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n" // Save a duplicate copy of return pc (in lr).

    "add       lr, lr, #" STRINGIZE_VALUE_OF(2 * PTR_SIZE) "\n" // The PC after the probe is at 2 instructions past the return point.
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n" // Store nzcv and fpsr (preloaded into x0 and x1 above).

    "stp       x2, x3, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n" // Store the probe handler function and arg (preloaded into x2 and x3 above).
    "str       x8, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_ERROR_FUNCTION_OFFSET) "]" "\n" // Store the probe handler function and arg (preloaded into x8 above).

    "add       x9, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_Q0_OFFSET) "\n"
    "stp       d0, d1, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q0)) "]" "\n"
    "stp       d2, d3, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q2)) "]" "\n"
    "stp       d4, d5, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q4)) "]" "\n"
    "stp       d6, d7, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q6)) "]" "\n"
    "stp       d8, d9, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q8)) "]" "\n"
    "stp       d10, d11, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q10)) "]" "\n"
    "stp       d12, d13, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q12)) "]" "\n"
    "stp       d14, d15, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q14)) "]" "\n"
    "stp       d16, d17, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q16)) "]" "\n"
    "stp       d18, d19, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q18)) "]" "\n"
    "stp       d20, d21, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q20)) "]" "\n"
    "stp       d22, d23, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q22)) "]" "\n"
    "stp       d24, d25, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q24)) "]" "\n"
    "stp       d26, d27, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q26)) "]" "\n"
    "stp       d28, d29, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q28)) "]" "\n"
    "stp       d30, d31, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q30)) "]" "\n"

    "mov       x27, sp" "\n" // Save the ProbeContext* in a callee saved register.

    // Note: we haven't changed the value of fp. Hence, it is still pointing to the frame of
    // the caller of the probe (which is what we want in order to play nice with debuggers e.g. lldb).
    "mov       x0, sp" "\n" // Set the ProbeContext* arg.
    "blr       x2" "\n" // Call the probe handler function (loaded into x2 above).

    "mov       sp, x27" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the ProbeContext before returning. That is except for x18.
    // x18 is "reserved for the platform. Conforming software should not make use of it."
    // Hence, the JITs would not be using it, and the probe should also not be modifying it.
    // See https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html.

    "add       x9, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_Q0_OFFSET) "\n"
    "ldp       d0, d1, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q0)) "]" "\n"
    "ldp       d2, d3, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q2)) "]" "\n"
    "ldp       d4, d5, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q4)) "]" "\n"
    "ldp       d6, d7, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q6)) "]" "\n"
    "ldp       d8, d9, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q8)) "]" "\n"
    "ldp       d10, d11, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q10)) "]" "\n"
    "ldp       d12, d13, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q12)) "]" "\n"
    "ldp       d14, d15, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q14)) "]" "\n"
    "ldp       d16, d17, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q16)) "]" "\n"
    "ldp       d18, d19, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q18)) "]" "\n"
    "ldp       d20, d21, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q20)) "]" "\n"
    "ldp       d22, d23, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q22)) "]" "\n"
    "ldp       d24, d25, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q24)) "]" "\n"
    "ldp       d26, d27, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q26)) "]" "\n"
    "ldp       d28, d29, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q28)) "]" "\n"
    "ldp       d30, d31, [x9, #" STRINGIZE_VALUE_OF(FPR_OFFSET(Q30)) "]" "\n"

    "ldp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n"
    "ldp       x2, x3, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "]" "\n"
    "ldp       x4, x5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X4_OFFSET) "]" "\n"
    "ldp       x6, x7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "]" "\n"
    "ldp       x8, x9, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "]" "\n"
    "ldp       x10, x11, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "]" "\n"
    "ldp       x12, x13, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "]" "\n"
    "ldp       x14, x15, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "]" "\n"
    "ldp       x16, x17, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "]" "\n"
    // x18 should not be modified by the probe. See comment above for details.
    "ldp       x19, x20, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X19_OFFSET) "]" "\n"
    "ldp       x21, x22, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X21_OFFSET) "]" "\n"
    "ldp       x23, x24, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X23_OFFSET) "]" "\n"
    "ldp       x25, x26, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X25_OFFSET) "]" "\n"

    // Remaining registers to restore are: fpsr, nzcv, x27, x28, fp, lr, sp, and pc.

    "mov       lr, #" STRINGIZE_VALUE_OF(STATE_PC_NOT_CHANGED) "\n"

    // The only way to set the pc on ARM64 (from user space) is via an indirect branch
    // or a ret, which means we'll need a free register to do so. For our purposes, lr
    // happens to be available in applications of the probe where we may want to
    // continue executing at a different location (i.e. change the pc) after the probe
    // returns. So, the ARM64 probe implementation will allow the probe handler to
    // either modify lr or pc, but not both in the same probe invocation. The probe
    // mechanism ensures that we never try to modify both lr and pc, else it will
    // fail with a RELEASE_ASSERT_NOT_REACHED in arm64ProbeError().

    // Determine if the probe handler changed the pc.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "add       x27, x27, #" STRINGIZE_VALUE_OF(2 * PTR_SIZE) "\n"
    "cmp       x27, x28" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePrepareOutgoingRecords) "\n"

    // pc was changed. Determine if the probe handler also changed lr.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_LR_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "cmp       x27, x28" "\n"
    "bne     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineError) "\n"

    "mov       lr, #" STRINGIZE_VALUE_OF(STATE_PC_CHANGED) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePrepareOutgoingRecords) ":" "\n"

    "ldr       fp, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n" // Preload the probe return site pc.

    // The probe handler may have moved the sp. For the return process, we may need
    // space for 2 OutgoingProbeRecords below the final sp value. We need to make
    // sure that the space for these 2 OutgoingProbeRecords do not overlap the
    // restore values of the registers.

    // All restore values are located at offset <= PROBE_CPU_FPSR_OFFSET. Hence,
    // we need to make sure that resultant sp > offset of fpsr + 2 * sizeof(OutgoingProbeRecord).

    "add       x27, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSR_OFFSET + 2 * OUT_SIZE) "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "cmp       x28, x27" "\n"
    "bgt     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineFillOutgoingProbeRecords) "\n"

    // There is overlap. We need to copy the ProbeContext to a safe area first.
    // Let's locate the "safe area" at 2x sizeof(ProbeContext) below where the OutgoingProbeRecords are.
    // This ensures that:
    // 1. The safe area does not overlap the OutgoingProbeRecords.
    // 2. The safe area does not overlap the ProbeContext.

    // x28 already contains [sp, #STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET)].
    "sub       x28, x28, #" STRINGIZE_VALUE_OF(2 * PROBE_SIZE) "\n"

    "mov       x27, sp" "\n" // Save the original ProbeContext*.

    // Make sure the stack pointer points to the safe area. This ensures that the
    // safe area is protected from interrupt handlers overwriting it.
    "mov       sp, x28" "\n" // sp now points to the new ProbeContext in the safe area.

    // Copy the relevant restore data to the new ProbeContext*.
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n" // Stash the pc changed state away so that we can use lr.

    "ldp       x28, lr, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "]" "\n" // copy x27 and x28.
    "stp       x28, lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "]" "\n"
    "ldp       x28, lr, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "]" "\n" // copy fp and lr.
    "stp       x28, lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "]" "\n"
    "ldp       x28, lr, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // copy sp and pc.
    "stp       x28, lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "ldp       x28, lr, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n" // copy nzcv and fpsr.
    "stp       x28, lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n"

    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n" // Retrieve the stashed the pc changed state.

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineFillOutgoingProbeRecords) ":" "\n"

    "cbnz       lr, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) "\n" // Skip lr restoration setup if state (in lr) == STATE_PC_CHANGED.

    // In order to restore lr, we need to do the restoration at the probe return site.
    // The probe return site expects sp to be pointing at an OutgoingProbeRecord such that
    // popping the OutgoingProbeRecord will yield the desired sp. The probe return site
    // also expects the lr value to be restored is stashed in the OutgoingProbeRecord.
    // We can make this happen by pushing 2 OutgoingProbeRecords instead of 1:
    // 1 for the probe return site, and 1 at ctiMasmProbeTrampolineEnd for returning from
    // this probe.

    // Fill in the OutgoingProbeStack for the probe return site.
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "sub       lr, lr, #" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "str       x27, [lr, #" STRINGIZE_VALUE_OF(OUT_LR_OFFSET) "]" "\n"

    // Set up the sp and pc values so that ctiMasmProbeTrampolineEnd will return to the probe return site.
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "str       fp, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n" // Store the probe return site pc (preloaded into fp above).

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Fill in the OutgoingProbeStack.
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "sub       lr, lr, #" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n"
    "stp       x27, x28, [lr, #" STRINGIZE_VALUE_OF(OUT_NZCV_OFFSET) "]" "\n"
    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "]" "\n"
    "stp       x27, x28, [lr, #" STRINGIZE_VALUE_OF(OUT_X27_OFFSET) "]" "\n"
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "stp       x27, x28, [lr, #" STRINGIZE_VALUE_OF(OUT_FP_OFFSET) "]" "\n"
    "mov       sp, lr" "\n"

    // Restore the remaining registers and pop the OutgoingProbeStack.
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * PTR_SIZE) "\n"
    "msr       nzcv, x27" "\n"
    "msr       fpsr, x28" "\n"
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * PTR_SIZE) "\n"
    "ldp       fp, lr, [sp], #" STRINGIZE_VALUE_OF(2 * PTR_SIZE) "\n"
    "ret" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineError) ":" "\n"
    // The probe handler changed both lr and pc. This is not supported for ARM64.
    "ldr       x1, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_ERROR_FUNCTION_OFFSET) "]" "\n"
    "mov       x0, sp" "\n" // Set the ProbeContext* arg.
    "blr       x1" "\n"
    "brk       #0x1000" // Should never return here.
);
#endif // COMPILER(GCC_OR_CLANG)

static NO_RETURN_DUE_TO_CRASH void arm64ProbeError(ProbeContext*)
{
    dataLog("MacroAssembler probe ERROR: ARM64 does not support the probe changing both LR and PC.\n");
    RELEASE_ASSERT_NOT_REACHED();
}

void MacroAssembler::probe(ProbeFunction function, void* arg)
{
    sub64(TrustedImm32(sizeof(IncomingProbeRecord)), sp);

    storePair64(x26, x27, sp, TrustedImm32(offsetof(IncomingProbeRecord, x26)));
    add64(TrustedImm32(sizeof(IncomingProbeRecord)), sp, x26);
    storePair64(lr, x26, sp, TrustedImm32(offsetof(IncomingProbeRecord, lr))); // Save lr and original sp value.
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), x26);
    move(TrustedImmPtr(arg), x27);
    storePair64(x26, x27, sp, TrustedImm32(offsetof(IncomingProbeRecord, probeHandlerFunction)));
    move(TrustedImmPtr(reinterpret_cast<void*>(arm64ProbeError)), x27);
    store64(x27, Address(sp, offsetof(IncomingProbeRecord, probeErrorFunction)));

    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), x26);
    m_assembler.blr(x26);

    // ctiMasmProbeTrampoline should have restored every register except for lr and the sp.
    load64(Address(sp, offsetof(OutgoingProbeRecord, lr)), lr);
    add64(TrustedImm32(sizeof(OutgoingProbeRecord)), sp);
}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM64)

