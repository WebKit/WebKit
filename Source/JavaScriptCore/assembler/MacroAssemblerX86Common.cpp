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

#if ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
#include "MacroAssembler.h"

#include "ProbeContext.h"
#include <wtf/InlineASM.h>

#if OS(DARWIN)
#include <sys/sysctl.h>
#endif

#if COMPILER(MSVC)
#include <intrin.h>
#endif

namespace JSC {

JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampoline, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampoline);
#if CPU(X86_64)
JSC_DECLARE_JIT_OPERATION(ctiMasmProbeTrampolineSIMD, void, ());
JSC_ANNOTATE_JIT_OPERATION_PROBE(ctiMasmProbeTrampolineSIMD);
#endif

// The following are offsets for Probe::State fields accessed by the ctiMasmProbeTrampoline stub.

#if CPU(X86)
#define PTR_SIZE 4
#else // CPU(X86_64)
#define PTR_SIZE 8
#endif

#define PROBE_PROBE_FUNCTION_OFFSET (0 * PTR_SIZE)
#define PROBE_ARG_OFFSET (1 * PTR_SIZE)
#define PROBE_INIT_STACK_FUNCTION_OFFSET (2 * PTR_SIZE)
#define PROBE_INIT_STACK_ARG_OFFSET (3 * PTR_SIZE)

#define PROBE_FIRST_GPR_OFFSET (4 * PTR_SIZE)
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
#else // CPU(X86_64)
#define PROBE_CPU_XMM8_OFFSET (PROBE_FIRST_XMM_OFFSET + (8 * XMM_SIZE))
#define PROBE_CPU_XMM9_OFFSET (PROBE_FIRST_XMM_OFFSET + (9 * XMM_SIZE))
#define PROBE_CPU_XMM10_OFFSET (PROBE_FIRST_XMM_OFFSET + (10 * XMM_SIZE))
#define PROBE_CPU_XMM11_OFFSET (PROBE_FIRST_XMM_OFFSET + (11 * XMM_SIZE))
#define PROBE_CPU_XMM12_OFFSET (PROBE_FIRST_XMM_OFFSET + (12 * XMM_SIZE))
#define PROBE_CPU_XMM13_OFFSET (PROBE_FIRST_XMM_OFFSET + (13 * XMM_SIZE))
#define PROBE_CPU_XMM14_OFFSET (PROBE_FIRST_XMM_OFFSET + (14 * XMM_SIZE))
#define PROBE_CPU_XMM15_OFFSET (PROBE_FIRST_XMM_OFFSET + (15 * XMM_SIZE))

// For the case when we want to save 128 bits for each XMM register, the layout is different.
#define PROBE_CPU_XMM0_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (0 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM1_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (1 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM2_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (2 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM3_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (3 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM4_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (4 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM5_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (5 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM6_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (6 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM7_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (7 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM8_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (8 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM9_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (9 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM10_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (10 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM11_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (11 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM12_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (12 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM13_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (13 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM14_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (14 * 2 * XMM_SIZE))
#define PROBE_CPU_XMM15_VECTOR_OFFSET (PROBE_FIRST_XMM_OFFSET + (15 * 2 * XMM_SIZE))
// We save extra room for XMM registers in case we save vectors, but normally
// we the doubles together. This is to match ARM64, where we store pair in the
// non-vector case.
#define PROBE_SIZE (PROBE_FIRST_XMM_OFFSET + 16 * 2 * XMM_SIZE)
#endif // CPU(X86_64)

#if COMPILER(MSVC) || CPU(X86)
#define PROBE_EXECUTOR_OFFSET PROBE_SIZE // Stash the executeJSCJITProbe function pointer at the end of the ProbeContext.
#endif

// The outgoing record to be popped off the stack at the end consists of:
// eflags, eax, ecx, ebp, eip.
#define OUT_SIZE        (5 * PTR_SIZE)

// These ASSERTs remind you that if you change the layout of Probe::State,
// you need to change ctiMasmProbeTrampoline offsets above to match.
#define PROBE_OFFSETOF(x) offsetof(struct Probe::State, x)
#define PROBE_OFFSETOF_REG(x, reg) offsetof(struct Probe::State, x) + reg * sizeof((reinterpret_cast<Probe::State*>(0))->x[reg])
static_assert(PROBE_OFFSETOF(probeFunction) == PROBE_PROBE_FUNCTION_OFFSET, "Probe::State::probeFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(arg) == PROBE_ARG_OFFSET, "Probe::State::arg's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackFunction) == PROBE_INIT_STACK_FUNCTION_OFFSET, "Probe::State::initializeStackFunction's offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF(initializeStackArg) == PROBE_INIT_STACK_ARG_OFFSET, "Probe::State::initializeStackArg's offset matches ctiMasmProbeTrampoline");

