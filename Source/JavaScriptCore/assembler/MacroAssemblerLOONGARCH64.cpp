/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023-2024 Loongson Technology. All rights reserved.
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
#include "MacroAssembler.h"

#if ENABLE(ASSEMBLER) && CPU(LOONGARCH64)

#include "OperationResult.h"
#include "ProbeContext.h"
#include <wtf/InlineASM.h>

namespace JSC {

JSC_DECLARE_NOEXCEPT_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);

using namespace LOONGARCH64Registers;

#define PTR_SIZE 8
#define GPREG_SIZE 8
#define FPREG_SIZE 8

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INITIALIZE_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INITIALIZE_STACK_ARG_OFFSET (3 * PTR_SIZE)
#define PROBE_CONTEXT_STATE_SIZE (4 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET PROBE_CONTEXT_STATE_SIZE
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
#define PROBE_CPU_R12_OFFSET (PROBE_FIRST_GPREG_OFFSET + (12 * GPREG_SIZE))
#define PROBE_CPU_R13_OFFSET (PROBE_FIRST_GPREG_OFFSET + (13 * GPREG_SIZE))
#define PROBE_CPU_R14_OFFSET (PROBE_FIRST_GPREG_OFFSET + (14 * GPREG_SIZE))
#define PROBE_CPU_R15_OFFSET (PROBE_FIRST_GPREG_OFFSET + (15 * GPREG_SIZE))
#define PROBE_CPU_R16_OFFSET (PROBE_FIRST_GPREG_OFFSET + (16 * GPREG_SIZE))
#define PROBE_CPU_R17_OFFSET (PROBE_FIRST_GPREG_OFFSET + (17 * GPREG_SIZE))
#define PROBE_CPU_R18_OFFSET (PROBE_FIRST_GPREG_OFFSET + (18 * GPREG_SIZE))
#define PROBE_CPU_R19_OFFSET (PROBE_FIRST_GPREG_OFFSET + (19 * GPREG_SIZE))
#define PROBE_CPU_R20_OFFSET (PROBE_FIRST_GPREG_OFFSET + (20 * GPREG_SIZE))
#define PROBE_CPU_R21_OFFSET (PROBE_FIRST_GPREG_OFFSET + (21 * GPREG_SIZE))
#define PROBE_CPU_R22_OFFSET (PROBE_FIRST_GPREG_OFFSET + (22 * GPREG_SIZE))
#define PROBE_CPU_R23_OFFSET (PROBE_FIRST_GPREG_OFFSET + (23 * GPREG_SIZE))
#define PROBE_CPU_R24_OFFSET (PROBE_FIRST_GPREG_OFFSET + (24 * GPREG_SIZE))
#define PROBE_CPU_R25_OFFSET (PROBE_FIRST_GPREG_OFFSET + (25 * GPREG_SIZE))
#define PROBE_CPU_R26_OFFSET (PROBE_FIRST_GPREG_OFFSET + (26 * GPREG_SIZE))
#define PROBE_CPU_R27_OFFSET (PROBE_FIRST_GPREG_OFFSET + (27 * GPREG_SIZE))
#define PROBE_CPU_R28_OFFSET (PROBE_FIRST_GPREG_OFFSET + (28 * GPREG_SIZE))
#define PROBE_CPU_R29_OFFSET (PROBE_FIRST_GPREG_OFFSET + (29 * GPREG_SIZE))
#define PROBE_CPU_R30_OFFSET (PROBE_FIRST_GPREG_OFFSET + (30 * GPREG_SIZE))
#define PROBE_CPU_R31_OFFSET (PROBE_FIRST_GPREG_OFFSET + (31 * GPREG_SIZE))
#define PROBE_CPU_GPREG_ARRAY_SIZE (32 * GPREG_SIZE)

#define PROBE_FIRST_SPREG_OFFSET PROBE_CONTEXT_STATE_SIZE + PROBE_CPU_GPREG_ARRAY_SIZE
#define PROBE_CPU_PC_OFFSET (PROBE_FIRST_SPREG_OFFSET + (0 * GPREG_SIZE))
#define PROBE_CPU_SPREG_ARRAY_SIZE (1 * GPREG_SIZE)

