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

#if ENABLE(JIT) && USE(JSVALUE32_64)

#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "LinkBuffer.h"

namespace JSC {

void JIT::privateCompileCTIMachineTrampolines(RefPtr<ExecutablePool>* executablePool, JSGlobalData* globalData, TrampolineStructure *trampolines)
{
#if ENABLE(JIT_OPTIMIZE_MOD)
    Label softModBegin = align();
    softModulo();
#endif
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    // (1) This function provides fast property access for string length
    Label stringLengthBegin = align();

    // regT0 holds payload, regT1 holds tag

    Jump string_failureCases1 = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));
    Jump string_failureCases2 = branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr));

    // Checks out okay! - get the length from the Ustring.
    load32(Address(regT0, OBJECT_OFFSETOF(JSString, m_length)), regT2);

    Jump string_failureCases3 = branch32(Above, regT2, Imm32(INT_MAX));
    move(regT2, regT0);
    move(Imm32(JSValue::Int32Tag), regT1);

    ret();
#endif

    // (2) Trampolines for the slow cases of op_call / op_call_eval / op_construct.

#if ENABLE(JIT_OPTIMIZE_CALL)
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
#endif // ENABLE(JIT_OPTIMIZE_CALL)

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

#if CPU(X86) || CPU(ARM_TRADITIONAL)
    Label nativeCallThunk = align();
    preserveReturnAddressAfterCall(regT0);
    emitPutToCallFrameHeader(regT0, RegisterFile::ReturnPC); // Push return address

    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT1);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ScopeChain);

#if CPU(X86)
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
#if COMPILER(MSVC)
#pragma pack(push)
#pragma pack(4)
#endif // COMPILER(MSVC)
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
#if COMPILER(MSVC)
#pragma pack(pop)
#endif // COMPILER(MSVC)
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
    loadPtr(Address(regT1, -(int)sizeof(Register) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT2);
    loadPtr(Address(regT1, -(int)sizeof(Register) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT3);
    storePtr(regT2, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, thisValue) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
    storePtr(regT3, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, thisValue) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

#if COMPILER(MSVC) || OS(LINUX)
    // ArgList is passed by reference so is stackPointerRegister + 4 * sizeof(Register)
    addPtr(Imm32(OBJECT_OFFSETOF(NativeCallFrameStructure, result)), stackPointerRegister, X86Registers::ecx);

    // Plant callee
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86Registers::eax);
    storePtr(X86Registers::eax, Address(stackPointerRegister, OBJECT_OFFSETOF(NativeCallFrameStructure, callee)));

    // Plant callframe
    move(callFrameRegister, X86Registers::edx);

    call(Address(X86Registers::eax, OBJECT_OFFSETOF(JSFunction, m_data)));

    // JSValue is a non-POD type, so eax points to it
    emitLoad(0, regT1, regT0, X86Registers::eax);
#else
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, X86Registers::edx); // callee
    move(callFrameRegister, X86Registers::ecx); // callFrame
    call(Address(X86Registers::edx, OBJECT_OFFSETOF(JSFunction, m_data)));
#endif

    // We've put a few temporaries on the stack in addition to the actual arguments
    // so pull them off now
    addPtr(Imm32(NativeCallFrameSize - sizeof(NativeFunctionCalleeSignature)), stackPointerRegister);

#elif CPU(ARM_TRADITIONAL)
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);

    // Allocate stack space for our arglist
    COMPILE_ASSERT((sizeof(ArgList) & 0x7) == 0 && sizeof(JSValue) == 8 && sizeof(Register) == 8, ArgList_should_by_8byte_aligned);
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

    // Argument passing method:
    // r0 - points to return value
    // r1 - callFrame
    // r2 - callee
    // stack: this(JSValue) and a pointer to ArgList