static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::eax) == PROBE_CPU_EAX_OFFSET, "Probe::State::cpu.gprs[eax]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::ecx) == PROBE_CPU_ECX_OFFSET, "Probe::State::cpu.gprs[ecx]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::edx) == PROBE_CPU_EDX_OFFSET, "Probe::State::cpu.gprs[edx]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::ebx) == PROBE_CPU_EBX_OFFSET, "Probe::State::cpu.gprs[ebx]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::esp) == PROBE_CPU_ESP_OFFSET, "Probe::State::cpu.gprs[esp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::ebp) == PROBE_CPU_EBP_OFFSET, "Probe::State::cpu.gprs[ebp]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::esi) == PROBE_CPU_ESI_OFFSET, "Probe::State::cpu.gprs[esi]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::edi) == PROBE_CPU_EDI_OFFSET, "Probe::State::cpu.gprs[edi]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.sprs, X86Registers::eip) == PROBE_CPU_EIP_OFFSET, "Probe::State::cpu.gprs[eip]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.sprs, X86Registers::eflags) == PROBE_CPU_EFLAGS_OFFSET, "Probe::State::cpu.sprs[eflags]'s offset matches ctiMasmProbeTrampoline");

#if CPU(X86_64)
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r8) == PROBE_CPU_R8_OFFSET, "Probe::State::cpu.gprs[r8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r9) == PROBE_CPU_R9_OFFSET, "Probe::State::cpu.gprs[r9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r10) == PROBE_CPU_R10_OFFSET, "Probe::State::cpu.gprs[r10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r11) == PROBE_CPU_R11_OFFSET, "Probe::State::cpu.gprs[r11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r12) == PROBE_CPU_R12_OFFSET, "Probe::State::cpu.gprs[r12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r13) == PROBE_CPU_R13_OFFSET, "Probe::State::cpu.gprs[r13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r14) == PROBE_CPU_R14_OFFSET, "Probe::State::cpu.gprs[r14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.gprs, X86Registers::r15) == PROBE_CPU_R15_OFFSET, "Probe::State::cpu.gprs[r15]'s offset matches ctiMasmProbeTrampoline");
#endif // CPU(X86_64)

static_assert(!(PROBE_CPU_XMM0_OFFSET & 0x7), "Probe::State::cpu.fprs[xmm0]'s offset should be 8 byte aligned");

static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm0) == PROBE_CPU_XMM0_OFFSET, "Probe::State::cpu.fprs[xmm0]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm1) == PROBE_CPU_XMM1_OFFSET, "Probe::State::cpu.fprs[xmm1]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm2) == PROBE_CPU_XMM2_OFFSET, "Probe::State::cpu.fprs[xmm2]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm3) == PROBE_CPU_XMM3_OFFSET, "Probe::State::cpu.fprs[xmm3]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm4) == PROBE_CPU_XMM4_OFFSET, "Probe::State::cpu.fprs[xmm4]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm5) == PROBE_CPU_XMM5_OFFSET, "Probe::State::cpu.fprs[xmm5]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm6) == PROBE_CPU_XMM6_OFFSET, "Probe::State::cpu.fprs[xmm6]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm7) == PROBE_CPU_XMM7_OFFSET, "Probe::State::cpu.fprs[xmm7]'s offset matches ctiMasmProbeTrampoline");

#if CPU(X86_64)
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm8) == PROBE_CPU_XMM8_OFFSET, "Probe::State::cpu.fprs[xmm8]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm9) == PROBE_CPU_XMM9_OFFSET, "Probe::State::cpu.fprs[xmm9]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm10) == PROBE_CPU_XMM10_OFFSET, "Probe::State::cpu.fprs[xmm10]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm11) == PROBE_CPU_XMM11_OFFSET, "Probe::State::cpu.fprs[xmm11]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm12) == PROBE_CPU_XMM12_OFFSET, "Probe::State::cpu.fprs[xmm12]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm13) == PROBE_CPU_XMM13_OFFSET, "Probe::State::cpu.fprs[xmm13]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm14) == PROBE_CPU_XMM14_OFFSET, "Probe::State::cpu.fprs[xmm14]'s offset matches ctiMasmProbeTrampoline");
static_assert(PROBE_OFFSETOF_REG(cpu.fprs.fprs, X86Registers::xmm15) == PROBE_CPU_XMM15_OFFSET, "Probe::State::cpu.fprs[xmm15]'s offset matches ctiMasmProbeTrampoline");
#endif // CPU(X86_64)

static_assert(sizeof(Probe::State) == PROBE_SIZE, "Probe::State::size's matches ctiMasmProbeTrampoline");
#if COMPILER(MSVC) || CPU(X86)
static_assert((PROBE_EXECUTOR_OFFSET + PTR_SIZE) <= (PROBE_SIZE + OUT_SIZE), "Must have room after ProbeContext to stash the probe handler");
#endif

