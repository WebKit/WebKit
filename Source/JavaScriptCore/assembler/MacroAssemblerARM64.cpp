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

#if ENABLE(ASSEMBLER) && CPU(ARM64)
#include "MacroAssembler.h"

#include "JSCPtrTag.h"
#include "ProbeContext.h"
#include <wtf/InlineASM.h>

#if OS(LINUX)
#include <asm/hwcap.h>
#if __has_include(<sys/auxv.h>)
#include <sys/auxv.h>
#else
#include <linux/auxvec.h>
// Provide an implementation for C libraries which do not ship one.
static unsigned long getauxval(unsigned long type)
{
    char** env = environ;
    while (*env++) { /* no-op */ }

    for (auto* auxv = reinterpret_cast<unsigned long*>(env); *auxv != AT_NULL; auxv += 2) {
        if (*auxv == type)
            return auxv[1];
    }

    errno = ENOENT;
    return 0;
}
#endif
#endif

namespace JSC {

JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);
JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampolineSIMD, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampolineSIMD);

using namespace ARM64Registers;

#if COMPILER(GCC_COMPATIBLE)

// The following are offsets for Probe::State fields accessed
// by the ctiMasmProbeTrampoline stub.
#if CPU(ADDRESS64)
#define PTR_SIZE 8
#else
#define PTR_SIZE 4
#endif

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INIT_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INIT_STACK_ARG_OFFSET (3 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET (4 * PTR_SIZE)

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
#define PROBE_SIZE (PROBE_FIRST_FPREG_OFFSET + (32 * 2 * FPREG_SIZE))

#define SAVED_PROBE_RETURN_PC_OFFSET        (PROBE_SIZE + (0 * GPREG_SIZE))
#define PROBE_SIZE_PLUS_EXTRAS              (PROBE_SIZE + (3 * GPREG_SIZE))

// These ASSERTs remind you that if you change the layout of Probe::State,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct Probe::State, x)
static_assert(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, "Probe::State::probeFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, "Probe::State::arg's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INIT_STACK_FUNCTION_OFFSET, "Probe::State::initializeStackFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackArg) == PROBE_INIT_STACK_ARG_OFFSET, "Probe::State::initializeStackArg's offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_X0_OFFSET & 0x7), "Probe::State::cpu.gprs[x0]'s offset should be 8 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x0]) == PROBE_CPU_X0_OFFSET, "Probe::State::cpu.gprs[x0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x1]) == PROBE_CPU_X1_OFFSET, "Probe::State::cpu.gprs[x1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x2]) == PROBE_CPU_X2_OFFSET, "Probe::State::cpu.gprs[x2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x3]) == PROBE_CPU_X3_OFFSET, "Probe::State::cpu.gprs[x3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x4]) == PROBE_CPU_X4_OFFSET, "Probe::State::cpu.gprs[x4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x5]) == PROBE_CPU_X5_OFFSET, "Probe::State::cpu.gprs[x5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x6]) == PROBE_CPU_X6_OFFSET, "Probe::State::cpu.gprs[x6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x7]) == PROBE_CPU_X7_OFFSET, "Probe::State::cpu.gprs[x7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x8]) == PROBE_CPU_X8_OFFSET, "Probe::State::cpu.gprs[x8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x9]) == PROBE_CPU_X9_OFFSET, "Probe::State::cpu.gprs[x9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x10]) == PROBE_CPU_X10_OFFSET, "Probe::State::cpu.gprs[x10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x11]) == PROBE_CPU_X11_OFFSET, "Probe::State::cpu.gprs[x11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x12]) == PROBE_CPU_X12_OFFSET, "Probe::State::cpu.gprs[x12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x13]) == PROBE_CPU_X13_OFFSET, "Probe::State::cpu.gprs[x13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x14]) == PROBE_CPU_X14_OFFSET, "Probe::State::cpu.gprs[x14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x15]) == PROBE_CPU_X15_OFFSET, "Probe::State::cpu.gprs[x15]'s offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x16]) == PROBE_CPU_X16_OFFSET, "Probe::State::cpu.gprs[x16]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x17]) == PROBE_CPU_X17_OFFSET, "Probe::State::cpu.gprs[x17]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x18]) == PROBE_CPU_X18_OFFSET, "Probe::State::cpu.gprs[x18]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x19]) == PROBE_CPU_X19_OFFSET, "Probe::State::cpu.gprs[x19]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x20]) == PROBE_CPU_X20_OFFSET, "Probe::State::cpu.gprs[x20]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x21]) == PROBE_CPU_X21_OFFSET, "Probe::State::cpu.gprs[x21]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x22]) == PROBE_CPU_X22_OFFSET, "Probe::State::cpu.gprs[x22]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x23]) == PROBE_CPU_X23_OFFSET, "Probe::State::cpu.gprs[x23]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x24]) == PROBE_CPU_X24_OFFSET, "Probe::State::cpu.gprs[x24]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x25]) == PROBE_CPU_X25_OFFSET, "Probe::State::cpu.gprs[x25]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x26]) == PROBE_CPU_X26_OFFSET, "Probe::State::cpu.gprs[x26]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x27]) == PROBE_CPU_X27_OFFSET, "Probe::State::cpu.gprs[x27]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::x28]) == PROBE_CPU_X28_OFFSET, "Probe::State::cpu.gprs[x28]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::fp]) == PROBE_CPU_FP_OFFSET, "Probe::State::cpu.gprs[fp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::lr]) == PROBE_CPU_LR_OFFSET, "Probe::State::cpu.gprs[lr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARM64Registers::sp]) == PROBE_CPU_SP_OFFSET, "Probe::State::cpu.gprs[sp]'s offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::pc]) == PROBE_CPU_PC_OFFSET, "Probe::State::cpu.sprs[pc]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::nzcv]) == PROBE_CPU_NZCV_OFFSET, "Probe::State::cpu.sprs[nzcv]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[ARM64Registers::fpsr]) == PROBE_CPU_FPSR_OFFSET, "Probe::State::cpu.sprs[fpsr]'s offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_Q0_OFFSET & 0x7), "Probe::State::cpu.fprs[q0]'s offset should be 8 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q0]) == PROBE_CPU_Q0_OFFSET, "Probe::State::cpu.fprs[q0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q1]) == PROBE_CPU_Q1_OFFSET, "Probe::State::cpu.fprs[q1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q2]) == PROBE_CPU_Q2_OFFSET, "Probe::State::cpu.fprs[q2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q3]) == PROBE_CPU_Q3_OFFSET, "Probe::State::cpu.fprs[q3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q4]) == PROBE_CPU_Q4_OFFSET, "Probe::State::cpu.fprs[q4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q5]) == PROBE_CPU_Q5_OFFSET, "Probe::State::cpu.fprs[q5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q6]) == PROBE_CPU_Q6_OFFSET, "Probe::State::cpu.fprs[q6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q7]) == PROBE_CPU_Q7_OFFSET, "Probe::State::cpu.fprs[q7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q8]) == PROBE_CPU_Q8_OFFSET, "Probe::State::cpu.fprs[q8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q9]) == PROBE_CPU_Q9_OFFSET, "Probe::State::cpu.fprs[q9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q10]) == PROBE_CPU_Q10_OFFSET, "Probe::State::cpu.fprs[q10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q11]) == PROBE_CPU_Q11_OFFSET, "Probe::State::cpu.fprs[q11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q12]) == PROBE_CPU_Q12_OFFSET, "Probe::State::cpu.fprs[q12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q13]) == PROBE_CPU_Q13_OFFSET, "Probe::State::cpu.fprs[q13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q14]) == PROBE_CPU_Q14_OFFSET, "Probe::State::cpu.fprs[q14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q15]) == PROBE_CPU_Q15_OFFSET, "Probe::State::cpu.fprs[q15]'s offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q16]) == PROBE_CPU_Q16_OFFSET, "Probe::State::cpu.fprs[q16]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q17]) == PROBE_CPU_Q17_OFFSET, "Probe::State::cpu.fprs[q17]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q18]) == PROBE_CPU_Q18_OFFSET, "Probe::State::cpu.fprs[q18]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q19]) == PROBE_CPU_Q19_OFFSET, "Probe::State::cpu.fprs[q19]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q20]) == PROBE_CPU_Q20_OFFSET, "Probe::State::cpu.fprs[q20]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q21]) == PROBE_CPU_Q21_OFFSET, "Probe::State::cpu.fprs[q21]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q22]) == PROBE_CPU_Q22_OFFSET, "Probe::State::cpu.fprs[q22]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q23]) == PROBE_CPU_Q23_OFFSET, "Probe::State::cpu.fprs[q23]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q24]) == PROBE_CPU_Q24_OFFSET, "Probe::State::cpu.fprs[q24]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q25]) == PROBE_CPU_Q25_OFFSET, "Probe::State::cpu.fprs[q25]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q26]) == PROBE_CPU_Q26_OFFSET, "Probe::State::cpu.fprs[q26]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q27]) == PROBE_CPU_Q27_OFFSET, "Probe::State::cpu.fprs[q27]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q28]) == PROBE_CPU_Q28_OFFSET, "Probe::State::cpu.fprs[q28]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q29]) == PROBE_CPU_Q29_OFFSET, "Probe::State::cpu.fprs[q29]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q30]) == PROBE_CPU_Q30_OFFSET, "Probe::State::cpu.fprs[q30]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARM64Registers::q31]) == PROBE_CPU_Q31_OFFSET, "Probe::State::cpu.fprs[q31]'s offset matches ctiMasmProbeTrampoline");

