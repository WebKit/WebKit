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

#if !CPU(X86)
#error "JITStubsX86.h should only be #included if CPU(X86)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsX86.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#if COMPILER(GCC)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) % 16 == 0x0, JITStackFrame_maintains_16byte_stack_alignment);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedEBX) == 0x3c, JITStackFrame_stub_argument_space_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x58, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x50, JITStackFrame_code_offset_matches_ctiTrampoline);

asm (
".text\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "pushl %ebp" "\n"
    "movl %esp, %ebp" "\n"
    "pushl %esi" "\n"
    "pushl %edi" "\n"
    "pushl %ebx" "\n"
    "subl $0x3c, %esp" "\n"
    "movl 0x58(%esp), %edi" "\n"
    "call *0x50(%esp)" "\n"
    "addl $0x3c, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "movl %esp, %ecx" "\n"
    "call " LOCAL_REFERENCE(cti_vm_throw) "\n"
    "int3" "\n"
);

asm (
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "addl $0x3c, %esp" "\n"
    "popl %ebx" "\n"
    "popl %edi" "\n"
    "popl %esi" "\n"
    "popl %ebp" "\n"
    "ret" "\n"
);

#endif // COMPILER(GCC)

#if COMPILER(MSVC)

// These ASSERTs remind you that, if you change the layout of JITStackFrame, you
// need to change the assembly trampolines below to match.
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) % 16 == 0x0, JITStackFrame_maintains_16byte_stack_alignment);
COMPILE_ASSERT(offsetof(struct JITStackFrame, savedEBX) == 0x3c, JITStackFrame_stub_argument_space_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, callFrame) == 0x58, JITStackFrame_callFrame_offset_matches_ctiTrampoline);
COMPILE_ASSERT(offsetof(struct JITStackFrame, code) == 0x50, JITStackFrame_code_offset_matches_ctiTrampoline);

extern "C" {

    __declspec(naked) EncodedJSValue ctiTrampoline(void* code, JSStack*, CallFrame*, void* /*unused1*/, void* /*unused2*/, VM*)
    {
        __asm {
            push ebp;
            mov ebp, esp;
            push esi;
            push edi;
            push ebx;
            sub esp, 0x3c;
            mov ecx, esp;
            mov edi, [esp + 0x58];
            call [esp + 0x50];
            add esp, 0x3c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            ret;
        }
    }

    __declspec(naked) void ctiVMThrowTrampoline()
    {
        __asm {
            mov ecx, esp;
            call cti_vm_throw;
            add esp, 0x3c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            ret;
        }
    }

    __declspec(naked) void ctiOpThrowNotCaught()
    {
        __asm {
            add esp, 0x3c;
            pop ebx;
            pop edi;
            pop esi;
            pop ebp;
            ret;
        }
    }
}

#endif // COMPILER(MSVC)

} // namespace JSC

#endif // JITStubsX86_h
