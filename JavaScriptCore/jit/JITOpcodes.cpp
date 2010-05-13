/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#include "JIT.h"

#if ENABLE(JIT)

#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "LinkBuffer.h"

namespace JSC {

#if !USE(JSVALUE32_64)

#define RECORD_JUMP_TARGET(targetOffset) \
   do { m_labels[m_bytecodeIndex + (targetOffset)].used(); } while (false)

void JIT::privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* globalData, TrampolineStructure *trampolines)
{
#if ENABLE(JIT_OPTIMIZE_MOD)
    Label softModBegin = align();
    softModulo();
#endif
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    // (2) The second function provides fast property access for string length
    Label stringLengthBegin = align();

    // Check eax is a string
    Jump string_failureCases1 = emitJumpIfNotJSCell(regT0);
    Jump string_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr));

    // Checks out okay! - get the length from the Ustring.
    load32(Address(regT0, OBJECT_OFFSETOF(JSString, m_length)), regT0);

    Jump string_failureCases3 = branch32(Above, regT0, Imm32(JSImmediate::maxImmediateInt));

    // regT0 contains a 64 bit value (is positive, is zero extended) so we don't need sign extend here.
    emitFastArithIntToImmNoCheck(regT0, regT0);
    
    ret();
#endif

    // (3) Trampolines for the slow cases of op_call / op_call_eval / op_construct.
    COMPILE_ASSERT(sizeof(CodeType) == 4, CodeTypeEnumMustBe32Bit);

    // VirtualCallLink Trampoline
    // regT0 holds callee, regT1 holds argCount.  regT2 will hold the FunctionExecutable.
    Label virtualCallLinkBegin = align();
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    Jump isNativeFunc2 = branch32(Equal, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), Imm32(0));

    Jump hasCodeBlock2 = branch32(GreaterThan, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), Imm32(0));
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callJSFunction2 = call();
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    emitGetJITStubArg(2, regT1); // argCount
    restoreReturnAddressBeforeReturn(regT3);
    hasCodeBlock2.link(this);

    // Check argCount matches callee arity.
    Jump arityCheckOkay2 = branch32(Equal, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), regT1);
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 1); // return address
    restoreArgumentReference();
    Call callArityCheck2 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(2, regT1); // argCount
    restoreReturnAddressBeforeReturn(regT3);
    arityCheckOkay2.link(this);

    isNativeFunc2.link(this);

    compileOpCallInitializeCallFrame();
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 1); // return address
    restoreArgumentReference();
    Call callLazyLinkCall = call();
    restoreReturnAddressBeforeReturn(regT3);
    jump(regT0);

    // VirtualCall Trampoline
    // regT0 holds callee, regT1 holds argCount.  regT2 will hold the FunctionExecutable.
    Label virtualCallBegin = align();
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    Jump isNativeFunc3 = branch32(Equal, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), Imm32(0));

    Jump hasCodeBlock3 = branch32(GreaterThan, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), Imm32(0));
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callJSFunction1 = call();
    emitGetJITStubArg(2, regT1); // argCount
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    hasCodeBlock3.link(this);
    
    // Check argCount matches callee arity.
    Jump arityCheckOkay3 = branch32(Equal, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParameters)), regT1);
    preserveReturnAddressAfterCall(regT3);
    emitPutJITStubArg(regT3, 1); // return address
    restoreArgumentReference();
    Call callArityCheck1 = call();
    move(regT1, callFrameRegister);
    emitGetJITStubArg(2, regT1); // argCount
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    arityCheckOkay3.link(this);

    isNativeFunc3.link(this);

    compileOpCallInitializeCallFrame();
    loadPtr(Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_jitCode)), regT0);
    jump(regT0);

    Label nativeCallThunk = align();
    preserveReturnAddressAfterCall(regT0);
    emitPutToCallFrameHeader(regT0, RegisterFile::ReturnPC); // Push return address

    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT1);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ScopeChain);
    

#if CPU(X86_64)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, X86Registers::ecx);

    // Allocate stack space for our arglist
    subPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
    COMPILE_ASSERT((sizeof(ArgList) & 0xf) == 0, ArgList_should_by_16byte_aligned);
    
    // Set up arguments
    subPtr(Imm32(1), X86Registers::ecx); // Don't include 'this' in argcount

    // Push argcount
    storePtr(X86Registers::ecx, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_argCount)));

    // Calculate the start of the callframe header, and store in edx
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int32_t)sizeof(Register)), callFrameRegister, X86Registers::edx);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (ecx)
    mul32(Imm32(sizeof(Register)), X86Registers::ecx, X86Registers::ecx);
    subPtr(X86Registers::ecx, X86Registers::edx);

    // push pointer to arguments
    storePtr(X86Registers::edx, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_args)));
    
    // ArgList is passed by reference so is stackPointerRegister
    move(stackPointerRegister, X86Registers::ecx);
    
    // edx currently points to the first argument, edx-sizeof(Register) points to 'this'
    loadPtr(Address(X86Registers::edx, -(int32_t)sizeof(Register)), X86Registers::edx);
    
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86Registers::esi);

    move(callFrameRegister, X86Registers::edi); 

    call(Address(X86Registers::esi, OBJECT_OFFSETOF(JSFunction, m_data)));
    
    addPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
#elif CPU(X86)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    /* We have two structs that we use to describe the stackframe we set up for our
     * call to native code.  NativeCallFrameStructure describes the how we set up the stack
     * in advance of the call.  NativeFunctionCalleeSignature describes the callframe
     * as the native code expects it.  We do this as we are using the fastcall calling
     * convention which results in the callee popping its arguments off the stack, but
     * not the rest of the callframe so we need a nice way to ensure we increment the
     * stack pointer by the right amount after the call.
     */
#if COMPILER(MSVC) || OS(LINUX)
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in EDX
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
        JSValue result;
    };
    struct NativeFunctionCalleeSignature {
        JSObject* callee;
        JSValue thisValue;
        ArgList* argPointer;
    };
