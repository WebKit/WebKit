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

#if ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
#include "MacroAssembler.h"

#include <wtf/InlineASM.h>

namespace JSC {

#if ENABLE(MASM_PROBE)

extern "C" void ctiMasmProbeTrampoline();

#if COMPILER(GCC_OR_CLANG)

// The following are offsets for ProbeContext fields accessed by the ctiMasmProbeTrampoline stub.

#if CPU(X86)
#define PTR_SIZE 4
#else // CPU(X86_64)
#define PTR_SIZE 8
#endif

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)

#define PROBE_FIRST_GPR_OFFSET (2 * PTR_SIZE)
#define PROBE_CPU_EAX_OFFSET (PROBE_FIRST_GPR_OFFSET + (0 * PTR_SIZE))
#define PROBE_CPU_ECX_OFFSET (PROBE_FIRST_GPR_OFFSET + (1 * PTR_SIZE))
#define PROBE_CPU_EDX_OFFSET (PROBE_FIRST_GPR_OFFSET + (2 * PTR_SIZE))
#define PROBE_CPU_EBX_OFFSET (PROBE_FIRST_GPR_OFFSET + (3 * PTR_SIZE))
#define PROBE_CPU_ESP_OFFSET (PROBE_FIRST_GPR_OFFSET + (4 * PTR_SIZE))
#define PROBE_CPU_EBP_OFFSET (PROBE_FIRST_GPR_OFFSET + (5 * PTR_SIZE))
#define PROBE_CPU_ESI_OFFSET (PROBE_FIRST_GPR_OFFSET + (6 * PTR_SIZE))
#define PROBE_CPU_EDI_OFFSET (PROBE_FIRST_GPR_OFFSET + (7 * PTR_SIZE))

#if CPU(X86)
#define PROBE_FIRST_SPR_OFFSET (PROBE_FIRST_GPR_OFFSET + (8 * PTR_SIZE))
#else // CPU(X86_64)
#define PROBE_CPU_R8_OFFSET (PROBE_FIRST_GPR_OFFSET + (8 * PTR_SIZE))
#define PROBE_CPU_R9_OFFSET (PROBE_FIRST_GPR_OFFSET + (9 * PTR_SIZE))
#define PROBE_CPU_R10_OFFSET (PROBE_FIRST_GPR_OFFSET + (10 * PTR_SIZE))
#define PROBE_CPU_R11_OFFSET (PROBE_FIRST_GPR_OFFSET + (11 * PTR_SIZE))
#define PROBE_CPU_R12_OFFSET (PROBE_FIRST_GPR_OFFSET + (12 * PTR_SIZE))
#define PROBE_CPU_R13_OFFSET (PROBE_FIRST_GPR_OFFSET + (13 * PTR_SIZE))
#define PROBE_CPU_R14_OFFSET (PROBE_FIRST_GPR_OFFSET + (14 * PTR_SIZE))
#define PROBE_CPU_R15_OFFSET (PROBE_FIRST_GPR_OFFSET + (15 * PTR_SIZE))
#define PROBE_FIRST_SPR_OFFSET (PROBE_FIRST_GPR_OFFSET + (16 * PTR_SIZE))
#endif // CPU(X86_64)

#define PROBE_CPU_EIP_OFFSET (PROBE_FIRST_SPR_OFFSET + (0 * PTR_SIZE))
#define PROBE_CPU_EFLAGS_OFFSET (PROBE_FIRST_SPR_OFFSET + (1 * PTR_SIZE))
#define PROBE_FIRST_XMM_OFFSET (PROBE_FIRST_SPR_OFFSET + (2 * PTR_SIZE))

#define XMM_SIZE 8
#define PROBE_CPU_XMM0_OFFSET (PROBE_FIRST_XMM_OFFSET + (0 * XMM_SIZE))
#define PROBE_CPU_XMM1_OFFSET (PROBE_FIRST_XMM_OFFSET + (1 * XMM_SIZE))
#define PROBE_CPU_XMM2_OFFSET (PROBE_FIRST_XMM_OFFSET + (2 * XMM_SIZE))
#define PROBE_CPU_XMM3_OFFSET (PROBE_FIRST_XMM_OFFSET + (3 * XMM_SIZE))
#define PROBE_CPU_XMM4_OFFSET (PROBE_FIRST_XMM_OFFSET + (4 * XMM_SIZE))
#define PROBE_CPU_XMM5_OFFSET (PROBE_FIRST_XMM_OFFSET + (5 * XMM_SIZE))
#define PROBE_CPU_XMM6_OFFSET (PROBE_FIRST_XMM_OFFSET + (6 * XMM_SIZE))
#define PROBE_CPU_XMM7_OFFSET (PROBE_FIRST_XMM_OFFSET + (7 * XMM_SIZE))