static_assert(sizeof(Probe::State) == PROBE_SIZE, "Probe::State's size matches ctiMasmProbeTrampoline");

// Conditions for using ldp and stp.
static_assert(PROBE_CPU_PC_OFFSET == PROBE_CPU_SP_OFFSET + GPREG_SIZE, "PROBE_CPU_SP_OFFSET and PROBE_CPU_PC_OFFSET must be adjacent");
static_assert(!(PROBE_SIZE_PLUS_EXTRAS & 0xf), "PROBE_SIZE_PLUS_EXTRAS should be 16 byte aligned"); // the Probe::State copying code relies on this.

#undef PROBE_OFFSETOF

#define FPR_OFFSET(fpr) (PROBE_CPU_##fpr##_OFFSET - PROBE_CPU_Q0_OFFSET)

#define VECTOR_OFFSET(fpr) (2 * (PROBE_CPU_##fpr##_OFFSET - PROBE_CPU_Q0_OFFSET))

struct IncomingProbeRecord {
    UCPURegister x24;
    UCPURegister x25;
    UCPURegister x26;
    UCPURegister x30; // lr
    UCPURegister x27; // Saved in trampoline to use as scratch.
    UCPURegister unusedForAlignment;
};

#define IN_X24_OFFSET (0 * GPREG_SIZE)
#define IN_X25_OFFSET (1 * GPREG_SIZE)
#define IN_X26_OFFSET (2 * GPREG_SIZE)
#define IN_X30_OFFSET (3 * GPREG_SIZE)
#define IN_X27_OFFSET (4 * GPREG_SIZE)
// The 5th slot is unused. It's only there for alignment.
#define IN_SIZE       (6 * GPREG_SIZE)