#undef PROBE_OFFSETOF

#if CPU(X86)
#if COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "pushfl" "\n"

    // MacroAssemblerX86Common::probe() has already generated code to store some values.
    // Together with the eflags pushed above, the top of stack now looks like
    // this:
    //     esp[0 * ptrSize]: eflags
    //     esp[1 * ptrSize]: return address / saved eip
    //     esp[2 * ptrSize]: saved ebx
    //     esp[3 * ptrSize]: saved edx
    //     esp[4 * ptrSize]: saved ecx
    //     esp[5 * ptrSize]: saved eax
    //
    // Incoming registers contain:
    //     ecx: Probe::executeJSCJITProbe
    //     edx: probe function
    //     ebx: probe arg
    //     eax: scratch (was ctiMasmProbeTrampoline)

    "movl %esp, %eax" "\n"
    "subl $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %esp" "\n"

    // The X86_64 ABI specifies that the worse case stack alignment requirement is 32 bytes.
    "andl $~0x1f, %esp" "\n"

    "movl %ebp, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%esp)" "\n"
    "movl %esp, %ebp" "\n" // Save the Probe::State*.

    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_EXECUTOR_OFFSET) "(%ebp)" "\n"
    "movl %edx, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%ebp)" "\n"
    "movl %ebx, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(%ebp)" "\n"
    "movl %esi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%ebp)" "\n"
    "movl %edi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%ebp)" "\n"

    "movl 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%ebp)" "\n"
    "movl 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%ebp)" "\n"
    "movl 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%ebp)" "\n"
    "movl 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%ebp)" "\n"
    "movl 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%ebp)" "\n"
    "movl 5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp)" "\n"

    "movl %eax, %ecx" "\n"
    "addl $" STRINGIZE_VALUE_OF(6 * PTR_SIZE) ", %ecx" "\n"
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
    "movl %ebp, 0(%esp)" "\n" // the Probe::State* arg.

    "call *" STRINGIZE_VALUE_OF(PROBE_EXECUTOR_OFFSET) "(%ebp)" "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "movl $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %ecx" "\n"
    "movl %ebp, %eax" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %edx" "\n"
    "addl %ecx, %eax" "\n"
    "cmpl %eax, %edx" "\n"
    "jge " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "subl %ecx, %edx" "\n"
    "andl $~0x1f, %edx" "\n" // Keep the stack pointer 32 bytes aligned.
    "xorl %eax, %eax" "\n"
    "movl %edx, %esp" "\n"

    "movl $" STRINGIZE_VALUE_OF(PROBE_SIZE) ", %ecx" "\n"

    // Copy the Probe::State to the safe place.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "movl (%ebp, %eax), %edx" "\n"
    "movl %edx, (%esp, %eax)" "\n"
    "addl $" STRINGIZE_VALUE_OF(PTR_SIZE) ", %eax" "\n"
    "cmpl %eax, %ecx" "\n"
    "jg " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "movl %esp, %ebp" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "xorl %ecx, %ecx" "\n"
    "addl " STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "(%ebp), %ecx" "\n"
    "je " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"

    // Reserve stack space for the arg while maintaining the required stack
    // pointer 32 byte alignment:
    "subl $0x20, %esp" "\n"
    "movl %ebp, 0(%esp)" "\n" // the Probe::State* arg.
    "call *%ecx" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning.

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

    // Copy remaining restore values from the Probe::State to the restore area.
    // Note: We already ensured above that the Probe::State is in a safe location before
    // calling the initializeStackFunction. The initializeStackFunction is not allowed to
    // change the stack pointer again.
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
    "popfl" "\n"
    "popl %eax" "\n"
    "popl %ecx" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
    ".previous" "\n"
);
#endif

