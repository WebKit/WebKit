/*
 * Copyright (C) 2021 Igalia S.L.
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

#if ENABLE(ASSEMBLER) && CPU(RISCV64)

#include "ProbeContext.h"
#include <wtf/InlineASM.h>

namespace JSC {

JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);

using namespace RISCV64Registers;

#define PTR_SIZE 8
#define GPREG_SIZE 8
#define FPREG_SIZE 8

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INITIALIZE_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INITIALIZE_STACK_ARG_OFFSET (3 * PTR_SIZE)
#define PROBE_CONTEXT_STATE_SIZE (4 * PTR_SIZE)

#define PROBE_FIRST_GPREG_OFFSET PROBE_CONTEXT_STATE_SIZE
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
#define PROBE_CPU_X29_OFFSET (PROBE_FIRST_GPREG_OFFSET + (29 * GPREG_SIZE))
#define PROBE_CPU_X30_OFFSET (PROBE_FIRST_GPREG_OFFSET + (30 * GPREG_SIZE))
#define PROBE_CPU_X31_OFFSET (PROBE_FIRST_GPREG_OFFSET + (31 * GPREG_SIZE))
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

static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x0]) == PROBE_CPU_X0_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x1]) == PROBE_CPU_X1_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x2]) == PROBE_CPU_X2_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x3]) == PROBE_CPU_X3_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x4]) == PROBE_CPU_X4_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x5]) == PROBE_CPU_X5_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x6]) == PROBE_CPU_X6_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x7]) == PROBE_CPU_X7_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x8]) == PROBE_CPU_X8_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x9]) == PROBE_CPU_X9_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x10]) == PROBE_CPU_X10_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x11]) == PROBE_CPU_X11_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x12]) == PROBE_CPU_X12_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x13]) == PROBE_CPU_X13_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x14]) == PROBE_CPU_X14_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x15]) == PROBE_CPU_X15_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x16]) == PROBE_CPU_X16_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x17]) == PROBE_CPU_X17_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x18]) == PROBE_CPU_X18_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x19]) == PROBE_CPU_X19_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x20]) == PROBE_CPU_X20_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x21]) == PROBE_CPU_X21_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x22]) == PROBE_CPU_X22_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x23]) == PROBE_CPU_X23_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x24]) == PROBE_CPU_X24_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x25]) == PROBE_CPU_X25_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x26]) == PROBE_CPU_X26_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x27]) == PROBE_CPU_X27_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x28]) == PROBE_CPU_X28_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x29]) == PROBE_CPU_X29_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x30]) == PROBE_CPU_X30_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.gprs[RISCV64Registers::x31]) == PROBE_CPU_X31_OFFSET);

static_assert(PROBE_OFFSETOF(cpu.sprs[RISCV64Registers::pc]) == PROBE_CPU_PC_OFFSET);

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f0]) == PROBE_CPU_F0_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f1]) == PROBE_CPU_F1_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f2]) == PROBE_CPU_F2_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f3]) == PROBE_CPU_F3_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f4]) == PROBE_CPU_F4_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f5]) == PROBE_CPU_F5_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f6]) == PROBE_CPU_F6_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f7]) == PROBE_CPU_F7_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f8]) == PROBE_CPU_F8_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f9]) == PROBE_CPU_F9_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f10]) == PROBE_CPU_F10_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f11]) == PROBE_CPU_F11_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f12]) == PROBE_CPU_F12_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f13]) == PROBE_CPU_F13_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f14]) == PROBE_CPU_F14_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f15]) == PROBE_CPU_F15_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f16]) == PROBE_CPU_F16_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f17]) == PROBE_CPU_F17_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f18]) == PROBE_CPU_F18_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f19]) == PROBE_CPU_F19_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f20]) == PROBE_CPU_F20_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f21]) == PROBE_CPU_F21_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f22]) == PROBE_CPU_F22_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f23]) == PROBE_CPU_F23_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f24]) == PROBE_CPU_F24_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f25]) == PROBE_CPU_F25_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f26]) == PROBE_CPU_F26_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f27]) == PROBE_CPU_F27_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f28]) == PROBE_CPU_F28_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f29]) == PROBE_CPU_F29_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f30]) == PROBE_CPU_F30_OFFSET);
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[RISCV64Registers::f31]) == PROBE_CPU_F31_OFFSET);

static_assert(sizeof(Probe::State) == PROBE_SIZE);

#define PROBE_ALIGNED_STACK_SIZE (PROBE_SIZE + PROBE_SAVED_RETURN_PC_SIZE)
static_assert(!(PROBE_ALIGNED_STACK_SIZE & 0xf));

struct IncomingProbeRecord {
    UCPURegister x1;
    UCPURegister x25;
    UCPURegister x26;
    UCPURegister x27;
};

#define IN_X1_OFFSET  (0 * GPREG_SIZE)
#define IN_X25_OFFSET (1 * GPREG_SIZE)
#define IN_X26_OFFSET (2 * GPREG_SIZE)
#define IN_X27_OFFSET (3 * GPREG_SIZE)
#define IN_SIZE       (4 * GPREG_SIZE)

static_assert(offsetof(IncomingProbeRecord, x1) == IN_X1_OFFSET);
static_assert(offsetof(IncomingProbeRecord, x25) == IN_X25_OFFSET);
static_assert(offsetof(IncomingProbeRecord, x26) == IN_X26_OFFSET);
static_assert(offsetof(IncomingProbeRecord, x27) == IN_X27_OFFSET);
static_assert(sizeof(IncomingProbeRecord) == IN_SIZE);
static_assert(!(IN_SIZE & 0xf));

struct OutgoingProbeRecord {
    UCPURegister x25;
    UCPURegister x26;
    UCPURegister x27;
    UCPURegister x8;
    UCPURegister x1;
    UCPURegister padding;
};

#define OUT_X25_OFFSET (0 * GPREG_SIZE)
#define OUT_X26_OFFSET (1 * GPREG_SIZE)
#define OUT_X27_OFFSET (2 * GPREG_SIZE)
#define OUT_X8_OFFSET  (3 * GPREG_SIZE)
#define OUT_X1_OFFSET  (4 * GPREG_SIZE)
#define OUT_SIZE       (6 * GPREG_SIZE)

static_assert(offsetof(OutgoingProbeRecord, x25) == OUT_X25_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, x26) == OUT_X26_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, x27) == OUT_X27_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, x8) == OUT_X8_OFFSET);
static_assert(offsetof(OutgoingProbeRecord, x1) == OUT_X1_OFFSET);
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

    "mv x27, sp" "\n"
    "addi sp, sp, " STRINGIZE_VALUE_OF(-(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE)) "\n"

    "sd x25, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(sp)" "\n"
    "sd x26, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(sp)" "\n"

    "addi x26, x27, " STRINGIZE_VALUE_OF(IN_SIZE) "\n"

    // Move over the link register
    "ld x25, " STRINGIZE_VALUE_OF(IN_X1_OFFSET) "(x27)" "\n"
    "sd x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X1_OFFSET) "(sp)" "\n"

    // Insert the original sp value
    "sd x26, " STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "(sp)" "\n"

    // Also handle x25, x26 and x27
    "ld x25, " STRINGIZE_VALUE_OF(IN_X25_OFFSET) "(x27)" "\n"
    "sd x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X25_OFFSET) "(sp)" "\n"
    "ld x25, " STRINGIZE_VALUE_OF(IN_X26_OFFSET) "(x27)" "\n"
    "sd x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X26_OFFSET) "(sp)" "\n"
    "ld x25, " STRINGIZE_VALUE_OF(IN_X27_OFFSET) "(x27)" "\n"
    "sd x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "(sp)" "\n"

    // x0 -- zero register, stored for completeness
    "sd x0, " STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "(sp)" "\n"
    // x1 -- return address register, handled above
    // x2 -- stack pointer register, handled above
    // x3 -- global pointer, not stored
    // x4 -- thread pointer, not stored
    "sd x5, " STRINGIZE_VALUE_OF(PROBE_CPU_X5_OFFSET) "(sp)" "\n"
    "sd x6, " STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "(sp)" "\n"
    "sd x7, " STRINGIZE_VALUE_OF(PROBE_CPU_X7_OFFSET) "(sp)" "\n"
    "sd x8, " STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "(sp)" "\n"
    "sd x9, " STRINGIZE_VALUE_OF(PROBE_CPU_X9_OFFSET) "(sp)" "\n"
    "sd x10, " STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "(sp)" "\n"
    "sd x11, " STRINGIZE_VALUE_OF(PROBE_CPU_X11_OFFSET) "(sp)" "\n"
    "sd x12, " STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "(sp)" "\n"
    "sd x13, " STRINGIZE_VALUE_OF(PROBE_CPU_X13_OFFSET) "(sp)" "\n"
    "sd x14, " STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "(sp)" "\n"
    "sd x15, " STRINGIZE_VALUE_OF(PROBE_CPU_X15_OFFSET) "(sp)" "\n"
    "sd x16, " STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "(sp)" "\n"
    "sd x17, " STRINGIZE_VALUE_OF(PROBE_CPU_X17_OFFSET) "(sp)" "\n"
    "sd x18, " STRINGIZE_VALUE_OF(PROBE_CPU_X18_OFFSET) "(sp)" "\n"
    "sd x19, " STRINGIZE_VALUE_OF(PROBE_CPU_X19_OFFSET) "(sp)" "\n"
    "sd x20, " STRINGIZE_VALUE_OF(PROBE_CPU_X20_OFFSET) "(sp)" "\n"
    "sd x21, " STRINGIZE_VALUE_OF(PROBE_CPU_X21_OFFSET) "(sp)" "\n"
    "sd x22, " STRINGIZE_VALUE_OF(PROBE_CPU_X22_OFFSET) "(sp)" "\n"
    "sd x23, " STRINGIZE_VALUE_OF(PROBE_CPU_X23_OFFSET) "(sp)" "\n"
    "sd x24, " STRINGIZE_VALUE_OF(PROBE_CPU_X24_OFFSET) "(sp)" "\n"
    // x25 -- incoming probe member, handled above
    // x26 -- incoming probe member, handled above
    // x27 -- incoming probe member, handled above
    "sd x28, " STRINGIZE_VALUE_OF(PROBE_CPU_X28_OFFSET) "(sp)" "\n"
    "sd x29, " STRINGIZE_VALUE_OF(PROBE_CPU_X29_OFFSET) "(sp)" "\n"
    "sd x30, " STRINGIZE_VALUE_OF(PROBE_CPU_X30_OFFSET) "(sp)" "\n"
    "sd x31, " STRINGIZE_VALUE_OF(PROBE_CPU_X31_OFFSET) "(sp)" "\n"

    "fsd f0, " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "(sp)" "\n"
    "fsd f1, " STRINGIZE_VALUE_OF(PROBE_CPU_F1_OFFSET) "(sp)" "\n"
    "fsd f2, " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "(sp)" "\n"
    "fsd f3, " STRINGIZE_VALUE_OF(PROBE_CPU_F3_OFFSET) "(sp)" "\n"
    "fsd f4, " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "(sp)" "\n"
    "fsd f5, " STRINGIZE_VALUE_OF(PROBE_CPU_F5_OFFSET) "(sp)" "\n"
    "fsd f6, " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "(sp)" "\n"
    "fsd f7, " STRINGIZE_VALUE_OF(PROBE_CPU_F7_OFFSET) "(sp)" "\n"
    "fsd f8, " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "(sp)" "\n"
    "fsd f9, " STRINGIZE_VALUE_OF(PROBE_CPU_F9_OFFSET) "(sp)" "\n"
    "fsd f10, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "(sp)" "\n"
    "fsd f11, " STRINGIZE_VALUE_OF(PROBE_CPU_F11_OFFSET) "(sp)" "\n"
    "fsd f12, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "(sp)" "\n"
    "fsd f13, " STRINGIZE_VALUE_OF(PROBE_CPU_F13_OFFSET) "(sp)" "\n"
    "fsd f14, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "(sp)" "\n"
    "fsd f15, " STRINGIZE_VALUE_OF(PROBE_CPU_F15_OFFSET) "(sp)" "\n"
    "fsd f16, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "(sp)" "\n"
    "fsd f17, " STRINGIZE_VALUE_OF(PROBE_CPU_F17_OFFSET) "(sp)" "\n"
    "fsd f18, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "(sp)" "\n"
    "fsd f19, " STRINGIZE_VALUE_OF(PROBE_CPU_F19_OFFSET) "(sp)" "\n"
    "fsd f20, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "(sp)" "\n"
    "fsd f21, " STRINGIZE_VALUE_OF(PROBE_CPU_F21_OFFSET) "(sp)" "\n"
    "fsd f22, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "(sp)" "\n"
    "fsd f23, " STRINGIZE_VALUE_OF(PROBE_CPU_F23_OFFSET) "(sp)" "\n"
    "fsd f24, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "(sp)" "\n"
    "fsd f25, " STRINGIZE_VALUE_OF(PROBE_CPU_F25_OFFSET) "(sp)" "\n"
    "fsd f26, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "(sp)" "\n"
    "fsd f27, " STRINGIZE_VALUE_OF(PROBE_CPU_F27_OFFSET) "(sp)" "\n"
    "fsd f28, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "(sp)" "\n"
    "fsd f29, " STRINGIZE_VALUE_OF(PROBE_CPU_F29_OFFSET) "(sp)" "\n"
    "fsd f30, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "(sp)" "\n"
    "fsd f31, " STRINGIZE_VALUE_OF(PROBE_CPU_F31_OFFSET) "(sp)" "\n"

    "sd ra, " STRINGIZE_VALUE_OF(PROBE_SAVED_RETURN_PC_OFFSET) "(sp)" "\n"
    "sd ra, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "(sp)" "\n"

    "mv x27, sp" "\n"

    "mv x10, sp" "\n"
    "la ra, " SYMBOL_STRING(executeJSCJITProbe) "\n"
    "jalr ra" "\n"

    "ld x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "(x27)" "\n"
    "addi x26, x27, " STRINGIZE_VALUE_OF(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE) "\n"
    "bge x25, x26, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    "addi x25, x25, " STRINGIZE_VALUE_OF(-(PROBE_ALIGNED_STACK_SIZE + OUT_SIZE)) "\n"
    "andi x25, x25, -16" "\n"
    "mv sp, x25" "\n"

    "mv x28, x27" "\n"
    "mv x29, x25" "\n"
    "addi x30, x28, " STRINGIZE_VALUE_OF(PROBE_ALIGNED_STACK_SIZE) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "ld x31, 0(x28)" "\n"
    "sd x31, 0(x29)" "\n"
    "addi x28, x28, " STRINGIZE_VALUE_OF(GPREG_SIZE) "\n"
    "addi x29, x29, " STRINGIZE_VALUE_OF(GPREG_SIZE) "\n"
    "blt x28, x30, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "mv x27, x25" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"

    // Call initializeStackFunction, if present
    "ld x26, " STRINGIZE_VALUE_OF(PROBE_INITIALIZE_STACK_FUNCTION_OFFSET) "(x27)" "\n"
    "beqz x26, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"
    "mv x10, x27" "\n"
    "jalr x26" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"
    "mv sp, x27" "\n"

    "fld f0, " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "(sp)" "\n"
    "fld f1, " STRINGIZE_VALUE_OF(PROBE_CPU_F1_OFFSET) "(sp)" "\n"
    "fld f2, " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "(sp)" "\n"
    "fld f3, " STRINGIZE_VALUE_OF(PROBE_CPU_F3_OFFSET) "(sp)" "\n"
    "fld f4, " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "(sp)" "\n"
    "fld f5, " STRINGIZE_VALUE_OF(PROBE_CPU_F5_OFFSET) "(sp)" "\n"
    "fld f6, " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "(sp)" "\n"
    "fld f7, " STRINGIZE_VALUE_OF(PROBE_CPU_F7_OFFSET) "(sp)" "\n"
    "fld f8, " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "(sp)" "\n"
    "fld f9, " STRINGIZE_VALUE_OF(PROBE_CPU_F9_OFFSET) "(sp)" "\n"
    "fld f10, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "(sp)" "\n"
    "fld f11, " STRINGIZE_VALUE_OF(PROBE_CPU_F11_OFFSET) "(sp)" "\n"
    "fld f12, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "(sp)" "\n"
    "fld f13, " STRINGIZE_VALUE_OF(PROBE_CPU_F13_OFFSET) "(sp)" "\n"
    "fld f14, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "(sp)" "\n"
    "fld f15, " STRINGIZE_VALUE_OF(PROBE_CPU_F15_OFFSET) "(sp)" "\n"
    "fld f16, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "(sp)" "\n"
    "fld f17, " STRINGIZE_VALUE_OF(PROBE_CPU_F17_OFFSET) "(sp)" "\n"
    "fld f18, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "(sp)" "\n"
    "fld f19, " STRINGIZE_VALUE_OF(PROBE_CPU_F19_OFFSET) "(sp)" "\n"
    "fld f20, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "(sp)" "\n"
    "fld f21, " STRINGIZE_VALUE_OF(PROBE_CPU_F21_OFFSET) "(sp)" "\n"
    "fld f22, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "(sp)" "\n"
    "fld f23, " STRINGIZE_VALUE_OF(PROBE_CPU_F23_OFFSET) "(sp)" "\n"
    "fld f24, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "(sp)" "\n"
    "fld f25, " STRINGIZE_VALUE_OF(PROBE_CPU_F25_OFFSET) "(sp)" "\n"
    "fld f26, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "(sp)" "\n"
    "fld f27, " STRINGIZE_VALUE_OF(PROBE_CPU_F27_OFFSET) "(sp)" "\n"
    "fld f28, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "(sp)" "\n"
    "fld f29, " STRINGIZE_VALUE_OF(PROBE_CPU_F29_OFFSET) "(sp)" "\n"
    "fld f30, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "(sp)" "\n"
    "fld f31, " STRINGIZE_VALUE_OF(PROBE_CPU_F31_OFFSET) "(sp)" "\n"

    // x0 -- zero register, loaded for completeness
    "ld x0, " STRINGIZE_VALUE_OF(PROBE_CPU_X0_OFFSET) "(sp)" "\n"
    // x1 -- return address register, loaded at the end
    // x2 -- stack pointer register, loaded at the end
    // x3 -- global pointer, not loaded
    // x4 -- thread pointer, not loaded
    "ld x5, " STRINGIZE_VALUE_OF(PROBE_CPU_X5_OFFSET) "(sp)" "\n"
    "ld x6, " STRINGIZE_VALUE_OF(PROBE_CPU_X6_OFFSET) "(sp)" "\n"
    "ld x7, " STRINGIZE_VALUE_OF(PROBE_CPU_X7_OFFSET) "(sp)" "\n"
    "ld x8, " STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "(sp)" "\n"
    "ld x9, " STRINGIZE_VALUE_OF(PROBE_CPU_X9_OFFSET) "(sp)" "\n"
    "ld x10, " STRINGIZE_VALUE_OF(PROBE_CPU_X10_OFFSET) "(sp)" "\n"
    "ld x11, " STRINGIZE_VALUE_OF(PROBE_CPU_X11_OFFSET) "(sp)" "\n"
    "ld x12, " STRINGIZE_VALUE_OF(PROBE_CPU_X12_OFFSET) "(sp)" "\n"
    "ld x13, " STRINGIZE_VALUE_OF(PROBE_CPU_X13_OFFSET) "(sp)" "\n"
    "ld x14, " STRINGIZE_VALUE_OF(PROBE_CPU_X14_OFFSET) "(sp)" "\n"
    "ld x15, " STRINGIZE_VALUE_OF(PROBE_CPU_X15_OFFSET) "(sp)" "\n"
    "ld x16, " STRINGIZE_VALUE_OF(PROBE_CPU_X16_OFFSET) "(sp)" "\n"
    "ld x17, " STRINGIZE_VALUE_OF(PROBE_CPU_X17_OFFSET) "(sp)" "\n"
    "ld x18, " STRINGIZE_VALUE_OF(PROBE_CPU_X18_OFFSET) "(sp)" "\n"
    "ld x19, " STRINGIZE_VALUE_OF(PROBE_CPU_X19_OFFSET) "(sp)" "\n"
    "ld x20, " STRINGIZE_VALUE_OF(PROBE_CPU_X20_OFFSET) "(sp)" "\n"
    "ld x21, " STRINGIZE_VALUE_OF(PROBE_CPU_X21_OFFSET) "(sp)" "\n"
    "ld x22, " STRINGIZE_VALUE_OF(PROBE_CPU_X22_OFFSET) "(sp)" "\n"
    "ld x23, " STRINGIZE_VALUE_OF(PROBE_CPU_X23_OFFSET) "(sp)" "\n"
    "ld x24, " STRINGIZE_VALUE_OF(PROBE_CPU_X24_OFFSET) "(sp)" "\n"
    "ld x28, " STRINGIZE_VALUE_OF(PROBE_CPU_X28_OFFSET) "(sp)" "\n"
    "ld x29, " STRINGIZE_VALUE_OF(PROBE_CPU_X29_OFFSET) "(sp)" "\n"
    "ld x30, " STRINGIZE_VALUE_OF(PROBE_CPU_X30_OFFSET) "(sp)" "\n"
    "ld x31, " STRINGIZE_VALUE_OF(PROBE_CPU_X31_OFFSET) "(sp)" "\n"

    "ld x25, " STRINGIZE_VALUE_OF(PROBE_CPU_X2_OFFSET) "(sp)" "\n"
    "ld x26, " STRINGIZE_VALUE_OF(PROBE_SAVED_RETURN_PC_OFFSET) "(sp)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "(sp)" "\n"
    "bne x26, x27, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    "addi x25, x25, " STRINGIZE_VALUE_OF(-RA_RESTORATION_SIZE) "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_X1_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(RA_RESTORATION_RA_OFFSET) "(x25)" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    "addi x25, x25, " STRINGIZE_VALUE_OF(-OUT_SIZE) "\n"

    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_X25_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(OUT_X25_OFFSET) "(x25)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_X26_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(OUT_X26_OFFSET) "(x25)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_X27_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(OUT_X27_OFFSET) "(x25)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_X8_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(OUT_X8_OFFSET) "(x25)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "(sp)" "\n"
    "sd x27, " STRINGIZE_VALUE_OF(OUT_X1_OFFSET) "(x25)" "\n"

    "mv sp, x25" "\n"
    "ld x25, " STRINGIZE_VALUE_OF(OUT_X25_OFFSET) "(sp)" "\n"
    "ld x26, " STRINGIZE_VALUE_OF(OUT_X26_OFFSET) "(sp)" "\n"
    "ld x27, " STRINGIZE_VALUE_OF(OUT_X27_OFFSET) "(sp)" "\n"
    "ld ra, " STRINGIZE_VALUE_OF(OUT_X1_OFFSET) "(sp)" "\n"
    "addi sp, sp, " STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "ret" "\n");

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth)
{
    sub64(TrustedImm32(sizeof(IncomingProbeRecord)), sp);
    store64(ra, Address(sp, offsetof(IncomingProbeRecord, x1)));
    store64(x25, Address(sp, offsetof(IncomingProbeRecord, x25)));
    store64(x26, Address(sp, offsetof(IncomingProbeRecord, x26)));
    store64(x27, Address(sp, offsetof(IncomingProbeRecord, x27)));

    move(TrustedImmPtr(tagCFunction<OperationPtrTag>(ctiMasmProbeTrampoline)), x27);
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), x25);
    move(TrustedImmPtr(arg), x26);
    call(x27, OperationPtrTag);

    load64(Address(sp, offsetof(RARestorationRecord, ra)), ra);
    add64(TrustedImm32(sizeof(RARestorationRecord)), sp);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
