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

#if ENABLE(ASSEMBLER) && CPU(MIPS)
#include "MacroAssembler.h"

#include "ProbeContext.h"
#include <wtf/InlineASM.h>
#include <wtf/MathExtras.h>

namespace JSC {

JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);

using namespace MIPSRegisters;

#if COMPILER(GCC_COMPATIBLE)

// The following are offsets for Probe::State fields accessed
// by the ctiMasmProbeTrampoline stub.

#define PTR_SIZE 4
#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INIT_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INIT_STACK_ARG_OFFSET (3 * PTR_SIZE)

#define PROBE_INSTRUCTIONS_AFTER_CALL 2

#define PROBE_FIRST_GPREG_OFFSET (4 * PTR_SIZE)

#define GPREG_SIZE 4
#define PROBE_CPU_ZR_OFFSET (PROBE_FIRST_GPREG_OFFSET + (0 * GPREG_SIZE))
#define PROBE_CPU_AT_OFFSET (PROBE_FIRST_GPREG_OFFSET + (1 * GPREG_SIZE))
#define PROBE_CPU_V0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (2 * GPREG_SIZE))
#define PROBE_CPU_V1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (3 * GPREG_SIZE))
#define PROBE_CPU_A0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (4 * GPREG_SIZE))
#define PROBE_CPU_A1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (5 * GPREG_SIZE))
#define PROBE_CPU_A2_OFFSET (PROBE_FIRST_GPREG_OFFSET + (6 * GPREG_SIZE))
#define PROBE_CPU_A3_OFFSET (PROBE_FIRST_GPREG_OFFSET + (7 * GPREG_SIZE))
#define PROBE_CPU_T0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (8 * GPREG_SIZE))
#define PROBE_CPU_T1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (9 * GPREG_SIZE))
#define PROBE_CPU_T2_OFFSET (PROBE_FIRST_GPREG_OFFSET + (10 * GPREG_SIZE))
#define PROBE_CPU_T3_OFFSET (PROBE_FIRST_GPREG_OFFSET + (11 * GPREG_SIZE))
#define PROBE_CPU_T4_OFFSET (PROBE_FIRST_GPREG_OFFSET + (12 * GPREG_SIZE))
#define PROBE_CPU_T5_OFFSET (PROBE_FIRST_GPREG_OFFSET + (13 * GPREG_SIZE))
#define PROBE_CPU_T6_OFFSET (PROBE_FIRST_GPREG_OFFSET + (14 * GPREG_SIZE))
#define PROBE_CPU_T7_OFFSET (PROBE_FIRST_GPREG_OFFSET + (15 * GPREG_SIZE))
#define PROBE_CPU_S0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (16 * GPREG_SIZE))
#define PROBE_CPU_S1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (17 * GPREG_SIZE))
#define PROBE_CPU_S2_OFFSET (PROBE_FIRST_GPREG_OFFSET + (18 * GPREG_SIZE))
#define PROBE_CPU_S3_OFFSET (PROBE_FIRST_GPREG_OFFSET + (19 * GPREG_SIZE))
#define PROBE_CPU_S4_OFFSET (PROBE_FIRST_GPREG_OFFSET + (20 * GPREG_SIZE))
#define PROBE_CPU_S5_OFFSET (PROBE_FIRST_GPREG_OFFSET + (21 * GPREG_SIZE))
#define PROBE_CPU_S6_OFFSET (PROBE_FIRST_GPREG_OFFSET + (22 * GPREG_SIZE))
#define PROBE_CPU_S7_OFFSET (PROBE_FIRST_GPREG_OFFSET + (23 * GPREG_SIZE))
#define PROBE_CPU_T8_OFFSET (PROBE_FIRST_GPREG_OFFSET + (24 * GPREG_SIZE))
#define PROBE_CPU_T9_OFFSET (PROBE_FIRST_GPREG_OFFSET + (25 * GPREG_SIZE))
#define PROBE_CPU_K0_OFFSET (PROBE_FIRST_GPREG_OFFSET + (26 * GPREG_SIZE))
#define PROBE_CPU_K1_OFFSET (PROBE_FIRST_GPREG_OFFSET + (27 * GPREG_SIZE))
#define PROBE_CPU_GP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (28 * GPREG_SIZE))
#define PROBE_CPU_SP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (29 * GPREG_SIZE))
#define PROBE_CPU_FP_OFFSET (PROBE_FIRST_GPREG_OFFSET + (30 * GPREG_SIZE))
#define PROBE_CPU_RA_OFFSET (PROBE_FIRST_GPREG_OFFSET + (31 * GPREG_SIZE))

#define PROBE_FIRST_SPREG_OFFSET (PROBE_FIRST_GPREG_OFFSET + (32 * GPREG_SIZE))