static_assert(IN_X24_OFFSET == offsetof(IncomingProbeRecord, x24), "IN_X24_OFFSET is incorrect");
static_assert(IN_X25_OFFSET == offsetof(IncomingProbeRecord, x25), "IN_X25_OFFSET is incorrect");
static_assert(IN_X26_OFFSET == offsetof(IncomingProbeRecord, x26), "IN_X26_OFFSET is incorrect");
static_assert(IN_X30_OFFSET == offsetof(IncomingProbeRecord, x30), "IN_X23_OFFSET is incorrect");
static_assert(IN_X27_OFFSET == offsetof(IncomingProbeRecord, x27), "IN_X27_OFFSET is incorrect");
static_assert(IN_SIZE == sizeof(IncomingProbeRecord), "IN_SIZE is incorrect");
static_assert(!(sizeof(IncomingProbeRecord) & 0xf), "IncomingProbeStack must be 16-byte aligned");

struct OutgoingProbeRecord {
    UCPURegister nzcv;
    UCPURegister fpsr;
    UCPURegister x27;
    UCPURegister x28;
    UCPURegister fp;
    UCPURegister lr;
};

#define OUT_NZCV_OFFSET (0 * GPREG_SIZE)
#define OUT_FPSR_OFFSET (1 * GPREG_SIZE)
#define OUT_X27_OFFSET  (2 * GPREG_SIZE)
#define OUT_X28_OFFSET  (3 * GPREG_SIZE)
#define OUT_FP_OFFSET   (4 * GPREG_SIZE)
#define OUT_LR_OFFSET   (5 * GPREG_SIZE)
#define OUT_SIZE        (6 * GPREG_SIZE)

static_assert(OUT_NZCV_OFFSET == offsetof(OutgoingProbeRecord, nzcv), "OUT_NZCV_OFFSET is incorrect");
static_assert(OUT_FPSR_OFFSET == offsetof(OutgoingProbeRecord, fpsr), "OUT_FPSR_OFFSET is incorrect");
static_assert(OUT_X27_OFFSET == offsetof(OutgoingProbeRecord, x27), "OUT_X27_OFFSET is incorrect");
static_assert(OUT_X28_OFFSET == offsetof(OutgoingProbeRecord, x28), "OUT_X28_OFFSET is incorrect");
static_assert(OUT_FP_OFFSET == offsetof(OutgoingProbeRecord, fp), "OUT_FP_OFFSET is incorrect");
static_assert(OUT_LR_OFFSET == offsetof(OutgoingProbeRecord, lr), "OUT_LR_OFFSET is incorrect");
static_assert(OUT_SIZE == sizeof(OutgoingProbeRecord), "OUT_SIZE is incorrect");
static_assert(!(sizeof(OutgoingProbeRecord) & 0xf), "OutgoingProbeStack must be 16-byte aligned");

