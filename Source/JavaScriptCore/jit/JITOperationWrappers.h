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

#ifndef JITOperationWrappers_h
#define JITOperationWrappers_h

#include "JITOperations.h"
#include <wtf/Compiler.h>
#include <wtf/InlineASM.h>

#if COMPILER(MSVC)
#include <intrin.h>
#endif

namespace JSC {

#if CPU(MIPS)
#if WTF_MIPS_PIC
#define LOAD_FUNCTION_TO_T9(function) \
        ".set noreorder" "\n" \
        ".cpload $25" "\n" \
        ".set reorder" "\n" \
        "la $t9, " LOCAL_REFERENCE(function) "\n"
#else
#define LOAD_FUNCTION_TO_T9(function) "" "\n"
#endif
#endif

#if COMPILER(GCC) && CPU(X86_64)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm( \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov (%rsp), %" STRINGIZE(register) "\n" \
        "jmp " LOCAL_REFERENCE(function##WithReturnAddress) "\n" \
    );
#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rsi)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rsi)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rcx)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, rcx)
#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, r8)

#elif COMPILER(GCC) && CPU(X86)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, offset) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov (%esp), %eax\n" \
        "mov %eax, " STRINGIZE(offset) "(%esp)\n" \
        "jmp " LOCAL_REFERENCE(function##WithReturnAddress) "\n" \
    );
#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 8)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 8)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 16)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 20)
#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 28)

#elif CPU(ARM64)

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, register) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov " STRINGIZE(register) ", lr" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, x1)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)    FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, x1)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, x3)
#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, x3)
#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, x4)

#elif COMPILER(GCC) && CPU(ARM_THUMB2)

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov a2, lr" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "mov a4, lr" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