#else
    struct NativeCallFrameStructure {
      //  CallFrame* callFrame; // passed in ECX
      //  JSObject* callee; // passed in EDX
        JSValue thisValue;
        ArgList* argPointer;
        ArgList args;
    };
    struct NativeFunctionCalleeSignature {
        JSValue thisValue;
        ArgList* argPointer;
    };
#endif
    const int NativeCallFrameSize = (sizeof(NativeCallFrameStructure) + 15) & ~15;
    // Allocate system stack frame
    subPtr(Imm32(NativeCallFrameSize), stackPointerRegister);

    // Set up arguments
    subPtr(Imm32(1), regT0); // Don't include 'this' in argcount

    // push argcount
    storePtr(regT0, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, args) + OBJECT_OFFSETOF(ArgList, m_argCount)));
    
    // Calculate the start of the callframe header, and store in regT1
    addPtr(Imm32(-RegisterFile::CallFrameHeaderSize * (int)sizeof(Register)), callFrameRegister, regT1);
    
    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (regT0)
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    subPtr(regT0, regT1);
    storePtr(regT1, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, args) + OBJECT_OFFSETOF(ArgList, m_args)));

    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(OBJECT_OFFSETOF(NativeCallFrameStructure, args)), stackPointerRegister, regT0);
    storePtr(regT0, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, argPointer)));

    // regT1 currently points to the first argument, regT1 - sizeof(Register) points to 'this'
    loadPtr(Address(regT1, -(int)sizeof(Register)), regT1);
    storePtr(regT1, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, thisValue)));

#if COMPILER(MSVC) || OS(LINUX)
    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(OBJECT_OFFSETOF(NativeCallFrameStructure, result)), stackPointerRegister, X86Registers::ecx);

    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86Registers::eax);
    storePtr(X86Registers::eax, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, callee)));

    // Plant callframe
    move(callFrameRegister, X86Registers::edx);

    call(Address(X86Registers::eax, OBJECT_OFFSETOF(JSFunction, m_data)));

    // JSValue is a non-POD type
    loadPtr(Address(X86Registers::eax), X86Registers::eax);
#else
    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86Registers::edx);

    // Plant callframe
    move(callFrameRegister, X86Registers::ecx);
    call(Address(X86Registers::edx, OBJECT_OFFSETOF(JSFunction, m_data)));
#endif

    // We've put a few temporaries on the stack in addition to the actual arguments
    // so pull them off now
    addPtr(Imm32(NativeCallFrameSize - sizeof(NativeFunctionCalleeSignature)), stackPointerRegister);

#elif CPU(ARM)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    // Allocate stack space for our arglist
    COMPILE_ASSERT((sizeof(ArgList) & 0x7) == 0, ArgList_should_by_8byte_aligned);
    subPtr(Imm32(sizeof(ArgList)), stackPointerRegister);

    // Set up arguments
    subPtr(Imm32(1), regT0); // Don't include 'this' in argcount

    // Push argcount
    storePtr(regT0, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_argCount)));

    // Calculate the start of the callframe header, and store in regT1
    move(callFrameRegister, regT1);
    sub32(Imm32(RegisterFile::CallFrameHeaderSize * (int32_t)sizeof(Register)), regT1);

    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (regT1)
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    subPtr(regT0, regT1);

    // push pointer to arguments
    storePtr(regT1, Address(stackPointerRegister, OBJECT_OFFSETOF(ArgList, m_args)));

    // regT1 currently points to the first argument, regT1-sizeof(Register) points to 'this'

#if OS(WINCE)
    // Setup arg3:
    loadPtr(Address(regT1, -(int32_t)sizeof(Register)), ARMRegisters::r3);

    // Setup arg2:
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT2);

    // Setup arg1:
    move(callFrameRegister, regT1);

    // Setup arg0:
    move(stackPointerRegister, regT0);
    subPtr(Imm32(sizeof(Register)), stackPointerRegister);
    storePtr(regT0, Address(stackPointerRegister));

    call(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_data)));

    loadPtr(Address(regT0), regT0);

    addPtr(Imm32(sizeof(Register) + sizeof(ArgList)), stackPointerRegister);
#else // OS(WINCE)
    // Setup arg3:
    loadPtr(Address(regT1, -(int32_t)sizeof(Register)), regT2);

    // Setup arg2:
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT1);

    // Setup arg1:
    move(callFrameRegister, regT0);

    // Setup arg4: This is a plain hack
    move(stackPointerRegister, ARMRegisters::r3);

    call(Address(regT1, OBJECT_OFFSETOF(JSFunction, m_data)));

    addPtr(Imm32(sizeof(ArgList)), stackPointerRegister);
#endif // OS(WINCE)

#elif CPU(MIPS)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    // Allocate stack space for our arglist
    COMPILE_ASSERT(!(sizeof(ArgList) & 0x7), ArgList_should_by_8byte_aligned);
    subPtr(Imm32(sizeof(ArgList) + 24), stackPointerRegister);

    // Set up arguments
    subPtr(Imm32(1), regT0); // Don't include 'this' in argcount

    // Push argcount to 24 + offset($sp)
    storePtr(regT0, Address(stackPointerRegister, 24 + OBJECT_OFFSETOF(ArgList, m_argCount)));

    // Calculate the start of the callframe header, and store in regT1
    move(callFrameRegister, regT1);
    sub32(Imm32(RegisterFile::CallFrameHeaderSize * (int32_t)sizeof(Register)), regT1);

    // Calculate start of arguments as callframe header - sizeof(Register) * argcount (regT1)
    mul32(Imm32(sizeof(Register)), regT0, regT0);
    subPtr(regT0, regT1);

    // push pointer to arguments to 24 + offset($sp)
    storePtr(regT1, Address(stackPointerRegister, 24 + OBJECT_OFFSETOF(ArgList, m_args)));

    // Setup arg3: regT1 currently points to the first argument, regT1-sizeof(Register) points to 'this'
    loadPtr(Address(regT1, -(int32_t)sizeof(Register)), MIPSRegisters::a3);

    // Setup arg2:
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, MIPSRegisters::a2);

    // Setup arg1:
    move(callFrameRegister, MIPSRegisters::a1);

    // Setup arg4: ArgList is passed by reference.  At 16($sp), store ($sp + 24)
    addPtr(Imm32(24), stackPointerRegister, regT2);
    storePtr(regT2, Address(stackPointerRegister, 16));

    // Setup arg0 as 20($sp) to hold the returned structure.
    ASSERT(sizeof(JSValue) == 4);
    addPtr(Imm32(20), stackPointerRegister, MIPSRegisters::a0);

    // Call
    call(Address(MIPSRegisters::a2, OBJECT_OFFSETOF(JSFunction, m_data)));

    // Get returned value from 0($v0) which is the same as 20($sp)
    loadPtr(Address(returnValueRegister, 0), returnValueRegister);

    // Restore stack space
    addPtr(Imm32(sizeof(ArgList) + 24), stackPointerRegister);