#if OS(WINCE)
    // Setup arg4:
    push(stackPointerRegister);

    // Setup arg3:
    // regT1 currently points to the first argument, regT1-sizeof(Register) points to 'this'
    load32(Address(regT1, -(int32_t)sizeof(void*) * 2), ARMRegisters::r3);
    push(ARMRegisters::r3);
    load32(Address(regT1, -(int32_t)sizeof(void*)), regT3);
    storePtr(regT3, Address(stackPointerRegister));

    // Setup arg2:
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT2);

    // Setup arg1:
    move(callFrameRegister, regT1);

    // Setup arg0:
    move(stackPointerRegister, regT0);

    call(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_data)));

    load32(Address(stackPointerRegister, 0), regT0);
    load32(Address(stackPointerRegister, 4), regT1);

    addPtr(Imm32(sizeof(ArgList) + 8), stackPointerRegister);
#else // OS(WINCE)
    move(stackPointerRegister, regT3);
    subPtr(Imm32(8), stackPointerRegister);
    move(stackPointerRegister, regT0);
    subPtr(Imm32(8 + 4 + 4 /* padding */), stackPointerRegister);

    // Setup arg4:
    storePtr(regT3, Address(stackPointerRegister, 8));

    // Setup arg3:
    // regT1 currently points to the first argument, regT1-sizeof(Register) points to 'this'
    load32(Address(regT1, -(int32_t)sizeof(void*) * 2), regT3);
    storePtr(regT3, Address(stackPointerRegister, 0));
    load32(Address(regT1, -(int32_t)sizeof(void*)), regT3);
    storePtr(regT3, Address(stackPointerRegister, 4));

    // Setup arg2:
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT2);

    // Setup arg1:
    move(callFrameRegister, regT1);

    call(Address(regT2, OBJECT_OFFSETOF(JSFunction, m_data)));

    // Load return value
    load32(Address(stackPointerRegister, 16), regT0);
    load32(Address(stackPointerRegister, 20), regT1);

    addPtr(Imm32(sizeof(ArgList) + 16 + 8), stackPointerRegister);
#endif // OS(WINCE)

#endif

    // Check for an exception
    move(ImmPtr(&globalData->exception), regT2);
    Jump sawException = branch32(NotEqual, tagFor(0, regT2), Imm32(JSValue::EmptyValueTag));

    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT3);

    // Restore our caller's "r".
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);

    // Return.
    restoreReturnAddressBeforeReturn(regT3);
    ret();

    // Handle an exception
    sawException.link(this);
    // Grab the return address.
    emitGetFromCallFrameHeaderPtr(RegisterFile::ReturnPC, regT1);
    move(ImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    move(ImmPtr(FunctionPtr(ctiVMThrowTrampoline).value()), regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));
    restoreReturnAddressBeforeReturn(regT2);
    ret();

#elif ENABLE(JIT_OPTIMIZE_NATIVE_CALL)
#error "JIT_OPTIMIZE_NATIVE_CALL not yet supported on this platform."
#else
    breakpoint();
#endif

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

    trampolines->ctiVirtualCall = trampolineAt(finalCode, virtualCallBegin);
    trampolines->ctiNativeCallThunk = adoptRef(new NativeExecutable(JITCode(JITCode::HostFunction(trampolineAt(finalCode, nativeCallThunk)))));
#if ENABLE(JIT_OPTIMIZE_PROPERTY_ACCESS)
    trampolines->ctiStringLengthTrampoline = trampolineAt(finalCode, stringLengthBegin);
#else
    UNUSED_PARAM(ctiStringLengthTrampoline);
#endif
#if ENABLE(JIT_OPTIMIZE_CALL)
    trampolines->ctiVirtualCallLink = trampolineAt(finalCode, virtualCallLinkBegin);
#else
    UNUSED_PARAM(ctiVirtualCallLink);
#endif
#if ENABLE(JIT_OPTIMIZE_MOD)
    trampolines->ctiSoftModulo = trampolineAt(finalCode, softModBegin);
#endif
}