#if CPU(X86)
#define PROBE_SIZE (PROBE_CPU_XMM7_OFFSET + XMM_SIZE)
#define PROBE_ALIGNED_SIZE (PROBE_SIZE + (2 * XMM_SIZE))
#else // CPU(X86_64)
#define PROBE_CPU_XMM8_OFFSET (PROBE_FIRST_XMM_OFFSET + (8 * XMM_SIZE))
#define PROBE_CPU_XMM9_OFFSET (PROBE_FIRST_XMM_OFFSET + (9 * XMM_SIZE))
#define PROBE_CPU_XMM10_OFFSET (PROBE_FIRST_XMM_OFFSET + (10 * XMM_SIZE))
#define PROBE_CPU_XMM11_OFFSET (PROBE_FIRST_XMM_OFFSET + (11 * XMM_SIZE))
#define PROBE_CPU_XMM12_OFFSET (PROBE_FIRST_XMM_OFFSET + (12 * XMM_SIZE))
#define PROBE_CPU_XMM13_OFFSET (PROBE_FIRST_XMM_OFFSET + (13 * XMM_SIZE))
#define PROBE_CPU_XMM14_OFFSET (PROBE_FIRST_XMM_OFFSET + (14 * XMM_SIZE))
#define PROBE_CPU_XMM15_OFFSET (PROBE_FIRST_XMM_OFFSET + (15 * XMM_SIZE))
#define PROBE_SIZE (PROBE_CPU_XMM15_OFFSET + XMM_SIZE)
#define PROBE_ALIGNED_SIZE (PROBE_SIZE + (4 * XMM_SIZE))
#endif // CPU(X86_64)

// These ASSERTs remind you that if you change the layout of ProbeContext,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct ProbeContext, x)
COMPILE_ASSERT(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, ProbeContext_probeFunction_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, ProbeContext_arg_offset_matches_ctiMasmProbeTrampoline);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::eax]) == PROBE_CPU_EAX_OFFSET, ProbeContext_cpu_eax_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::ecx]) == PROBE_CPU_ECX_OFFSET, ProbeContext_cpu_ecx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::edx]) == PROBE_CPU_EDX_OFFSET, ProbeContext_cpu_edx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::ebx]) == PROBE_CPU_EBX_OFFSET, ProbeContext_cpu_ebx_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::esp]) == PROBE_CPU_ESP_OFFSET, ProbeContext_cpu_esp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::ebp]) == PROBE_CPU_EBP_OFFSET, ProbeContext_cpu_ebp_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::esi]) == PROBE_CPU_ESI_OFFSET, ProbeContext_cpu_esi_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::edi]) == PROBE_CPU_EDI_OFFSET, ProbeContext_cpu_edi_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[X86Registers::eip]) == PROBE_CPU_EIP_OFFSET, ProbeContext_cpu_eip_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.sprs[X86Registers::eflags]) == PROBE_CPU_EFLAGS_OFFSET, ProbeContext_cpu_eflags_offset_matches_ctiMasmProbeTrampoline);

#if CPU(X86_64)
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r8]) == PROBE_CPU_R8_OFFSET, ProbeContext_cpu_r8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r9]) == PROBE_CPU_R9_OFFSET, ProbeContext_cpu_r9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r10]) == PROBE_CPU_R10_OFFSET, ProbeContext_cpu_r10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r11]) == PROBE_CPU_R11_OFFSET, ProbeContext_cpu_r11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r12]) == PROBE_CPU_R12_OFFSET, ProbeContext_cpu_r12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r13]) == PROBE_CPU_R13_OFFSET, ProbeContext_cpu_r13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r14]) == PROBE_CPU_R14_OFFSET, ProbeContext_cpu_r14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.gprs[X86Registers::r15]) == PROBE_CPU_R15_OFFSET, ProbeContext_cpu_r15_offset_matches_ctiMasmProbeTrampoline);
#endif // CPU(X86_64)

