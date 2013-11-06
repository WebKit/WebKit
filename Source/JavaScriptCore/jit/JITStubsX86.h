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

#ifndef JITStubsX86_h
#define JITStubsX86_h

#include "JITStubsX86Common.h"

#if !CPU(X86)
#error "JITStubsX86.h should only be #included if CPU(X86)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsX86.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#if COMPILER(GCC)

#if USE(MASM_PROBE)
asm (
".globl " SYMBOL_STRING(ctiMasmProbeTrampoline) "\n"
HIDE_SYMBOL(ctiMasmProbeTrampoline) "\n"
SYMBOL_STRING(ctiMasmProbeTrampoline) ":" "\n"

    "pushfd" "\n"

    // MacroAssembler::probe() has already generated code to store some values.
    // Together with the eflags pushed above, the top of stack now looks like
    // this:
    //     esp[0 * ptrSize]: eflags
    //     esp[1 * ptrSize]: return address / saved eip
    //     esp[2 * ptrSize]: probeFunction
    //     esp[3 * ptrSize]: arg1
    //     esp[4 * ptrSize]: arg2
    //     esp[5 * ptrSize]: saved eax
    //     esp[6 * ptrSize]: saved esp

    "movl %esp, %eax" "\n"
    "subl $" STRINGIZE_VALUE_OF(PROBE_SIZE) ", %esp" "\n"

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
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_ARG1_OFFSET) "(%ebp)" "\n"
    "movl 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_ARG2_OFFSET) "(%ebp)" "\n"
    "movl 5 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp)" "\n"
    "movl 6 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax), %ecx" "\n"
    "movl %ecx, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp)" "\n"

    "movdqa %xmm0, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm1, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm2, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm3, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm4, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm5, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm6, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%ebp)" "\n"
    "movdqa %xmm7, " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%ebp)" "\n"

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

    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM0_OFFSET) "(%ebp), %xmm0" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM1_OFFSET) "(%ebp), %xmm1" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM2_OFFSET) "(%ebp), %xmm2" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM3_OFFSET) "(%ebp), %xmm3" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM4_OFFSET) "(%ebp), %xmm4" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM5_OFFSET) "(%ebp), %xmm5" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM6_OFFSET) "(%ebp), %xmm6" "\n"
    "movdqa " STRINGIZE_VALUE_OF(PROBE_CPU_XMM7_OFFSET) "(%ebp), %xmm7" "\n"

    // There are 6 more registers left to restore:
    //     eax, ecx, ebp, esp, eip, and eflags.
    // We need to handle these last few restores carefully because:
    //
    // 1. We need to push the return address on the stack for ret to use.
    //    That means we need to write to the stack.
    // 2. The user probe function may have altered the restore value of esp to
    //    point to the vicinity of one of the restore values for the remaining
    //    registers left to be restored.
    //    That means, for requirement 1, we may end up writing over some of the
    //    restore values. We can check for this, and first copy the restore
    //    values to a "safe area" on the stack before commencing with the action
    //    for requirement 1.
    // 3. For requirement 2, we need to ensure that the "safe area" is
    //    protected from interrupt handlers overwriting it. Hence, the esp needs
    //    to be adjusted to include the "safe area" before we start copying the
    //    the restore values.

    "movl %ebp, %eax" "\n"
    "addl $" STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) ", %eax" "\n"
    "cmpl %eax, " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp)" "\n"
    "jg " SYMBOL_STRING(ctiMasmProbeTrampolineEnd) "\n"

    // Locate the "safe area" at 2x sizeof(ProbeContext) below where the new
    // rsp will be. This time we don't have to 32-byte align it because we're
    // not using to store any xmm regs.
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %eax" "\n"
    "subl $2 * " STRINGIZE_VALUE_OF(PROBE_SIZE) ", %eax" "\n"
    "movl %eax, %esp" "\n"

    "subl $" STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) ", %eax" "\n"
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

SYMBOL_STRING(ctiMasmProbeTrampolineEnd) ":" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ESP_OFFSET) "(%ebp), %eax" "\n"
    "subl $5 * " STRINGIZE_VALUE_OF(PTR_SIZE) ", %eax" "\n"
    // At this point, %esp should be < %eax.

    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EFLAGS_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, 0 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EAX_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, 1 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_ECX_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, 2 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EBP_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, 3 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax)" "\n"
    "movl " STRINGIZE_VALUE_OF(PROBE_CPU_EIP_OFFSET) "(%ebp), %ecx" "\n"
    "movl %ecx, 4 * " STRINGIZE_VALUE_OF(PTR_SIZE) "(%eax)" "\n"
    "movl %eax, %esp" "\n"

    "popfd" "\n"
    "popl %eax" "\n"
    "popl %ecx" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
);
#endif // USE(MASM_PROBE)

#endif // COMPILER(GCC)

} // namespace JSC

#endif // JITStubsX86_h