void JIT::emit_op_mov(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    if (m_codeBlock->isConstantRegisterIndex(src))
        emitStore(dst, getConstantOperand(src));
    else {
        emitLoad(src, regT1, regT0);
        emitStore(dst, regT1, regT0);
        map(m_bytecodeIndex + OPCODE_LENGTH(op_mov), dst, regT1, regT0);
    }
}

void JIT::emit_op_end(Instruction* currentInstruction)
{
    if (m_codeBlock->needsFullScopeChain())
        JITStubCall(this, cti_op_end).call();
    ASSERT(returnValueRegister != callFrameRegister);
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    restoreReturnAddressBeforeReturn(Address(callFrameRegister, RegisterFile::ReturnPC * static_cast<int>(sizeof(Register))));
    ret();
}

void JIT::emit_op_jmp(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[1].u.operand;
    addJump(jump(), target);
}

void JIT::emit_op_loop_if_lesseq(Instruction* currentInstruction)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    emitTimeoutCheck();

    if (isOperandConstantImmediateInt(op1)) {
        emitLoad(op2, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        addJump(branch32(GreaterThanOrEqual, regT0, Imm32(getConstantOperand(op1).asInt32())), target);
        return;
    }

    if (isOperandConstantImmediateInt(op2)) {
        emitLoad(op1, regT1, regT0);
        addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
        addJump(branch32(LessThanOrEqual, regT0, Imm32(getConstantOperand(op2).asInt32())), target);
        return;
    }

    emitLoad2(op1, regT1, regT0, op2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag)));
    addSlowCase(branch32(NotEqual, regT3, Imm32(JSValue::Int32Tag)));
    addJump(branch32(LessThanOrEqual, regT0, regT2), target);
}

void JIT::emitSlow_op_loop_if_lesseq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned op1 = currentInstruction[1].u.operand;
    unsigned op2 = currentInstruction[2].u.operand;
    unsigned target = currentInstruction[3].u.operand;

    if (!isOperandConstantImmediateInt(op1) && !isOperandConstantImmediateInt(op2))
        linkSlowCase(iter); // int32 check
    linkSlowCase(iter); // int32 check

    JITStubCall stubCall(this, cti_op_loop_if_lesseq);
    stubCall.addArgument(op1);
    stubCall.addArgument(op2);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(NonZero, regT0), target);
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

    // Load the operands into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitLoadPayload(value, regT2);
    emitLoadPayload(baseVal, regT0);
    emitLoadPayload(proto, regT1);

    // Check that value, baseVal, and proto are cells.
    emitJumpSlowCaseIfNotJSCell(value);
    emitJumpSlowCaseIfNotJSCell(baseVal);
    emitJumpSlowCaseIfNotJSCell(proto);

    // Check that baseVal 'ImplementsDefaultHasInstance'.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(ImplementsDefaultHasInstance)));

    // Optimistically load the result true, and start looping.
    // Initially, regT1 still contains proto and regT2 still contains value.
    // As we loop regT2 will be updated with its prototype, recursively walking the prototype chain.
    move(Imm32(JSValue::TrueTag), regT0);
    Label loop(this);

    // Load the prototype of the cell in regT2.  If this is equal to regT1 - WIN!
    // Otherwise, check if we've hit null - if we have then drop out of the loop, if not go again.
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    load32(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT2);
    Jump isInstance = branchPtr(Equal, regT2, regT1);
    branchTest32(NonZero, regT2).linkTo(loop, this);

    // We get here either by dropping out of the loop, or if value was not an Object.  Result is false.
    move(Imm32(JSValue::FalseTag), regT0);

    // isInstance jumps right down to here, to skip setting the result to false (it has already set true).
    isInstance.link(this);
    emitStoreBool(dst, regT0);
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
    stubCall.addArgument(value);
    stubCall.addArgument(baseVal);
    stubCall.addArgument(proto);
    stubCall.call(dst);
}