#if COMPILER(MSVC)
extern "C" __declspec(naked) void ctiMasmProbeTrampoline()
{
    __asm {
        pushfd;

        // MacroAssemblerX86Common::probe() has already generated code to store some values.
        // Together with the eflags pushed above, the top of stack now looks like
        // this:
        //     esp[0 * ptrSize]: eflags
        //     esp[1 * ptrSize]: return address / saved eip
        //     esp[2 * ptrSize]: saved ebx
        //     esp[3 * ptrSize]: saved edx
        //     esp[4 * ptrSize]: saved ecx
        //     esp[5 * ptrSize]: saved eax
        //
        // Incoming registers contain:
        //     ecx: Probe::executeJSCJITProbe
        //     edx: probe function
        //     ebx: probe arg
        //     eax: scratch (was ctiMasmProbeTrampoline)

        mov eax, esp
        sub esp, PROBE_SIZE + OUT_SIZE

        // The X86_64 ABI specifies that the worse case stack alignment requirement is 32 bytes.
        and esp, ~0x1f

        mov [PROBE_CPU_EBP_OFFSET + esp], ebp
        mov ebp, esp // Save the ProbeContext*.

        mov [PROBE_EXECUTOR_OFFSET + ebp], ecx
        mov [PROBE_PROBE_FUNCTION_OFFSET + ebp], edx
        mov [PROBE_ARG_OFFSET + ebp], ebx
        mov [PROBE_CPU_ESI_OFFSET + ebp], esi
        mov [PROBE_CPU_EDI_OFFSET + ebp], edi

        mov ecx, [0 * PTR_SIZE + eax]
        mov [PROBE_CPU_EFLAGS_OFFSET + ebp], ecx
        mov ecx, [1 * PTR_SIZE + eax]
        mov [PROBE_CPU_EIP_OFFSET + ebp], ecx
        mov ecx, [2 * PTR_SIZE + eax]
        mov [PROBE_CPU_EBX_OFFSET + ebp], ecx
        mov ecx, [3 * PTR_SIZE + eax]
        mov [PROBE_CPU_EDX_OFFSET + ebp], ecx
        mov ecx, [4 * PTR_SIZE + eax]
        mov [PROBE_CPU_ECX_OFFSET + ebp], ecx
        mov ecx, [5 * PTR_SIZE + eax]
        mov [PROBE_CPU_EAX_OFFSET + ebp], ecx

        mov ecx, eax
        add ecx, 6 * PTR_SIZE
        mov [PROBE_CPU_ESP_OFFSET + ebp], ecx

        movq qword ptr[PROBE_CPU_XMM0_OFFSET + ebp], xmm0
        movq qword ptr[PROBE_CPU_XMM1_OFFSET + ebp], xmm1
        movq qword ptr[PROBE_CPU_XMM2_OFFSET + ebp], xmm2
        movq qword ptr[PROBE_CPU_XMM3_OFFSET + ebp], xmm3
        movq qword ptr[PROBE_CPU_XMM4_OFFSET + ebp], xmm4
        movq qword ptr[PROBE_CPU_XMM5_OFFSET + ebp], xmm5
        movq qword ptr[PROBE_CPU_XMM6_OFFSET + ebp], xmm6
        movq qword ptr[PROBE_CPU_XMM7_OFFSET + ebp], xmm7

        // Reserve stack space for the arg while maintaining the required stack
        // pointer 32 byte alignment:
        sub esp, 0x20
        mov [0 + esp], ebp // the ProbeContext* arg.

        call [PROBE_EXECUTOR_OFFSET + ebp]

        // Make sure the ProbeContext is entirely below the result stack pointer so
        // that register values are still preserved when we call the initializeStack
        // function.
        mov ecx, PROBE_SIZE + OUT_SIZE
        mov eax, ebp
        mov edx, [PROBE_CPU_ESP_OFFSET + ebp]
        add eax, ecx
        cmp edx, eax
        jge ctiMasmProbeTrampolineProbeContextIsSafe

        // Allocate a safe place on the stack below the result stack pointer to stash the ProbeContext.
        sub edx, ecx
        and edx, ~0x1f // Keep the stack pointer 32 bytes aligned.
        xor eax, eax
        mov esp, edx

        mov ecx, PROBE_SIZE

        // Copy the ProbeContext to the safe place.
        ctiMasmProbeTrampolineCopyLoop :
        mov edx, [ebp + eax]
        mov [esp + eax], edx
        add eax, PTR_SIZE
        cmp ecx, eax
        jg ctiMasmProbeTrampolineCopyLoop

        mov ebp, esp

        // Call initializeStackFunction if present.
        ctiMasmProbeTrampolineProbeContextIsSafe :
        xor ecx, ecx
        add ecx, [PROBE_INIT_STACK_FUNCTION_OFFSET + ebp]
        je ctiMasmProbeTrampolineRestoreRegisters

        // Reserve stack space for the arg while maintaining the required stack
        // pointer 32 byte alignment:
        sub esp, 0x20
        mov [0 + esp], ebp // the ProbeContext* arg.
        call ecx

        ctiMasmProbeTrampolineRestoreRegisters :

        // To enable probes to modify register state, we copy all registers
        // out of the ProbeContext before returning.

        mov edx, [PROBE_CPU_EDX_OFFSET + ebp]
        mov ebx, [PROBE_CPU_EBX_OFFSET + ebp]
        mov esi, [PROBE_CPU_ESI_OFFSET + ebp]
        mov edi, [PROBE_CPU_EDI_OFFSET + ebp]

        movq xmm0, qword ptr[PROBE_CPU_XMM0_OFFSET + ebp]
        movq xmm1, qword ptr[PROBE_CPU_XMM1_OFFSET + ebp]
        movq xmm2, qword ptr[PROBE_CPU_XMM2_OFFSET + ebp]
        movq xmm3, qword ptr[PROBE_CPU_XMM3_OFFSET + ebp]
        movq xmm4, qword ptr[PROBE_CPU_XMM4_OFFSET + ebp]
        movq xmm5, qword ptr[PROBE_CPU_XMM5_OFFSET + ebp]
        movq xmm6, qword ptr[PROBE_CPU_XMM6_OFFSET + ebp]
        movq xmm7, qword ptr[PROBE_CPU_XMM7_OFFSET + ebp]

        // There are 6 more registers left to restore:
        //     eax, ecx, ebp, esp, eip, and eflags.

        // The restoration process at ctiMasmProbeTrampolineEnd below works by popping
        // 5 words off the stack into eflags, eax, ecx, ebp, and eip. These 5 words need
        // to be pushed on top of the final esp value so that just by popping the 5 words,
        // we'll get the esp that the probe wants to set. Let's call this area (for storing
        // these 5 words) the restore area.
        mov ecx, [PROBE_CPU_ESP_OFFSET + ebp]
        sub ecx, 5 * PTR_SIZE

        // ecx now points to the restore area.

        // Copy remaining restore values from the ProbeContext to the restore area.
        // Note: We already ensured above that the ProbeContext is in a safe location before
        // calling the initializeStackFunction. The initializeStackFunction is not allowed to
        // change the stack pointer again.
        mov eax, [PROBE_CPU_EFLAGS_OFFSET + ebp]
        mov [0 * PTR_SIZE + ecx], eax
        mov eax, [PROBE_CPU_EAX_OFFSET + ebp]
        mov [1 * PTR_SIZE + ecx], eax
        mov eax, [PROBE_CPU_ECX_OFFSET + ebp]
        mov [2 * PTR_SIZE + ecx], eax
        mov eax, [PROBE_CPU_EBP_OFFSET + ebp]
        mov [3 * PTR_SIZE + ecx], eax
        mov eax, [PROBE_CPU_EIP_OFFSET + ebp]
        mov [4 * PTR_SIZE + ecx], eax
        mov esp, ecx

        // Do the remaining restoration by popping off the restore area.
        popfd
        pop eax
        pop ecx
        pop ebp
        ret
    }
}
#endif // COMPILER(MSVC)