#elif ENABLE(JIT_OPTIMIZE_NATIVE_CALL)
#error "JIT_OPTIMIZE_NATIVE_CALL not yet supported on this platform."
#else
    breakpoint();
#endif

    // Check for an exception
    loadPtr(&(globalData->exception), regT2);
    Jump exceptionHandler = branchTestPtr(NonZero, regT2);

    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    
    // Restore our caller's "r".
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    
    // Return.
    restoreReturnAddressBeforeReturn(regT1);
    ret();

    // Handle an exception
    exceptionHandler.link(this);
    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    move(ImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    move(ImmPtr(FunctionPtr(ctiVMThrowTrampoline).value()), regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof (void*));
    restoreReturnAddressBeforeReturn(regT2);
    ret();
    

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    Call string_failureCases1Call = makeTailRecursiveCall(string_failureCases1);
    Call string_failureCases2Call = makeTailRecursiveCall(string_failureCases2);
    Call string_failureCases3Call = makeTailRecursiveCall(string_failureCases3);
#endif

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(this, m_globalData->executableAllocator.poolForSize(m_assembler.size()));

#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    patchBuffer.link(string_failureCases1Call, FunctionPtr(cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases2Call, FunctionPtr(cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases3Call, FunctionPtr(cti_op_get_by_id_string_fail));
#endif
    patchBuffer.link(callArityCheck1, FunctionPtr(cti_op_call_arityCheck));
    patchBuffer.link(callJSFunction1, FunctionPtr(cti_op_call_JSFunction));
#if ENABLE(JIT_OPTIMIZE_CALL)
    patchBuffer.link(callArityCheck2, FunctionPtr(cti_op_call_arityCheck));
    patchBuffer.link(callJSFunction2, FunctionPtr(cti_op_call_JSFunction));
    patchBuffer.link(callLazyLinkCall, FunctionPtr(cti_vm_lazyLinkCall));
#endif

    CodeRef finalCode = patchBuffer.finalizeCode();
    *executablePool = finalCode.m_executablePool;

    trampolines->ctiVirtualCallLink = trampolineAt(finalCode, virtualCallLinkBegin);
    trampolines->ctiVirtualCall = trampolineAt(finalCode, virtualCallBegin);
    trampolines->ctiNativeCallThunk = adoptRef(new NativeExecutable(JITCode(JITCode::HostFunction(trampolineAt(finalCode, nativeCallThunk)))));
#if ENABLE(JIT_OPTIMIZE_MOD)
    trampolines->ctiSoftModulo = trampolineAt(finalCode, softModBegin);
#endif
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    trampolines->ctiStringLengthTrampoline = trampolineAt(finalCode, stringLengthBegin);
#else
    UNUSED_PARAM(ctiStringLengthTrampoline);
#endif
}

void JIT::emit_op_mov(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    if (m_codeBlock->isConstantRegisterIndex(src)) {
        storePtr(ImmPtr(JSValue::encode(getConstantOperand(src))), Address(callFrameRegister, dst * sizeof(Register)));
        if (dst == m_lastResultBytecodeRegister)
            killLastResultRegister();
    } else if ((src == m_lastResultBytecodeRegister) || (dst == m_lastResultBytecodeRegister)) {
        // If either the src or dst is the cached register go though
        // get/put registers to make sure we track this correctly.
        emitGetVirtualRegister(src, regT0);
        emitPutVirtualRegister(dst);
    } else {
        // Perform the copy via regT1; do not disturb any mapping in regT0.
        loadPtr(Address(callFrameRegister, src * sizeof(Register)), regT1);
        storePtr(regT1, Address(callFrameRegister, dst * sizeof(Register)));
    }
}

void JIT::emit_op_end(Instruction* currentInstruction)
{
    if (m_codeBlock->needsFullScopeChain())
        JITStubCall(this, cti_op_end).call();
    ASSERT(returnValueRegister != callFrameRegister);
    emitGetVirtualRegister(currentInstruction[1].u.operand, returnValueRegister);
    restoreReturnAddressBeforeReturn(Address(callFrameRegister, RegisterFile::ReturnPC * static_cast<int>(sizeof(Register))));
    ret();
}