COMPILE_ASSERT(!(PROBE_CPU_XMM0_OFFSET & 0x7), ProbeContext_cpu_xmm0_offset_should_be_8_byte_aligned);

COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm0]) == PROBE_CPU_XMM0_OFFSET, ProbeContext_cpu_xmm0_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm1]) == PROBE_CPU_XMM1_OFFSET, ProbeContext_cpu_xmm1_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm2]) == PROBE_CPU_XMM2_OFFSET, ProbeContext_cpu_xmm2_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm3]) == PROBE_CPU_XMM3_OFFSET, ProbeContext_cpu_xmm3_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm4]) == PROBE_CPU_XMM4_OFFSET, ProbeContext_cpu_xmm4_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm5]) == PROBE_CPU_XMM5_OFFSET, ProbeContext_cpu_xmm5_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm6]) == PROBE_CPU_XMM6_OFFSET, ProbeContext_cpu_xmm6_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm7]) == PROBE_CPU_XMM7_OFFSET, ProbeContext_cpu_xmm7_offset_matches_ctiMasmProbeTrampoline);

#if CPU(X86_64)
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm8]) == PROBE_CPU_XMM8_OFFSET, ProbeContext_cpu_xmm8_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm9]) == PROBE_CPU_XMM9_OFFSET, ProbeContext_cpu_xmm9_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm10]) == PROBE_CPU_XMM10_OFFSET, ProbeContext_cpu_xmm10_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm11]) == PROBE_CPU_XMM11_OFFSET, ProbeContext_cpu_xmm11_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm12]) == PROBE_CPU_XMM12_OFFSET, ProbeContext_cpu_xmm12_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm13]) == PROBE_CPU_XMM13_OFFSET, ProbeContext_cpu_xmm13_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm14]) == PROBE_CPU_XMM14_OFFSET, ProbeContext_cpu_xmm14_offset_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(PROBE_OFFSETOF(cpu.fprs[X86Registers::xmm15]) == PROBE_CPU_XMM15_OFFSET, ProbeContext_cpu_xmm15_offset_matches_ctiMasmProbeTrampoline);
#endif // CPU(X86_64)

COMPILE_ASSERT(sizeof(ProbeContext) == PROBE_SIZE, ProbeContext_size_matches_ctiMasmProbeTrampoline);
COMPILE_ASSERT(!(PROBE_ALIGNED_SIZE & 0x1f), ProbeContext_aligned_size_offset_should_be_32_byte_aligned);

#undef PROBE_OFFSETOF