#endif // CPU(X86)

#if CPU(X86_64)
#if COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "pushq %rbp" "\n"
    "movq  %rsp, %rbp" "\n"

    "pushfq" "\n"

    // MacroAssemblerX86Common::probe() has already generated code to store some values.
    // Together with the rbp and rflags pushed above, the top of stack now looks like this:
    //     rbp[-1 * ptrSize]: rflags
    //     rbp[0 * ptrSize]: rbp / previousCallFrame
    //     rbp[1 * ptrSize]: return address / saved rip
    //     rbp[2 * ptrSize]: saved rbx
    //     rbp[3 * ptrSize]: saved rdx
    //     rbp[4 * ptrSize]: saved rax
    //
    // Incoming registers contain:
    //     rdx: probe function
    //     rbx: probe arg
    //     rax: scratch (was ctiMasmProbeTrampoline)

    "subq $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %rsp" "\n"

    // The X86_64 ABI specifies that the worse case stack alignment requirement is 32 bytes.
    "andq $~0x1f, %rsp" "\n"
    // Since sp points to the Probe::State, we've ensured that it's protected from interrupts before we initialize it.

    "movq %rdx, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%rsp)" "\n"
    "movq %rbx, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(%rsp)" "\n"
    "movq %rsi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%rsp)" "\n"
    "movq %rdi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%rsp)" "\n"

    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rsp)" "\n"

    "movq -1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rsp)" "\n"
    "movq 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rsp)" "\n"
    "movq 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rsp)" "\n"
    "movq 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rsp)" "\n"
    "movq 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rsp)" "\n"
    "movq 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rsp)" "\n"

    "movq %rbp, %rcx" "\n"
    "addq $" STRINGIZE_VALUE_OF(6 * PTR_SIZE) ", %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rsp)" "\n"

    "movq %r8, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "(%rsp)" "\n"
    "movq %r9, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "(%rsp)" "\n"
    "movq %r10, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "(%rsp)" "\n"
    "movq %r11, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "(%rsp)" "\n"
    "movq %r12, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "(%rsp)" "\n"
    "movq %r13, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "(%rsp)" "\n"
    "movq %r14, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "(%rsp)" "\n"
    "movq %r15, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "(%rsp)" "\n"

    "movq %xmm0, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%rsp)" "\n"
    "movq %xmm1, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%rsp)" "\n"
    "movq %xmm2, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%rsp)" "\n"
    "movq %xmm3, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%rsp)" "\n"
    "movq %xmm4, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%rsp)" "\n"
    "movq %xmm5, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%rsp)" "\n"
    "movq %xmm6, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%rsp)" "\n"
    "movq %xmm7, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%rsp)" "\n"
    "movq %xmm8, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM8_OFFSET) "(%rsp)" "\n"
    "movq %xmm9, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM9_OFFSET) "(%rsp)" "\n"
    "movq %xmm10, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM10_OFFSET) "(%rsp)" "\n"
    "movq %xmm11, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM11_OFFSET) "(%rsp)" "\n"
    "movq %xmm12, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM12_OFFSET) "(%rsp)" "\n"
    "movq %xmm13, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM13_OFFSET) "(%rsp)" "\n"
    "movq %xmm14, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM14_OFFSET) "(%rsp)" "\n"
    "movq %xmm15, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM15_OFFSET) "(%rsp)" "\n"

    "movq %rsp, %rdi" "\n" // the Probe::State* arg.
    "call " SYMBOL_STRING(executeJSCJITProbe) "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "movq %rsp, %rbp" "\n"
    "movq $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %rcx" "\n"
    "movq %rsp, %rax" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rdx" "\n"
    "addq %rcx, %rax" "\n"
    "cmpq %rax, %rdx" "\n"
    "jge " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "subq %rcx, %rdx" "\n"
    "andq $~0x1f, %rdx" "\n" // Keep the stack pointer 32 bytes aligned.
    "xorq %rax, %rax" "\n"
    "movq %rdx, %rsp" "\n"

    "movq $" STRINGIZE_VALUE_OF(PROBE_SIZE) ", %rcx" "\n"

    // Copy the Probe::State to the safe place.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) ":" "\n"
    "movq (%rbp, %rax), %rdx" "\n"
    "movq %rdx, (%rsp, %rax)" "\n"
    "addq $" STRINGIZE_VALUE_OF(PTR_SIZE) ", %rax" "\n"
    "cmpq %rax, %rcx" "\n"
    "jg " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoop) "\n"

    "movq %rsp, %rbp" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafe) ":" "\n"
    "xorq %rcx, %rcx" "\n"
    "addq " STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "(%rbp), %rcx" "\n"
    "je " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) "\n"

    "movq %rbp, %rdi" "\n" // the Probe::State* arg.
    "call *%rcx" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegisters) ":" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning.

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

    // Copy remaining restore values from the Probe::State to the restore area.
    // Note: We already ensured above that the Probe::State is in a safe location before
    // calling the initializeStackFunction. The initializeStackFunction is not allowed to
    // change the stack pointer again.
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
    ".previous" "\n"
);