struct LRRestorationRecord {
    UCPURegister lr;
    UCPURegister unusedDummyToEnsureSizeIs16ByteAligned;
};

#define LR_RESTORATION_LR_OFFSET (0 * GPREG_SIZE)
#define LR_RESTORATION_SIZE      (2 * GPREG_SIZE)

static_assert(LR_RESTORATION_LR_OFFSET == offsetof(LRRestorationRecord, lr), "LR_RESTORATION_LR_OFFSET is incorrect");
static_assert(LR_RESTORATION_SIZE == sizeof(LRRestorationRecord), "LR_RESTORATION_SIZE is incorrect");
static_assert(!(sizeof(LRRestorationRecord) & 0xf), "LRRestorationRecord must be 16-byte aligned");

#if CPU(ARM64E)
#define JIT_PROBE_PC_PTR_TAG 0xeeac
#define JIT_PROBE_STACK_INITIALIZATION_FUNCTION_PTR_TAG 0x315c
static_assert(JIT_PROBE_PC_PTR_TAG == JITProbePCPtrTag);
static_assert(JIT_PROBE_STACK_INITIALIZATION_FUNCTION_PTR_TAG == JITProbeStackInitializationFunctionPtrTag);
#endif

// We use x29 and x30 instead of fp and lr because GCC's inline assembler does not recognize fp and lr.
// See https://bugs.webkit.org/show_bug.cgi?id=175512 for details.
asm (
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssemblerARM64::probe() has already generated code to store some values in an
    // IncomingProbeRecord. sp points to the IncomingProbeRecord.
    //
    // Incoming register values:
    //     x24: probe function
    //     x25: probe arg
    //     x26: scratch, was ctiMasmProbeTrampoline
    //     x30: return address

    "str       x27, [sp, #" STRINGIZE_VALUE_OF(IN_X27_OFFSET) "]" "\n"
    "mov       x26, sp" "\n"
    "sub       x27, sp, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n"
    "bic       sp, x27, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.

    "stp       x24, x25, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n" // Store the probe handler function and arg (preloaded into x24 and x25

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n"
    "mrs       x0, nzcv" "\n" // Preload nzcv.
    "stp       x2, x3, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "]" "\n"
    "stp       x4, x5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X4_OFFSET) "]" "\n"
    "mrs       x1, fpsr" "\n" // Preload fpsr.
    "stp       x6, x7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "]" "\n"
    "stp       x8, x9, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "]" "\n"

    "ldp       x2, x3, [x26, #" STRINGIZE_VALUE_OF(IN_X24_OFFSET) "]" "\n" // Preload saved x24 and x25.
    "ldp       x4, x5, [x26, #" STRINGIZE_VALUE_OF(IN_X26_OFFSET) "]" "\n" // Preload saved x26 and lr.
    "ldr       x27, [x26, #" STRINGIZE_VALUE_OF(IN_X27_OFFSET) "]" "\n"

    "add       x26, x26, #" STRINGIZE_VALUE_OF(IN_SIZE) "\n" // Compute the sp before the probe.

    "stp       x10, x11, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "]" "\n"
    "stp       x12, x13, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "]" "\n"
    "stp       x14, x15, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "]" "\n"
    "stp       x16, x17, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "]" "\n"
    "stp       x18, x19, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X18_OFFSET) "]" "\n"
    "stp       x20, x21, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X20_OFFSET) "]" "\n"
    "stp       x22, x23, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X22_OFFSET) "]" "\n"
    "stp       x2,  x3,  [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X24_OFFSET) "]" "\n" // Store saved r24 and r25 (preloaded into x2 and x3 above).
    "stp       x4,  x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X26_OFFSET) "]" "\n" // Store saved r26 (preloaded into x4) and r27.
    "stp       x28, x29, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X28_OFFSET) "]" "\n"
    "stp       x5,  x26, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n" // Save values lr and sp (original sp value computed into x26 above).

    "add       x30, x30, #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n" // The PC after the probe is at 2 instructions past the return point.
#if CPU(ARM64E)
    "movz      x27, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "pacib     x30, x27" "\n"