#if CPU(X86)
asm (
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "pushfd" "\n"

    // MacroAssemblerX86Common::probe() has already generated code to store some values.
    // Together with the eflags pushed above, the top of stack now looks like
    // this:
    //     esp[0 * ptrSize]: eflags
    //     esp[1 * ptrSize]: return address / saved eip
    //     esp[2 * ptrSize]: probe handler function
    //     esp[3 * ptrSize]: probe arg
    //     esp[4 * ptrSize]: saved eax
    //     esp[5 * ptrSize]: saved esp

    "movl %esp, %eax" "\n"
    "subl $" STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) ", %esp" "\n"

    // The X86_64 ABI specifies that the worse case stack alignment requirement
    // is 32 bytes.
    "andl $~0x1f, %esp" "\n"

    "movl %ebp, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%esp)" "\n"
    "movl %esp, %ebp" "\n" // Save the ProbeContext*.

    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%ebp)" "\n"
    "movl %edx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%ebp)" "\n"
    "movl %ebx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%ebp)" "\n"
    "movl %esi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%ebp)" "\n"
    "movl %edi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%ebp)" "\n"

    "movl 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%ebp)" "\n"
    "movl 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%ebp)" "\n"
    "movl 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%ebp)" "\n"
    "movl 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(%ebp)" "\n"
    "movl 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp)" "\n"
    "movl 5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp)" "\n"

    "movq %xmm0, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%ebp)" "\n"
    "movq %xmm1, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%ebp)" "\n"
    "movq %xmm2, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%ebp)" "\n"
    "movq %xmm3, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%ebp)" "\n"
    "movq %xmm4, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%ebp)" "\n"
    "movq %xmm5, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%ebp)" "\n"
    "movq %xmm6, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%ebp)" "\n"
    "movq %xmm7, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%ebp)" "\n"

    // Reserve stack space for the arg while maintaining the required stack
    // pointer 32 byte alignment:
    "subl $0x20, %esp" "\n"
    "movl %ebp, 0(%esp)" "\n" // the ProbeContext* arg.

    "call *" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%ebp)" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the ProbeContext before returning.

    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%ebp), %edx" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%ebp), %ebx" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%ebp), %esi" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%ebp), %edi" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%ebp), %xmm0" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%ebp), %xmm1" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%ebp), %xmm2" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%ebp), %xmm3" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%ebp), %xmm4" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%ebp), %xmm5" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%ebp), %xmm6" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%ebp), %xmm7" "\n"

    // There are 6 more registers left to restore:
    //     eax, ecx, ebp, esp, eip, and eflags.

    // The restoration process at ctiMasmProbeTrampolineEnd below works by popping
    // 5 words off the stack into eflags, eax, ecx, ebp, and eip. These 5 words need
    // to be pushed on top of the final esp value so that just by popping the 5 words,
    // we'll get the esp that the probe wants to set. Let's call this area (for storing
    // these 5 words) the restore area.
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %ecx" "\n"
    "subl $5 * " STRINGIZE_VALUE_OF(PTR_SIZE) ", %ecx" "\n"

    // ecx now points to the restore area.

    // Before we copy values from the ProbeContext to the restore area, we need to
    // make sure that the restore area does not overlap any of the values that we'll
    // be copying from in the ProbeContext. All the restore values to be copied from
    // comes from offset <= PROBE_CPU_EFLAGS_OFFSET in the ProbeContext.
    "movl %ebp, %eax" "\n"
    "addl $" STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) ", %eax" "\n"
    "cmpl %eax, %ecx" "\n"
    "jg " SYMBOL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    // Getting here means that the restore area will overlap the ProbeContext data
    // that we will need to get the restoration values from. So, let's move that
    // data to a safe place before we start writing into the restore area.
    // Let's locate the "safe area" at 2x sizeof(ProbeContext) below where the
    // restore area. This ensures that:
    // 1. The safe area does not overlap the restore area.
    // 2. The safe area does not overlap the ProbeContext.
    //    This makes it so that we can use memcpy (does not require memmove) semantics
    //    to copy the restore values to the safe area.
    // Note: the safe area does not have to 32-byte align it because we're not using
    // it to store any xmm regs.
    "movl %ecx, %eax" "\n"
    "subl $2 * " STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) ", %eax" "\n"

    // eax now points to the safe area.

    // Make sure the stack pointer points to the safe area. This ensures that the
    // safe area is protected from interrupt handlers overwriting it.
    "movl %eax, %esp" "\n"

    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%eax)" "\n"
    "movl %eax, %ebp" "\n"

    // We used ecx above as scratch register. Let's restore it to points to the
    // restore area.
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %ecx" "\n"
    "subl $5 * " STRINGIZE_VALUE_OF(PTR_SIZE) ", %ecx" "\n"

    // ecx now points to the restore area.

    SYMBOL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Copy remaining restore values from the ProbeContext to the restore area.
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%ebp), %eax" "\n"
    "movl %eax, 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%ecx)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp), %eax" "\n"
    "movl %eax, 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%ecx)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%ebp), %eax" "\n"
    "movl %eax, 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%ecx)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%ebp), %eax" "\n"
    "movl %eax, 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%ecx)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%ebp), %eax" "\n"
    "movl %eax, 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%ecx)" "\n"
    "movl %ecx, %esp" "\n"

    // Do the remaining restoration by popping off the restore area.
    "popfd" "\n"
    "popl %eax" "\n"
    "popl %ecx" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
);
#endif // CPU(X86)

