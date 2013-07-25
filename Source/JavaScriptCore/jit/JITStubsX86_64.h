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

#ifndef JITStubsX86_64_h
#define JITStubsX86_64_h

#include "JITStubsX86Common.h"

#if !CPU(X86_64)
#error "JITStubsX86_64.h should only be #included if CPU(X86_64)"
#endif

#if !USE(JSVALUE64)
#error "JITStubsX86_64.h only implements USE(JSVALUE64)"
#endif

namespace JSC {

#if COMPILER(GCC)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x58, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x48, JITStackFrame_code_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedRBX) == 0x78, JITStackFrame_stub_argument_space_matches_ctiTrampoline);

asm (
".text\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "pushq %rbp" "\n"
    "movq %rsp, %rbp" "\n"
    "pushq %r12" "\n"
    "pushq %r13" "\n"
    "pushq %r14" "\n"
    "pushq %r15" "\n"
    "pushq %rbx" "\n"
    // Form the JIT stubs area
    "pushq %r9" "\n"
    "pushq %r8" "\n"
    "pushq %rcx" "\n"
    "pushq %rdx" "\n"
    "pushq %rsi" "\n"
    "pushq %rdi" "\n"
    "subq $0x48, %rsp" "\n"
    "movq $512, %r12" "\n"
    "movq $0xFFFF000000000000, %r14" "\n"
    "movq $0xFFFF000000000002, %r15" "\n"
    "movq %rdx, %r13" "\n"
    "call *%rdi" "\n"
    "addq $0x78, %rsp" "\n"
    "popq %rbx" "\n"
    "popq %r15" "\n"
    "popq %r14" "\n"
    "popq %r13" "\n"
    "popq %r12" "\n"
    "popq %rbp" "\n"
    "ret" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "movq %rsp, %rdi" "\n"
    "call " LOCAL_REFERENCE(cti_vm_throw) "\n"
    "int3" "\n"
);

asm (
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "addq $0x78, %rsp" "\n"
    "popq %rbx" "\n"
    "popq %r15" "\n"
    "popq %r14" "\n"
    "popq %r13" "\n"
    "popq %r12" "\n"
    "popq %rbp" "\n"
    "ret" "\n"
);

#if USE(MASM_PROBE)
asm (
".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    // MacroAssembler::probe() has already generated code to save/store the
    // values for rax and rsp in the ProbeContext. Save/store the remaining
    // registers here.

    "popq %rax" "\n"
    "movq %rax, " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rsp)" "\n"

    "movq %rbp, " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rsp)" "\n"
    "movq %rsp, %rbp" "\n" // Save the ProbeContext*.

    "movq %rcx, " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rbp)" "\n"
    "movq %rdx, " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rbp)" "\n"
    "movq %rbx, " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rbp)" "\n"
    "movq %rsi, " STRINGIZE_VALUE_OF(PROBE_CPU_ESI_OFFSET) "(%rbp)" "\n"
    "movq %rdi, " STRINGIZE_VALUE_OF(PROBE_CPU_EDI_OFFSET) "(%rbp)" "\n"

    "movq %r8, " STRINGIZE_VALUE_OF(PROBE_CPU_R8_OFFSET) "(%rbp)" "\n"
    "movq %r9, " STRINGIZE_VALUE_OF(PROBE_CPU_R9_OFFSET) "(%rbp)" "\n"
    "movq %r10, " STRINGIZE_VALUE_OF(PROBE_CPU_R10_OFFSET) "(%rbp)" "\n"
    "movq %r11, " STRINGIZE_VALUE_OF(PROBE_CPU_R11_OFFSET) "(%rbp)" "\n"
    "movq %r12, " STRINGIZE_VALUE_OF(PROBE_CPU_R12_OFFSET) "(%rbp)" "\n"
    "movq %r13, " STRINGIZE_VALUE_OF(PROBE_CPU_R13_OFFSET) "(%rbp)" "\n"
    "movq %r14, " STRINGIZE_VALUE_OF(PROBE_CPU_R14_OFFSET) "(%rbp)" "\n"
    "movq %r15, " STRINGIZE_VALUE_OF(PROBE_CPU_R15_OFFSET) "(%rbp)" "\n"

    "movdqa %xmm0, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm1, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm2, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm3, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm4, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm5, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm6, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%rbp)" "\n"
    "movdqa %xmm7, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%rbp)" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rax" "\n"
    "movq %rax, " STRINGIZE_VALUE_OF(PROBE_JIT_STACK_FRAME_OFFSET) "(%rbp)" "\n"

    "movq %rbp, %rdi" "\n" // the ProbeContext* arg.
    "call *" STRINGIZE_VALUE_OF(PROBE_PROBE_FUNCTION_OFFSET) "(%rbp)" "\n"

    // To enable probes to modify register state, we copy all regiesters
    // out of the ProbeContext before returning.

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%rbp), %rax" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%rbp), %rcx" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EDX_OFFSET) "(%rbp), %rdx" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EBX_OFFSET) "(%rbp), %rbx" "\n"
    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%rbp), %rsp" "\n"
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

    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%rbp), %xmm0" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%rbp), %xmm1" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%rbp), %xmm2" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%rbp), %xmm3" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%rbp), %xmm4" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%rbp), %xmm5" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%rbp), %xmm6" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%rbp), %xmm7" "\n"

    "pushq " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%rbp)" "\n"

    "movq " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%rbp), %rbp" "\n"
    "ret" "\n"
);
#endif // USE(MASM_PROBE)

#endif // COMPILER(GCC)

#if COMPILER(MSVC)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines in JITStubsMSVC64.asm to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) % 16 == 0x0, JITStackFrame_maintains_16byte_stack_alignment);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedRBX) == 0x58, JITStackFrame_stub_argument_space_matches_ctiTrampoline);

#endif // COMPILER(MSVC)

} // namespace JSC

#endif // JITStubsX86_64_h