// EncodedJSValue in JSVALUE32_64 is a 64-bit integer. When being compiled in ARM EABI, it must be aligned even-numbered register (r0, r2 or [sp]).
// As a result, return address will be at a 4-byte further location in the following cases.
#if COMPILER_SUPPORTS(EABI) && CPU(ARM)
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJI "str lr, [sp, #4]"
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "str lr, [sp, #12]"
#else
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJI "str lr, [sp, #0]"
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "str lr, [sp, #8]"
#endif

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        INSTRUCTION_STORE_RETURN_ADDRESS_EJI "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
    asm ( \
    ".text" "\n" \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    ".thumb" "\n" \
    ".thumb_func " THUMB_FUNC_PARAM(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#elif COMPILER(GCC) && CPU(ARM_TRADITIONAL)

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    asm ( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    INLINE_ARM_FUNCTION(function) \
    SYMBOL_STRING(function) ":" "\n" \
        "mov a2, lr" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    asm ( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    INLINE_ARM_FUNCTION(function) \
    SYMBOL_STRING(function) ":" "\n" \
        "mov a4, lr" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

// EncodedJSValue in JSVALUE32_64 is a 64-bit integer. When being compiled in ARM EABI, it must be aligned even-numbered register (r0, r2 or [sp]).
// As a result, return address will be at a 4-byte further location in the following cases.
#if COMPILER_SUPPORTS(EABI) && CPU(ARM)
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJI "str lr, [sp, #4]"
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "str lr, [sp, #12]"
#else
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJI "str lr, [sp, #0]"
#define INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "str lr, [sp, #8]"
#endif

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
    asm ( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    INLINE_ARM_FUNCTION(function) \
    SYMBOL_STRING(function) ":" "\n" \
        INSTRUCTION_STORE_RETURN_ADDRESS_EJI "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
    asm ( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    INLINE_ARM_FUNCTION(function) \
    SYMBOL_STRING(function) ":" "\n" \
        INSTRUCTION_STORE_RETURN_ADDRESS_EJJI "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#elif COMPILER(GCC) && CPU(MIPS)

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
    LOAD_FUNCTION_TO_T9(function##WithReturnAddress) \
        "move $a1, $ra" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
    LOAD_FUNCTION_TO_T9(function##WithReturnAddress) \
        "move $a3, $ra" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
    LOAD_FUNCTION_TO_T9(function##WithReturnAddress) \
        "sw $ra, 20($sp)" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
    LOAD_FUNCTION_TO_T9(function##WithReturnAddress) \
        "sw $ra, 28($sp)" "\n" \
        "b " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
    );

#elif COMPILER(GCC) && CPU(SH4)

#define SH4_SCRATCH_REGISTER "r11"

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "sts pr, r5" "\n" \
        "bra " LOCAL_REFERENCE(function) "WithReturnAddress" "\n" \
        "nop" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "sts pr, r7" "\n" \
        "mov.l 2f, " SH4_SCRATCH_REGISTER "\n" \
        "braf " SH4_SCRATCH_REGISTER "\n" \
        "nop" "\n" \
        "1: .balign 4" "\n" \
        "2: .long " LOCAL_REFERENCE(function) "WithReturnAddress-1b" "\n" \
    );

#define FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, offset, scratch) \
    asm( \
    ".text" "\n" \
    ".globl " SYMBOL_STRING(function) "\n" \
    HIDE_SYMBOL(function) "\n" \
    SYMBOL_STRING(function) ":" "\n" \
        "sts pr, " scratch "\n" \
        "mov.l " scratch ", @(" STRINGIZE(offset) ", r15)" "\n" \
        "mov.l 2f, " scratch "\n" \
        "braf " scratch "\n" \
        "nop" "\n" \
        "1: .balign 4" "\n" \
        "2: .long " LOCAL_REFERENCE(function) "WithReturnAddress-1b" "\n" \
    );

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function)  FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 0, SH4_SCRATCH_REGISTER)
#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) FUNCTION_WRAPPER_WITH_RETURN_ADDRESS(function, 8, SH4_SCRATCH_REGISTER)

#elif COMPILER(MSVC) && CPU(X86)

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
__declspec(naked) EncodedJSValue JIT_OPERATION function(ExecState*, EncodedJSValue, StringImpl*) \
{ \
    __asm { \
        __asm mov eax, [esp] \
        __asm mov [esp + 20], eax \
        __asm jmp function##WithReturnAddress \
    } \
}

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
__declspec(naked) EncodedJSValue JIT_OPERATION function(ExecState*, JSCell*, StringImpl*) \
{ \
    __asm { \
        __asm mov eax, [esp] \
        __asm mov [esp + 16], eax \
        __asm jmp function##WithReturnAddress \
    } \
}

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJCI(function) \
__declspec(naked) void JIT_OPERATION function(ExecState*, EncodedJSValue, JSCell*, StringImpl*) \
{ \
    __asm { \
        __asm mov eax, [esp] \
        __asm mov [esp + 24], eax \
        __asm jmp function##WithReturnAddress \
    } \
}

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
__declspec(naked) void JIT_OPERATION function(ExecState*, EncodedJSValue, EncodedJSValue, StringImpl*) \
{ \
    __asm { \
        __asm mov eax, [esp] \
        __asm mov [esp + 28], eax \
        __asm jmp function##WithReturnAddress \
    } \
}

#elif COMPILER(MSVC)

#define _P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    void* JIT_OPERATION function(ExecState* exec) { return function##WithReturnAddress(exec, ReturnAddressPtr(*(void**)_AddressOfReturnAddress())); }

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
    EncodedJSValue JIT_OPERATION function(ExecState* exec) { return function##WithReturnAddress(exec, ReturnAddressPtr(*(void**)_AddressOfReturnAddress())); }

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
    EncodedJSValue JIT_OPERATION function(ExecState* exec, JSCell* cell, StringImpl* string) { return function##WithReturnAddress(exec, cell, string, ReturnAddressPtr(*(void**)_AddressOfReturnAddress())); }

#define _J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
    EncodedJSValue JIT_OPERATION function(ExecState* exec, EncodedJSValue value, StringImpl* string) { return function##WithReturnAddress(exec, value, string, ReturnAddressPtr(*(void**)_AddressOfReturnAddress())); }

#define _V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
    void JIT_OPERATION function(ExecState* exec, EncodedJSValue value, EncodedJSValue baseValue, StringImpl* string) { return function##WithReturnAddress(exec, value, baseValue, string, ReturnAddressPtr(*(void**)_AddressOfReturnAddress())); }

#endif

#define P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
void* JIT_OPERATION function##WithReturnAddress(ExecState*, ReturnAddressPtr) REFERENCED_FROM_ASM WTF_INTERNAL; \
_P_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function) \
EncodedJSValue JIT_OPERATION function##WithReturnAddress(ExecState*, ReturnAddressPtr) REFERENCED_FROM_ASM WTF_INTERNAL; \
_J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_E(function)

#define J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function) \
EncodedJSValue JIT_OPERATION function##WithReturnAddress(ExecState*, JSCell*, StringImpl*, ReturnAddressPtr) REFERENCED_FROM_ASM WTF_INTERNAL; \
_J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(function)

#define J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function) \
EncodedJSValue JIT_OPERATION function##WithReturnAddress(ExecState*, EncodedJSValue, StringImpl*, ReturnAddressPtr) REFERENCED_FROM_ASM WTF_INTERNAL; \
_J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(function)

#define V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function) \
void JIT_OPERATION function##WithReturnAddress(ExecState*, EncodedJSValue, EncodedJSValue, StringImpl*, ReturnAddressPtr) REFERENCED_FROM_ASM WTF_INTERNAL; \
_V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(function)

} // namespace JSC

#endif // JITOperationWrappers_h