void JIT::emit_op_jmp(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[1].u.operand;
    addJump(jump(), target);
    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_loop_if_lesseq(Instruction* currentInstruction)
{
    emitTimeoutCheck();

    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;
    if (isOperandConstantImmediateInt(op2)) {
        emitGetVirtualRegister(op1, regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(JSVALUE64)
        int32_t op2imm = getConstantOperandImmediateInt(op2);
#else
        int32_t op2imm = static_cast<int32_t>(JSImmediate::rawValue(getConstantOperand(op2)));
#endif
        addJump(branch32(LessThanOrEqual, regT0, Imm32(op2imm)), target);
    } else {
        emitGetVirtualRegisters(op1, regT0, op2, regT1);
        emitJumpSlowCaseIfNotImmediateInteger(regT0);
        emitJumpSlowCaseIfNotImmediateInteger(regT1);
        addJump(branch32(LessThanOrEqual, regT0, regT1), target);
    }
}

void JIT::emit_op_new_object(Instruction* currentInstruction)
{
    JITStubCall(this, cti_op_new_object).call(currentInstruction[1].u.operand);
}

void JIT::emit_op_instanceof(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned value = currentInstruction[2].u.operand;
    unsigned baseVal = currentInstruction[3].u.operand;
    unsigned proto = currentInstruction[4].u.operand;

    // Load the operands (baseVal, proto, and value respectively) into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitGetVirtualRegister(value, regT2);
    emitGetVirtualRegister(baseVal, regT0);
    emitGetVirtualRegister(proto, regT1);

    // Check that baseVal & proto are cells.
    emitJumpSlowCaseIfNotJSCell(regT2, value);
    emitJumpSlowCaseIfNotJSCell(regT0, baseVal);
    emitJumpSlowCaseIfNotJSCell(regT1, proto);

    // Check that baseVal 'ImplementsDefaultHasInstance'.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(ImplementsDefaultHasInstance)));

    // Optimistically load the result true, and start looping.
    // Initially, regT1 still contains proto and regT2 still contains value.
    // As we loop regT2 will be updated with its prototype, recursively walking the prototype chain.
    move(ImmPtr(JSValue::encode(jsBoolean(true))), regT0);
    Label loop(this);

    // Load the prototype of the object in regT2.  If this is equal to regT1 - WIN!
    // Otherwise, check if we've hit null - if we have then drop out of the loop, if not go again.
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype)), regT2);
    Jump isInstance = branchPtr(Equal, regT2, regT1);
    emitJumpIfJSCell(regT2).linkTo(loop, this);

    // We get here either by dropping out of the loop, or if value was not an Object.  Result is false.
    move(ImmPtr(JSValue::encode(jsBoolean(false))), regT0);

    // isInstance jumps right down to here, to skip setting the result to false (it has already set true).
    isInstance.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_new_func(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_func);
    stubCall.addArgument(ImmPtr(m_codeBlock->functionDecl(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_call(Instruction* currentInstruction)
{
    compileOpCall(op_call, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_call_eval(Instruction* currentInstruction)
{
    compileOpCall(op_call_eval, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_load_varargs(Instruction* currentInstruction)
{
    int argCountDst = currentInstruction[1].u.operand;
    int argsOffset = currentInstruction[2].u.operand;

    JITStubCall stubCall(this, cti_op_load_varargs);
    stubCall.addArgument(Imm32(argsOffset));
    stubCall.call();
    // Stores a naked int32 in the register file.
    store32(returnValueRegister, Address(callFrameRegister, argCountDst * sizeof(Register)));
}

void JIT::emit_op_call_varargs(Instruction* currentInstruction)
{
    compileOpCallVarargs(currentInstruction);
}

void JIT::emit_op_construct(Instruction* currentInstruction)
{
    compileOpCall(op_construct, currentInstruction, m_callLinkInfoIndex++);
}

void JIT::emit_op_get_global_var(Instruction* currentInstruction)
{
    JSVariableObject* globalObject = static_cast<JSVariableObject*>(currentInstruction[2].u.jsCell);
    move(ImmPtr(globalObject), regT0);
    emitGetVariableObjectRegister(regT0, currentInstruction[3].u.operand, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_global_var(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[3].u.operand, regT1);
    JSVariableObject* globalObject = static_cast<JSVariableObject*>(currentInstruction[1].u.jsCell);
    move(ImmPtr(globalObject), regT0);
    emitPutVariableObjectRegister(regT1, regT0, currentInstruction[2].u.operand);
}

void JIT::emit_op_get_scoped_var(Instruction* currentInstruction)
{
    int skip = currentInstruction[3].u.operand + m_codeBlock->needsFullScopeChain();

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT0);
    while (skip--)
        loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, next)), regT0);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, object)), regT0);
    emitGetVariableObjectRegister(regT0, currentInstruction[2].u.operand, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_put_scoped_var(Instruction* currentInstruction)
{
    int skip = currentInstruction[2].u.operand + m_codeBlock->needsFullScopeChain();

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1);
    emitGetVirtualRegister(currentInstruction[3].u.operand, regT0);
    while (skip--)
        loadPtr(Address(regT1, OBJECT_OFFSETOF(ScopeChainNode, next)), regT1);

    loadPtr(Address(regT1, OBJECT_OFFSETOF(ScopeChainNode, object)), regT1);
    emitPutVariableObjectRegister(regT0, regT1, currentInstruction[1].u.operand);
}

void JIT::emit_op_tear_off_activation(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_tear_off_activation);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.call();
}

void JIT::emit_op_tear_off_arguments(Instruction*)
{
    JITStubCall(this, cti_op_tear_off_arguments).call();
}

void JIT::emit_op_ret(Instruction* currentInstruction)
{
    // We could JIT generate the deref, only calling out to C when the refcount hits zero.
    if (m_codeBlock->needsFullScopeChain())
        JITStubCall(this, cti_op_ret_scopeChain).call();

    ASSERT(callFrameRegister != regT1);
    ASSERT(regT1 != returnValueRegister);
    ASSERT(returnValueRegister != callFrameRegister);

    // Return the result in %eax.
    emitGetVirtualRegister(currentInstruction[1].u.operand, returnValueRegister);

    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);

    // Restore our caller's "r".
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    // Return.
    restoreReturnAddressBeforeReturn(regT1);
    ret();
}

void JIT::emit_op_new_array(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_array);
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(Imm32(currentInstruction[3].u.operand));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_construct_verify(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addSlowCase(branch8(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_type)), Imm32(ObjectType)));

}

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    
    Jump isImm = emitJumpIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr)));
    isImm.link(this);

    if (dst != src)
        emitPutVirtualRegister(dst);

}