#endif
    "str       x30, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n" // Save a duplicate copy of return pc (in lr).
    "str       x30, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n" // Store nzcv and fpsr (preloaded into x0 and x1 above).

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

    "mov       x27, sp" "\n" // Save the Probe::State* in a callee saved register.

    // Note: we haven't changed the value of fp. Hence, it is still pointing to the frame of
    // the caller of the probe (which is what we want in order to play nice with debuggers e.g. lldb).
    "mov       x0, sp" "\n" // Set the Probe::State* arg.
    "bl      " SYMBOL_STRING(executeJSCJITProbe) "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "ldr       x1, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // Result sp.
    "add       x2, x27, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n" // End of Probe::State + buffer.
    "cmp       x1, x2" "\n"
    "bge     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "sub       x1, x1, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n"
    "bic       x1, x1, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "mov       sp, x1" "\n" // Set the new sp to protect that memory from interrupts before we copy the Probe::State.

    // Copy the Probe::State to the safe place.
    // Note: we have to copy from low address to higher address because we're moving the
    // Probe::State to a lower address.
    "mov       x5, x27" "\n"
    "mov       x6, x1" "\n"
    "add       x7, x27, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "ldp       x3, x4, [x5], #16" "\n"
    "stp       x3, x4, [x6], #16" "\n"
    "cmp       x5, x7" "\n"
    "blt     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "mov       x27, x1" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "ldr       x2, [x27, #" STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "]" "\n"
    "cbz       x2, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"

    "mov       x0, x27" "\n" // Set the Probe::State* arg.
#if CPU(ARM64E)
    "movz      lr, #" STRINGIZE_VALUE_OF(JIT_PROBE_STACK_INITIALIZATION_FUNCTION_PTR_TAG) "\n"
    "blrab     x2, lr" "\n" // Call the initializeStackFunction (loaded into x2 above).all the probe handler.
#else
    "blr       x2" "\n" // Call the initializeStackFunction (loaded into x2 above).
#endif

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    "mov       sp, x27" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning. That is except for x18.
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

    // The only way to set the pc on ARM64 (from user space) is via an indirect branch
    // or a ret, which means we'll need a free register to do so. For our purposes, lr
    // happens to be available in applications of the probe where we may want to
    // continue executing at a different location (i.e. change the pc) after the probe
    // returns. So, the ARM64 probe implementation will allow the probe handler to
    // either modify lr or pc, but not both in the same probe invocation. The probe
    // mechanism ensures that we never try to modify both lr and pc with a RELEASE_ASSERT
    // in Probe::executeJSCJITProbe().

    // Determine if the probe handler changed the pc.
    "ldr       x30, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // preload the target sp.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "cmp       x27, x28" "\n"
    "bne     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) "\n"

     // We didn't change the PC. So, let's prepare for setting a potentially new lr value.

     // 1. Make room for the LRRestorationRecord. The probe site will pop this off later.
    "sub       x30, x30, #" STRINGIZE_VALUE_OF(LR_RESTORATION_SIZE) "\n"
     // 2. Store the lp value to restore at the probe return site.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "str       x27, [x30, #" STRINGIZE_VALUE_OF(LR_RESTORATION_LR_OFFSET) "]" "\n"
     // 3. Force the return ramp to return to the probe return site.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n"
#if CPU(ARM64E)
    "movz      x28, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "autib     x27, x28" "\n"
    "mov       x28, x27" "\n"
    "xpaci     x28" "\n"
    "cmp       x28, x27" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone) "\n"
    "brk       #0xc471" "\n"
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone) ":" "\n"
#endif
    "sub       x27, x27, #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n" // The return point PC is at 2 instructions before the end of the probe.
#if CPU(ARM64E)
    "movz      x28, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "pacib     x27, x28" "\n"
#endif
    "str       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Fill in the OutgoingProbeRecord.
    "sub       x30, x30, #" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_NZCV_OFFSET) "]" "\n"
    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_X27_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n" // Set up the outgoing record so that we'll jump to the new PC.
#if CPU(ARM64E)
    "movz      x27, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "autib     x28, x27" "\n"
    "mov       x27, x28" "\n"
    "xpaci     x27" "\n"
    "cmp       x27, x28" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone2) "\n"
    "brk       #0xc471" "\n"
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone2) ":" "\n"
    "add       x27, x30, #48" "\n" // Compute sp at return point.
    "pacib     x28, x27" "\n"
#endif
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_FP_OFFSET) "]" "\n"
    "mov       sp, x30" "\n"

    // Restore the remaining registers and pop the OutgoingProbeRecord.
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
    "msr       nzcv, x27" "\n"
    "msr       fpsr, x28" "\n"
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
    "ldp       x29, x30, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
