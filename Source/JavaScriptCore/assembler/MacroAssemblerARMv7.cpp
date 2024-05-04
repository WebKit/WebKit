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

#if ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)
#include "MacroAssembler.h"

#include "ProbeContext.h"
#include <wtf/InlineASM.h>

namespace JSC {

JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);

using namespace ARMRegisters;

#if COMPILER(GCC_COMPATIBLE)

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
static_assert(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, "Probe::State::probeFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, "Probe::State::arg's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INIT_STACK_FUNCTION_OFFSET, "Probe::State::initializeStackFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackArg) == PROBE_INIT_STACK_ARG_OFFSET, "Probe::State::initializeStackArg's offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_R0_OFFSET & 0x3), "Probe::State::cpu.gprs[r0]'s offset should be 4 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r0]) == PROBE_CPU_R0_OFFSET, "Probe::State::cpu.gprs[r0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r1]) == PROBE_CPU_R1_OFFSET, "Probe::State::cpu.gprs[r1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r2]) == PROBE_CPU_R2_OFFSET, "Probe::State::cpu.gprs[r2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r3]) == PROBE_CPU_R3_OFFSET, "Probe::State::cpu.gprs[r3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r4]) == PROBE_CPU_R4_OFFSET, "Probe::State::cpu.gprs[r4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r5]) == PROBE_CPU_R5_OFFSET, "Probe::State::cpu.gprs[r5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r6]) == PROBE_CPU_R6_OFFSET, "Probe::State::cpu.gprs[r6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r7]) == PROBE_CPU_R7_OFFSET, "Probe::State::cpu.gprs[r7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r8]) == PROBE_CPU_R8_OFFSET, "Probe::State::cpu.gprs[r8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r9]) == PROBE_CPU_R9_OFFSET, "Probe::State::cpu.gprs[r9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r10]) == PROBE_CPU_R10_OFFSET, "Probe::State::cpu.gprs[r10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::r11]) == PROBE_CPU_R11_OFFSET, "Probe::State::cpu.gprs[r11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::ip]) == PROBE_CPU_IP_OFFSET, "Probe::State::cpu.gprs[ip]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::sp]) == PROBE_CPU_SP_OFFSET, "Probe::State::cpu.gprs[sp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::lr]) == PROBE_CPU_LR_OFFSET, "Probe::State::cpu.gprs[lr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[ARMRegisters::pc]) == PROBE_CPU_PC_OFFSET, "Probe::State::cpu.gprs[pc]'s offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::apsr]) == PROBE_CPU_APSR_OFFSET, "Probe::State::cpu.sprs[apsr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[ARMRegisters::fpscr]) == PROBE_CPU_FPSCR_OFFSET, "Probe::State::cpu.sprs[fpscr]'s offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_D0_OFFSET & 0x7), "Probe::State::cpu.fprs[d0]'s offset should be 8 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d0]) == PROBE_CPU_D0_OFFSET, "Probe::State::cpu.fprs[d0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d1]) == PROBE_CPU_D1_OFFSET, "Probe::State::cpu.fprs[d1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d2]) == PROBE_CPU_D2_OFFSET, "Probe::State::cpu.fprs[d2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d3]) == PROBE_CPU_D3_OFFSET, "Probe::State::cpu.fprs[d3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d4]) == PROBE_CPU_D4_OFFSET, "Probe::State::cpu.fprs[d4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d5]) == PROBE_CPU_D5_OFFSET, "Probe::State::cpu.fprs[d5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d6]) == PROBE_CPU_D6_OFFSET, "Probe::State::cpu.fprs[d6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d7]) == PROBE_CPU_D7_OFFSET, "Probe::State::cpu.fprs[d7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d8]) == PROBE_CPU_D8_OFFSET, "Probe::State::cpu.fprs[d8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d9]) == PROBE_CPU_D9_OFFSET, "Probe::State::cpu.fprs[d9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d10]) == PROBE_CPU_D10_OFFSET, "Probe::State::cpu.fprs[d10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d11]) == PROBE_CPU_D11_OFFSET, "Probe::State::cpu.fprs[d11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d12]) == PROBE_CPU_D12_OFFSET, "Probe::State::cpu.fprs[d12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d13]) == PROBE_CPU_D13_OFFSET, "Probe::State::cpu.fprs[d13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d14]) == PROBE_CPU_D14_OFFSET, "Probe::State::cpu.fprs[d14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d15]) == PROBE_CPU_D15_OFFSET, "Probe::State::cpu.fprs[d15]'s offset matches ctiMasmProbeTrampoline");