// And now, the slower version that saves the full width of vectors in xmm registers.

asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(ctiMasmProbeTrampolineSIMD) "\n"
    HIDE_SYMBOL(ctiMasmProbeTrampolineSIMD) "\n"
    SYMBOL_STRING(ctiMasmProbeTrampolineSIMD) ":" "\n"

    "pushq %rbp" "\n"
    "movq  %rsp, %rbp" "\n"

    "pushfq" "\n"

    // MacroAssemblerX86Common::probe() has already generated code to store some values.
    // Together with the rbp and rflags pushed above, the top of stack now looks like this:
    //     rbp[-1 * ptrSize]: rflags
    //     rbp[0 * ptrSize]: rbp / previousCallFrame
    //     rbp[1 * ptrSize]: return address / saved rip
    //     rbp[2 * ptrSize]: saved rbx
    //     rbp[3 * ptrSize]: saved rdx
    //     rbp[4 * ptrSize]: saved rax
    //
    // Incoming registers contain:
    //     rdx: probe function
    //     rbx: probe arg
    //     rax: scratch (was ctiMasmProbeTrampoline)

    "subq $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %rsp" "\n"

    // The X86_64 ABI specifies that the worse case stack alignment requirement is 32 bytes.
    "andq $~0x1f, %rsp" "\n"
    // Since sp points to the Probe::State, we've ensured that it's protected from interrupts before we initialize it.

    "movq %rdx, " STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%rsp)" "\n"
    "movq %rbx, " STRINGIZE_VALUE_OF(PROBE_ARG_OFFSET) "(%rsp)" "\n"
    "movq %rsi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%rsp)" "\n"
    "movq %rdi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%rsp)" "\n"

    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rsp)" "\n"

    "movq -1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%rsp)" "\n"
    "movq 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rsp)" "\n"
    "movq 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rsp)" "\n"
    "movq 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rsp)" "\n"
    "movq 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rsp)" "\n"
    "movq 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%rbp), %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rsp)" "\n"

    "movq %rbp, %rcx" "\n"
    "addq $" STRINGIZE_VALUE_OF(6 * PTR_SIZE) ", %rcx" "\n"
    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rsp)" "\n"

    "movq %r8, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "(%rsp)" "\n"
    "movq %r9, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "(%rsp)" "\n"
    "movq %r10, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "(%rsp)" "\n"
    "movq %r11, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "(%rsp)" "\n"
    "movq %r12, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "(%rsp)" "\n"
    "movq %r13, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "(%rsp)" "\n"
    "movq %r14, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "(%rsp)" "\n"
    "movq %r15, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "(%rsp)" "\n"

    "vmovaps %xmm0,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm1,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm2,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm3,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm4,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm5,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm6,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm7,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm8,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM8_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm9,  " STRINGIZE_VALUE_OF(PROBE_CPU_XMM9_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm10, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM10_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm11, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM11_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm12, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM12_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm13, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM13_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm14, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM14_VECTOR_OFFSET) "(%rsp)" "\n"
    "vmovaps %xmm15, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM15_VECTOR_OFFSET) "(%rsp)" "\n"

    "movq %rsp, %rdi" "\n" // the Probe::State* arg.
    "call " SYMBOL_STRING(executeJSCJITProbe) "\n"

    // Make sure the Probe::State is entirely below the result stack pointer so
    // that register values are still preserved when we call the initializeStack
    // function.
    "movq %rsp, %rbp" "\n"
    "movq $" STRINGIZE_VALUE_OF(PROBE_SIZE + OUT_SIZE) ", %rcx" "\n"
    "movq %rsp, %rax" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rdx" "\n"
    "addq %rcx, %rax" "\n"
    "cmpq %rax, %rdx" "\n"
    "jge " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafeSIMD) "\n"

    // Allocate a safe place on the stack below the result stack pointer to stash the Probe::State.
    "subq %rcx, %rdx" "\n"
    "andq $~0x1f, %rdx" "\n" // Keep the stack pointer 32 bytes aligned.
    "xorq %rax, %rax" "\n"
    "movq %rdx, %rsp" "\n"

    "movq $" STRINGIZE_VALUE_OF(PROBE_SIZE) ", %rcx" "\n"

    // Copy the Probe::State to the safe place.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoopSIMD) ":" "\n"
    "movq (%rbp, %rax), %rdx" "\n"
    "movq %rdx, (%rsp, %rax)" "\n"
    "addq $" STRINGIZE_VALUE_OF(PTR_SIZE) ", %rax" "\n"
    "cmpq %rax, %rcx" "\n"
    "jg " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineCopyLoopSIMD) "\n"

    "movq %rsp, %rbp" "\n"

    // Call initializeStackFunction if present.
    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineProbeStateIsSafeSIMD) ":" "\n"
    "xorq %rcx, %rcx" "\n"
    "addq " STRINGIZE_VALUE_OF(PROBE_INIT_STACK_FUNCTION_OFFSET) "(%rbp), %rcx" "\n"
    "je " LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegistersSIMD) "\n"

    "movq %rbp, %rdi" "\n" // the Probe::State* arg.
    "call *%rcx" "\n"

    LOCAL_LABEL_STRING(ctiMasmProbeTrampolineRestoreRegistersSIMD) ":" "\n"

    // To enable probes to modify register state, we copy all registers
    // out of the Probe::State before returning.

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

    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_VECTOR_OFFSET) "(%rbp), %xmm0" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_VECTOR_OFFSET) "(%rbp), %xmm1" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_VECTOR_OFFSET) "(%rbp), %xmm2" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_VECTOR_OFFSET) "(%rbp), %xmm3" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_VECTOR_OFFSET) "(%rbp), %xmm4" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_VECTOR_OFFSET) "(%rbp), %xmm5" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_VECTOR_OFFSET) "(%rbp), %xmm6" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_VECTOR_OFFSET) "(%rbp), %xmm7" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM8_VECTOR_OFFSET) "(%rbp), %xmm8" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM9_VECTOR_OFFSET) "(%rbp), %xmm9" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM10_VECTOR_OFFSET) "(%rbp), %xmm10" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM11_VECTOR_OFFSET) "(%rbp), %xmm11" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM12_VECTOR_OFFSET) "(%rbp), %xmm12" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM13_VECTOR_OFFSET) "(%rbp), %xmm13" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM14_VECTOR_OFFSET) "(%rbp), %xmm14" "\n"
    "vmovaps " STRINGIZE_VALUE_OF(PROBE_CPU_XMM15_VECTOR_OFFSET) "(%rbp), %xmm15" "\n"

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

    // Copy remaining restore values from the Probe::State to the restore area.
    // Note: We already ensured above that the Probe::State is in a safe location before
    // calling the initializeStackFunction. The initializeStackFunction is not allowed to
    // change the stack pointer again.
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
    ".previous" "\n"
);
#endif // COMPILER(GCC_COMPATIBLE)
#endif // CPU(X86_64)