#if CPU(ARM64E)
    "retab" "\n"
#else
    "ret" "\n"
#endif
    ".previous" "\n"
);

// And now, the slower version that saves the full width of FP registers:
asm (
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampolineSIMD) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampolineSIMD) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampolineSIMD) ":" "\n"

    // MacroAssemblerARM64::probe() has already generated code to store some values in an
    // IncomingProbeRecord. sp points to the IncomingProbeRecord.
    //
    // Incoming register values:
    //     x24: probe function
    //     x25: probe arg
    //     x26: scratch, was ctiMasmProbeTrampoline
    //     x30: return address

    "str       x27, [sp, #" STRINGIZE_VALUE_OF(IN_X27_OFFSET) "]" "\n"
    "mov       x26, sp" "\n"
    "sub       x27, sp, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n"
    "bic       sp, x27, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.

    "stp       x24, x25, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n" // Store the probe handler function and arg (preloaded into x24 and x25

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "]" "\n"
    "mrs       x0, nzcv" "\n" // Preload nzcv.
    "stp       x2, x3, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "]" "\n"
    "stp       x4, x5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X4_OFFSET) "]" "\n"
    "mrs       x1, fpsr" "\n" // Preload fpsr.
    "stp       x6, x7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "]" "\n"
    "stp       x8, x9, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "]" "\n"

    "ldp       x2, x3, [x26, #" STRINGIZE_VALUE_OF(IN_X24_OFFSET) "]" "\n" // Preload saved x24 and x25.
    "ldp       x4, x5, [x26, #" STRINGIZE_VALUE_OF(IN_X26_OFFSET) "]" "\n" // Preload saved x26 and lr.
    "ldr       x27, [x26, #" STRINGIZE_VALUE_OF(IN_X27_OFFSET) "]" "\n"

    "add       x26, x26, #" STRINGIZE_VALUE_OF(IN_SIZE) "\n" // Compute the sp before the probe.

    "stp       x10, x11, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "]" "\n"
    "stp       x12, x13, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "]" "\n"
    "stp       x14, x15, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "]" "\n"
    "stp       x16, x17, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "]" "\n"
    "stp       x18, x19, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X18_OFFSET) "]" "\n"
    "stp       x20, x21, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X20_OFFSET) "]" "\n"
    "stp       x22, x23, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X22_OFFSET) "]" "\n"
    "stp       x2,  x3,  [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X24_OFFSET) "]" "\n" // Store saved r24 and r25 (preloaded into x2 and x3 above).
    "stp       x4,  x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X26_OFFSET) "]" "\n" // Store saved r26 (preloaded into x4) and r27.
    "stp       x28, x29, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X28_OFFSET) "]" "\n"
    "stp       x5,  x26, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n" // Save values lr and sp (original sp value computed into x26 above).

    "add       x30, x30, #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n" // The PC after the probe is at 2 instructions past the return point.
#if CPU(ARM64E)
    "movz      x27, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "pacib     x30, x27" "\n"
#endif
    "str       x30, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n" // Save a duplicate copy of return pc (in lr).
    "str       x30, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "stp       x0, x1, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n" // Store nzcv and fpsr (preloaded into x0 and x1 above).

    "add       x9, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_Q0_OFFSET) "\n"
    "stp       q0,   q1, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q0)) "]" "\n"
    "stp       q2,   q3, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q2)) "]" "\n"
    "stp       q4,   q5, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q4)) "]" "\n"
    "stp       q6,   q7, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q6)) "]" "\n"
    "stp       q8,   q9, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q8)) "]" "\n"
    "stp       q10, q11, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q10)) "]" "\n"
    "stp       q12, q13, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q12)) "]" "\n"
    "stp       q14, q15, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q14)) "]" "\n"
    "stp       q16, q17, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q16)) "]" "\n"
    "stp       q18, q19, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q18)) "]" "\n"
    "stp       q20, q21, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q20)) "]" "\n"
    "stp       q22, q23, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q22)) "]" "\n"
    "stp       q24, q25, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q24)) "]" "\n"
    "stp       q26, q27, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q26)) "]" "\n"
    "stp       q28, q29, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q28)) "]" "\n"
    "stp       q30, q31, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q30)) "]" "\n"

    "mov       x27, sp" "\n" // Save the Probe::State* in a callee saved register.

    // Note: we haven't changed the value of fp. Hence, it is still pointing to the frame of
    // the caller of the probe (which is what we want in order to play nice with debuggers e.g. lldb).
    "mov       x0, sp" "\n" // Set the Probe::State* arg.
    "bl      " SYMBOL_STRING(executeJSCJITProbe) "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "ldr       x1, [x27, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // Result sp.
    "add       x2, x27, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n" // End of Probe::State + buffer.
    "cmp       x1, x2" "\n"
    "bge     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafeSIMD) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "sub       x1, x1, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE) "\n"
    "bic       x1, x1, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "mov       sp, x1" "\n" // Set the new sp to protect that memory from interrupts before we copy the Probe::State.

    // Copy the Probe::State to the safe place.
    // Note: we have to copy from low address to higher address because we're moving the
    // Probe::State to a lower address.
    "mov       x5, x27" "\n"
    "mov       x6, x1" "\n"
    "add       x7, x27, #" STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoopSIMD) ":" "\n"
    "ldp       x3, x4, [x5], #16" "\n"
    "stp       x3, x4, [x6], #16" "\n"
    "cmp       x5, x7" "\n"
    "blt     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoopSIMD) "\n"

    "mov       x27, x1" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafeSIMD) ":" "\n"
    "ldr       x2, [x27, #" STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "]" "\n"
    "cbz       x2, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegistersSIMD) "\n"

    "mov       x0, x27" "\n" // Set the Probe::State* arg.