#define PROBE_CPU_FIR_OFFSET (PROBE_FIRST_SPREG_OFFSET + (0 * GPREG_SIZE))
#define PROBE_CPU_FCCR_OFFSET (PROBE_FIRST_SPREG_OFFSET + (25 * GPREG_SIZE))
#define PROBE_CPU_FEXR_OFFSET (PROBE_FIRST_SPREG_OFFSET + (26 * GPREG_SIZE))
#define PROBE_CPU_FENR_OFFSET (PROBE_FIRST_SPREG_OFFSET + (28 * GPREG_SIZE))
#define PROBE_CPU_FCSR_OFFSET (PROBE_FIRST_SPREG_OFFSET + (31 * GPREG_SIZE))
#define PROBE_CPU_PC_OFFSET (PROBE_FIRST_SPREG_OFFSET + (32 * GPREG_SIZE))

#define PROBE_FIRST_FPREG_OFFSET (PROBE_FIRST_SPREG_OFFSET + (34 * GPREG_SIZE))

#define FPREG_SIZE 8
#define PROBE_CPU_F0_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (0 * FPREG_SIZE))
#define PROBE_CPU_F1_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (1 * FPREG_SIZE))
#define PROBE_CPU_F2_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (2 * FPREG_SIZE))
#define PROBE_CPU_F3_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (3 * FPREG_SIZE))
#define PROBE_CPU_F4_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (4 * FPREG_SIZE))
#define PROBE_CPU_F5_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (5 * FPREG_SIZE))
#define PROBE_CPU_F6_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (6 * FPREG_SIZE))
#define PROBE_CPU_F7_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (7 * FPREG_SIZE))
#define PROBE_CPU_F8_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (8 * FPREG_SIZE))
#define PROBE_CPU_F9_OFFSET  (PROBE_FIRST_FPREG_OFFSET + (9 * FPREG_SIZE))
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

#define PROBE_SIZE (PROBE_FIRST_FPREG_OFFSET + (32 * FPREG_SIZE))

#define SAVED_PROBE_RETURN_PC_OFFSET        (PROBE_SIZE + (0 * PTR_SIZE))
#define PROBE_SIZE_PLUS_EXTRAS              (PROBE_SIZE + (2 * PTR_SIZE))
// PROBE_SIZE_PLUS_EXTRAS = PROBE_SIZE + SAVED_PROBE_RETURN_PC + padding

#define FIR    0
#define FCCR  25
#define FEXR  26
#define FENR  28
#define FCSR  31

// These ASSERTs remind you that if you change the layout of Probe::State,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct Probe::State, x)
static_assert(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, "Probe::State::probeFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, "Probe::State::arg's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INIT_STACK_FUNCTION_OFFSET, "Probe::State::initializeStackFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackArg) == PROBE_INIT_STACK_ARG_OFFSET, "Probe::State::initializeStackArg's offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_ZR_OFFSET & 0x3), "Probe::State::cpu.gprs[zero]'s offset should be 4 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::zero]) == PROBE_CPU_ZR_OFFSET, "Probe::State::cpu.gprs[zero]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::at]) == PROBE_CPU_AT_OFFSET, "Probe::State::cpu.gprs[at]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::v0]) == PROBE_CPU_V0_OFFSET, "Probe::State::cpu.gprs[v0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::v1]) == PROBE_CPU_V1_OFFSET, "Probe::State::cpu.gprs[v1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::a0]) == PROBE_CPU_A0_OFFSET, "Probe::State::cpu.gprs[a0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::a1]) == PROBE_CPU_A1_OFFSET, "Probe::State::cpu.gprs[a1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::a2]) == PROBE_CPU_A2_OFFSET, "Probe::State::cpu.gprs[a2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::a3]) == PROBE_CPU_A3_OFFSET, "Probe::State::cpu.gprs[a3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t0]) == PROBE_CPU_T0_OFFSET, "Probe::State::cpu.gprs[t0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t1]) == PROBE_CPU_T1_OFFSET, "Probe::State::cpu.gprs[t1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t2]) == PROBE_CPU_T2_OFFSET, "Probe::State::cpu.gprs[t2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t3]) == PROBE_CPU_T3_OFFSET, "Probe::State::cpu.gprs[t3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t4]) == PROBE_CPU_T4_OFFSET, "Probe::State::cpu.gprs[t4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t5]) == PROBE_CPU_T5_OFFSET, "Probe::State::cpu.gprs[t5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t6]) == PROBE_CPU_T6_OFFSET, "Probe::State::cpu.gprs[t6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t7]) == PROBE_CPU_T7_OFFSET, "Probe::State::cpu.gprs[t7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s0]) == PROBE_CPU_S0_OFFSET, "Probe::State::cpu.gprs[s0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s1]) == PROBE_CPU_S1_OFFSET, "Probe::State::cpu.gprs[s1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s2]) == PROBE_CPU_S2_OFFSET, "Probe::State::cpu.gprs[s2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s3]) == PROBE_CPU_S3_OFFSET, "Probe::State::cpu.gprs[s3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s4]) == PROBE_CPU_S4_OFFSET, "Probe::State::cpu.gprs[s4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s5]) == PROBE_CPU_S5_OFFSET, "Probe::State::cpu.gprs[s5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s6]) == PROBE_CPU_S6_OFFSET, "Probe::State::cpu.gprs[s6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::s7]) == PROBE_CPU_S7_OFFSET, "Probe::State::cpu.gprs[s7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t8]) == PROBE_CPU_T8_OFFSET, "Probe::State::cpu.gprs[t8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::t9]) == PROBE_CPU_T9_OFFSET, "Probe::State::cpu.gprs[t9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::k0]) == PROBE_CPU_K0_OFFSET, "Probe::State::cpu.gprs[k0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::k1]) == PROBE_CPU_K1_OFFSET, "Probe::State::cpu.gprs[k1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::gp]) == PROBE_CPU_GP_OFFSET, "Probe::State::cpu.gprs[gp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::sp]) == PROBE_CPU_SP_OFFSET, "Probe::State::cpu.gprs[sp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::fp]) == PROBE_CPU_FP_OFFSET, "Probe::State::cpu.gprs[fp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.gprs[MIPSRegisters::ra]) == PROBE_CPU_RA_OFFSET, "Probe::State::cpu.gprs[ra]'s offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::fir]) == PROBE_CPU_FIR_OFFSET, "Probe::State::cpu.sprs[fir]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::fccr]) == PROBE_CPU_FCCR_OFFSET, "Probe::State::cpu.sprs[fccr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::fexr]) == PROBE_CPU_FEXR_OFFSET, "Probe::State::cpu.sprs[fexr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::fenr]) == PROBE_CPU_FENR_OFFSET, "Probe::State::cpu.sprs[fenr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::fcsr]) == PROBE_CPU_FCSR_OFFSET, "Probe::State::cpu.sprs[fcsr]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.sprs[MIPSRegisters::pc]) == PROBE_CPU_PC_OFFSET, "Probe::State::cpu.sprs[pc]'s offset matches ctiMasmProbeTrampoline");