void JIT::emit_op_new_func(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_new_func);
    stubCall.addArgument(ImmPtr(m_codeBlock->functionDecl(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_get_global_var(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(currentInstruction[2].u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int index = currentInstruction[3].u.operand;

    loadPtr(&globalObject->d()->registers, regT2);

    emitLoad(index, regT1, regT0, regT2);
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_global_var), dst, regT1, regT0);
}

void JIT::emit_op_put_global_var(Instruction* currentInstruction)
{
    JSGlobalObject* globalObject = static_cast<JSGlobalObject*>(currentInstruction[1].u.jsCell);
    ASSERT(globalObject->isGlobalObject());
    int index = currentInstruction[2].u.operand;
    int value = currentInstruction[3].u.operand;

    emitLoad(value, regT1, regT0);

    loadPtr(&globalObject->d()->registers, regT2);
    emitStore(index, regT1, regT0, regT2);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_put_global_var), value, regT1, regT0);
}

void JIT::emit_op_get_scoped_var(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int index = currentInstruction[2].u.operand;
    int skip = currentInstruction[3].u.operand + m_codeBlock->needsFullScopeChain();

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT2);
    while (skip--)
        loadPtr(Address(regT2, OBJECT_OFFSETOF(ScopeChainNode, next)), regT2);

    loadPtr(Address(regT2, OBJECT_OFFSETOF(ScopeChainNode, object)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSVariableObject, d)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSVariableObject::JSVariableObjectData, registers)), regT2);

    emitLoad(index, regT1, regT0, regT2);
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_get_scoped_var), dst, regT1, regT0);
}

void JIT::emit_op_put_scoped_var(Instruction* currentInstruction)
{
    int index = currentInstruction[1].u.operand;
    int skip = currentInstruction[2].u.operand + m_codeBlock->needsFullScopeChain();
    int value = currentInstruction[3].u.operand;

    emitLoad(value, regT1, regT0);

    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT2);
    while (skip--)
        loadPtr(Address(regT2, OBJECT_OFFSETOF(ScopeChainNode, next)), regT2);

    loadPtr(Address(regT2, OBJECT_OFFSETOF(ScopeChainNode, object)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSVariableObject, d)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSVariableObject::JSVariableObjectData, registers)), regT2);

    emitStore(index, regT1, regT0, regT2);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_put_scoped_var), value, regT1, regT0);
}

void JIT::emit_op_tear_off_activation(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_tear_off_activation);
    stubCall.addArgument(currentInstruction[1].u.operand);
    stubCall.call();
}

void JIT::emit_op_tear_off_arguments(Instruction*)
{
    JITStubCall(this, cti_op_tear_off_arguments).call();
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

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImm = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));
    addSlowCase(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr)));
    isImm.link(this);

    if (dst != src)
        emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_to_primitive), dst, regT1, regT0);
}

void JIT::emitSlow_op_to_primitive(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;

    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_to_primitive);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(dst);
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

void JIT::emit_op_resolve_global(Instruction* currentInstruction, bool dynamic)
{
    // FIXME: Optimize to use patching instead of so many memory accesses.

    unsigned dst = currentInstruction[1].u.operand;
    void* globalObject = currentInstruction[2].u.jsCell;

    unsigned currentIndex = m_globalResolveInfoIndex++;
    void* structureAddress = &(m_codeBlock->globalResolveInfo(currentIndex).structure);
    void* offsetAddr = &(m_codeBlock->globalResolveInfo(currentIndex).offset);

    // Verify structure.
    move(ImmPtr(globalObject), regT0);
    loadPtr(structureAddress, regT1);
    addSlowCase(branchPtr(NotEqual, regT1, Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure))));

    // Load property.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSGlobalObject, m_externalStorage)), regT2);
    load32(offsetAddr, regT3);
    load32(BaseIndex(regT2, regT3, TimesEight), regT0); // payload
    load32(BaseIndex(regT2, regT3, TimesEight, 4), regT1); // tag
    emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + dynamic ? OPCODE_LENGTH(op_resolve_global_dynamic) : OPCODE_LENGTH(op_resolve_global), dst, regT1, regT0);
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
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoadTag(src, regT0);

    xor32(Imm32(JSValue::FalseTag), regT0);
    addSlowCase(branchTest32(NonZero, regT0, Imm32(~1)));
    xor32(Imm32(JSValue::TrueTag), regT0);

    emitStoreBool(dst, regT0, (dst == src));
}

