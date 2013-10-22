/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef JITStubsARM64_h
#define JITStubsARM64_h

#if !CPU(ARM64)
#error "JITStubsARM64.h should only be #included if CPU(ARM64)"
#endif

#if !USE(JSVALUE64)
#error "JITStubsARM64.h only implements USE(JSVALUE64)"
#endif

namespace JSC {

#if COMPILER(GCC)

#define THUNK_RETURN_ADDRESS_OFFSET      0x30
#define PRESERVED_RETURN_ADDRESS_OFFSET  0x38
#define PRESERVED_X19_OFFSET             0x40
#define PRESERVED_X20_OFFSET             0x48
#define PRESERVED_X21_OFFSET             0x50
#define PRESERVED_X22_OFFSET             0x58
#define PRESERVED_X23_OFFSET             0x60
#define PRESERVED_X24_OFFSET             0x68
#define PRESERVED_X25_OFFSET             0x70
#define PRESERVED_X26_OFFSET             0x78
#define PRESERVED_X27_OFFSET             0x80
#define PRESERVED_X28_OFFSET             0x88
#define REGISTER_FILE_OFFSET             0x90
#define CALLFRAME_OFFSET                 0x98
#define PROFILER_REFERENCE_OFFSET        0xa0
#define VM_OFFSET                        0xa8
#define SIZEOF_JITSTACKFRAME             0xb0

asm (
".section __TEXT,__text,regular,pure_instructions" "\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
".align 2" "\n"
HIDE_SYMBOL(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "sub sp, sp, #" STRINGIZE_VALUE_OF(SIZEOF_JITSTACKFRAME) "\n"
    "str lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "str x19, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X19_OFFSET) "]" "\n"
    "str x20, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X20_OFFSET) "]" "\n"
    "str x21, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X21_OFFSET) "]" "\n"
    "str x22, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X22_OFFSET) "]" "\n"
    "str x23, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X23_OFFSET) "]" "\n"
    "str x24, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X24_OFFSET) "]" "\n"
    "str x25, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X25_OFFSET) "]" "\n"
    "str x26, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X26_OFFSET) "]" "\n"
    "str x27, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X27_OFFSET) "]" "\n"
    "str x28, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X28_OFFSET) "]" "\n"
    "str x1, [sp, #" STRINGIZE_VALUE_OF(REGISTER_FILE_OFFSET) "]" "\n"
    "str x2, [sp, #" STRINGIZE_VALUE_OF(CALLFRAME_OFFSET) "]" "\n"
    "str x4, [sp, #" STRINGIZE_VALUE_OF(PROFILER_REFERENCE_OFFSET) "]" "\n"
    "str x5, [sp, #" STRINGIZE_VALUE_OF(VM_OFFSET) "]" "\n"
    "mov x25, x2" "\n" // callFrameRegister = ARM64Registers::x25
    "mov x26, #512" "\n" // timeoutCheckRegister = ARM64Registers::x26
    "mov x27, #0xFFFF000000000000" "\n" // tagTypeNumberRegister = ARM64Registers::x27
    "add x28, x27, #2" "\n" // ( #0xFFFF000000000002 ) tagMaskRegister = ARM64Registers::x28
    "blr x0" "\n"
    "ldr x28, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X28_OFFSET) "]" "\n"
    "ldr x27, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X27_OFFSET) "]" "\n"
    "ldr x26, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X26_OFFSET) "]" "\n"
    "ldr x25, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X25_OFFSET) "]" "\n"
    "ldr x24, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X24_OFFSET) "]" "\n"
    "ldr x23, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X23_OFFSET) "]" "\n"
    "ldr x22, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X22_OFFSET) "]" "\n"
    "ldr x21, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X21_OFFSET) "]" "\n"
    "ldr x20, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X20_OFFSET) "]" "\n"
    "ldr x19, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X19_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(SIZEOF_JITSTACKFRAME) "\n"
    "ret" "\n"

HIDE_SYMBOL(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "ldr x28, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X28_OFFSET) "]" "\n"
    "ldr x27, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X27_OFFSET) "]" "\n"
    "ldr x26, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X26_OFFSET) "]" "\n"
    "ldr x25, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X25_OFFSET) "]" "\n"
    "ldr x24, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X24_OFFSET) "]" "\n"
    "ldr x23, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X23_OFFSET) "]" "\n"
    "ldr x22, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X22_OFFSET) "]" "\n"
    "ldr x21, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X21_OFFSET) "]" "\n"
    "ldr x20, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X20_OFFSET) "]" "\n"
    "ldr x19, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_X19_OFFSET) "]" "\n"
    "ldr lr, [sp, #" STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "]" "\n"
    "add sp, sp, #" STRINGIZE_VALUE_OF(SIZEOF_JITSTACKFRAME) "\n"
    "ret" "\n"
);

#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".section __TEXT,__text,regular,pure_instructions" "\n" \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        ".align 2" "\n" \
        HIDE_SYMBOL(cti_##op) "\n"             \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        "str lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "bl " SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "ldr lr, [sp, #" STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "]" "\n" \
        "ret" "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION) \

#endif // COMPILER(GCC)

} // namespace JSC

#endif // JITStubsARM64_h
