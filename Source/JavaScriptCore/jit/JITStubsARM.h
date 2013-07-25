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

#ifndef JITStubsARM_h
#define JITStubsARM_h

#if !CPU(ARM_TRADITIONAL)
#error "JITStubsARM.h should only be #included if CPU(ARM_TRADITIONAL)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsARM.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

// Also update the MSVC section (defined at DEFINE_STUB_FUNCTION)
// when changing one of the following values.
#define THUNK_RETURN_ADDRESS_OFFSET 64
#define PRESERVEDR4_OFFSET          68


#if COMPILER(GCC)

asm (
".text" "\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
INLINE_ARM_FUNCTION(ctiTrampoline)
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "stmdb sp!, {r1-r3}" "\n"
    "stmdb sp!, {r4-r6, r8-r11, lr}" "\n"
    "sub sp, sp, #" STRINGIZE_VALUE_OF(PRESERVEDR4_OFFSET) "\n"
    "mov r5, r2" "\n"
    "mov r6, #512" "\n"
    // r0 contains the code
    "blx r0" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(PRESERVEDR4_OFFSET) "\n"
    "ldmia sp!, {r4-r6, r8-r11, lr}" "\n"
    "add sp, sp, #12" "\n"
    "bx lr" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".text" "\n"
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
INLINE_ARM_FUNCTION(ctiVMThrowTrampoline)
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "mov r0, sp" "\n"
    "bl " SYMBOL_STRING(cti_vm_throw) "\n"

// Both has the same return sequence
".text" "\n"
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
INLINE_ARM_FUNCTION(ctiOpThrowNotCaught)
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(PRESERVEDR4_OFFSET) "\n"
    "ldmia sp!, {r4-r6, r8-r11, lr}" "\n"
    "add sp, sp, #12" "\n"
    "bx lr" "\n"
);

#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        INLINE_ARM_FUNCTION(cti_##op) \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        "str lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bl " SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "ldr lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bx lr" "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

#endif // COMPILER(GCC)

#if COMPILER(RVCT)

__asm EncodedJSValue ctiTrampoline(void*, JSStack*, CallFrame*, void* /*unused1*/, void* /*unused2*/, VM*)
{
    ARM
    stmdb sp!, {r1-r3}
    stmdb sp!, {r4-r6, r8-r11, lr}
    sub sp, sp, # PRESERVEDR4_OFFSET
    mov r5, r2
    mov r6, #512
    mov lr, pc
    bx r0
    add sp, sp, # PRESERVEDR4_OFFSET
    ldmia sp!, {r4-r6, r8-r11, lr}
    add sp, sp, #12
    bx lr
}
__asm void ctiTrampolineEnd()
{
}

__asm void ctiVMThrowTrampoline()
{
    ARM
    PRESERVE8
    mov r0, sp
    bl cti_vm_throw
    add sp, sp, # PRESERVEDR4_OFFSET
    ldmia sp!, {r4-r6, r8-r11, lr}
    add sp, sp, #12
    bx lr
}

__asm void ctiOpThrowNotCaught()
{
    ARM
    add sp, sp, # PRESERVEDR4_OFFSET
    ldmia sp!, {r4-r8, lr}
    add sp, sp, #12
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

#if COMPILER(MSVC)

#define DEFINE_STUB_FUNCTION(rtype, op) extern "C" rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

/* The following is a workaround for MSVC toolchain; inline assembler is not supported */

/* The following section is a template to generate code for GeneratedJITStubs_MSVC.asm */
/* The pattern "#xxx#" will be replaced with "xxx" */

/*
MSVC_BEGIN(    AREA Trampoline, CODE)
MSVC_BEGIN()
MSVC_BEGIN(    EXPORT ctiTrampoline)
MSVC_BEGIN(    EXPORT ctiTrampolineEnd)
MSVC_BEGIN(    EXPORT ctiVMThrowTrampoline)
MSVC_BEGIN(    EXPORT ctiOpThrowNotCaught)
MSVC_BEGIN()
MSVC_BEGIN(ctiTrampoline PROC)
MSVC_BEGIN(    stmdb sp!, {r1-r3})
MSVC_BEGIN(    stmdb sp!, {r4-r6, r8-r11, lr})
MSVC_BEGIN(    sub sp, sp, #68 ; sync with PRESERVEDR4_OFFSET)
MSVC_BEGIN(    mov r5, r2)
MSVC_BEGIN(    mov r6, #512)
MSVC_BEGIN(    ; r0 contains the code)
MSVC_BEGIN(    mov lr, pc)
MSVC_BEGIN(    bx r0)
MSVC_BEGIN(    add sp, sp, #68 ; sync with PRESERVEDR4_OFFSET)
MSVC_BEGIN(    ldmia sp!, {r4-r6, r8-r11, lr})
MSVC_BEGIN(    add sp, sp, #12)
MSVC_BEGIN(    bx lr)
MSVC_BEGIN(ctiTrampolineEnd)
MSVC_BEGIN(ctiTrampoline ENDP)
MSVC_BEGIN()
MSVC_BEGIN(ctiVMThrowTrampoline PROC)
MSVC_BEGIN(    mov r0, sp)
MSVC_BEGIN(    bl cti_vm_throw)
MSVC_BEGIN(ctiOpThrowNotCaught)
MSVC_BEGIN(    add sp, sp, #68 ; sync with PRESERVEDR4_OFFSET)
MSVC_BEGIN(    ldmia sp!, {r4-r6, r8-r11, lr})
MSVC_BEGIN(    add sp, sp, #12)
MSVC_BEGIN(    bx lr)
MSVC_BEGIN(ctiVMThrowTrampoline ENDP)
MSVC_BEGIN()

MSVC(    EXPORT cti_#op#)
MSVC(    IMPORT JITStubThunked_#op#)
MSVC(cti_#op# PROC)
MSVC(    str lr, [sp, #64] ; sync with THUNK_RETURN_ADDRESS_OFFSET)
MSVC(    bl JITStubThunked_#op#)
MSVC(    ldr lr, [sp, #64] ; sync with THUNK_RETURN_ADDRESS_OFFSET)
MSVC(    bx lr)
MSVC(cti_#op# ENDP)
MSVC()

MSVC_END(    END)
*/
#endif // COMPILER(MSVC)


static void performARMJITAssertions()
{
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, thunkReturnAddress) == THUNK_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedR4) == PRESERVEDR4_OFFSET);
}

} // namespace JSC

#endif // JITStubsARM_h