void JIT::emit_op_strcat(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_strcat);
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(Imm32(currentInstruction[3].u.operand));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve_base(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve_skip(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_skip);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(Imm32(currentInstruction[3].u.operand + m_codeBlock->needsFullScopeChain()));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve_global(Instruction* currentInstruction, bool)
{
    // Fast case
    void* globalObject = currentInstruction[2].u.jsCell;
    unsigned currentIndex = m_globalResolveInfoIndex++;
    void* structureAddress = &(m_codeBlock->globalResolveInfo(currentIndex).structure);
    void* offsetAddr = &(m_codeBlock->globalResolveInfo(currentIndex).offset);

    // Check Structure of global object
    move(ImmPtr(globalObject), regT0);
    loadPtr(structureAddress, regT1);
    addSlowCase(branchPtr(NotEqual, regT1, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)))); // Structures don't match

    // Load cached property
    // Assume that the global object always uses external storage.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSGlobalObject, m_externalStorage)), regT0);
    load32(offsetAddr, regT1);
    loadPtr(BaseIndex(regT0, regT1, ScalePtr), regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_resolve_global(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    void* globalObject = currentInstruction[2].u.jsCell;
    Identifier* ident = &m_codeBlock->identifier(currentInstruction[3].u.operand);
    
    unsigned currentIndex = m_globalResolveInfoIndex++;
    
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_resolve_global);
    stubCall.addArgument(ImmPtr(globalObject));
    stubCall.addArgument(ImmPtr(ident));
    stubCall.addArgument(Imm32(currentIndex));
    stubCall.call(dst);
}

void JIT::emit_op_not(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);
    xorPtr(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool)), regT0);
    addSlowCase(branchTestPtr(NonZero, regT0, Imm32(static_cast<int32_t>(~JSImmediate::ExtendedPayloadBitBoolValue))));
    xorPtr(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool | JSImmediate::ExtendedPayloadBitBoolValue)), regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_jfalse(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[2].u.operand;
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    addJump(branchPtr(Equal, regT0, ImmPtr(JSValue::encode(jsNumber(m_globalData, 0)))), target);
    Jump isNonZero = emitJumpIfImmediateInteger(regT0);

    addJump(branchPtr(Equal, regT0, ImmPtr(JSValue::encode(jsBoolean(false)))), target);
    addSlowCase(branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(jsBoolean(true)))));

    isNonZero.link(this);
    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_jeq_null(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addJump(branchTest8(NonZero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    andPtr(Imm32(~JSImmediate::ExtendedTagBitUndefined), regT0);
    addJump(branchPtr(Equal, regT0, ImmPtr(JSValue::encode(jsNull()))), target);            

    wasNotImmediate.link(this);
    RECORD_JUMP_TARGET(target);
};
void JIT::emit_op_jneq_null(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addJump(branchTest8(Zero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    andPtr(Imm32(~JSImmediate::ExtendedTagBitUndefined), regT0);
    addJump(branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(jsNull()))), target);            

    wasNotImmediate.link(this);
    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_jneq_ptr(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    JSCell* ptr = currentInstruction[2].u.jsCell;
    unsigned target = currentInstruction[3].u.operand;
    
    emitGetVirtualRegister(src, regT0);
    addJump(branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(JSValue(ptr)))), target);            

    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_jsr(Instruction* currentInstruction)
{
    int retAddrDst = currentInstruction[1].u.operand;
    int target = currentInstruction[2].u.operand;
    DataLabelPtr storeLocation = storePtrWithPatch(ImmPtr(0), Address(callFrameRegister, sizeof(Register) * retAddrDst));
    addJump(jump(), target);
    m_jsrSites.append(JSRInfo(storeLocation, label()));
    killLastResultRegister();
    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_sret(Instruction* currentInstruction)
{
    jump(Address(callFrameRegister, sizeof(Register) * currentInstruction[1].u.operand));
    killLastResultRegister();
}

void JIT::emit_op_eq(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    set32(Equal, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_bitnot(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);
    emitJumpSlowCaseIfNotImmediateInteger(regT0);
#if USE(JSVALUE64)
    not32(regT0);
    emitFastArithIntToImmNoCheck(regT0, regT0);
#else
    xorPtr(Imm32(~JSImmediate::TagTypeNumber), regT0);
#endif
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve_with_base(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_with_base);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.call(currentInstruction[2].u.operand);
}

void JIT::emit_op_new_func_exp(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_func_exp);
    stubCall.addArgument(ImmPtr(m_codeBlock->functionExpr(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_jtrue(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[2].u.operand;
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    Jump isZero = branchPtr(Equal, regT0, ImmPtr(JSValue::encode(jsNumber(m_globalData, 0))));
    addJump(emitJumpIfImmediateInteger(regT0), target);

    addJump(branchPtr(Equal, regT0, ImmPtr(JSValue::encode(jsBoolean(true)))), target);
    addSlowCase(branchPtr(NotEqual, regT0, ImmPtr(JSValue::encode(jsBoolean(false)))));

    isZero.link(this);
    RECORD_JUMP_TARGET(target);
}

void JIT::emit_op_neq(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    set32(NotEqual, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);

    emitPutVirtualRegister(currentInstruction[1].u.operand);

}

void JIT::emit_op_bitxor(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    xorPtr(regT1, regT0);
    emitFastArithReTagImmediate(regT0, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_bitor(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    orPtr(regT1, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_throw(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_throw);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.call();
    ASSERT(regT0 == returnValueRegister);
#ifndef NDEBUG
    // cti_op_throw always changes it's return address,
    // this point in the code should never be reached.
    breakpoint();
#endif
}

void JIT::emit_op_get_pnames(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int i = currentInstruction[3].u.operand;
    int size = currentInstruction[4].u.operand;
    int breakTarget = currentInstruction[5].u.operand;

    JumpList isNotObject;

    emitGetVirtualRegister(base, regT0);
    if (!m_codeBlock->isKnownNotImmediate(base))
        isNotObject.append(emitJumpIfNotJSCell(regT0));
    if (base != m_codeBlock->thisRegister()) {
        loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
        isNotObject.append(branch8(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_type)), Imm32(ObjectType)));
    }

    // We could inline the case where you have a valid cache, but
    // this call doesn't seem to be hot.
    Label isObject(this);
    JITStubCall getPnamesStubCall(this, cti_op_get_pnames);
    getPnamesStubCall.addArgument(regT0);
    getPnamesStubCall.call(dst);
    load32(Address(regT0, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStringsSize)), regT3);
    store32(Imm32(0), addressFor(i));
    store32(regT3, addressFor(size));
    Jump end = jump();

    isNotObject.link(this);
    move(regT0, regT1);
    and32(Imm32(~JSImmediate::ExtendedTagBitUndefined), regT1);
    addJump(branch32(Equal, regT1, Imm32(JSImmediate::FullTagTypeNull)), breakTarget);

    JITStubCall toObjectStubCall(this, cti_to_object);
    toObjectStubCall.addArgument(regT0);
    toObjectStubCall.call(base);
    jump().linkTo(isObject, this);
    
    end.link(this);
}

void JIT::emit_op_next_pname(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int i = currentInstruction[3].u.operand;
    int size = currentInstruction[4].u.operand;
    int it = currentInstruction[5].u.operand;
    int target = currentInstruction[6].u.operand;
    
    JumpList callHasProperty;

    Label begin(this);
    load32(addressFor(i), regT0);
    Jump end = branch32(Equal, regT0, addressFor(size));

    // Grab key @ i
    loadPtr(addressFor(it), regT1);
    loadPtr(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStrings)), regT2);

#if USE(JSVALUE64)
    loadPtr(BaseIndex(regT2, regT0, TimesEight), regT2);
#else
    loadPtr(BaseIndex(regT2, regT0, TimesFour), regT2);
#endif

    emitPutVirtualRegister(dst, regT2);

    // Increment i
    add32(Imm32(1), regT0);
    store32(regT0, addressFor(i));

    // Verify that i is valid:
    emitGetVirtualRegister(base, regT0);

    // Test base's structure
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure)))));

    // Test base's prototype chain
    loadPtr(Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedPrototypeChain))), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(StructureChain, m_vector)), regT3);
    addJump(branchTestPtr(Zero, Address(regT3)), target);

    Label checkPrototype(this);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype)), regT2);
    callHasProperty.append(emitJumpIfNotJSCell(regT2));
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(regT3)));
    addPtr(Imm32(sizeof(Structure*)), regT3);
    branchTestPtr(NonZero, Address(regT3)).linkTo(checkPrototype, this);

    // Continue loop.
    addJump(jump(), target);

    // Slow case: Ask the object if i is valid.
    callHasProperty.link(this);
    emitGetVirtualRegister(dst, regT1);
    JITStubCall stubCall(this, cti_has_property);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call();

    // Test for valid key.
    addJump(branchTest32(NonZero, regT0), target);
    jump().linkTo(begin, this);

    // End of loop.
    end.link(this);
}

