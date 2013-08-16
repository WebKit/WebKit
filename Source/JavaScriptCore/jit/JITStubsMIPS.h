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

#ifndef JITStubsMIPS_h
#define JITStubsMIPS_h

#if !CPU(MIPS)
#error "JITStubsMIPS.h should only be #included if CPU(MIPS)"
#endif

#if !USE(JSVALUE32_64)
#error "JITStubsMIPS.h only implements USE(JSVALUE32_64)"
#endif

namespace JSC {

#define PRESERVED_GP_OFFSET         60
#define PRESERVED_S0_OFFSET         64
#define PRESERVED_S1_OFFSET         68
#define PRESERVED_S2_OFFSET         72
#define PRESERVED_S3_OFFSET         76
#define PRESERVED_S4_OFFSET         80
#define PRESERVED_RETURN_ADDRESS_OFFSET 84
#define THUNK_RETURN_ADDRESS_OFFSET 88
#define REGISTER_FILE_OFFSET        92
#define VM_OFFSET                  108
#define STACK_LENGTH               112

asm (
".text" "\n"
".align 2" "\n"
".set noreorder" "\n"
".set nomacro" "\n"
".set nomips16" "\n"
".globl " SYMBOL_STRING(ctiTrampoline) "\n"
".ent " SYMBOL_STRING(ctiTrampoline) "\n"
SYMBOL_STRING(ctiTrampoline) ":" "\n"
    "addiu $29,$29,-" STRINGIZE_VALUE_OF(STACK_LENGTH) "\n"
    "sw    $31," STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "($29)" "\n"
    "sw    $20," STRINGIZE_VALUE_OF(PRESERVED_S4_OFFSET) "($29)" "\n"
    "sw    $19," STRINGIZE_VALUE_OF(PRESERVED_S3_OFFSET) "($29)" "\n"
    "sw    $18," STRINGIZE_VALUE_OF(PRESERVED_S2_OFFSET) "($29)" "\n"
    "sw    $17," STRINGIZE_VALUE_OF(PRESERVED_S1_OFFSET) "($29)" "\n"
    "sw    $16," STRINGIZE_VALUE_OF(PRESERVED_S0_OFFSET) "($29)" "\n"
#if WTF_MIPS_PIC
    "sw    $28," STRINGIZE_VALUE_OF(PRESERVED_GP_OFFSET) "($29)" "\n"
#endif
    "move  $16,$6       # set callFrameRegister" "\n"
    "li    $17,512      # set timeoutCheckRegister" "\n"
    "move  $25,$4       # move executableAddress to t9" "\n"
    "sw    $5," STRINGIZE_VALUE_OF(REGISTER_FILE_OFFSET) "($29) # store JSStack to current stack" "\n"
    "lw    $9," STRINGIZE_VALUE_OF(STACK_LENGTH + 20) "($29)    # load vm from previous stack" "\n"
    "jalr  $25" "\n"
    "sw    $9," STRINGIZE_VALUE_OF(VM_OFFSET) "($29)   # store vm to current stack" "\n"
    "lw    $16," STRINGIZE_VALUE_OF(PRESERVED_S0_OFFSET) "($29)" "\n"
    "lw    $17," STRINGIZE_VALUE_OF(PRESERVED_S1_OFFSET) "($29)" "\n"
    "lw    $18," STRINGIZE_VALUE_OF(PRESERVED_S2_OFFSET) "($29)" "\n"
    "lw    $19," STRINGIZE_VALUE_OF(PRESERVED_S3_OFFSET) "($29)" "\n"
    "lw    $20," STRINGIZE_VALUE_OF(PRESERVED_S4_OFFSET) "($29)" "\n"
    "lw    $31," STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "($29)" "\n"
    "jr    $31" "\n"
    "addiu $29,$29," STRINGIZE_VALUE_OF(STACK_LENGTH) "\n"
".set reorder" "\n"
".set macro" "\n"
".end " SYMBOL_STRING(ctiTrampoline) "\n"
".globl " SYMBOL_STRING(ctiTrampolineEnd) "\n"
HIDE_SYMBOL(ctiTrampolineEnd) "\n"
SYMBOL_STRING(ctiTrampolineEnd) ":" "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".set noreorder" "\n"
".set nomacro" "\n"
".set nomips16" "\n"
".globl " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
".ent " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
SYMBOL_STRING(ctiVMThrowTrampoline) ":" "\n"
#if WTF_MIPS_PIC
".set macro" "\n"
".cpload $31" "\n"
    "la    $25," SYMBOL_STRING(cti_vm_throw) "\n"
".set nomacro" "\n"
    "bal " SYMBOL_STRING(cti_vm_throw) "\n"
    "move  $4,$29" "\n"
#else
    "jal " SYMBOL_STRING(cti_vm_throw) "\n"
    "move  $4,$29" "\n"
#endif
    "lw    $16," STRINGIZE_VALUE_OF(PRESERVED_S0_OFFSET) "($29)" "\n"
    "lw    $17," STRINGIZE_VALUE_OF(PRESERVED_S1_OFFSET) "($29)" "\n"
    "lw    $18," STRINGIZE_VALUE_OF(PRESERVED_S2_OFFSET) "($29)" "\n"
    "lw    $19," STRINGIZE_VALUE_OF(PRESERVED_S3_OFFSET) "($29)" "\n"
    "lw    $20," STRINGIZE_VALUE_OF(PRESERVED_S4_OFFSET) "($29)" "\n"
    "lw    $31," STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "($29)" "\n"
    "jr    $31" "\n"
    "addiu $29,$29," STRINGIZE_VALUE_OF(STACK_LENGTH) "\n"
".set reorder" "\n"
".set macro" "\n"
".end " SYMBOL_STRING(ctiVMThrowTrampoline) "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".set noreorder" "\n"
".set nomacro" "\n"
".set nomips16" "\n"
".globl " SYMBOL_STRING(ctiVMHandleException) "\n"
".ent " SYMBOL_STRING(ctiVMHandleException) "\n"
SYMBOL_STRING(ctiVMHandleException) ":" "\n"
#if WTF_MIPS_PIC
".set macro" "\n"
".cpload $25" "\n"
    "la    $25," SYMBOL_STRING(cti_vm_handle_exception) "\n"
".set nomacro" "\n"
    "bal " SYMBOL_STRING(cti_vm_handle_exception) "\n"
    "move  $4,$16" "\n"
#else
    "jal " SYMBOL_STRING(cti_vm_handle_exception) "\n"
    "move  $4,$16" "\n"
#endif
    // When cti_vm_handle_exception returns, v0 has callFrame and v1 has handler address
    "move  $31,$3" "\n"
    "jr    $31" "\n"
    "move  $16,$2 " "\n"
".set reorder" "\n"
".set macro" "\n"
".end " SYMBOL_STRING(ctiVMHandleException) "\n"
);

asm (
".text" "\n"
".align 2" "\n"
".set noreorder" "\n"
".set nomacro" "\n"
".set nomips16" "\n"
".globl " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
".ent " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
SYMBOL_STRING(ctiOpThrowNotCaught) ":" "\n"
    "lw    $16," STRINGIZE_VALUE_OF(PRESERVED_S0_OFFSET) "($29)" "\n"
    "lw    $17," STRINGIZE_VALUE_OF(PRESERVED_S1_OFFSET) "($29)" "\n"
    "lw    $18," STRINGIZE_VALUE_OF(PRESERVED_S2_OFFSET) "($29)" "\n"
    "lw    $19," STRINGIZE_VALUE_OF(PRESERVED_S3_OFFSET) "($29)" "\n"
    "lw    $20," STRINGIZE_VALUE_OF(PRESERVED_S4_OFFSET) "($29)" "\n"
    "lw    $31," STRINGIZE_VALUE_OF(PRESERVED_RETURN_ADDRESS_OFFSET) "($29)" "\n"
    "jr    $31" "\n"
    "addiu $29,$29," STRINGIZE_VALUE_OF(STACK_LENGTH) "\n"
".set reorder" "\n"
".set macro" "\n"
".end " SYMBOL_STRING(ctiOpThrowNotCaught) "\n"
);


#if WTF_MIPS_PIC
#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".text" "\n" \
        ".align 2" "\n" \
        ".set noreorder" "\n" \
        ".set nomacro" "\n" \
        ".set nomips16" "\n" \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        ".ent " SYMBOL_STRING(cti_##op) "\n" \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        ".set macro" "\n" \
        ".cpload $25" "\n" \
        "sw    $31," STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "($29)" "\n" \
        "la    $25," SYMBOL_STRING(JITStubThunked_##op) "\n" \
        ".set nomacro" "\n" \
        ".reloc 1f,R_MIPS_JALR," SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "1: jalr $25" "\n" \
        "nop" "\n" \
        "lw    $31," STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "($29)" "\n" \
        "jr    $31" "\n" \
        "nop" "\n" \
        ".set reorder" "\n" \
        ".set macro" "\n" \
        ".end " SYMBOL_STRING(cti_##op) "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

#else // WTF_MIPS_PIC
#define DEFINE_STUB_FUNCTION(rtype, op) \
    extern "C" { \
        rtype JITStubThunked_##op(STUB_ARGS_DECLARATION); \
    }; \
    asm ( \
        ".text" "\n" \
        ".align 2" "\n" \
        ".set noreorder" "\n" \
        ".set nomacro" "\n" \
        ".set nomips16" "\n" \
        ".globl " SYMBOL_STRING(cti_##op) "\n" \
        ".ent " SYMBOL_STRING(cti_##op) "\n" \
        SYMBOL_STRING(cti_##op) ":" "\n" \
        "sw    $31," STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "($29)" "\n" \
        "jal " SYMBOL_STRING(JITStubThunked_##op) "\n" \
        "nop" "\n" \
        "lw    $31," STRINGIZE_VALUE_OF(THUNK_RETURN_ADDRESS_OFFSET) "($29)" "\n" \
        "jr    $31" "\n" \
        "nop" "\n" \
        ".set reorder" "\n" \
        ".set macro" "\n" \
        ".end " SYMBOL_STRING(cti_##op) "\n" \
        ); \
    rtype JITStubThunked_##op(STUB_ARGS_DECLARATION)

#endif // WTF_MIPS_PIC


static void performMIPSJITAssertions()
{
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedGP) == PRESERVED_GP_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedS0) == PRESERVED_S0_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedS1) == PRESERVED_S1_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedS2) == PRESERVED_S2_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, preservedReturnAddress) == PRESERVED_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, thunkReturnAddress) == THUNK_RETURN_ADDRESS_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, stack) == REGISTER_FILE_OFFSET);
    ASSERT(OBJECT_OFFSETOF(struct JITStackFrame, vm) == VM_OFFSET);
}

} // namespace JSC

#endif // JITStubsMIPS_h