static_assert(!(PROBE_CPU_F0_OFFSET & 0x7), "Probe::State::cpu.fprs[f0]'s offset should be 8 byte aligned");

static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f0])  == PROBE_CPU_F0_OFFSET,  "Probe::State::cpu.fprs[f0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f1])  == PROBE_CPU_F1_OFFSET,  "Probe::State::cpu.fprs[f1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f2])  == PROBE_CPU_F2_OFFSET,  "Probe::State::cpu.fprs[f2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f3])  == PROBE_CPU_F3_OFFSET,  "Probe::State::cpu.fprs[f3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f4])  == PROBE_CPU_F4_OFFSET,  "Probe::State::cpu.fprs[f4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f5])  == PROBE_CPU_F5_OFFSET,  "Probe::State::cpu.fprs[f5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f6])  == PROBE_CPU_F6_OFFSET,  "Probe::State::cpu.fprs[f6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f7])  == PROBE_CPU_F7_OFFSET,  "Probe::State::cpu.fprs[f7]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f8])  == PROBE_CPU_F8_OFFSET,  "Probe::State::cpu.fprs[f8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f9])  == PROBE_CPU_F9_OFFSET,  "Probe::State::cpu.fprs[f9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f10]) == PROBE_CPU_F10_OFFSET, "Probe::State::cpu.fprs[f10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f11]) == PROBE_CPU_F11_OFFSET, "Probe::State::cpu.fprs[f11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f12]) == PROBE_CPU_F12_OFFSET, "Probe::State::cpu.fprs[f12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f13]) == PROBE_CPU_F13_OFFSET, "Probe::State::cpu.fprs[f13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f14]) == PROBE_CPU_F14_OFFSET, "Probe::State::cpu.fprs[f14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f15]) == PROBE_CPU_F15_OFFSET, "Probe::State::cpu.fprs[f15]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f16]) == PROBE_CPU_F16_OFFSET, "Probe::State::cpu.fprs[f16]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f17]) == PROBE_CPU_F17_OFFSET, "Probe::State::cpu.fprs[f17]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f18]) == PROBE_CPU_F18_OFFSET, "Probe::State::cpu.fprs[f18]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f19]) == PROBE_CPU_F19_OFFSET, "Probe::State::cpu.fprs[f19]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f20]) == PROBE_CPU_F20_OFFSET, "Probe::State::cpu.fprs[f20]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f21]) == PROBE_CPU_F21_OFFSET, "Probe::State::cpu.fprs[f21]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f22]) == PROBE_CPU_F22_OFFSET, "Probe::State::cpu.fprs[f22]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f23]) == PROBE_CPU_F23_OFFSET, "Probe::State::cpu.fprs[f23]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f24]) == PROBE_CPU_F24_OFFSET, "Probe::State::cpu.fprs[f24]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f25]) == PROBE_CPU_F25_OFFSET, "Probe::State::cpu.fprs[f25]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f26]) == PROBE_CPU_F26_OFFSET, "Probe::State::cpu.fprs[f26]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f27]) == PROBE_CPU_F27_OFFSET, "Probe::State::cpu.fprs[f27]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f28]) == PROBE_CPU_F28_OFFSET, "Probe::State::cpu.fprs[f28]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f29]) == PROBE_CPU_F29_OFFSET, "Probe::State::cpu.fprs[f29]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f30]) == PROBE_CPU_F30_OFFSET, "Probe::State::cpu.fprs[f30]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(cpu.fprs.fprs[MIPSRegisters::f31]) == PROBE_CPU_F31_OFFSET, "Probe::State::cpu.fprs[f31]'s offset matches ctiMasmProbeTrampoline");