// What code is emitted for the probe?
// ==================================
// We want to keep the size of the emitted probe invocation code as compact as
// possible to minimize the perturbation to the JIT generated code. However,
// we also need to preserve the CPU registers and set up the Probe::State to be
// passed to the user probe function.
//
// Hence, we do only the minimum here to preserve a scratch register (i.e. rax
// in this case) and the stack pointer (i.e. rsp), and pass the probe arguments.
// We'll let the ctiMasmProbeTrampoline handle the rest of the probe invocation
// work i.e. saving the CPUState (and setting up the Probe::State), calling the
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
// position before we push the Probe::State frame. The saved rip will point to
// the address of the instruction immediately following the probe. 

void MacroAssembler::probe(Probe::Function function, void* arg, SavedFPWidth savedFPWidth)
{
#if CPU(X86_64) && COMPILER(GCC_COMPATIBLE)
    // Extra push so that the total number of pushes pad out to 32-bytes, and the
    // stack pointer remains 32 byte aligned as required by the ABI.
    push(RegisterID::eax);
#endif
    push(RegisterID::eax);

#if CPU(X86_64) && COMPILER(GCC_COMPATIBLE)
    if (savedFPWidth == SavedFPWidth::SaveVectors)
        move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampolineSIMD)), RegisterID::eax);
    else