#define PROBE_FIRST_FPREG_OFFSET PROBE_CONTEXT_STATE_SIZE + PROBE_CPU_GPREG_ARRAY_SIZE + PROBE_CPU_SPREG_ARRAY_SIZE
#define PROBE_CPU_F0_OFFSET (PROBE_FIRST_FPREG_OFFSET + (0 * FPREG_SIZE))
#define PROBE_CPU_F1_OFFSET (PROBE_FIRST_FPREG_OFFSET + (1 * FPREG_SIZE))
#define PROBE_CPU_F2_OFFSET (PROBE_FIRST_FPREG_OFFSET + (2 * FPREG_SIZE))
#define PROBE_CPU_F3_OFFSET (PROBE_FIRST_FPREG_OFFSET + (3 * FPREG_SIZE))
#define PROBE_CPU_F4_OFFSET (PROBE_FIRST_FPREG_OFFSET + (4 * FPREG_SIZE))
#define PROBE_CPU_F5_OFFSET (PROBE_FIRST_FPREG_OFFSET + (5 * FPREG_SIZE))
#define PROBE_CPU_F6_OFFSET (PROBE_FIRST_FPREG_OFFSET + (6 * FPREG_SIZE))
#define PROBE_CPU_F7_OFFSET (PROBE_FIRST_FPREG_OFFSET + (7 * FPREG_SIZE))
#define PROBE_CPU_F8_OFFSET (PROBE_FIRST_FPREG_OFFSET + (8 * FPREG_SIZE))
#define PROBE_CPU_F9_OFFSET (PROBE_FIRST_FPREG_OFFSET + (9 * FPREG_SIZE))
#define PROBE_CPU_F10_OFFSET (PROBE_FIRST_FPREG_OFFSET + (10 * FPREG_SIZE))
#define PROBE_CPU_F11_OFFSET (PROBE_FIRST_FPREG_OFFSET + (11 * FPREG_SIZE))
#define PROBE_CPU_F12_OFFSET (PROBE_FIRST_FPREG_OFFSET + (12 * FPREG_SIZE))
#define PROBE_CPU_F13_OFFSET (PROBE_FIRST_FPREG_OFFSET + (13 * FPREG_SIZE))
#define PROBE_CPU_F14_OFFSET (PROBE_FIRST_FPREG_OFFSET + (14 * FPREG_SIZE))
#define PROBE_CPU_F15_OFFSET (PROBE_FIRST_FPREG_OFFSET + (15 * FPREG_SIZE))
#define PROBE_CPU_F16_OFFSET (PROBE_FIRST_FPREG_OFFSET + (16 * FPREG_SIZE))
#define PROBE_CPU_F17_OFFSET (PROBE_FIRST_FPREG_OFFSET + (17 * FPREG_SIZE))
#define PROBE_CPU_F18_OFFSET (PROBE_FIRST_FPREG_OFFSET + (18 * FPREG_SIZE))
#define PROBE_CPU_F19_OFFSET (PROBE_FIRST_FPREG_OFFSET + (19 * FPREG_SIZE))
#define PROBE_CPU_F20_OFFSET (PROBE_FIRST_FPREG_OFFSET + (20 * FPREG_SIZE))
#define PROBE_CPU_F21_OFFSET (PROBE_FIRST_FPREG_OFFSET + (21 * FPREG_SIZE))
#define PROBE_CPU_F22_OFFSET (PROBE_FIRST_FPREG_OFFSET + (22 * FPREG_SIZE))
#define PROBE_CPU_F23_OFFSET (PROBE_FIRST_FPREG_OFFSET + (23 * FPREG_SIZE))
#define PROBE_CPU_F24_OFFSET (PROBE_FIRST_FPREG_OFFSET + (24 * FPREG_SIZE))
#define PROBE_CPU_F25_OFFSET (PROBE_FIRST_FPREG_OFFSET + (25 * FPREG_SIZE))
#define PROBE_CPU_F26_OFFSET (PROBE_FIRST_FPREG_OFFSET + (26 * FPREG_SIZE))
#define PROBE_CPU_F27_OFFSET (PROBE_FIRST_FPREG_OFFSET + (27 * FPREG_SIZE))
#define PROBE_CPU_F28_OFFSET (PROBE_FIRST_FPREG_OFFSET + (28 * FPREG_SIZE))
#define PROBE_CPU_F29_OFFSET (PROBE_FIRST_FPREG_OFFSET + (29 * FPREG_SIZE))
#define PROBE_CPU_F30_OFFSET (PROBE_FIRST_FPREG_OFFSET + (30 * FPREG_SIZE))
#define PROBE_CPU_F31_OFFSET (PROBE_FIRST_FPREG_OFFSET + (31 * FPREG_SIZE))
#define PROBE_CPU_FPREG_ARRAY_SIZE (32 * FPREG_SIZE)