static_assert(sizeof(Probe::State) == PROBE_SIZE, "Probe::State's size matches ctiMasmProbeTrampoline");
#undef PROBE_OFFSETOF

static_assert(MIPSRegisters::fir == FIR, "FIR matches MIPSRegisters::fir");
static_assert(MIPSRegisters::fccr == FCCR, "FCCR matches MIPSRegisters::fccr");
static_assert(MIPSRegisters::fexr == FEXR, "FEXR matches MIPSRegisters::fexr");
static_assert(MIPSRegisters::fenr == FENR, "FENR matches MIPSRegisters::fenr");
static_assert(MIPSRegisters::fcsr == FCSR, "FCSR matches MIPSRegisters::fcsr");

struct IncomingRecord {
    uintptr_t a0;
    uintptr_t a1;
    uintptr_t a2;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t ra;
};

#define IN_A0_OFFSET (0 * PTR_SIZE)
#define IN_A1_OFFSET (1 * PTR_SIZE)
#define IN_A2_OFFSET (2 * PTR_SIZE)
#define IN_S0_OFFSET (3 * PTR_SIZE)
#define IN_S1_OFFSET (4 * PTR_SIZE)
#define IN_RA_OFFSET (5 * PTR_SIZE)
#define IN_SIZE      (6 * PTR_SIZE)

static_assert(IN_A0_OFFSET == offsetof(IncomingRecord, a0), "IN_A0_OFFSET is incorrect");
static_assert(IN_A1_OFFSET == offsetof(IncomingRecord, a1), "IN_A1_OFFSET is incorrect");
static_assert(IN_A2_OFFSET == offsetof(IncomingRecord, a2), "IN_A2_OFFSET is incorrect");
static_assert(IN_S0_OFFSET == offsetof(IncomingRecord, s0), "IN_S0_OFFSET is incorrect");
static_assert(IN_S1_OFFSET == offsetof(IncomingRecord, s1), "IN_S1_OFFSET is incorrect");
static_assert(IN_RA_OFFSET == offsetof(IncomingRecord, ra), "IN_RA_OFFSET is incorrect");
static_assert(IN_SIZE == sizeof(IncomingRecord), "IN_SIZE is incorrect");

struct OutgoingRecord {
    uintptr_t fp;
    uintptr_t ra;
};

#define OUT_FP_OFFSET (0 * PTR_SIZE)
#define OUT_RA_OFFSET (1 * PTR_SIZE)
#define OUT_SIZE      (2 * PTR_SIZE)

static_assert(OUT_FP_OFFSET == offsetof(OutgoingRecord, fp), "OUT_FP_OFFSET is incorrect");
static_assert(OUT_RA_OFFSET == offsetof(OutgoingRecord, ra), "OUT_RA_OFFSET is incorrect");
static_assert(OUT_SIZE == sizeof(OutgoingRecord), "OUT_SIZE is incorrect");

struct RARestorationRecord {
    uintptr_t ra;
    uintptr_t padding;
};

#define RA_RESTORATION_RA_OFFSET (0 * PTR_SIZE)
#define RA_RESTORATION_SIZE      (2 * PTR_SIZE)

static_assert(RA_RESTORATION_RA_OFFSET == offsetof(RARestorationRecord, ra), "RA_RESTORATION_RA_OFFSET is incorrect");
static_assert(RA_RESTORATION_SIZE == sizeof(RARestorationRecord), "RA_RESTORATION_SIZE is incorrect");
static_assert(!(sizeof(RARestorationRecord) & 0x7), "RARestorationRecord must be 8-byte aligned");

asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"
    ".set push" "\n"
    ".set noreorder" "\n"
    ".set noat" "\n"

    // MacroAssemblerMIPS::probe() has already generated code to store some values in an
    // IncomingProbeRecord. sp points to the IncomingProbeRecord.
    //
    // Incoming register values:
    //     a0: probe function
    //     a1: probe arg
    //     a2: Probe::executeJSCJITProbe
    //     s0: scratch, was ctiMasmProbeTrampoline
    //     s1: scratch
    //     ra: return address

    "move      $s0, $sp" "\n"
    "addiu     $sp, $sp, -" STRINGIZE_VALUE_OF((PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE)) "\n" // Set the sp to protect the Probe::State from interrupts before we initialize it.
    "move      $s1, $sp" "\n"

    "sw        $a0, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "($sp)" "\n" // Store the probe handler function (preloaded into a0)
    "sw        $a1, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "($sp)" "\n"            // Store the probe handler arg (preloaded into a1)

    "sw        $at, " STRINGIZE_VALUE_OF(PROBE_CPU_AT_OFFSET) "($sp)" "\n"
    "sw        $v0, " STRINGIZE_VALUE_OF(PROBE_CPU_V0_OFFSET) "($sp)" "\n"
    "sw        $v1, " STRINGIZE_VALUE_OF(PROBE_CPU_V1_OFFSET) "($sp)" "\n"

    "lw        $v0, " STRINGIZE_VALUE_OF(IN_A0_OFFSET) "($s0)" "\n" // Load saved a0
    "lw        $v1, " STRINGIZE_VALUE_OF(IN_A1_OFFSET) "($s0)" "\n" // Load saved a1
    "lw        $at, " STRINGIZE_VALUE_OF(IN_A2_OFFSET) "($s0)" "\n" // Load saved a2
    "sw        $v0, " STRINGIZE_VALUE_OF(PROBE_CPU_A0_OFFSET) "($sp)" "\n" // Store saved a0
    "sw        $v1, " STRINGIZE_VALUE_OF(PROBE_CPU_A1_OFFSET) "($sp)" "\n" // Store saved a1
    "sw        $at, " STRINGIZE_VALUE_OF(PROBE_CPU_A2_OFFSET) "($sp)" "\n" // Store saved a2

    "sw        $a3, " STRINGIZE_VALUE_OF(PROBE_CPU_A3_OFFSET) "($sp)" "\n"
    "sw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_T0_OFFSET) "($sp)" "\n"
    "sw        $t1, " STRINGIZE_VALUE_OF(PROBE_CPU_T1_OFFSET) "($sp)" "\n"
    "sw        $t2, " STRINGIZE_VALUE_OF(PROBE_CPU_T2_OFFSET) "($sp)" "\n"
    "sw        $t3, " STRINGIZE_VALUE_OF(PROBE_CPU_T3_OFFSET) "($sp)" "\n"
    "sw        $t4, " STRINGIZE_VALUE_OF(PROBE_CPU_T4_OFFSET) "($sp)" "\n"
    "sw        $t5, " STRINGIZE_VALUE_OF(PROBE_CPU_T5_OFFSET) "($sp)" "\n"
    "sw        $t6, " STRINGIZE_VALUE_OF(PROBE_CPU_T6_OFFSET) "($sp)" "\n"
    "sw        $t7, " STRINGIZE_VALUE_OF(PROBE_CPU_T7_OFFSET) "($sp)" "\n"

    "lw        $v0, " STRINGIZE_VALUE_OF(IN_S0_OFFSET) "($s0)" "\n" // Load saved s0
    "lw        $v1, " STRINGIZE_VALUE_OF(IN_S1_OFFSET) "($s0)" "\n" // Load saved s1
    "sw        $v0, " STRINGIZE_VALUE_OF(PROBE_CPU_S0_OFFSET) "($sp)" "\n" // Store saved s0
    "sw        $v1, " STRINGIZE_VALUE_OF(PROBE_CPU_S1_OFFSET) "($sp)" "\n" // Store saved s1

    "sw        $s2, " STRINGIZE_VALUE_OF(PROBE_CPU_S2_OFFSET) "($sp)" "\n"
    "sw        $s3, " STRINGIZE_VALUE_OF(PROBE_CPU_S3_OFFSET) "($sp)" "\n"
    "sw        $s4, " STRINGIZE_VALUE_OF(PROBE_CPU_S4_OFFSET) "($sp)" "\n"
    "sw        $s5, " STRINGIZE_VALUE_OF(PROBE_CPU_S5_OFFSET) "($sp)" "\n"
    "sw        $s6, " STRINGIZE_VALUE_OF(PROBE_CPU_S6_OFFSET) "($sp)" "\n"
    "sw        $s7, " STRINGIZE_VALUE_OF(PROBE_CPU_S7_OFFSET) "($sp)" "\n"
    "sw        $t8, " STRINGIZE_VALUE_OF(PROBE_CPU_T8_OFFSET) "($sp)" "\n"
    "sw        $t9, " STRINGIZE_VALUE_OF(PROBE_CPU_T9_OFFSET) "($sp)" "\n"
    "sw        $k0, " STRINGIZE_VALUE_OF(PROBE_CPU_K0_OFFSET) "($sp)" "\n"
    "sw        $k1, " STRINGIZE_VALUE_OF(PROBE_CPU_K1_OFFSET) "($sp)" "\n"
    "sw        $gp, " STRINGIZE_VALUE_OF(PROBE_CPU_GP_OFFSET) "($sp)" "\n"
    "sw        $fp, " STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "($sp)" "\n"

    "lw        $v0, " STRINGIZE_VALUE_OF(IN_RA_OFFSET) "($s0)" "\n" // Load saved ra
    "addiu     $s0, $s0, " STRINGIZE_VALUE_OF(IN_SIZE) "\n" // Compute the sp before the probe
    "sw        $v0, " STRINGIZE_VALUE_OF(PROBE_CPU_RA_OFFSET) "($sp)" "\n" // Store saved ra
    "sw        $s0, " STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "($sp)" "\n" // Store original sp computed into s0

    "sw        $ra, " STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "($sp)" "\n" // Save a duplicate copy of return pc (in ra)
    "addiu     $ra, $ra, " STRINGIZE_VALUE_OF(PROBE_INSTRUCTIONS_AFTER_CALL * PTR_SIZE) "\n" // The PC after the probe is at 2 instructions past the return point.
    "sw        $ra, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "($sp)" "\n"

    "cfc1      $t0,  $" STRINGIZE_VALUE_OF(FIR) "\n"
    "sw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FIR_OFFSET) "($sp)" "\n"
    "cfc1      $t0,  $" STRINGIZE_VALUE_OF(FCCR) "\n"
    "sw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FCCR_OFFSET) "($sp)" "\n"
    "cfc1      $t0,  $" STRINGIZE_VALUE_OF(FEXR) "\n"
    "sw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FEXR_OFFSET) "($sp)" "\n"
    "cfc1      $t0,  $" STRINGIZE_VALUE_OF(FENR) "\n"
    "sw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FENR_OFFSET) "($sp)" "\n"
    "cfc1      $t0,  $" STRINGIZE_VALUE_OF(FCSR) "\n"
    "sw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FCSR_OFFSET) "($sp)" "\n"

    "sdc1      $f0,  " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "($sp)" "\n"
    "sdc1      $f2,  " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "($sp)" "\n"
    "sdc1      $f4,  " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "($sp)" "\n"
    "sdc1      $f6,  " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "($sp)" "\n"
    "sdc1      $f8,  " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "($sp)" "\n"
    "sdc1      $f10, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "($sp)" "\n"
    "sdc1      $f12, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "($sp)" "\n"
    "sdc1      $f14, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "($sp)" "\n"
    "sdc1      $f16, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "($sp)" "\n"
    "sdc1      $f18, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "($sp)" "\n"
    "sdc1      $f20, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "($sp)" "\n"
    "sdc1      $f22, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "($sp)" "\n"
    "sdc1      $f24, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "($sp)" "\n"
    "sdc1      $f26, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "($sp)" "\n"
    "sdc1      $f28, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "($sp)" "\n"
    "sdc1      $f30, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "($sp)" "\n"

    "move      $a0, $sp" "\n" // Set the Probe::State* arg.
    "addiu     $sp, $sp, -16" "\n" // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    "move      $t9, $a2" "\n" // Probe::executeJSCJITProbe()
    "jalr      $t9" "\n" // Call the probe handler.
    "nop" "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "lw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "($s1)" "\n" // Result sp.
    "addiu     $t1, $s1, " STRINGIZE_VALUE_OF((PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE)) "\n" // End of Probe::State + buffer.
    "sltu      $t2, $t0, $t1" "\n"
    "beqz      $t2, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"
    "nop" "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "addiu     $sp, $t0, -" STRINGIZE_VALUE_OF((PROBE_SIZE_PLUS_EXTRAS + OUT_SIZE)) "\n" // Set the new sp to protect that memory from interrupts before we copy the Probe::State.

    // Copy the Probe::State to the safe place.
    // Note: we have to copy from low address to higher address because we're moving the
    // Probe::State to a lower address.
    "move      $t0, $s1" "\n"
    "move      $t1, $sp" "\n"
    "addiu     $t2, $s1, " STRINGIZE_VALUE_OF(PROBE_SIZE_PLUS_EXTRAS) "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "lw        $t3, 0($t0)" "\n"
    "lw        $t4, 4($t0)" "\n"
    "sw        $t3, 0($t1)" "\n"
    "sw        $t4, 4($t1)" "\n"
    "addiu     $t0, $t0, 8" "\n"
    "addiu     $t1, $t1, 8" "\n"
    "bne       $t0, $t2, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"
    "nop" "\n"

    "move      $s1, $sp" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "lw        $t9, " STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "($s1)" "\n"
    "beqz      $t9, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"
    "nop" "\n"

    "move      $a0, $s1" "\n" // Set the Probe::State* arg.
    "jalr      $t9" "\n" // Call the initializeStackFunction (loaded into t9 above).
    "nop" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    "move      $sp, $s1" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning, except for zero, k0 and k1.

    "lw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FIR_OFFSET) "($sp)" "\n"
    "ctc1      $t0,  $" STRINGIZE_VALUE_OF(FIR) "\n"
    "lw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FCCR_OFFSET) "($sp)" "\n"
    "ctc1      $t0,  $" STRINGIZE_VALUE_OF(FCCR) "\n"
    "lw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FEXR_OFFSET) "($sp)" "\n"
    "ctc1      $t0,  $" STRINGIZE_VALUE_OF(FEXR) "\n"
    "lw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FENR_OFFSET) "($sp)" "\n"
    "ctc1      $t0,  $" STRINGIZE_VALUE_OF(FENR) "\n"
    "lw        $t0,  " STRINGIZE_VALUE_OF(PROBE_CPU_FCSR_OFFSET) "($sp)" "\n"
    "ctc1      $t0,  $" STRINGIZE_VALUE_OF(FCSR) "\n"

    "ldc1      $f0,  " STRINGIZE_VALUE_OF(PROBE_CPU_F0_OFFSET) "($sp)" "\n"
    "ldc1      $f2,  " STRINGIZE_VALUE_OF(PROBE_CPU_F2_OFFSET) "($sp)" "\n"
    "ldc1      $f4,  " STRINGIZE_VALUE_OF(PROBE_CPU_F4_OFFSET) "($sp)" "\n"
    "ldc1      $f6,  " STRINGIZE_VALUE_OF(PROBE_CPU_F6_OFFSET) "($sp)" "\n"
    "ldc1      $f8,  " STRINGIZE_VALUE_OF(PROBE_CPU_F8_OFFSET) "($sp)" "\n"
    "ldc1      $f10, " STRINGIZE_VALUE_OF(PROBE_CPU_F10_OFFSET) "($sp)" "\n"
    "ldc1      $f12, " STRINGIZE_VALUE_OF(PROBE_CPU_F12_OFFSET) "($sp)" "\n"
    "ldc1      $f14, " STRINGIZE_VALUE_OF(PROBE_CPU_F14_OFFSET) "($sp)" "\n"
    "ldc1      $f16, " STRINGIZE_VALUE_OF(PROBE_CPU_F16_OFFSET) "($sp)" "\n"
    "ldc1      $f18, " STRINGIZE_VALUE_OF(PROBE_CPU_F18_OFFSET) "($sp)" "\n"
    "ldc1      $f20, " STRINGIZE_VALUE_OF(PROBE_CPU_F20_OFFSET) "($sp)" "\n"
    "ldc1      $f22, " STRINGIZE_VALUE_OF(PROBE_CPU_F22_OFFSET) "($sp)" "\n"
    "ldc1      $f24, " STRINGIZE_VALUE_OF(PROBE_CPU_F24_OFFSET) "($sp)" "\n"
    "ldc1      $f26, " STRINGIZE_VALUE_OF(PROBE_CPU_F26_OFFSET) "($sp)" "\n"
    "ldc1      $f28, " STRINGIZE_VALUE_OF(PROBE_CPU_F28_OFFSET) "($sp)" "\n"
    "ldc1      $f30, " STRINGIZE_VALUE_OF(PROBE_CPU_F30_OFFSET) "($sp)" "\n"

    "lw        $at, " STRINGIZE_VALUE_OF(PROBE_CPU_AT_OFFSET) "($sp)" "\n"
    "lw        $v0, " STRINGIZE_VALUE_OF(PROBE_CPU_V0_OFFSET) "($sp)" "\n"
    "lw        $v1, " STRINGIZE_VALUE_OF(PROBE_CPU_V1_OFFSET) "($sp)" "\n"
    "lw        $a0, " STRINGIZE_VALUE_OF(PROBE_CPU_A0_OFFSET) "($sp)" "\n"
    "lw        $a1, " STRINGIZE_VALUE_OF(PROBE_CPU_A1_OFFSET) "($sp)" "\n"
    "lw        $a2, " STRINGIZE_VALUE_OF(PROBE_CPU_A2_OFFSET) "($sp)" "\n"
    "lw        $a3, " STRINGIZE_VALUE_OF(PROBE_CPU_A3_OFFSET) "($sp)" "\n"
    "lw        $t2, " STRINGIZE_VALUE_OF(PROBE_CPU_T2_OFFSET) "($sp)" "\n"
    "lw        $t3, " STRINGIZE_VALUE_OF(PROBE_CPU_T3_OFFSET) "($sp)" "\n"
    "lw        $t4, " STRINGIZE_VALUE_OF(PROBE_CPU_T4_OFFSET) "($sp)" "\n"
    "lw        $t5, " STRINGIZE_VALUE_OF(PROBE_CPU_T5_OFFSET) "($sp)" "\n"
    "lw        $t6, " STRINGIZE_VALUE_OF(PROBE_CPU_T6_OFFSET) "($sp)" "\n"
    "lw        $t7, " STRINGIZE_VALUE_OF(PROBE_CPU_T7_OFFSET) "($sp)" "\n"
    "lw        $s0, " STRINGIZE_VALUE_OF(PROBE_CPU_S0_OFFSET) "($sp)" "\n"
    "lw        $s1, " STRINGIZE_VALUE_OF(PROBE_CPU_S1_OFFSET) "($sp)" "\n"
    "lw        $s2, " STRINGIZE_VALUE_OF(PROBE_CPU_S2_OFFSET) "($sp)" "\n"
    "lw        $s3, " STRINGIZE_VALUE_OF(PROBE_CPU_S3_OFFSET) "($sp)" "\n"
    "lw        $s4, " STRINGIZE_VALUE_OF(PROBE_CPU_S4_OFFSET) "($sp)" "\n"
    "lw        $s5, " STRINGIZE_VALUE_OF(PROBE_CPU_S5_OFFSET) "($sp)" "\n"
    "lw        $s6, " STRINGIZE_VALUE_OF(PROBE_CPU_S6_OFFSET) "($sp)" "\n"
    "lw        $s7, " STRINGIZE_VALUE_OF(PROBE_CPU_S7_OFFSET) "($sp)" "\n"
    "lw        $t8, " STRINGIZE_VALUE_OF(PROBE_CPU_T8_OFFSET) "($sp)" "\n"
    "lw        $t9, " STRINGIZE_VALUE_OF(PROBE_CPU_T9_OFFSET) "($sp)" "\n"
    "lw        $gp, " STRINGIZE_VALUE_OF(PROBE_CPU_GP_OFFSET) "($sp)" "\n"

    // Remaining registers to restore are: t0, t1, fp, ra, sp, and pc.

    // The only way to set the pc on MIPS (from user space) is via an indirect branch
    // which means we'll need a free register to do so. For our purposes, ra
    // happens to be available in applications of the probe where we may want to
    // continue executing at a different location (i.e. change the pc) after the probe
    // returns. So, the MIPS probe implementation will allow the probe handler to
    // either modify ra or pc, but not both in the same probe invocation. The probe
    // mechanism ensures that we never try to modify both ra and pc with a RELEASE_ASSERT
    // in Probe::executeJSCJITProbe().

    // Determine if the probe handler changed the pc.
    "lw        $ra, " STRINGIZE_VALUE_OF(PROBE_CPU_SP_OFFSET) "($sp)" "\n" // preload the target sp.
    "lw        $t0, " STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "($sp)" "\n"
    "lw        $t1, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "($sp)" "\n"
    "addiu     $t0, $t0, " STRINGIZE_VALUE_OF(PROBE_INSTRUCTIONS_AFTER_CALL * PTR_SIZE) "\n"
    "bne       $t0, $t1, " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) "\n"
    "nop" "\n"

     // We didn't change the PC. So, let's prepare for setting a potentially new ra value.

     // 1. Make room for the RARestorationRecord. The probe site will pop this off later.
    "addiu     $ra, $ra, -" STRINGIZE_VALUE_OF(RA_RESTORATION_SIZE) "\n"
     // 2. Store the lp value to restore at the probe return site.
    "lw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_RA_OFFSET) "($sp)" "\n"
    "sw        $t0, " STRINGIZE_VALUE_OF(RA_RESTORATION_RA_OFFSET) "($ra)" "\n"
     // 3. Force the return ramp to return to the probe return site.
    "lw        $t0, " STRINGIZE_VALUE_OF(SAVED_PROBE_RETURN_PC_OFFSET) "($sp)" "\n"
    "sw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "($sp)" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Fill in the OutgoingProbeRecord.
    "addiu     $ra, $ra, -" STRINGIZE_VALUE_OF(OUT_SIZE) "\n"

    "lw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_FP_OFFSET) "($sp)" "\n"
    "lw        $t1, " STRINGIZE_VALUE_OF(PROBE_CPU_PC_OFFSET) "($sp)" "\n" // Set up the outgoing record so that we'll jump to the new PC.
    "sw        $t0, " STRINGIZE_VALUE_OF(OUT_FP_OFFSET) "($ra)" "\n"
    "sw        $t1, " STRINGIZE_VALUE_OF(OUT_RA_OFFSET) "($ra)" "\n"
    "lw        $t0, " STRINGIZE_VALUE_OF(PROBE_CPU_T0_OFFSET) "($sp)" "\n"
    "lw        $t1, " STRINGIZE_VALUE_OF(PROBE_CPU_T1_OFFSET) "($sp)" "\n"
    "move      $sp, $ra" "\n"

    // Restore the remaining registers.
    "lw        $fp, " STRINGIZE_VALUE_OF(OUT_FP_OFFSET) "($sp)" "\n"
    "lw        $ra, " STRINGIZE_VALUE_OF(OUT_RA_OFFSET) "($sp)" "\n"
    "addiu     $sp, $sp, " STRINGIZE_VALUE_OF(OUT_SIZE) "\n"
    "jr        $ra" "\n"
    "nop" "\n"
    ".set pop" "\n"
    ".previous" "\n"
);
#endif // COMPILER(GCC_COMPATIBLE)

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth)
{
    sub32(TrustedImm32(sizeof(IncomingRecord)), sp);
    store32(a0, Address(sp, offsetof(IncomingRecord, a0)));
    store32(a1, Address(sp, offsetof(IncomingRecord, a1)));
    store32(a2, Address(sp, offsetof(IncomingRecord, a2)));
    store32(s0, Address(sp, offsetof(IncomingRecord, s0)));
    store32(s1, Address(sp, offsetof(IncomingRecord, s1)));
    store32(ra, Address(sp, offsetof(IncomingRecord, ra)));
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), a0);
    move(TrustedImmPtr(arg), a1);
    move(TrustedImmPtr(reinterpret_cast<void*>(Probe::executeJSCJITProbe)), a2);
    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), s0);
    m_assembler.jalr(s0);
    m_assembler.nop();
    // If you change the following instructions, be sure to update PROBE_INSTRUCTIONS_AFTER_CALL as well
    load32(Address(sp, offsetof(RARestorationRecord, ra)), ra);
    add32(TrustedImm32(sizeof(RARestorationRecord)), sp);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(MIPS)