#else
    UNUSED_PARAM(savedFPWidth);
#endif
        move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), RegisterID::eax);

#if COMPILER(MSVC) || CPU(X86)
    push(RegisterID::ecx);
    move(TrustedImmPtr(reinterpret_cast<void*>(Probe::executeJSCJITProbe)), RegisterID::ecx);
#endif
    push(RegisterID::edx);
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), RegisterID::edx);
    push(RegisterID::ebx);
    move(TrustedImmPtr(arg), RegisterID::ebx);
    call(RegisterID::eax, CFunctionPtrTag);
}

MacroAssemblerX86Common::CPUID MacroAssemblerX86Common::getCPUID(unsigned level)
{
    return getCPUIDEx(level, 0);
}

MacroAssemblerX86Common::CPUID MacroAssemblerX86Common::getCPUIDEx(unsigned level, unsigned count)
{
    CPUID result { };
#if COMPILER(MSVC)
    __cpuidex(bitwise_cast<int*>(result.data()), level, count);
#else
    __asm__ (
        "cpuid\n"
        : "=a"(result[0]), "=b"(result[1]), "=c"(result[2]), "=d"(result[3])
        : "0"(level), "2"(count)
    );
#endif
    return result;
}

void MacroAssemblerX86Common::collectCPUFeatures()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        {
            CPUID cpuid = getCPUID(0x1);
            s_sse3CheckState = (cpuid[2] & (1 << 0)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_supplementalSSE3CheckState = (cpuid[2] & (1 << 9)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_sse4_1CheckState = (cpuid[2] & (1 << 19)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_sse4_2CheckState = (cpuid[2] & (1 << 20)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_popcntCheckState = (cpuid[2] & (1 << 23)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_avxCheckState = (cpuid[2] & (1 << 28)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
        }
#if OS(DARWIN)
        {
            uint32_t val = 0;
            size_t valSize = sizeof(val);
            int rc = sysctlbyname("hw.optional.bmi1", &val, &valSize, nullptr, 0);
            s_bmi1CheckState = (rc >= 0 && val) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;

            rc = sysctlbyname("hw.optional.avx2_0", &val, &valSize, nullptr, 0);
            s_avx2CheckState = (rc >= 0 && val) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
        }
#else
        {
            CPUID cpuid = getCPUID(0x7);
            s_bmi1CheckState = (cpuid[2] & (1 << 3)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
            s_avx2CheckState = (cpuid[2] & (1 << 5)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
        }
#endif
        {
            CPUID cpuid = getCPUID(0x80000001);
            s_lzcntCheckState = (cpuid[2] & (1 << 5)) ? CPUIDCheckState::Set : CPUIDCheckState::Clear;
        }
    });
}

MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_sse3CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_supplementalSSE3CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_sse4_1CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_sse4_2CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_avxCheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_avx2CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_lzcntCheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_bmi1CheckState = CPUIDCheckState::NotChecked;
MacroAssemblerX86Common::CPUIDCheckState MacroAssemblerX86Common::s_popcntCheckState = CPUIDCheckState::NotChecked;

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