void JIT::emit_op_push_scope(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_push_scope);
    stubCall.addArgument(currentInstruction[1].u.operand, regT2);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_pop_scope(Instruction*)
{
    JITStubCall(this, cti_op_pop_scope).call();
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);

    // Jump to a slow case if either operand is a number, or if both are JSCell*s.
    move(regT0, regT2);
    orPtr(regT1, regT2);
    addSlowCase(emitJumpIfJSCell(regT2));
    addSlowCase(emitJumpIfImmediateNumber(regT2));

    if (type == OpStrictEq)
        set32(Equal, regT1, regT0, regT0);
    else
        set32(NotEqual, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);

    emitPutVirtualRegister(dst);
}

void JIT::emit_op_stricteq(Instruction* currentInstruction)
{
    compileOpStrictEq(currentInstruction, OpStrictEq);
}

void JIT::emit_op_nstricteq(Instruction* currentInstruction)
{
    compileOpStrictEq(currentInstruction, OpNStrictEq);
}

void JIT::emit_op_to_jsnumber(Instruction* currentInstruction)
{
    int srcVReg = currentInstruction[2].u.operand;
    emitGetVirtualRegister(srcVReg, regT0);
    
    Jump wasImmediate = emitJumpIfImmediateInteger(regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, srcVReg);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addSlowCase(branch8(NotEqual, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_type)), Imm32(NumberType)));
    
    wasImmediate.link(this);

    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_push_new_scope(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_push_new_scope);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand, regT2);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_catch(Instruction* currentInstruction)
{
    killLastResultRegister(); // FIXME: Implicitly treat op_catch as a labeled statement, and remove this line of code.
    peek(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof (void*));
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_jmp_scopes(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_jmp_scopes);
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.call();
    addJump(jump(), currentInstruction[2].u.operand);
    RECORD_JUMP_TARGET(currentInstruction[2].u.operand);
}

void JIT::emit_op_switch_imm(Instruction* currentInstruction)
{
    unsigned tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->immediateSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Immediate));
    jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

    JITStubCall stubCall(this, cti_op_switch_imm);
    stubCall.addArgument(scrutinee, regT2);
    stubCall.addArgument(Imm32(tableIndex));
    stubCall.call();
    jump(regT0);
}

void JIT::emit_op_switch_char(Instruction* currentInstruction)
{
    unsigned tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->characterSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Character));
    jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

    JITStubCall stubCall(this, cti_op_switch_char);
    stubCall.addArgument(scrutinee, regT2);
    stubCall.addArgument(Imm32(tableIndex));
    stubCall.call();
    jump(regT0);
}

void JIT::emit_op_switch_string(Instruction* currentInstruction)
{
    unsigned tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset));

    JITStubCall stubCall(this, cti_op_switch_string);
    stubCall.addArgument(scrutinee, regT2);
    stubCall.addArgument(Imm32(tableIndex));
    stubCall.call();
    jump(regT0);
}

void JIT::emit_op_new_error(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_error);
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(ImmPtr(JSValue::encode(m_codeBlock->getConstant(currentInstruction[3].u.operand))));
    stubCall.addArgument(Imm32(m_bytecodeIndex));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_debug(Instruction* currentInstruction)
{
#if ENABLE(DEBUG_WITH_BREAKPOINT)
    UNUSED_PARAM(currentInstruction);
    breakpoint();
#else
    JITStubCall stubCall(this, cti_op_debug);
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.addArgument(Imm32(currentInstruction[2].u.operand));
    stubCall.addArgument(Imm32(currentInstruction[3].u.operand));
    stubCall.call();
#endif
}

