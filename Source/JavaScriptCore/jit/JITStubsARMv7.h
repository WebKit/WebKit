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

#ifndef JITStubsARMv7_h
#define JITStubsARMv7_h

#if !CPU(ARM_THUMB2)
#error "JITStubsARMv7.h should only be #included if CPU(ARM_THUMB2)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsARMv7.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#define THUNK_RETURN_ADDRESS_OFFSET      0x38
#define PRESERVED_RETURN_ADDRESS_OFFSET  0x3C
#define PRESERVED_R4_OFFSET              0x40
#define PRESERVED_R5_OFFSET              0x44
#define PRESERVED_R6_OFFSET              0x48
#define PRESERVED_R7_OFFSET              0x4C
#define PRESERVED_R8_OFFSET              0x50
#define PRESERVED_R9_OFFSET              0x54
#define PRESERVED_R10_OFFSET             0x58
#define PRESERVED_R11_OFFSET             0x5C
#define REGISTER_FILE_OFFSET             0x60
#define FIRST_STACK_ARGUMENT             0x68

#if COMPILER(GCC)

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "sub sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "str lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "str r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "str r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "str r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "str r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "str r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "str r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "str r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "str r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "str r1, [sp, #" STRINGIZE_VALUE_OF(REGISTER_FILE_OFFSET) "]" "\n"
    "mov r5, r2" "\n"
    "mov r6, #512" "\n"
    "blx r0" "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "mov r0, sp" "\n"
    "bl " LOCAL_REFERENCE(cti_vm_throw) "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "ldr r11, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R11_OFFSET) "]" "\n"
    "ldr r10, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R10_OFFSET) "]" "\n"
    "ldr r9, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R9_OFFSET) "]" "\n"
    "ldr r8, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R8_OFFSET) "]" "\n"
    "ldr r7, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R7_OFFSET) "]" "\n"
    "ldr r6, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R6_OFFSET) "]" "\n"
    "ldr r5, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R5_OFFSET) "]" "\n"
    "ldr r4, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_R4_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(FIRST_STACK_ARGUMENT) "\n"
    "bx lr" "\n"
);

#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".text" "\n" \
        ".align 2" "\n" \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        HIDE_SYMBOL(cti_##op) "\n"             \
        ".thumb" "\n" \
        ".thumb_func " THUMB_FUNC_PARAM(cti_##op) "\n" \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        "str lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bl " SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "ldr lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bx lr" "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION) \

#endif // COMPILER(GCC)

#if COMPILER(RVCT)

__asm EncodedJSValue ctiTrampoline(void*, JSStack*, CallFrame*, void* /*unused1*/, void* /*unused2*/, VM*)
{
    PRESERVE8
    sub sp, sp, # FIRST_STACK_ARGUMENT
    str lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    str r4, [sp, # PRESERVED_R4_OFFSET ]
    str r5, [sp, # PRESERVED_R5_OFFSET ]
    str r6, [sp, # PRESERVED_R6_OFFSET ]
    str r7, [sp, # PRESERVED_R7_OFFSET ]
    str r8, [sp, # PRESERVED_R8_OFFSET ]
    str r9, [sp, # PRESERVED_R9_OFFSET ]
    str r10, [sp, # PRESERVED_R10_OFFSET ]
    str r11, [sp, # PRESERVED_R11_OFFSET ]
    str r1, [sp, # REGISTER_FILE_OFFSET ]
    mov r5, r2
    mov r6, #512
    blx r0
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

__asm void ctiVMThrowTrampoline()
{
    PRESERVE8
    mov r0, sp
    bl cti_vm_throw
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

__asm void ctiOpThrowNotCaught()
{
    PRESERVE8
    ldr r11, [sp, # PRESERVED_R11_OFFSET ]
    ldr r10, [sp, # PRESERVED_R10_OFFSET ]
    ldr r9, [sp, # PRESERVED_R9_OFFSET ]
    ldr r8, [sp, # PRESERVED_R8_OFFSET ]
    ldr r7, [sp, # PRESERVED_R7_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r6, [sp, # PRESERVED_R6_OFFSET ]
    ldr r5, [sp, # PRESERVED_R5_OFFSET ]
    ldr r4, [sp, # PRESERVED_R4_OFFSET ]
    ldr lr, [sp, # PRESERVED_RETURN_ADDRESS_OFFSET ]
    add sp, sp, # FIRST_STACK_ARGUMENT
    bx lr
}

#define DEFINE_STUB_FUNCTION(rtype, op) rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

/* The following is a workaround for RVCT toolchain; precompiler macros are not expanded before the code is passed to the assembler */

/* The following section is a template to generate code for GeneratedJITStubs_RVCT.h */
/* The pattern "#xxx#" will be replaced with "xxx" */

/*
RVCT(extern "C" #rtype# JITStubThunked_#op#(STUB_ARGS_DECLARATION);)
RVCT(__asm #rtype# cti_#op#(STUB_ARGS_DECLARATION))
RVCT({)
RVCT(    PRESERVE8)
RVCT(    IMPORT JITStubThunked_#op#)
RVCT(    str lr, [sp, # THUNK_RETURN_ADDRESS_OFFSET])
RVCT(    bl JITStubThunked_#op#)
RVCT(    ldr lr, [sp, # THUNK_RETURN_ADDRESS_OFFSET])
RVCT(    bx lr)
RVCT(})
RVCT()
*/

/* Include the generated file */
#include "GeneratedJITStubs_RVCT.h"

#endif // COMPILER(RVCT)


static void performARMv7JITAssertions()
{
    // Unfortunate the arm compiler does not like the use of offsetof on JITStackFrame (since it contains non POD types),
    // and the OBJECT_OFFSETOF macro does not appear constantish enough for it to be happy with its use in COMPILE_ASSERT
    // macros.
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedReturnAddress) == PRESERVED_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR4) == PRESERVED_R4_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR5) == PRESERVED_R5_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR6) == PRESERVED_R6_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR7) == PRESERVED_R7_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR8) == PRESERVED_R8_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR9) == PRESERVED_R9_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR10) == PRESERVED_R10_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR11) == PRESERVED_R11_OFFSET);

    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, stack) == REGISTER_FILE_OFFSET);
    // The fifth argument is the first item already on the stack.
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, unused1) == FIRST_STACK_ARGUMENT);

    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, thunkReturnAddress) == THUNK_RETURN_ADDRESS_OFFSET);
}

} // namespace JSC

#endif // JITStubsARMv7_h