#if CPU(ARM64E)
    "movz      lr, #" STRINGIZE_VALUE_OF(JIT_PROBE_STACK_INITIALIZATION_FUNCTION_PTR_TAG) "\n"
    "blrab     x2, lr" "\n" // Call the initializeStackFunction (loaded into x2 above).all the probe handler.
#else
    "blr       x2" "\n" // Call the initializeStackFunction (loaded into x2 above).
#endif

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegistersSIMD) ":" "\n"

    "mov       sp, x27" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning. That is except for x18.
    // x18 is "reserved for the platform. Conforming software should not make use of it."
    // Hence, the JITs would not be using it, and the probe should also not be modifying it.
    // See https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html.

    "add       x9, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_Q0_OFFSET) "\n"
    "ldp       q0,   q1, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q0)) "]" "\n"
    "ldp       q2,   q3, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q2)) "]" "\n"
    "ldp       q4,   q5, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q4)) "]" "\n"
    "ldp       q6,   q7, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q6)) "]" "\n"
    "ldp       q8,   q9, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q8)) "]" "\n"
    "ldp       q10, q11, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q10)) "]" "\n"
    "ldp       q12, q13, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q12)) "]" "\n"
    "ldp       q14, q15, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q14)) "]" "\n"
    "ldp       q16, q17, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q16)) "]" "\n"
    "ldp       q18, q19, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q18)) "]" "\n"
    "ldp       q20, q21, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q20)) "]" "\n"
    "ldp       q22, q23, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q22)) "]" "\n"
    "ldp       q24, q25, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q24)) "]" "\n"
    "ldp       q26, q27, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q26)) "]" "\n"
    "ldp       q28, q29, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q28)) "]" "\n"
    "ldp       q30, q31, [x9, #" STRINGIZE_VALUE_OF(VECTOR_OFFSET(Q30)) "]" "\n"

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

    // The only way to set the pc on ARM64 (from user space) is via an indirect branch
    // or a ret, which means we'll need a free register to do so. For our purposes, lr
    // happens to be available in applications of the probe where we may want to
    // continue executing at a different location (i.e. change the pc) after the probe
    // returns. So, the ARM64 probe implementation will allow the probe handler to
    // either modify lr or pc, but not both in the same probe invocation. The probe
    // mechanism ensures that we never try to modify both lr and pc with a RELEASE_ASSERT
    // in Probe::executeJSCJITProbe().

    // Determine if the probe handler changed the pc.
    "ldr       x30, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // preload the target sp.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"
    "cmp       x27, x28" "\n"
    "bne     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEndSIMD) "\n"

     // We didn't change the PC. So, let's prepare for setting a potentially new lr value.

     // 1. Make room for the LRRestorationRecord. The probe site will pop this off later.
    "sub       x30, x30, #" STRINGIZE_VALUE_OF(LR_RESTORATION_SIZE) "\n"
     // 2. Store the lp value to restore at the probe return site.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "str       x27, [x30, #" STRINGIZE_VALUE_OF(LR_RESTORATION_LR_OFFSET) "]" "\n"
     // 3. Force the return ramp to return to the probe return site.
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "]" "\n"
#if CPU(ARM64E)
    "movz      x28, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "autib     x27, x28" "\n"
    "mov       x28, x27" "\n"
    "xpaci     x28" "\n"
    "cmp       x28, x27" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDoneSIMD) "\n"
    "brk       #0xc471" "\n"
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDoneSIMD) ":" "\n"
#endif
    "sub       x27, x27, #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n" // The return point PC is at 2 instructions before the end of the probe.