void JIT::emit_op_eq_null(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    setTest8(NonZero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), regT0);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    andPtr(Imm32(~JSImmediate::ExtendedTagBitUndefined), regT0);
    setPtr(Equal, regT0, Imm32(JSImmediate::FullTagTypeNull), regT0);

    wasNotImmediate.link(this);

    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);

}

void JIT::emit_op_neq_null(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    setTest8(Zero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), regT0);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    andPtr(Imm32(~JSImmediate::ExtendedTagBitUndefined), regT0);
    setPtr(NotEqual, regT0, Imm32(JSImmediate::FullTagTypeNull), regT0);

    wasNotImmediate.link(this);

    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);

}

void JIT::emit_op_enter(Instruction*)
{
    // Even though CTI doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    size_t count = m_codeBlock->m_numVars;
    for (size_t j = 0; j < count; ++j)
        emitInitRegister(j);

}

void JIT::emit_op_enter_with_activation(Instruction* currentInstruction)
{
    // Even though CTI doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    size_t count = m_codeBlock->m_numVars;
    for (size_t j = 0; j < count; ++j)
        emitInitRegister(j);

    JITStubCall(this, cti_op_push_activation).call(currentInstruction[1].u.operand);
}

void JIT::emit_op_create_arguments(Instruction*)
{
    Jump argsCreated = branchTestPtr(NonZero, Address(callFrameRegister, sizeof(Register) * RegisterFile::ArgumentsRegister));
    if (m_codeBlock->m_numParameters == 1)
        JITStubCall(this, cti_op_create_arguments_no_params).call();
    else
        JITStubCall(this, cti_op_create_arguments).call();
    argsCreated.link(this);
}
    
void JIT::emit_op_init_arguments(Instruction*)
{
    storePtr(ImmPtr(0), Address(callFrameRegister, sizeof(Register) * RegisterFile::ArgumentsRegister));
}

void JIT::emit_op_convert_this(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT1);
    addSlowCase(branchTest8(NonZero, Address(regT1, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(NeedsThisConversion)));

}

void JIT::emit_op_profile_will_call(Instruction* currentInstruction)
{
    peek(regT1, OBJECT_OFFSETOF(JITStackFrame, enabledProfilerReference) / sizeof (void*));
    Jump noProfiler = branchTestPtr(Zero, Address(regT1));

    JITStubCall stubCall(this, cti_op_profile_will_call);
    stubCall.addArgument(currentInstruction[1].u.operand, regT1);
    stubCall.call();
    noProfiler.link(this);

}

void JIT::emit_op_profile_did_call(Instruction* currentInstruction)
{
    peek(regT1, OBJECT_OFFSETOF(JITStackFrame, enabledProfilerReference) / sizeof (void*));
    Jump noProfiler = branchTestPtr(Zero, Address(regT1));

    JITStubCall stubCall(this, cti_op_profile_did_call);
    stubCall.addArgument(currentInstruction[1].u.operand, regT1);
    stubCall.call();
    noProfiler.link(this);
}


// Slow cases

void JIT::emitSlow_op_convert_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_convert_this);
    stubCall.addArgument(regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_construct_verify(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_to_primitive(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_to_primitive);
    stubCall.addArgument(regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_loop_if_lesseq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;
    if (isOperandConstantImmediateInt(op2)) {
        linkSlowCase(iter);
        JITStubCall stubCall(this, cti_op_loop_if_lesseq);
        stubCall.addArgument(regT0);
        stubCall.addArgument(currentInstruction[2].u.operand, regT2);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(NonZero, regT0), target);
    } else {
        linkSlowCase(iter);
        linkSlowCase(iter);
        JITStubCall stubCall(this, cti_op_loop_if_lesseq);
        stubCall.addArgument(regT0);
        stubCall.addArgument(regT1);
        stubCall.call();
        emitJumpSlowToHot(branchTest32(NonZero, regT0), target);
    }
}

void JIT::emitSlow_op_put_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned base = currentInstruction[1].u.operand;
    unsigned property = currentInstruction[2].u.operand;
    unsigned value = currentInstruction[3].u.operand;

    linkSlowCase(iter); // property int32 check
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base not array check
    linkSlowCase(iter); // in vector check

    JITStubCall stubPutByValCall(this, cti_op_put_by_val);
    stubPutByValCall.addArgument(regT0);
    stubPutByValCall.addArgument(property, regT2);
    stubPutByValCall.addArgument(value, regT2);
    stubPutByValCall.call();
}

void JIT::emitSlow_op_not(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    xorPtr(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool)), regT0);
    JITStubCall stubCall(this, cti_op_not);
    stubCall.addArgument(regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_jfalse(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_jtrue);
    stubCall.addArgument(regT0);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(Zero, regT0), currentInstruction[2].u.operand); // inverted!
}

void JIT::emitSlow_op_bitnot(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_bitnot);
    stubCall.addArgument(regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_jtrue(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_jtrue);
    stubCall.addArgument(regT0);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(NonZero, regT0), currentInstruction[2].u.operand);
}

void JIT::emitSlow_op_bitxor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_bitxor);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_bitor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_bitor);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_eq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_eq);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call();
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_neq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_eq);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call();
    xor32(Imm32(0x1), regT0);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_stricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_stricteq);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_nstricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_nstricteq);
    stubCall.addArgument(regT0);
    stubCall.addArgument(regT1);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_instanceof(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned value = currentInstruction[2].u.operand;
    unsigned baseVal = currentInstruction[3].u.operand;
    unsigned proto = currentInstruction[4].u.operand;

    linkSlowCaseIfNotJSCell(iter, value);
    linkSlowCaseIfNotJSCell(iter, baseVal);
    linkSlowCaseIfNotJSCell(iter, proto);
    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_instanceof);
    stubCall.addArgument(value, regT2);
    stubCall.addArgument(baseVal, regT2);
    stubCall.addArgument(proto, regT2);
    stubCall.call(dst);
}