void JIT::emitSlow_op_not(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_not);
    stubCall.addArgument(src);
    stubCall.call(dst);
}

void JIT::emit_op_jfalse(Instruction* currentInstruction)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(cond, regT1, regT0);

    Jump isTrue = branch32(Equal, regT1, Imm32(JSValue::TrueTag));
    addJump(branch32(Equal, regT1, Imm32(JSValue::FalseTag)), target);

    Jump isNotInteger = branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag));
    Jump isTrue2 = branch32(NotEqual, regT0, Imm32(0));
    addJump(jump(), target);

    if (supportsFloatingPoint()) {
        isNotInteger.link(this);

        addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));

        zeroDouble(fpRegT0);
        emitLoadDouble(cond, fpRegT1);
        addJump(branchDouble(DoubleEqualOrUnordered, fpRegT0, fpRegT1), target);
    } else
        addSlowCase(isNotInteger);

    isTrue.link(this);
    isTrue2.link(this);
}

void JIT::emitSlow_op_jfalse(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_jtrue);
    stubCall.addArgument(cond);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(Zero, regT0), target); // Inverted.
}

void JIT::emit_op_jtrue(Instruction* currentInstruction)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(cond, regT1, regT0);

    Jump isFalse = branch32(Equal, regT1, Imm32(JSValue::FalseTag));
    addJump(branch32(Equal, regT1, Imm32(JSValue::TrueTag)), target);

    Jump isNotInteger = branch32(NotEqual, regT1, Imm32(JSValue::Int32Tag));
    Jump isFalse2 = branch32(Equal, regT0, Imm32(0));
    addJump(jump(), target);

    if (supportsFloatingPoint()) {
        isNotInteger.link(this);

        addSlowCase(branch32(Above, regT1, Imm32(JSValue::LowestTag)));

        zeroDouble(fpRegT0);
        emitLoadDouble(cond, fpRegT1);
        addJump(branchDouble(DoubleNotEqual, fpRegT0, fpRegT1), target);
    } else
        addSlowCase(isNotInteger);

    isFalse.link(this);
    isFalse2.link(this);
}

void JIT::emitSlow_op_jtrue(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_jtrue);
    stubCall.addArgument(cond);
    stubCall.call();
    emitJumpSlowToHot(branchTest32(NonZero, regT0), target);
}

void JIT::emit_op_jeq_null(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addJump(branchTest8(NonZero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target);

    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);

    set32(Equal, regT1, Imm32(JSValue::NullTag), regT2);
    set32(Equal, regT1, Imm32(JSValue::UndefinedTag), regT1);
    or32(regT2, regT1);

    addJump(branchTest32(NonZero, regT1), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jneq_null(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addJump(branchTest8(Zero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined)), target);

    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);

    set32(Equal, regT1, Imm32(JSValue::NullTag), regT2);
    set32(Equal, regT1, Imm32(JSValue::UndefinedTag), regT1);
    or32(regT2, regT1);

    addJump(branchTest32(Zero, regT1), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jneq_ptr(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    JSCell* ptr = currentInstruction[2].u.jsCell;
    unsigned target = currentInstruction[3].u.operand;

    emitLoad(src, regT1, regT0);
    addJump(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)), target);
    addJump(branchPtr(NotEqual, regT0, ImmPtr(ptr)), target);
}