#define PROBE_SIZE (PROBE_CONTEXT_STATE_SIZE + PROBE_CPU_GPREG_ARRAY_SIZE + PROBE_CPU_SPREG_ARRAY_SIZE + PROBE_CPU_FPREG_ARRAY_SIZE)

#define PROBE_SAVED_RETURN_PC_OFFSET PROBE_SIZE
#define PROBE_SAVED_RETURN_PC_SIZE (1 * GPREG_SIZE)

#define PROBE_OFFSETOF(x) offsetof(struct Probe::State, x)

static_assert(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET);
static_assert(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET);
static_assert(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INITIALIZE_STACK_FUNCTION_OFFSET);
static_assert(PROBE_OFFSETOF(initializeStackArg) == PROBE_INITIALIZE_STACK_ARG_OFFSET);

static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r0]) == PROBE_CPU_R0_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r1]) == PROBE_CPU_R1_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r2]) == PROBE_CPU_R2_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r3]) == PROBE_CPU_R3_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r4]) == PROBE_CPU_R4_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r5]) == PROBE_CPU_R5_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r6]) == PROBE_CPU_R6_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r7]) == PROBE_CPU_R7_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r8]) == PROBE_CPU_R8_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r9]) == PROBE_CPU_R9_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r10]) == PROBE_CPU_R10_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r11]) == PROBE_CPU_R11_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r12]) == PROBE_CPU_R12_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r13]) == PROBE_CPU_R13_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r14]) == PROBE_CPU_R14_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r15]) == PROBE_CPU_R15_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r16]) == PROBE_CPU_R16_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r17]) == PROBE_CPU_R17_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r18]) == PROBE_CPU_R18_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r19]) == PROBE_CPU_R19_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r20]) == PROBE_CPU_R20_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r21]) == PROBE_CPU_R21_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r22]) == PROBE_CPU_R22_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r23]) == PROBE_CPU_R23_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r24]) == PROBE_CPU_R24_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r25]) == PROBE_CPU_R25_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r26]) == PROBE_CPU_R26_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r27]) == PROBE_CPU_R27_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r28]) == PROBE_CPU_R28_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r29]) == PROBE_CPU_R29_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r30]) == PROBE_CPU_R30_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[LOONGARCH64Registers::r31]) == PROBE_CPU_R31_OFFSET);