void JIT::emitSlow_op_call(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_call);
}

void JIT::emitSlow_op_call_eval(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_call_eval);
}

void JIT::emitSlow_op_call_varargs(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallVarargsSlowCase(currentInstruction, iter);
}

void JIT::emitSlow_op_construct(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    compileOpCallSlowCase(currentInstruction, iter, m_callLinkInfoIndex++, op_construct);
}

void JIT::emitSlow_op_to_jsnumber(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCaseIfNotJSCell(iter, currentInstruction[2].u.operand);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_to_jsnumber);
    stubCall.addArgument(regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

#endif // !USE(JSVALUE32_64)

void JIT::emit_op_resolve_global_dynamic(Instruction* currentInstruction)
{
    int skip = currentInstruction[6].u.operand + m_codeBlock->needsFullScopeChain();
    
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT0);
    while (skip--) {
        loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, object)), regT1);
        addSlowCase(checkStructure(regT1, m_globalData->activationStructure.get()));
        loadPtr(Address(regT0, OBJECT_OFFSETOF(ScopeChainNode, next)), regT0);
    }
    emit_op_resolve_global(currentInstruction, true);
}

void JIT::emitSlow_op_resolve_global_dynamic(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    void* globalObject = currentInstruction[2].u.jsCell;
    Identifier* ident = &m_codeBlock->identifier(currentInstruction[3].u.operand);
    int skip = currentInstruction[6].u.operand + m_codeBlock->needsFullScopeChain();
    while (skip--)
        linkSlowCase(iter);
    JITStubCall resolveStubCall(this, cti_op_resolve);
    resolveStubCall.addArgument(ImmPtr(ident));
    resolveStubCall.call(dst);
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_resolve_global_dynamic));
    
    unsigned currentIndex = m_globalResolveInfoIndex++;
    
    linkSlowCase(iter); // We managed to skip all the nodes in the scope chain, but the cache missed.
    JITStubCall stubCall(this, cti_op_resolve_global);
    stubCall.addArgument(ImmPtr(globalObject));
    stubCall.addArgument(ImmPtr(ident));
    stubCall.addArgument(Imm32(currentIndex));
    stubCall.call(dst);
}

void JIT::emit_op_new_regexp(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_regexp);
    stubCall.addArgument(ImmPtr(m_codeBlock->regexp(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

// For both JSValue32_64 and JSValue32
#if ENABLE(JIT_OPTIMIZE_MOD)
#if CPU(ARM_TRADITIONAL)
void JIT::softModulo()
{
    push(regS0);
    push(regS1);
    push(regT1);
    push(regT3);
#if USE(JSVALUE32_64)
    m_assembler.mov_r(regT3, regT2);
    m_assembler.mov_r(regT2, regT0);
#else
    m_assembler.mov_r(regT3, m_assembler.asr(regT2, 1));
    m_assembler.mov_r(regT2, m_assembler.asr(regT0, 1));
#endif
    m_assembler.mov_r(regT1, ARMAssembler::getOp2(0));
    
    m_assembler.teq_r(regT3, ARMAssembler::getOp2(0));
    m_assembler.rsb_r(regT3, regT3, ARMAssembler::getOp2(0), ARMAssembler::MI);
    m_assembler.eor_r(regT1, regT1, ARMAssembler::getOp2(1), ARMAssembler::MI);
    
    m_assembler.teq_r(regT2, ARMAssembler::getOp2(0));
    m_assembler.rsb_r(regT2, regT2, ARMAssembler::getOp2(0), ARMAssembler::MI);
    m_assembler.eor_r(regT1, regT1, ARMAssembler::getOp2(2), ARMAssembler::MI);
    
    Jump exitBranch = branch32(LessThan, regT2, regT3);

    m_assembler.sub_r(regS1, regT3, ARMAssembler::getOp2(1));
    m_assembler.tst_r(regS1, regT3);
    m_assembler.and_r(regT2, regT2, regS1, ARMAssembler::EQ);
    m_assembler.and_r(regT0, regS1, regT3);
    Jump exitBranch2 = branchTest32(Zero, regT0);
    
    m_assembler.clz_r(regS1, regT2);
    m_assembler.clz_r(regS0, regT3);
    m_assembler.sub_r(regS0, regS0, regS1);

    m_assembler.rsbs_r(regS0, regS0, ARMAssembler::getOp2(31));

    m_assembler.mov_r(regS0, m_assembler.lsl(regS0, 1), ARMAssembler::NE);

    m_assembler.add_r(ARMRegisters::pc, ARMRegisters::pc, m_assembler.lsl(regS0, 2), ARMAssembler::NE);
    m_assembler.mov_r(regT0, regT0);
    
    for (int i = 31; i > 0; --i) {
        m_assembler.cmp_r(regT2, m_assembler.lsl(regT3, i));
        m_assembler.sub_r(regT2, regT2, m_assembler.lsl(regT3, i), ARMAssembler::CS);
    }

    m_assembler.cmp_r(regT2, regT3);
    m_assembler.sub_r(regT2, regT2, regT3, ARMAssembler::CS);
    
    exitBranch.link(this);
    exitBranch2.link(this);
    
    m_assembler.teq_r(regT1, ARMAssembler::getOp2(0));
    m_assembler.rsb_r(regT2, regT2, ARMAssembler::getOp2(0), ARMAssembler::GT);
    
#if USE(JSVALUE32_64)
    m_assembler.mov_r(regT0, regT2);
#else
    m_assembler.mov_r(regT0, m_assembler.lsl(regT2, 1));
    m_assembler.eor_r(regT0, regT0, ARMAssembler::getOp2(1));
#endif
    pop(regT3);
    pop(regT1);
    pop(regS1);
    pop(regS0);
    ret();
}
#else
#error "JIT_OPTIMIZE_MOD not yet supported on this platform."
#endif // CPU(ARM_TRADITIONAL)
#endif
} // namespace JSC

#endif // ENABLE(JIT)
