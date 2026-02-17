/*
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

#include "config.h"
#include <wtf/StackSwitch.h>

#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/InlineASM.h>
#include <wtf/Threading.h>

namespace WTF {

// C-linkage wrappers for target functions so they can be called from assembly
#define DEFINE_STACK_SWITCH_WRAPPER(trampolineName, argType, argName, callExpression) \
    extern "C" void trampolineName##Wrapper(argType argName); \
    extern "C" void trampolineName##Wrapper(argType argName) \
    { \
        callExpression; \
    }

WTF_FOR_EACH_STACK_SWITCH_TRAMPOLINE(DEFINE_STACK_SWITCH_WRAPPER)
#undef DEFINE_STACK_SWITCH_WRAPPER

#if CPU(ARM64E)

// Trampoline that switches from the OS-provided stack to a custom sequestered stack.
// Parameters:
//   x0 = context pointer (passed as argument to target function, or 'this' pointer for member functions)
//   x1 = new stack pointer (top of stack, since stacks grow down)
//
// This function:
// 1. Saves callee-saved registers on the current (OS) stack
// 2. Switches to the new stack
// 3. Sets up the rest of its frame (saved sp, lr) on the new stack
// 4. Calls the target function via its wrapper
// 5. When function returns, restores the OS stack and all saved registers
// 6. Returns to the original caller
#define DEFINE_STACK_SWITCH_TRAMPOLINE_ARM64E(trampolineName, argType, argName, callExpression) \
    __asm__( \
        ".text" "\n" \
        ".balign 16" "\n" \
        ".globl " SYMBOL_STRING(trampolineName) "\n" \
        SYMBOL_STRING(trampolineName) ":" "\n" \
        \
        "pacibsp" "\n" \
        "stp x19, x20, [sp, #-16]!" "\n" \
        "mov x19, sp" "\n" \
        \
        "mov sp, x1" "\n" \
        "stp x29, x30, [sp, #-16]!" "\n" \
        "mov x29, sp" "\n" \
        \
        "bl " SYMBOL_STRING(trampolineName##Wrapper) "\n" \
        \
        "mov sp, x19" "\n" \
        "ldp x29, x30, [sp], #16" "\n" \
        "ldp x19, x20, [sp], #16" "\n" \
        "retab" "\n" \
        \
        ".previous" "\n" \
    );

WTF_FOR_EACH_STACK_SWITCH_TRAMPOLINE(DEFINE_STACK_SWITCH_TRAMPOLINE_ARM64E)
#undef DEFINE_STACK_SWITCH_TRAMPOLINE_ARM64E

#else // !CPU(ARM64E)

IGNORE_WARNINGS_BEGIN("missing-noreturn")

#define DEFINE_STACK_SWITCH_STUB(trampolineName, argType, argName, callExpression) \
    extern "C" void trampolineName(argType, void*) \
    { \
        RELEASE_ASSERT_NOT_REACHED(); \
    }

WTF_FOR_EACH_STACK_SWITCH_TRAMPOLINE(DEFINE_STACK_SWITCH_STUB)
#undef DEFINE_STACK_SWITCH_STUB

IGNORE_WARNINGS_END

#endif // CPU(ARM64E)

} // namespace WTF
