/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "pas_machine_current_stack_pointer.h"

#if PAS_OS(DARWIN)
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif

#if PAS_PLATFORM(IOS_FAMILY)
#define THUMB_FUNC_PARAM(name) SYMBOL_STRING(name)
#else
#define THUMB_FUNC_PARAM(name)
#endif

PAS_BEGIN_EXTERN_C;

#if !(defined(NDEBUG) && (PAS_X86_64 || PAS_ARM64 || PAS_ARM))

#if PAS_X86_64 && PAS_OS(WINDOWS)

__asm__(
    ".text" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "movq %rsp, %rax" "\n"
    "addq $40, %rax" "\n" // Account for return address and shadow stack
    "ret" "\n"

    ".section .drectve" "\n"
    ".ascii \"-export:pas_machine_current_stack_pointer\"" "\n"
);

#elif PAS_X86_64
__asm__(
    ".text" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "movq  %rsp, %rax" "\n"
    "addq $8, %rax" "\n" // Account for return address.
    "ret" "\n"
    ".previous" "\n"
);

#elif PAS_ARM64E
__asm__(
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "pacibsp" "\n"
    "mov x0, sp" "\n"
    "retab" "\n"
    ".previous" "\n"
);

#elif PAS_ARM64 && PAS_OS(WINDOWS)
__asm__(
    ".text" "\n"
    ".align 4" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "mov x0, sp" "\n"
    "ret" "\n"

    ".section .drectve" "\n"
    ".ascii \"-export:pas_machine_current_stack_pointer\"" "\n"
);

#elif PAS_ARM64
__asm__(
    ".text" "\n"
    ".balign 16" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "mov x0, sp" "\n"
    "ret" "\n"
    ".previous" "\n"
);

#elif PAS_ARM32
__asm__(
    ".text" "\n"
    ".align 2" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    ".thumb" "\n"
    ".thumb_func " THUMB_FUNC_PARAM(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

    "mov r0, sp" "\n"
    "bx  lr" "\n"
    ".previous" "\n"
);

#elif PAS_MIPS
__asm__(
    ".text" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"
    ".set push" "\n"
    ".set noreorder" "\n"
    ".set noat" "\n"

    "move $v0, $sp" "\n"
    "jr   $ra" "\n"
    "nop" "\n"
    ".set pop" "\n"
    ".previous" "\n"
);

#elif PAS_RISCV
__asm__(
    ".text" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

     "mv x10, sp" "\n"
     "ret" "\n"
     ".previous" "\n"
);

#elif PAS_LOONGARCH64
__asm__(
    ".text" "\n"
    ".globl " SYMBOL_STRING(pas_machine_current_stack_pointer) "\n"
    SYMBOL_STRING(pas_machine_current_stack_pointer) ":" "\n"

     "move $r4, $r3" "\n"
     "jr   $r1" "\n"
     ".previous" "\n"
);

#else
#error "Unsupported platform: need implementation of pas_machine_current_stack_pointer."
#endif // CPU cases

#endif // NDEBUG && (PAS_X86_64 || PAS_ARM64 || PAS_ARM)

PAS_END_EXTERN_C;
