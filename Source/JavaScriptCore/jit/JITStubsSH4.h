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

#ifndef JITStubsSH4_h
#define JITStubsSH4_h

#if !CPU(SH4)
#error "JITStubsSH4.h should only be #included if CPU(SH4)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsSH4.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#define THUNK_RETURN_ADDRESS_OFFSET 56
#define SAVED_R8_OFFSET 60

#define SYMBOL_STRING(name) #name
/* code (r4), JSStack* (r5), CallFrame* (r6), void* unused1 (r7), void* unused2(sp), VM (sp)*/

asm volatile (
".text\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "mov.l r7, @-r15" "\n"
    "mov.l r6, @-r15" "\n"
    "mov.l r5, @-r15" "\n"
    "mov.l r14, @-r15" "\n"
    "sts.l pr, @-r15" "\n"
    "mov.l r13, @-r15" "\n"
    "mov.l r11, @-r15" "\n"
    "mov.l r10, @-r15" "\n"
    "mov.l r9, @-r15" "\n"
    "mov.l r8, @-r15" "\n"
    "add #-" STRINGIZE_VALUE_OF(SAVED_R8_OFFSET) ", r15" "\n"
    "mov r6, r14" "\n"
    "jsr @r4" "\n"
    "nop" "\n"
    "add #" STRINGIZE_VALUE_OF(SAVED_R8_OFFSET) ", r15" "\n"
    "mov.l @r15+,r8" "\n"
    "mov.l @r15+,r9" "\n"
    "mov.l @r15+,r10" "\n"
    "mov.l @r15+,r11" "\n"
    "mov.l @r15+,r13" "\n"
    "lds.l @r15+,pr" "\n"
    "mov.l @r15+,r14" "\n"
    "add #12, r15" "\n"
    "rts" "\n"
    "nop" "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm volatile (
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
HIDE_SYMBOL(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
    "mov.l .L2" SYMBOL_STRING(cti_vm_throw) ",r0" "\n"
    "mov r15, r4" "\n"
    "mov.l @(r0,r12),r11" "\n"
    "jsr @r11" "\n"
    "nop" "\n"
    "add #" STRINGIZE_VALUE_OF(SAVED_R8_OFFSET) ", r15" "\n"
    "mov.l @r15+,r8" "\n"
    "mov.l @r15+,r9" "\n"
    "mov.l @r15+,r10" "\n"
    "mov.l @r15+,r11" "\n"
    "mov.l @r15+,r13" "\n"
    "lds.l @r15+,pr" "\n"
    "mov.l @r15+,r14" "\n"
    "add #12, r15" "\n"
    "rts" "\n"
    "nop" "\n"
    ".align 2" "\n"
    ".L2" SYMBOL_STRING(cti_vm_throw) ":.long " SYMBOL_STRING(cti_vm_throw) "@GOT \n"
);

asm volatile (
".globl " SYMBOL_STRING(ctiVMHandleException) "\n"
HIDE_SYMBOL(ctiVMHandleException) "\n"
SYMBOL_STRING(ctiVMHandleException) ":" "\n"
    "mov.l .L2" SYMBOL_STRING(cti_vm_handle_exception) ",r0" "\n"
    "mov r14, r4" "\n"
    "mov.l @(r0,r12),r11" "\n"
    "jsr @r11" "\n"
    // When cti_vm_handle_exception returns, r0 has callFrame and r1 has handler address
    "nop" "\n"
    "mov r0, r14" "\n"
    "lds r1, pr" "\n"
    "rts" "\n"
    "nop" "\n"
    ".align 2" "\n"
    ".L2" SYMBOL_STRING(cti_vm_handle_exception) ":.long " SYMBOL_STRING(cti_vm_handle_exception) "@GOT \n"
);

asm volatile (
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "add #" STRINGIZE_VALUE_OF(SAVED_R8_OFFSET) ", r15" "\n"
    "mov.l @r15+,r8" "\n"
    "mov.l @r15+,r9" "\n"
    "mov.l @r15+,r10" "\n"
    "mov.l @r15+,r11" "\n"
    "mov.l @r15+,r13" "\n"
    "lds.l @r15+,pr" "\n"
    "mov.l @r15+,r14" "\n"
    "add #12, r15" "\n"
    "rts" "\n"
    "nop" "\n"
);


#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm volatile( \
    ".align 2" "\n" \
    ".globl " SYMBOL_STRING(cti_##op) "\n" \
    SYMBOL_STRING(cti_##op) ":" "\n" \
    "sts pr, r11" "\n" \
    "mov.l r11, @(" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) ", r15)" "\n" \
    "mov.l .L2" SYMBOL_STRING(JITStubThunked_##op) ",r0" "\n" \
    "mov.l @(r0,r12),r11" "\n" \
    "jsr @r11" "\n" \
    "nop" "\n" \
    "mov.l @(" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) ", r15), r11 " "\n" \
    "lds r11, pr " "\n" \
    "rts" "\n" \
    "nop" "\n" \
    ".align 2" "\n" \
    ".L2" SYMBOL_STRING(JITStubThunked_##op) ":.long " SYMBOL_STRING(JITStubThunked_##op) "@GOT \n" \
    ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

static void performSH4JITAssertions()
{
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, thunkReturnAddress) == THUNK_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, savedR8) == SAVED_R8_OFFSET);
}

} // namespace JSC

#endif // JITStubsSH4_h