#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d16]) == PROBE_CPU_D16_OFFSET, "Probe::State::cpu.fprs[d16]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d17]) == PROBE_CPU_D17_OFFSET, "Probe::State::cpu.fprs[d17]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d18]) == PROBE_CPU_D18_OFFSET, "Probe::State::cpu.fprs[d18]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d19]) == PROBE_CPU_D19_OFFSET, "Probe::State::cpu.fprs[d19]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d20]) == PROBE_CPU_D20_OFFSET, "Probe::State::cpu.fprs[d20]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d21]) == PROBE_CPU_D21_OFFSET, "Probe::State::cpu.fprs[d21]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d22]) == PROBE_CPU_D22_OFFSET, "Probe::State::cpu.fprs[d22]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d23]) == PROBE_CPU_D23_OFFSET, "Probe::State::cpu.fprs[d23]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d24]) == PROBE_CPU_D24_OFFSET, "Probe::State::cpu.fprs[d24]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d25]) == PROBE_CPU_D25_OFFSET, "Probe::State::cpu.fprs[d25]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d26]) == PROBE_CPU_D26_OFFSET, "Probe::State::cpu.fprs[d26]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d27]) == PROBE_CPU_D27_OFFSET, "Probe::State::cpu.fprs[d27]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d28]) == PROBE_CPU_D28_OFFSET, "Probe::State::cpu.fprs[d28]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d29]) == PROBE_CPU_D29_OFFSET, "Probe::State::cpu.fprs[d29]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d30]) == PROBE_CPU_D30_OFFSET, "Probe::State::cpu.fprs[d30]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[ARMRegisters::d31]) == PROBE_CPU_D31_OFFSET, "Probe::State::cpu.fprs[d31]'s offset matches ctiMasmProbeTrampoline");
#endif // CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)

static_assert(sizeof(Probe::State) == PROBE_SIZE, "Probe::State's size matches ctiMasmProbeTrampoline");
#undef PROBE_OFFSETOF

struct IncomingRecord {
    uintptr_t lr;
    uintptr_t ip;
    uintptr_t apsr;
    uintptr_t r0;
    uintptr_t r1;
    uintptr_t r2;
};

#define IN_LR_OFFSET (0 * PTR_SIZE)
#define IN_IP_OFFSET (1 * PTR_SIZE)
#define IN_APSR_OFFSET (2 * PTR_SIZE)
#define IN_R0_OFFSET (3 * PTR_SIZE)
#define IN_R1_OFFSET (4 * PTR_SIZE)
#define IN_R2_OFFSET (5 * PTR_SIZE)
#define IN_SIZE      (6 * PTR_SIZE)

static_assert(IN_LR_OFFSET == offsetof(IncomingRecord, lr), "IN_LR_OFFSET is incorrect");
static_assert(IN_IP_OFFSET == offsetof(IncomingRecord, ip), "IN_IP_OFFSET is incorrect");
static_assert(IN_APSR_OFFSET == offsetof(IncomingRecord, apsr), "IN_APSR_OFFSET is incorrect");
static_assert(IN_R0_OFFSET == offsetof(IncomingRecord, r0), "IN_R0_OFFSET is incorrect");
static_assert(IN_R1_OFFSET == offsetof(IncomingRecord, r1), "IN_R1_OFFSET is incorrect");
static_assert(IN_R2_OFFSET == offsetof(IncomingRecord, r2), "IN_R2_OFFSET is incorrect");
static_assert(IN_SIZE == sizeof(IncomingRecord), "IN_SIZE is incorrect");