#if CPU(X86_64)
asm (
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "pushfq" "\n"

    // MacroAssemblerX86Common::probe() has already generated code to store some values.
    // Together with the rflags pushed above, the top of stack now looks like
    // this:
    //     esp[0 * ptrSize]: rflags
    //     esp[1 * ptrSize]: return address / saved rip
    //     esp[2 * ptrSize]: probe handler function
    //     esp[3 * ptrSize]: probe arg
    //     esp[4 * ptrSize]: saved rax
    //     esp[5 * ptrSize]: saved rsp

    "movq %rsp, %rax" "\n"
    "subq $" STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) ", %rsp" "\n"

    // The X86_64 ABI specifies that the worse case stack alignment requirement
    // is 32 bytes.
    "andq $~0x1f, %rsp" "\n"

    "movq %rbp, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rsp)" "\n"
    "movq %rsp, %rbp" "\n" // Save the ProbeContext*.

    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rbp)" "\n"
    "movq %rdx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rbp)" "\n"
    "movq %rbx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rbp)" "\n"
    "movq %rsi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%rbp)" "\n"
    "movq %rdi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%rbp)" "\n"

    "movq 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rbp)" "\n"
    "movq 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rbp)" "\n"
    "movq 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%rbp)" "\n"
    "movq 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(%rbp)" "\n"
    "movq 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rbp)" "\n"
    "movq 5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rax), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp)" "\n"

    "movq %r8, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "(%rbp)" "\n"
    "movq %r9, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "(%rbp)" "\n"
    "movq %r10, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "(%rbp)" "\n"
    "movq %r11, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "(%rbp)" "\n"
    "movq %r12, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "(%rbp)" "\n"
    "movq %r13, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "(%rbp)" "\n"
    "movq %r14, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "(%rbp)" "\n"
    "movq %r15, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "(%rbp)" "\n"

    "movq %xmm0, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%rbp)" "\n"
    "movq %xmm1, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%rbp)" "\n"
    "movq %xmm2, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%rbp)" "\n"
    "movq %xmm3, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%rbp)" "\n"
    "movq %xmm4, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%rbp)" "\n"
    "movq %xmm5, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%rbp)" "\n"
    "movq %xmm6, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%rbp)" "\n"
    "movq %xmm7, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%rbp)" "\n"
    "movq %xmm8, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM8_OFFSET) "(%rbp)" "\n"
    "movq %xmm9, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM9_OFFSET) "(%rbp)" "\n"
    "movq %xmm10, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM10_OFFSET) "(%rbp)" "\n"
    "movq %xmm11, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM11_OFFSET) "(%rbp)" "\n"
    "movq %xmm12, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM12_OFFSET) "(%rbp)" "\n"
    "movq %xmm13, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM13_OFFSET) "(%rbp)" "\n"
    "movq %xmm14, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM14_OFFSET) "(%rbp)" "\n"
    "movq %xmm15, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM15_OFFSET) "(%rbp)" "\n"

    "movq %rbp, %rdi" "\n" // the ProbeContext* arg.
    "call *" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%rbp)" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the ProbeContext before returning.

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rbp), %rdx" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rbp), %rbx" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%rbp), %rsi" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%rbp), %rdi" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "(%rbp), %r8" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "(%rbp), %r9" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "(%rbp), %r10" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "(%rbp), %r11" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "(%rbp), %r12" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "(%rbp), %r13" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "(%rbp), %r14" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "(%rbp), %r15" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%rbp), %xmm0" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%rbp), %xmm1" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%rbp), %xmm2" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%rbp), %xmm3" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%rbp), %xmm4" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%rbp), %xmm5" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%rbp), %xmm6" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%rbp), %xmm7" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM8_OFFSET) "(%rbp), %xmm8" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM9_OFFSET) "(%rbp), %xmm9" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM10_OFFSET) "(%rbp), %xmm10" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM11_OFFSET) "(%rbp), %xmm11" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM12_OFFSET) "(%rbp), %xmm12" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM13_OFFSET) "(%rbp), %xmm13" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM14_OFFSET) "(%rbp), %xmm14" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_XMM15_OFFSET) "(%rbp), %xmm15" "\n"

    // There are 6 more registers left to restore:
    //     rax, rcx, rbp, rsp, rip, and rflags.

    // The restoration process at ctiMasmProbeTrampolineEnd below works by popping
    // 5 words off the stack into rflags, rax, rcx, rbp, and rip. These 5 words need
    // to be pushed on top of the final esp value so that just by popping the 5 words,
    // we'll get the esp that the probe wants to set. Let's call this area (for storing
    // these 5 words) the restore area.
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rcx" "\n"
    "subq $5 * " STRINGIZE_VALUE_OF(PTR_SIZE) ", %rcx" "\n"

    // rcx now points to the restore area.

    // Before we copy values from the ProbeContext to the restore area, we need to
    // make sure that the restore area does not overlap any of the values that we'll
    // be copying from in the ProbeContext. All the restore values to be copied from
    // comes from offset <= PROBE_CPU_EFLAGS_OFFSET in the ProbeContext.
    "movq %rbp, %rax" "\n"
    "addq $" STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) ", %rax" "\n"
    "cmpq %rax, %rcx" "\n"
    "jg " SYMBOL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    // Getting here means that the restore area will overlap the ProbeContext data
    // that we will need to get the restoration values from. So, let's move that
    // data to a safe place before we start writing into the restore area.
    // Let's locate the "safe area" at 2x sizeof(ProbeContext) below where the
    // restore area. This ensures that:
    // 1. The safe area does not overlap the restore area.
    // 2. The safe area does not overlap the ProbeContext.
    //    This makes it so that we can use memcpy (does not require memmove) semantics
    //    to copy the restore values to the safe area.
    // Note: the safe area does not have to 32-byte align it because we're not using
    // it to store any xmm regs.
    "movq %rcx, %rax" "\n"
    "subq $2 * " STRINGIZE_VALUE_OF(PROBE_ALIGNED_SIZE) ", %rax" "\n"

    // rax now points to the safe area.

    // Make sure the stack pointer points to the safe area. This ensures that the
    // safe area is protected from interrupt handlers overwriting it.
    "movq %rax, %rsp" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rax)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rax)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rax)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rax)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rax)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rax)" "\n"
    "movq %rax, %rbp" "\n"

    // We used rcx above as scratch register. Let's restore it to points to the
    // restore area.
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rcx" "\n"
    "subq $5 * " STRINGIZE_VALUE_OF(PTR_SIZE) ", %rcx" "\n"

    // rcx now points to the restore area.

    SYMBOL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"

    // Copy remaining restore values from the ProbeContext to the restore area.
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rcx)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rcx)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rcx)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rcx)" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rcx)" "\n"
    "movq %rcx, %rsp" "\n"

    // Do the remaining restoration by popping off the restore area.
    "popfq" "\n"
    "popq %rax" "\n"
    "popq %rcx" "\n"
    "popq %rbp" "\n"
    "ret" "\n"
);
#endif // CPU(X86_64)