static_assert(PROBE_OFFSETOF(cpu.sprs[LOONGARCH64Registers::pc]) == PROBE_CPU_PC_OFFSET);

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f0]) == PROBE_CPU_F0_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f1]) == PROBE_CPU_F1_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f2]) == PROBE_CPU_F2_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f3]) == PROBE_CPU_F3_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f4]) == PROBE_CPU_F4_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f5]) == PROBE_CPU_F5_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f6]) == PROBE_CPU_F6_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f7]) == PROBE_CPU_F7_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f8]) == PROBE_CPU_F8_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f9]) == PROBE_CPU_F9_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f10]) == PROBE_CPU_F10_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f11]) == PROBE_CPU_F11_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f12]) == PROBE_CPU_F12_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f13]) == PROBE_CPU_F13_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f14]) == PROBE_CPU_F14_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f15]) == PROBE_CPU_F15_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f16]) == PROBE_CPU_F16_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f17]) == PROBE_CPU_F17_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f18]) == PROBE_CPU_F18_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f19]) == PROBE_CPU_F19_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f20]) == PROBE_CPU_F20_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f21]) == PROBE_CPU_F21_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f22]) == PROBE_CPU_F22_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f23]) == PROBE_CPU_F23_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f24]) == PROBE_CPU_F24_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f25]) == PROBE_CPU_F25_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f26]) == PROBE_CPU_F26_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f27]) == PROBE_CPU_F27_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f28]) == PROBE_CPU_F28_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f29]) == PROBE_CPU_F29_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f30]) == PROBE_CPU_F30_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[LOONGARCH64Registers::f31]) == PROBE_CPU_F31_OFFSET);

static_assert(sizeof(Probe::State) == PROBE_SIZE);

#define PROBE_ALIGNED_STACK_SIZE (PROBE_SIZE + PROBE_SAVED_RETURN_PC_SIZE)
static_assert(!(PROBE_ALIGNED_STACK_SIZE & 0xf));

struct IncomingProbeRecord {
    UCPURegister r1;
    UCPURegister r25;
    UCPURegister r26;
    UCPURegister r27;
};

#define IN_R1_OFFSET  (0 * GPREG_SIZE)
#define IN_R25_OFFSET (1 * GPREG_SIZE)
#define IN_R26_OFFSET (2 * GPREG_SIZE)
#define IN_R27_OFFSET (3 * GPREG_SIZE)
#define IN_SIZE       (4 * GPREG_SIZE)

static_assert(offsetof(IncomingProbeRecord, r1) == IN_R1_OFFSET);
static_assert(offsetof(IncomingProbeRecord, r25) == IN_R25_OFFSET);
static_assert(offsetof(IncomingProbeRecord, r26) == IN_R26_OFFSET);
static_assert(offsetof(IncomingProbeRecord, r27) == IN_R27_OFFSET);
static_assert(sizeof(IncomingProbeRecord) == IN_SIZE);
static_assert(!(IN_SIZE & 0xf));

struct OutgoingProbeRecord {
    UCPURegister r25;
    UCPURegister r26;
    UCPURegister r27;
    UCPURegister r22;
    UCPURegister r1;
    UCPURegister padding;
};

#define OUT_R25_OFFSET (0 * GPREG_SIZE)
#define OUT_R26_OFFSET (1 * GPREG_SIZE)
#define OUT_R27_OFFSET (2 * GPREG_SIZE)
#define OUT_R22_OFFSET (3 * GPREG_SIZE)
#define OUT_R1_OFFSET  (4 * GPREG_SIZE)
#define OUT_SIZE       (6 * GPREG_SIZE)

static_assert(offsetof(OutgoingProbeRecord, r25) == OUT_R25_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, r26) == OUT_R26_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, r27) == OUT_R27_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, r22) == OUT_R22_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, r1) == OUT_R1_OFFSET);
static_assert(sizeof(OutgoingProbeRecord) == OUT_SIZE);
static_assert(!(OUT_SIZE & 0xf));

struct RARestorationRecord {
    UCPURegister ra;
    UCPURegister padding;
};

#define RA_RESTORATION_RA_OFFSET (0 * GPREG_SIZE)
#define RA_RESTORATION_SIZE      (2 * GPREG_SIZE)

static_assert(offsetof(RARestorationRecord, ra) == RA_RESTORATION_RA_OFFSET);
static_assert(sizeof(RARestorationRecord) == RA_RESTORATION_SIZE);
static_assert(!(RA_RESTORATION_SIZE & 0xf));