void JIT::emit_op_jsr(Instruction* currentInstruction)
{
    int retAddrDst = currentInstruction[1].u.operand;
    int target = currentInstruction[2].u.operand;
    DataLabelPtr storeLocation = storePtrWithPatch(ImmPtr(0), Address(callFrameRegister, sizeof(Register) * retAddrDst));
    addJump(jump(), target);
    m_jsrSites.append(JSRInfo(storeLocation, label()));
}

void JIT::emit_op_sret(Instruction* currentInstruction)
{
    jump(Address(callFrameRegister, sizeof(Register) * currentInstruction[1].u.operand));
}

void JIT::emit_op_eq(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Equal, regT1, Imm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, Imm32(JSValue::LowestTag)));

    set8(Equal, regT0, regT2, regT0);
    or32(Imm32(JSValue::FalseTag), regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_eq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned op1 = currentInstruction[2].u.operand;
    unsigned op2 = currentInstruction[3].u.operand;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr)));
    genericCase.append(branchPtr(NotEqual, Address(regT2), ImmPtr(m_globalData->jsStringVPtr)));

    // String case.
    JITStubCall stubCallEqStrings(this, cti_op_eq_strings);
    stubCallEqStrings.addArgument(regT0);
    stubCallEqStrings.addArgument(regT2);
    stubCallEqStrings.call();
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    JITStubCall stubCallEq(this, cti_op_eq);
    stubCallEq.addArgument(op1);
    stubCallEq.addArgument(op2);
    stubCallEq.call(regT0);

    storeResult.link(this);
    or32(Imm32(JSValue::FalseTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_neq(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Equal, regT1, Imm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, Imm32(JSValue::LowestTag)));

    set8(NotEqual, regT0, regT2, regT0);
    or32(Imm32(JSValue::FalseTag), regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_neq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchPtr(NotEqual, Address(regT0), ImmPtr(m_globalData->jsStringVPtr)));
    genericCase.append(branchPtr(NotEqual, Address(regT2), ImmPtr(m_globalData->jsStringVPtr)));

    // String case.
    JITStubCall stubCallEqStrings(this, cti_op_eq_strings);
    stubCallEqStrings.addArgument(regT0);
    stubCallEqStrings.addArgument(regT2);
    stubCallEqStrings.call(regT0);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    JITStubCall stubCallEq(this, cti_op_eq);
    stubCallEq.addArgument(regT1, regT0);
    stubCallEq.addArgument(regT3, regT2);
    stubCallEq.call(regT0);

    storeResult.link(this);
    xor32(Imm32(0x1), regT0);
    or32(Imm32(JSValue::FalseTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitLoadTag(src1, regT0);
    emitLoadTag(src2, regT1);

    // Jump to a slow case if either operand is double, or if both operands are
    // cells and/or Int32s.
    move(regT0, regT2);
    and32(regT1, regT2);
    addSlowCase(branch32(Below, regT2, Imm32(JSValue::LowestTag)));
    addSlowCase(branch32(AboveOrEqual, regT2, Imm32(JSValue::CellTag)));

    if (type == OpStrictEq)
        set8(Equal, regT0, regT1, regT0);
    else
        set8(NotEqual, regT0, regT1, regT0);

    or32(Imm32(JSValue::FalseTag), regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emit_op_stricteq(Instruction* currentInstruction)
{
    compileOpStrictEq(currentInstruction, OpStrictEq);
}

void JIT::emitSlow_op_stricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_stricteq);
    stubCall.addArgument(src1);
    stubCall.addArgument(src2);
    stubCall.call(dst);
}

void JIT::emit_op_nstricteq(Instruction* currentInstruction)
{
    compileOpStrictEq(currentInstruction, OpNStrictEq);
}

void JIT::emitSlow_op_nstricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_nstricteq);
    stubCall.addArgument(src1);
    stubCall.addArgument(src2);
    stubCall.call(dst);
}

