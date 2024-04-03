/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#include "StackPointer.h"

#include "InlineASM.h"

namespace WTF {

#if USE(ASM_CURRENT_STACK_POINTER)

#if CPU(X86) && COMPILER(MSVC)
extern "C" __declspec(naked) void currentStackPointer()
{
    __asm {
        mov eax, esp
        add eax, 4
        ret
    }
}

#elif CPU(X86) & COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"
    "movl %esp, %eax" "\n"
    "addl $4, %eax" "\n"
    "ret" "\n"
    ".previous" "\n"
);

#elif CPU(X86_64) && OS(WINDOWS)

// The Win64 port will use a hack where we define currentStackPointer in
// LowLevelInterpreter.asm.

#elif CPU(X86_64) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

    "movq  %rsp, %rax" "\n"
    "addq $8, %rax" "\n" // Account for return address.
    "ret" "\n"
    ".previous" "\n"
);

#elif CPU(ARM64E) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

    "pacibsp" "\n"
    "mov x0, sp" "\n"
    "retab" "\n"
    ".previous" "\n"
);

#elif CPU(ARM64) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

    "mov x0, sp" "\n"
    "ret" "\n"
    ".previous" "\n"
);

#elif CPU(ARM_THUMB2) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".align 2" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    ".thumb" "\n"
    ".thumb_func " THUMB_FUNC_PARAM(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

    "mov r0, sp" "\n"
    "bx  lr" "\n"
    ".previous" "\n"
);

#elif CPU(MIPS) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"
    ".set push" "\n"
    ".set noreorder" "\n"
    ".set noat" "\n"

    "move $v0, $sp" "\n"
    "jr   $ra" "\n"
    "nop" "\n"
    ".set pop" "\n"
    ".previous" "\n"
);

#elif CPU(RISCV64) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

     "mv x10, sp" "\n"
     "ret" "\n"
     ".previous" "\n"
);

#elif CPU(LOONGARCH64) && COMPILER(GCC_COMPATIBLE)
asm (
    ".text" "\n"
    ".globl " SYMBOL_STRING(currentStackPointer) "\n"
    SYMBOL_STRING(currentStackPointer) ":" "\n"

     "move $r4, $r3" "\n"
     "jr   $r1" "\n"
     ".previous" "\n"
);

#else
#error "Unsupported platform: need implementation of currentStackPointer."
#endif

#elif USE(GENERIC_CURRENT_STACK_POINTER)
constexpr size_t sizeOfFrameHeader = 2 * sizeof(void*);

SUPPRESS_ASAN NEVER_INLINE
void* currentStackPointer()
{
#if COMPILER(GCC_COMPATIBLE)
    return reinterpret_cast<uint8_t*>(__builtin_frame_address(0)) + sizeOfFrameHeader;
#else
    // Make sure that sp is the only local variable declared in this function.
    void* sp = reinterpret_cast<uint8_t*>(&sp) + sizeOfFrameHeader + sizeof(sp);
    return sp;
#endif
}
#endif // USE(GENERIC_CURRENT_STACK_POINTER)

} // namespace WTF