asm(
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "move $r27, $r3" "\n"
    "addi.d $r3, $r3, " STRINGIZE_VALUE_OF(-(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE)) "\n"

    "st.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "\n"
    "st.d $r26, $r3, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "\n"

    "addi.d $r26, $r27, " STRINGIZE_VALUE_OF(IN_SIZE) "\n"

    // Move over the link register
    "ld.d $r25, $r27, " STRINGIZE_VALUE_OF(IN_R1_OFFSET) "\n"
    "st.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R1_OFFSET) "\n"

    // Insert the original $r3 value
    "st.d $r26, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R3_OFFSET) "\n"

    // Also handle $r25, $r26 and $r27
    "ld.d $r25, $r27, " STRINGIZE_VALUE_OF(IN_R25_OFFSET) "\n"
    "st.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R25_OFFSET) "\n"
    "ld.d $r25, $r27, " STRINGIZE_VALUE_OF(IN_R26_OFFSET) "\n"
    "st.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R26_OFFSET) "\n"
    "ld.d $r25, $r27, " STRINGIZE_VALUE_OF(IN_R27_OFFSET) "\n"
    "st.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R27_OFFSET) "\n"

    // $r0 -- zero register, stored for completeness
    "st.d $r0, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "\n"
    // $r1 -- return address register, handled above
    // $r2 -- tp, not stored
    // $r3 -- stack pointer register, handled above
    "st.d $r4, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R4_OFFSET) "\n"
    "st.d $r5, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R5_OFFSET) "\n"
    "st.d $r6, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R6_OFFSET) "\n"
    "st.d $r7, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R7_OFFSET) "\n"
    "st.d $r8, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "\n"
    "st.d $r9, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "\n"
    "st.d $r10, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "\n"
    "st.d $r11, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "\n"
    "st.d $r12, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "\n"
    "st.d $r13, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "\n"
    "st.d $r14, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "\n"
    "st.d $r15, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "\n"
    "st.d $r16, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R16_OFFSET) "\n"
    "st.d $r17, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R17_OFFSET) "\n"
    "st.d $r18, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R18_OFFSET) "\n"
    "st.d $r19, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R19_OFFSET) "\n"
    "st.d $r20, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R20_OFFSET) "\n"
    // $r21 -- rx, not stored
    "st.d $r22, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R22_OFFSET) "\n"
    "st.d $r23, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R23_OFFSET) "\n"
    "st.d $r24, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R24_OFFSET) "\n"
    // $r25 -- incoming probe member, handled above
    // $r26 -- incoming probe member, handled above
    // $r27 -- incoming probe member, handled above
    "st.d $r28, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R28_OFFSET) "\n"
    "st.d $r29, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R29_OFFSET) "\n"
    "st.d $r30, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R30_OFFSET) "\n"
    "st.d $r31, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R31_OFFSET) "\n"

    "fst.d $f0, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "\n"
    "fst.d $f1, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F1_OFFSET) "\n"
    "fst.d $f2, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "\n"
    "fst.d $f3, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F3_OFFSET) "\n"
    "fst.d $f4, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "\n"
    "fst.d $f5, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F5_OFFSET) "\n"
    "fst.d $f6, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "\n"
    "fst.d $f7, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F7_OFFSET) "\n"
    "fst.d $f8, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "\n"
    "fst.d $f9, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F9_OFFSET) "\n"
    "fst.d $f10, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "\n"
    "fst.d $f11, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F11_OFFSET) "\n"
    "fst.d $f12, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "\n"
    "fst.d $f13, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F13_OFFSET) "\n"
    "fst.d $f14, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "\n"
    "fst.d $f15, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F15_OFFSET) "\n"
    "fst.d $f16, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "\n"
    "fst.d $f17, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F17_OFFSET) "\n"
    "fst.d $f18, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "\n"
    "fst.d $f19, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F19_OFFSET) "\n"
    "fst.d $f20, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "\n"
    "fst.d $f21, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F21_OFFSET) "\n"
    "fst.d $f22, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "\n"
    "fst.d $f23, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F23_OFFSET) "\n"
    "fst.d $f24, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "\n"
    "fst.d $f25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F25_OFFSET) "\n"
    "fst.d $f26, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "\n"
    "fst.d $f27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F27_OFFSET) "\n"
    "fst.d $f28, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "\n"
    "fst.d $f29, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F29_OFFSET) "\n"
    "fst.d $f30, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "\n"
    "fst.d $f31, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F31_OFFSET) "\n"

    "st.d $r1, $r3, " STRINGIZE_VALUE_OF(PROBE_SAVED_RETURN_PC_OFFSET) "\n"
    "st.d $r1, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "\n"

    "move $r27, $r3" "\n"

    "move $r4, $r3" "\n"
    "la $r1, " SYMBOL_STRING(executeJSCJITProbe) "\n"
    "jirl $r1, $r1, 0" "\n"

    "ld.d $r25, $r27, " STRINGIZE_VALUE_OF(PROBE_CPU_R3_OFFSET) "\n"
    "addi.d $r26, $r27, " STRINGIZE_VALUE_OF(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE) "\n"
    "bge $r25, $r26, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    "addi.d $r25, $r25, " STRINGIZE_VALUE_OF(-(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE)) "\n"
    "li.d $r19, -16" "\n"
    "and $r25, $r25, $r19" "\n"
    "move $r3, $r25" "\n"

    "move $r17, $r27" "\n"
    "move $r18, $r25" "\n"
    "addi.d $r19, $r17, " STRINGIZE_VALUE_OF(PROBE_ALIGNED_STACK_SIZE) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "ld.d $r20, $r17, 0" "\n"
    "st.d $r20, $r18, 0" "\n"
    "addi.d $r17, $r17, " STRINGIZE_VALUE_OF(GPREG_SIZE) "\n"
    "addi.d $r18, $r18, " STRINGIZE_VALUE_OF(GPREG_SIZE) "\n"
    "blt $r17, $r19, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "move $r27, $r25" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"

    // Call initializeStackFunction, if present
    "ld.d $r16, $r27, " STRINGIZE_VALUE_OF(PROBE_INITIALIZE_STACK_FUNCTION_OFFSET) "\n"
    "beqz $r16, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"
    "move $r4, $r27" "\n"
    "jirl $r1, $r16, 0" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"
    "move $r3, $r27" "\n"

    "fld.d $f0, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "\n"
    "fld.d $f1, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F1_OFFSET) "\n"
    "fld.d $f2, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "\n"
    "fld.d $f3, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F3_OFFSET) "\n"
    "fld.d $f4, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "\n"
    "fld.d $f5, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F5_OFFSET) "\n"
    "fld.d $f6, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "\n"
    "fld.d $f7, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F7_OFFSET) "\n"
    "fld.d $f8, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "\n"
    "fld.d $f9, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F9_OFFSET) "\n"
    "fld.d $f10, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "\n"
    "fld.d $f11, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F11_OFFSET) "\n"
    "fld.d $f12, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "\n"
    "fld.d $f13, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F13_OFFSET) "\n"
    "fld.d $f14, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "\n"
    "fld.d $f15, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F15_OFFSET) "\n"
    "fld.d $f16, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "\n"
    "fld.d $f17, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F17_OFFSET) "\n"
    "fld.d $f18, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "\n"
    "fld.d $f19, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F19_OFFSET) "\n"
    "fld.d $f20, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "\n"
    "fld.d $f21, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F21_OFFSET) "\n"
    "fld.d $f22, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "\n"
    "fld.d $f23, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F23_OFFSET) "\n"
    "fld.d $f24, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "\n"
    "fld.d $f25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F25_OFFSET) "\n"
    "fld.d $f26, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "\n"
    "fld.d $f27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F27_OFFSET) "\n"
    "fld.d $f28, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "\n"
    "fld.d $f29, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F29_OFFSET) "\n"
    "fld.d $f30, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "\n"
    "fld.d $f31, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_F31_OFFSET) "\n"

    // $r0 -- zero register, loaded for completeness
    "ld.d $r0, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "\n"
    // $r1 -- return address register, loaded at the end
    // $r2 -- tp, not loaded
    // $r3 -- stack pointer register, handled above
    "ld.d $r4, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R4_OFFSET) "\n"
    "ld.d $r5, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R5_OFFSET) "\n"
    "ld.d $r6, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R6_OFFSET) "\n"
    "ld.d $r7, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R7_OFFSET) "\n"
    "ld.d $r8, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "\n"
    "ld.d $r9, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "\n"
    "ld.d $r10, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "\n"
    "ld.d $r11, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "\n"
    "ld.d $r12, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "\n"
    "ld.d $r13, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "\n"
    "ld.d $r14, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "\n"
    "ld.d $r15, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "\n"
    "ld.d $r16, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R16_OFFSET) "\n"
    "ld.d $r17, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R17_OFFSET) "\n"
    "ld.d $r18, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R18_OFFSET) "\n"
    "ld.d $r19, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R19_OFFSET) "\n"
    "ld.d $r20, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R20_OFFSET) "\n"
    // $r21 -- rx, not loaded
    "ld.d $r22, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R22_OFFSET) "\n"
    "ld.d $r23, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R23_OFFSET) "\n"
    "ld.d $r24, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R24_OFFSET) "\n"
    // $r25 -- incoming probe member, handled above
    // $r26 -- incoming probe member, handled above
    // $r27 -- incoming probe member, handled above
    "ld.d $r28, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R28_OFFSET) "\n"
    "ld.d $r29, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R29_OFFSET) "\n"
    "ld.d $r30, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R30_OFFSET) "\n"
    "ld.d $r31, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R31_OFFSET) "\n"

    "ld.d $r25, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R3_OFFSET) "\n"
    "ld.d $r26, $r3, " STRINGIZE_VALUE_OF(PROBE_SAVED_RETURN_PC_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "\n"
    "bne $r26, $r27, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    "addi.d $r25, $r25, " STRINGIZE_VALUE_OF(-RA_RESTORATION_SIZE) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R1_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(RA_RESTORATION_RA_OFFSET) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    "addi.d $r25, $r25, " STRINGIZE_VALUE_OF(-OUT_SIZE) "\n"

    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R25_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(OUT_R25_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R26_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(OUT_R26_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R27_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(OUT_R27_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_R22_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(OUT_R22_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "\n"
    "st.d $r27, $r25, " STRINGIZE_VALUE_OF(OUT_R1_OFFSET) "\n"

    "move $r3, $r25" "\n"
    "ld.d $r25, $r3, " STRINGIZE_VALUE_OF(OUT_R25_OFFSET) "\n"
    "ld.d $r26, $r3, " STRINGIZE_VALUE_OF(OUT_R26_OFFSET) "\n"
    "ld.d $r27, $r3, " STRINGIZE_VALUE_OF(OUT_R27_OFFSET) "\n"
    "ld.d $r1, $r3, " STRINGIZE_VALUE_OF(OUT_R1_OFFSET) "\n"
    "addi.d $r3, $r3, " STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "jr $r1" "\n");

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth)
{
    sub64(TrustedImm32(sizeof(IncomingProbeRecord)), sp);
    store64(ra, Address(sp, offsetof(IncomingProbeRecord, r1)));
    store64(r25, Address(sp, offsetof(IncomingProbeRecord, r25)));
    store64(r26, Address(sp, offsetof(IncomingProbeRecord, r26)));
    store64(r27, Address(sp, offsetof(IncomingProbeRecord, r27)));

    move(TrustedImmPtr(tagCFunction<OperationPtrTag>(ctiMasmProbeTrampoline)), r27);
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), r25);
    move(TrustedImmPtr(arg), r26);
    call(r27, OperationPtrTag);

    load64(Address(sp, offsetof(RARestorationRecord, ra)), ra);
    add64(TrustedImm32(sizeof(RARestorationRecord)), sp);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(LOONGARCH64)