asm (
    ".text" "\n"
    ".align 2" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    ".thumb" "\n"
    ".thumb_func " THUMB_FUNC_PARAM(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssemblerARMv7::probe() has already generated code to store some values.
    // The top of stack now contains the IncomingRecord.
    //
    // Incoming register values:
    //     r0: probe function
    //     r1: probe arg
    //     r2: Probe::executeJSCJITProbe
    //     ip: scratch, was ctiMasmProbeTrampoline
    //     lr: return address

    "mov       ip, sp" "\n"
    "str       r2, [ip, #-" STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n" // Stash Probe::executeJSCJITProbe.

    "mov       r2, sp" "\n"
    "sub       r2, r2, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n"

    // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "bic       r2, r2, #0xf" "\n"
    "mov       sp, r2" "\n" // Set the sp to protect the Probe::State from interrupts before we initialize it.
    "ldr       r2, [ip, #-" STRINGIZE_VALUE_OF(PTR_SIZE) "]" "\n" // Reload Probe::executeJSCJITProbe.

    "str       r0, [sp, #" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "]" "\n"
    "str       r1, [sp, #" STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "]" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "]" "\n"

    "add       r0, ip, #" STRINGIZE_VALUE_OF(IN_SIZE) "\n"
    "str       r0, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"

    "add       lr, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R3_OFFSET) "\n"
    "stmia     lr, { r3-r11 }" "\n"

    "vmrs      lr, FPSCR" "\n"
    "str       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_FPSCR_OFFSET) "]" "\n"

    "ldr       r4, [ip, #" STRINGIZE_VALUE_OF(IN_LR_OFFSET) "]" "\n"
    "ldr       r5, [ip, #" STRINGIZE_VALUE_OF(IN_IP_OFFSET) "]" "\n"
    "ldr       r6, [ip, #" STRINGIZE_VALUE_OF(IN_APSR_OFFSET) "]" "\n"
    "ldr       r7, [ip, #" STRINGIZE_VALUE_OF(IN_R0_OFFSET) "]" "\n"
    "ldr       r8, [ip, #" STRINGIZE_VALUE_OF(IN_R1_OFFSET) "]" "\n"
    "ldr       r9, [ip, #" STRINGIZE_VALUE_OF(IN_R2_OFFSET) "]" "\n"
    "str       r4, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "str       r5, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"
    "str       r6, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_APSR_OFFSET) "]" "\n"
    "str       r7, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R0_OFFSET) "]" "\n"
    "str       r8, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R1_OFFSET) "]" "\n"
    "str       r9, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_R2_OFFSET) "]" "\n"

    "add       ip, sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_D0_OFFSET) "\n"
    "vstmia.64 ip!, { d0-d15 }" "\n"
#if CPU(ARM_NEON) || CPU(ARM_VFP_V3_D32)
    "vstmia.64 ip!, { d16-d31 }" "\n"
#endif

    // r5 is a callee saved register. We'll use it for preserving the Probe::State*.
    // https://stackoverflow.com/questions/261419/arm-to-c-calling-convention-registers-to-save#261496
    "mov       r5, sp" "\n"

    "mov       r0, sp" "\n" // the Probe::State* arg.
    "blx       r2" "\n" // Call Probe::executeJSCJITProbe.

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "ldr       r1, [r5, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n" // Result sp.
    "add       r2, r5, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n" // End of ProveContext + buffer.
    "cmp       r1, r2" "\n"
    "bge     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "sub       r1, r1, #" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) "\n"
    "bic       r1, r1, #0xf" "\n" // The ARM EABI specifies that the stack needs to be 16 byte aligned.
    "mov       sp, r1" "\n" // Set the new sp to protect that memory from interrupts before we copy the Probe::State.

    // Copy the Probe::State to the safe place.
    // Note: we have to copy from low address to higher address because we're moving the
    // Probe::State to a lower address.
    "add       r7, r5, #" STRINGIZE_VALUE_OF(PROBE_SIZE) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "ldr       r3, [r5], #4" "\n"
    "ldr       r4, [r5], #4" "\n"
    "str       r3, [r1], #4" "\n"
    "str       r4, [r1], #4" "\n"
    "cmp       r5, r7" "\n"
    "blt     " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "mov       r5, sp" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "ldr       r2, [r5, #" STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "]" "\n"
    "cbz       r2, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"

    "mov       r0, r5" "\n" // Set the Probe::State* arg.
    "blx       r2" "\n" // Call the initializeStackFunction (loaded into r2 above).

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    "mov       sp, r5" "\n" // Ensure that sp points to the Probe::State*.

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
    "msr       APSR_nzcvq, ip" "\n"
    "ldr       lr, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_LR_OFFSET) "]" "\n"
    "ldr       ip, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_IP_OFFSET) "]" "\n"

    // Restore the sp and pc.
    "ldr       sp, [sp, #" STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "]" "\n"
    "pop       { pc }" "\n"
    ".previous" "\n"
);
#endif // COMPILER(GCC_COMPATIBLE)

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth)
{
    sub32(TrustedImm32(sizeof(IncomingRecord)), sp);

    store32(lr, Address(sp, offsetof(IncomingRecord, lr)));
    store32(ip, Address(sp, offsetof(IncomingRecord, ip)));
    m_assembler.mrs(ip, apsr);
    cachedDataTempRegister().invalidate();
    store32(ip, Address(sp, offsetof(IncomingRecord, apsr)));
    store32(r0, Address(sp, offsetof(IncomingRecord, r0)));
    store32(r1, Address(sp, offsetof(IncomingRecord, r1)));
    store32(r2, Address(sp, offsetof(IncomingRecord, r2)));

    // The following may emit a T1 mov instruction, which is effectively a movs.
    // This means we must first preserve the apsr flags above first.
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), r0);
    move(TrustedImmPtr(arg), r1);
    move(TrustedImmPtr(reinterpret_cast<void*>(Probe::executeJSCJITProbe)), r2);
    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), ip);
    m_assembler.blx(ip);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)