#endif // COMPILER(GCC_OR_CLANG)

// What code is emitted for the probe?
// ==================================
// We want to keep the size of the emitted probe invocation code as compact as
// possible to minimize the perturbation to the JIT generated code. However,
// we also need to preserve the CPU registers and set up the ProbeContext to be
// passed to the user probe function.
//
// Hence, we do only the minimum here to preserve a scratch register (i.e. rax
// in this case) and the stack pointer (i.e. rsp), and pass the probe arguments.
// We'll let the ctiMasmProbeTrampoline handle the rest of the probe invocation
// work i.e. saving the CPUState (and setting up the ProbeContext), calling the
// user probe function, and restoring the CPUState before returning to JIT
// generated code.
//
// What registers need to be saved?
// ===============================
// The registers are saved for 2 reasons:
// 1. To preserve their state in the JITted code. This means that all registers
//    that are not callee saved needs to be saved. We also need to save the
//    condition code registers because the probe can be inserted between a test
//    and a branch.
// 2. To allow the probe to inspect the values of the registers for debugging
//    purposes. This means all registers need to be saved.
//
// In summary, save everything. But for reasons stated above, we should do the
// minimum here and let ctiMasmProbeTrampoline do the heavy lifting to save the
// full set.
//
// What values are in the saved registers?
// ======================================
// Conceptually, the saved registers should contain values as if the probe
// is not present in the JIT generated code. Hence, they should contain values
// that are expected at the start of the instruction immediately following the
// probe.
//
// Specifically, the saved stack pointer register will point to the stack
// position before we push the ProbeContext frame. The saved rip will point to
// the address of the instruction immediately following the probe. 

void MacroAssembler::probe(ProbeFunction function, void* arg)
{
    push(RegisterID::esp);
    push(RegisterID::eax);
    move(TrustedImmPtr(arg), RegisterID::eax);
    push(RegisterID::eax);
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), RegisterID::eax);
    push(RegisterID::eax);
    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), RegisterID::eax);
    call(RegisterID::eax);
}

#endif // ENABLE(MASM_PROBE)

#if CPU(X86) && !OS(MAC_OS_X)
MacroAssemblerX86Common::SSE2CheckState MacroAssemblerX86Common::s_sse2CheckState = NotCheckedSSE2;
#endif

MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_sse4_1CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_avxCheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_lzcntCheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_bmi1CheckState = CPUIDCheckState::NotChecked;

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
