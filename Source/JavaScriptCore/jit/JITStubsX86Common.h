/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JITStubsX86Common_h
#define JITStubsX86Common_h

#include "MacroAssembler.h"

#if !CPU(X86) && !CPU(X86_64)
#error "JITStubsX86Common.h should only be #included if CPU(X86) || CPU(X86_64)"
#endif

namespace JSC {

#if COMPILER(GCC)

#if USE(MASM_PROBE)
// The following are offsets for MacroAssembler::ProbeContext fields accessed
// by the ctiMasmProbeTrampoline stub.

#if CPU(X86)
#define PTR_SIZE 4
#else // CPU(X86_64)
#define PTR_SIZE 8
#endif

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG1_OFFSET (1 * PTR_SIZE)
#define PROBE_ARG2_OFFSET (2 * PTR_SIZE)

#define PROBE_CPU_EAX_OFFSET (4 * PTR_SIZE)
#define PROBE_CPU_EBX_OFFSET (5 * PTR_SIZE)
#define PROBE_CPU_ECX_OFFSET (6 * PTR_SIZE)
#define PROBE_CPU_EDX_OFFSET (7 * PTR_SIZE)
#define PROBE_CPU_ESI_OFFSET (8 * PTR_SIZE)
#define PROBE_CPU_EDI_OFFSET (9 * PTR_SIZE)
#define PROBE_CPU_EBP_OFFSET (10 * PTR_SIZE)
#define PROBE_CPU_ESP_OFFSET (11 * PTR_SIZE)

#if CPU(X86)
#define PROBE_FIRST_SPECIAL_OFFSET (12 * PTR_SIZE)
#else // CPU(X86_64)
#define PROBE_CPU_R8_OFFSET (12 * PTR_SIZE)
#define PROBE_CPU_R9_OFFSET (13 * PTR_SIZE)
#define PROBE_CPU_R10_OFFSET (14 * PTR_SIZE)
#define PROBE_CPU_R11_OFFSET (15 * PTR_SIZE)
#define PROBE_CPU_R12_OFFSET (16 * PTR_SIZE)
#define PROBE_CPU_R13_OFFSET (17 * PTR_SIZE)
#define PROBE_CPU_R14_OFFSET (18 * PTR_SIZE)
#define PROBE_CPU_R15_OFFSET (19 * PTR_SIZE)
#define PROBE_FIRST_SPECIAL_OFFSET (20 * PTR_SIZE)
#endif // CPU(X86_64)

#define PROBE_CPU_EIP_OFFSET (PROBE_FIRST_SPECIAL_OFFSET + (0 * PTR_SIZE))
#define PROBE_CPU_EFLAGS_OFFSET (PROBE_FIRST_SPECIAL_OFFSET + (1 * PTR_SIZE))

#if CPU(X86)
#define PROBE_FIRST_XMM_OFFSET (PROBE_FIRST_SPECIAL_OFFSET + (4 * PTR_SIZE)) // After padding.
#else // CPU(X86_64)
#define PROBE_FIRST_XMM_OFFSET (PROBE_FIRST_SPECIAL_OFFSET + (2 * PTR_SIZE)) // After padding.
#endif // CPU(X86_64)

#define XMM_SIZE 16
#define PROBE_CPU_XMM0_OFFSET (PROBE_FIRST_XMM_OFFSET + (0 * XMM_SIZE))
#define PROBE_CPU_XMM1_OFFSET (PROBE_FIRST_XMM_OFFSET + (1 * XMM_SIZE))
#define PROBE_CPU_XMM2_OFFSET (PROBE_FIRST_XMM_OFFSET + (2 * XMM_SIZE))
#define PROBE_CPU_XMM3_OFFSET (PROBE_FIRST_XMM_OFFSET + (3 * XMM_SIZE))
#define PROBE_CPU_XMM4_OFFSET (PROBE_FIRST_XMM_OFFSET + (4 * XMM_SIZE))
#define PROBE_CPU_XMM5_OFFSET (PROBE_FIRST_XMM_OFFSET + (5 * XMM_SIZE))
#define PROBE_CPU_XMM6_OFFSET (PROBE_FIRST_XMM_OFFSET + (6 * XMM_SIZE))
#define PROBE_CPU_XMM7_OFFSET (PROBE_FIRST_XMM_OFFSET + (7 * XMM_SIZE))

#define PROBE_SIZE (PROBE_CPU_XMM7_OFFSET + XMM_SIZE)

// These ASSERTs remind you that if you change the layout of ProbeContext,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct MacroAssembler::ProbeContext, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeContext_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg1) == PROBE_ARG1_OFFSET, ProbeContext_arg1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg2) == PROBE_ARG2_OFFSET, ProbeContext_arg2_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.eax) == PROBE_CPU_EAX_OFFSET, ProbeContext_cpu_eax_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.ecx) == PROBE_CPU_ECX_OFFSET, ProbeContext_cpu_ecx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.edx) == PROBE_CPU_EDX_OFFSET, ProbeContext_cpu_edx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.ebx) == PROBE_CPU_EBX_OFFSET, ProbeContext_cpu_ebx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.esp) == PROBE_CPU_ESP_OFFSET, ProbeContext_cpu_esp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.ebp) == PROBE_CPU_EBP_OFFSET, ProbeContext_cpu_ebp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.esi) == PROBE_CPU_ESI_OFFSET, ProbeContext_cpu_esi_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.edi) == PROBE_CPU_EDI_OFFSET, ProbeContext_cpu_edi_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.eip) == PROBE_CPU_EIP_OFFSET, ProbeContext_cpu_eip_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.eflags) == PROBE_CPU_EFLAGS_OFFSET, ProbeContext_cpu_eflags_offset_matches_ctiMasmProbeTrampoline);

#if CPU(X86_64)
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r8) == PROBE_CPU_R8_OFFSET, ProbeContext_cpu_r8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r9) == PROBE_CPU_R9_OFFSET, ProbeContext_cpu_r9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r10) == PROBE_CPU_R10_OFFSET, ProbeContext_cpu_r10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r11) == PROBE_CPU_R11_OFFSET, ProbeContext_cpu_r11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r12) == PROBE_CPU_R12_OFFSET, ProbeContext_cpu_r12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r13) == PROBE_CPU_R13_OFFSET, ProbeContext_cpu_r13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r14) == PROBE_CPU_R14_OFFSET, ProbeContext_cpu_r14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.r15) == PROBE_CPU_R15_OFFSET, ProbeContext_cpu_r15_offset_matches_ctiMasmProbeTrampoline);
#endif // CPU(X86_64)

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm0) == PROBE_CPU_XMM0_OFFSET, ProbeContext_cpu_xmm0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm1) == PROBE_CPU_XMM1_OFFSET, ProbeContext_cpu_xmm1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm2) == PROBE_CPU_XMM2_OFFSET, ProbeContext_cpu_xmm2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm3) == PROBE_CPU_XMM3_OFFSET, ProbeContext_cpu_xmm3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm4) == PROBE_CPU_XMM4_OFFSET, ProbeContext_cpu_xmm4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm5) == PROBE_CPU_XMM5_OFFSET, ProbeContext_cpu_xmm5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm6) == PROBE_CPU_XMM6_OFFSET, ProbeContext_cpu_xmm6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.xmm7) == PROBE_CPU_XMM7_OFFSET, ProbeContext_cpu_xmm7_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(sizeof(MacroAssembler::ProbeContext) == PROBE_SIZE, ProbeContext_size_matches_ctiMasmProbeTrampoline);

// Also double check that the xmm registers are 16 byte (128-bit) aligned as
// required by the movdqa instruction used in the trampoline.
COMPILE_ASSERT(!(PROBE_OFFSETOF(cpu.xmm0) % 16), ProbeContext_xmm0_offset_not_aligned_properly);
#undef PROBE_OFFSETOF

#endif // USE(MASM_PROBE)

#endif // COMPILER(GCC)

} // namespace JSC

#endif // JITStubsX86Common