void JIT::emit_op_eq_null(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT1);
    setTest8(NonZero, Address(regT1, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), regT1);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    set8(Equal, regT1, Imm32(JSValue::NullTag), regT2);
    set8(Equal, regT1, Imm32(JSValue::UndefinedTag), regT1);
    or32(regT2, regT1);

    wasNotImmediate.link(this);

    or32(Imm32(JSValue::FalseTag), regT1);

    emitStoreBool(dst, regT1);
}

void JIT::emit_op_neq_null(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branch32(NotEqual, regT1, Imm32(JSValue::CellTag));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT1);
    setTest8(Zero, Address(regT1, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(MasqueradesAsUndefined), regT1);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    set8(NotEqual, regT1, Imm32(JSValue::NullTag), regT2);
    set8(NotEqual, regT1, Imm32(JSValue::UndefinedTag), regT1);
    and32(regT2, regT1);

    wasNotImmediate.link(this);

    or32(Imm32(JSValue::FalseTag), regT1);

    emitStoreBool(dst, regT1);
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

void JIT::emit_op_throw(Instruction* currentInstruction)
{
    unsigned exception = currentInstruction[1].u.operand;
    JITStubCall stubCall(this, cti_op_throw);
    stubCall.addArgument(exception);
    stubCall.call();

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

    emitLoad(base, regT1, regT0);
    if (!m_codeBlock->isKnownNotImmediate(base))
        isNotObject.append(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));
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
    addJump(branch32(Equal, regT1, Imm32(JSValue::NullTag)), breakTarget);
    addJump(branch32(Equal, regT1, Imm32(JSValue::UndefinedTag)), breakTarget);
    JITStubCall toObjectStubCall(this, cti_to_object);
    toObjectStubCall.addArgument(regT1, regT0);
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
    load32(BaseIndex(regT2, regT0, TimesEight), regT2);
    store32(Imm32(JSValue::CellTag), tagFor(dst));
    store32(regT2, payloadFor(dst));

    // Increment i
    add32(Imm32(1), regT0);
    store32(regT0, addressFor(i));

    // Verify that i is valid:
    loadPtr(addressFor(base), regT0);

    // Test base's structure
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure)))));

    // Test base's prototype chain
    loadPtr(Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedPrototypeChain))), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(StructureChain, m_vector)), regT3);
    addJump(branchTestPtr(Zero, Address(regT3)), target);

    Label checkPrototype(this);
    callHasProperty.append(branch32(Equal, Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), Imm32(JSValue::NullTag)));
    loadPtr(Address(regT2, OBJECT_OFFSETOF(Structure, m_prototype) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT2);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(regT3)));
    addPtr(Imm32(sizeof(Structure*)), regT3);
    branchTestPtr(NonZero, Address(regT3)).linkTo(checkPrototype, this);

    // Continue loop.
    addJump(jump(), target);

    // Slow case: Ask the object if i is valid.
    callHasProperty.link(this);
    loadPtr(addressFor(dst), regT1);
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
    stubCall.addArgument(currentInstruction[1].u.operand);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_pop_scope(Instruction*)
{
    JITStubCall(this, cti_op_pop_scope).call();
}

void JIT::emit_op_to_jsnumber(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isInt32 = branch32(Equal, regT1, Imm32(JSValue::Int32Tag));
    addSlowCase(branch32(AboveOrEqual, regT1, Imm32(JSValue::EmptyValueTag)));
    isInt32.link(this);

    if (src != dst)
        emitStore(dst, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_to_jsnumber), dst, regT1, regT0);
}

void JIT::emitSlow_op_to_jsnumber(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;

    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_to_jsnumber);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(dst);
}

void JIT::emit_op_push_new_scope(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_push_new_scope);
    stubCall.addArgument(ImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_catch(Instruction* currentInstruction)
{
    unsigned exception = currentInstruction[1].u.operand;

    // This opcode only executes after a return from cti_op_throw.

    // cti_op_throw may have taken us to a call frame further up the stack; reload
    // the call frame pointer to adjust.
    peek(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));

    // Now store the exception returned by cti_op_throw.
    emitStore(exception, regT1, regT0);
    map(m_bytecodeIndex + OPCODE_LENGTH(op_catch), exception, regT1, regT0);
}