#if CPU(ARM64E)
    "movz      x28, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "pacib     x27, x28" "\n"
#endif
    "str       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEndSIMD) ":" "\n"

    // Fill in the OutgoingProbeRecord.
    "sub       x30, x30, #" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_NZCV_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_NZCV_OFFSET) "]" "\n"
    "ldp       x27, x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_X27_OFFSET) "]" "\n"
    "ldr       x28, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n" // Set up the outgoing record so that we'll jump to the new PC.
#if CPU(ARM64E)
    "movz      x27, #" STRINGIZE_VALUE_OF(JIT_PROBE_PC_PTR_TAG) "\n"
    "autib     x28, x27" "\n"
    "mov       x27, x28" "\n"
    "xpaci     x27" "\n"
    "cmp       x27, x28" "\n"
    "beq     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone2SIMD) "\n"
    "brk       #0xc471" "\n"
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolinePCAuthDone2SIMD) ":" "\n"
    "add       x27, x30, #48" "\n" // Compute sp at return point.
    "pacib     x28, x27" "\n"
#endif
    "ldr       x27, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "]" "\n"
    "stp       x27, x28, [x30, #" STRINGIZE_VALUE_OF(OUT_FP_OFFSET) "]" "\n"
    "mov       sp, x30" "\n"

    // Restore the remaining registers and pop the OutgoingProbeRecord.
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
    "msr       nzcv, x27" "\n"
    "msr       fpsr, x28" "\n"
    "ldp       x27, x28, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
    "ldp       x29, x30, [sp], #" STRINGIZE_VALUE_OF(2 * GPREG_SIZE) "\n"
#if CPU(ARM64E)
    "retab" "\n"
#else
    "ret" "\n"
#endif
    ".previous" "\n"
);
#endif // COMPILER(GCC_COMPATIBLE)

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth savedFPWidth)
{
    sub64(TrustedImm32(sizeof(IncomingProbeRecord)), sp);

    storePair64(x24, x25, sp, TrustedImm32(offsetof(IncomingProbeRecord, x24)));
    storePair64(x26, x30, sp, TrustedImm32(offsetof(IncomingProbeRecord, x26))); // Note: x30 is lr.
    if (savedFPWidth == SavedFPWidth::SaveVectors)
        move(TrustedImmPtr(tagCFunction<OperationPtrTag>(ctiMasmProbeTrampolineSIMD)), x26);
    else
        move(TrustedImmPtr(tagCFunction<OperationPtrTag>(ctiMasmProbeTrampoline)), x26);
#if CPU(ARM64E)
    assertIsTaggedWith<JITProbePtrTag>(function);
#endif
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), x24);
    move(TrustedImmPtr(arg), x25);
    call(x26, OperationPtrTag);

    // ctiMasmProbeTrampoline should have restored every register except for lr and the sp.
    load64(Address(sp, offsetof(LRRestorationRecord, lr)), lr);
    add64(TrustedImm32(sizeof(LRRestorationRecord)), sp);
}

void MacroAssemblerARM64::collectCPUFeatures()
{
#if OS(LINUX)
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        // A register for describing ARM64 CPU features are only accessible in kernel mode.
        // Thus, some kernel support is necessary to collect CPU features. In Linux, the
        // kernel passes CPU feature flags in AT_HWCAP auxiliary vector which is passed
        // when the process starts. While this may pose a bit conservative information
        // (for example, the Linux kernel may add a flag for a feature after the feature
        // is shipped and implemented in some CPUs. In that case, even if the CPU has
        // that feature, the kernel does not tell it to users.), it is a stable approach.
        // https://www.kernel.org/doc/Documentation/arm64/elf_hwcaps.txt
        uint64_t hwcaps = getauxval(AT_HWCAP);

#if !defined(HWCAP_JSCVT)
#define HWCAP_JSCVT (1 << 13)
#endif

        s_jscvtCheckState = (hwcaps & HWCAP_JSCVT) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
    });
#elif HAVE(FJCVTZS_INSTRUCTION)
    s_jscvtCheckState = CPUIDCheckState::Set;
#else
    s_jscvtCheckState = CPUIDCheckState::Clear;
#endif
}

MacroAssemblerARM64::CPUIDCheckState MacroAssemblerARM64::s_jscvtCheckState = CPUIDCheckState::NotChecked;

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM64)