void JIT::emit_op_jmp_scopes(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_jmp_scopes);
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.call();
    addJump(jump(), currentInstruction[2].u.operand);
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
    stubCall.addArgument(scrutinee);
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
    stubCall.addArgument(scrutinee);
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
    stubCall.addArgument(scrutinee);
    stubCall.addArgument(Imm32(tableIndex));
    stubCall.call();
    jump(regT0);
}

void JIT::emit_op_new_error(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned type = currentInstruction[2].u.operand;
    unsigned message = currentInstruction[3].u.operand;

    JITStubCall stubCall(this, cti_op_new_error);
    stubCall.addArgument(Imm32(type));
    stubCall.addArgument(m_codeBlock->getConstant(message));
    stubCall.addArgument(Imm32(m_bytecodeIndex));
    stubCall.call(dst);
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


void JIT::emit_op_enter(Instruction*)
{
    // Even though JIT code doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    for (int i = 0; i < m_codeBlock->m_numVars; ++i)
        emitStore(i, jsUndefined());
}

void JIT::emit_op_enter_with_activation(Instruction* currentInstruction)
{
    emit_op_enter(currentInstruction);

    JITStubCall(this, cti_op_push_activation).call(currentInstruction[1].u.operand);
}

void JIT::emit_op_create_arguments(Instruction*)
{
    Jump argsCreated = branch32(NotEqual, tagFor(RegisterFile::ArgumentsRegister, callFrameRegister), Imm32(JSValue::EmptyValueTag));

    // If we get here the arguments pointer is a null cell - i.e. arguments need lazy creation.
    if (m_codeBlock->m_numParameters == 1)
        JITStubCall(this, cti_op_create_arguments_no_params).call();
    else
        JITStubCall(this, cti_op_create_arguments).call();

    argsCreated.link(this);
}

void JIT::emit_op_init_arguments(Instruction*)
{
    emitStore(RegisterFile::ArgumentsRegister, JSValue(), callFrameRegister);
}

void JIT::emit_op_convert_this(Instruction* currentInstruction)
{
    unsigned thisRegister = currentInstruction[1].u.operand;

    emitLoad(thisRegister, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, Imm32(JSValue::CellTag)));

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSCell, m_structure)), regT2);
    addSlowCase(branchTest8(NonZero, Address(regT2, OBJECT_OFFSETOF(Structure, m_typeInfo.m_flags)), Imm32(NeedsThisConversion)));

    map(m_bytecodeIndex + OPCODE_LENGTH(op_convert_this), thisRegister, regT1, regT0);
}

void JIT::emitSlow_op_convert_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned thisRegister = currentInstruction[1].u.operand;

    linkSlowCase(iter);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_convert_this);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(thisRegister);
}

void JIT::emit_op_profile_will_call(Instruction* currentInstruction)
{
    peek(regT2, OBJECT_OFFSETOF(JITStackFrame, enabledProfilerReference) / sizeof(void*));
    Jump noProfiler = branchTestPtr(Zero, Address(regT2));

    JITStubCall stubCall(this, cti_op_profile_will_call);
    stubCall.addArgument(currentInstruction[1].u.operand);
    stubCall.call();
    noProfiler.link(this);
}

void JIT::emit_op_profile_did_call(Instruction* currentInstruction)
{
    peek(regT2, OBJECT_OFFSETOF(JITStackFrame, enabledProfilerReference) / sizeof(void*));
    Jump noProfiler = branchTestPtr(Zero, Address(regT2));

    JITStubCall stubCall(this, cti_op_profile_did_call);
    stubCall.addArgument(currentInstruction[1].u.operand);
    stubCall.call();
    noProfiler.link(this);
}

}

#endif // ENABLE(JIT) && USE(JSVALUE32_64)
