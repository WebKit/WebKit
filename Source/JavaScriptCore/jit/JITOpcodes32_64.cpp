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

#if ENABLE(JIT)
#if USE(JSVALUE32_64)
#include "JIT.h"

#include "JITInlineMethods.h"
#include "JITStubCall.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "LinkBuffer.h"

namespace JSC {

PassRefPtr<ExecutableMemoryHandle> JIT::privateCompileCTIMachineTrampolines(JSGlobalData* globalData, TrampolineStructure *trampolines)
{
#if ENABLE(JIT_USE_SOFT_MODULO)
    Label softModBegin = align();
    softModulo();
#endif
    // (1) This function provides fast property access for string length
    Label stringLengthBegin = align();

    // regT0 holds payload, regT1 holds tag

    Jump string_failureCases1 = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    Jump string_failureCases2 = branchPtr(NotEqual, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr));

    // Checks out okay! - get the length from the Ustring.
    load32(Address(regT0, OBJECT_OFFSETOF(JSString, m_length)), regT2);

    Jump string_failureCases3 = branch32(Above, regT2, TrustedImm32(INT_MAX));
    move(regT2, regT0);
    move(TrustedImm32(JSValue::Int32Tag), regT1);

    ret();
    
    JumpList callSlowCase;
    JumpList constructSlowCase;

    // VirtualCallLink Trampoline
    // regT1, regT0 holds callee; callFrame is moved and partially initialized.
    Label virtualCallLinkBegin = align();
    callSlowCase.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    callSlowCase.append(emitJumpIfNotType(regT0, regT1, JSFunctionType));

    // Finish canonical initialization before JS function call.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), regT1);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    // Also initialize ReturnPC for use by lazy linking and exceptions.
    preserveReturnAddressAfterCall(regT3);
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    restoreArgumentReference();
    Call callLazyLinkCall = call();
    restoreReturnAddressBeforeReturn(regT3);
    jump(regT0);

    // VirtualConstructLink Trampoline
    // regT1, regT0 holds callee; callFrame is moved and partially initialized.
    Label virtualConstructLinkBegin = align();
    constructSlowCase.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    constructSlowCase.append(emitJumpIfNotType(regT0, regT1, JSFunctionType));

    // Finish canonical initialization before JS function call.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), regT1);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    // Also initialize ReturnPC for use by lazy linking and exeptions.
    preserveReturnAddressAfterCall(regT3);
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    restoreArgumentReference();
    Call callLazyLinkConstruct = call();
    restoreReturnAddressBeforeReturn(regT3);
    jump(regT0);

    // VirtualCall Trampoline
    // regT1, regT0 holds callee; regT2 will hold the FunctionExecutable.
    Label virtualCallBegin = align();
    callSlowCase.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    callSlowCase.append(emitJumpIfNotType(regT0, regT1, JSFunctionType));

    // Finish canonical initialization before JS function call.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), regT1);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    Jump hasCodeBlock1 = branch32(GreaterThanOrEqual, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParametersForCall)), TrustedImm32(0));
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callCompileCall = call();
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    hasCodeBlock1.link(this);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_jitCodeForCallWithArityCheck)), regT0);
    jump(regT0);

    // VirtualConstruct Trampoline
    // regT1, regT0 holds callee; regT2 will hold the FunctionExecutable.
    Label virtualConstructBegin = align();
    constructSlowCase.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    constructSlowCase.append(emitJumpIfNotType(regT0, regT1, JSFunctionType));

    // Finish canonical initialization before JS function call.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_scopeChain)), regT1);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    Jump hasCodeBlock2 = branch32(GreaterThanOrEqual, Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_numParametersForConstruct)), TrustedImm32(0));
    preserveReturnAddressAfterCall(regT3);
    restoreArgumentReference();
    Call callCompileConstruct = call();
    restoreReturnAddressBeforeReturn(regT3);
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    hasCodeBlock2.link(this);
    loadPtr(Address(regT2, OBJECT_OFFSETOF(FunctionExecutable, m_jitCodeForConstructWithArityCheck)), regT0);
    jump(regT0);

    callSlowCase.link(this);
    // Finish canonical initialization before JS function call.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT2, regT2);
    emitPutCellToCallFrameHeader(regT2, RegisterFile::ScopeChain);

    // Also initialize ReturnPC and CodeBlock, like a JS function would.
    preserveReturnAddressAfterCall(regT3);
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);
    emitPutImmediateToCallFrameHeader(0, RegisterFile::CodeBlock);

    storePtr(callFrameRegister, &m_globalData->topCallFrame);
    restoreArgumentReference();
    Call callCallNotJSFunction = call();
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    restoreReturnAddressBeforeReturn(regT3);
    ret();

    constructSlowCase.link(this);
    // Finish canonical initialization before JS function call.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT2, regT2);
    emitPutCellToCallFrameHeader(regT2, RegisterFile::ScopeChain);

    // Also initialize ReturnPC and CodeBlock, like a JS function would.
    preserveReturnAddressAfterCall(regT3);
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);
    emitPutImmediateToCallFrameHeader(0, RegisterFile::CodeBlock);

    storePtr(callFrameRegister, &m_globalData->topCallFrame);
    restoreArgumentReference();
    Call callConstructNotJSFunction = call();
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, callFrameRegister);
    restoreReturnAddressBeforeReturn(regT3);
    ret();

    // NativeCall Trampoline
    Label nativeCallThunk = privateCompileCTINativeCall(globalData);    
    Label nativeConstructThunk = privateCompileCTINativeCall(globalData, true);    

    Call string_failureCases1Call = makeTailRecursiveCall(string_failureCases1);
    Call string_failureCases2Call = makeTailRecursiveCall(string_failureCases2);
    Call string_failureCases3Call = makeTailRecursiveCall(string_failureCases3);

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(*m_globalData, this);

    patchBuffer.link(string_failureCases1Call, FunctionPtr(cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases2Call, FunctionPtr(cti_op_get_by_id_string_fail));
    patchBuffer.link(string_failureCases3Call, FunctionPtr(cti_op_get_by_id_string_fail));
    patchBuffer.link(callLazyLinkCall, FunctionPtr(cti_vm_lazyLinkCall));
    patchBuffer.link(callLazyLinkConstruct, FunctionPtr(cti_vm_lazyLinkConstruct));
    patchBuffer.link(callCompileCall, FunctionPtr(cti_op_call_jitCompile));
    patchBuffer.link(callCompileConstruct, FunctionPtr(cti_op_construct_jitCompile));
    patchBuffer.link(callCallNotJSFunction, FunctionPtr(cti_op_call_NotJSFunction));
    patchBuffer.link(callConstructNotJSFunction, FunctionPtr(cti_op_construct_NotJSConstruct));

    CodeRef finalCode = patchBuffer.finalizeCode();
    RefPtr<ExecutableMemoryHandle> executableMemory = finalCode.executableMemory();

    trampolines->ctiVirtualCallLink = patchBuffer.trampolineAt(virtualCallLinkBegin);
    trampolines->ctiVirtualConstructLink = patchBuffer.trampolineAt(virtualConstructLinkBegin);
    trampolines->ctiVirtualCall = patchBuffer.trampolineAt(virtualCallBegin);
    trampolines->ctiVirtualConstruct = patchBuffer.trampolineAt(virtualConstructBegin);
    trampolines->ctiNativeCall = patchBuffer.trampolineAt(nativeCallThunk);
    trampolines->ctiNativeConstruct = patchBuffer.trampolineAt(nativeConstructThunk);
    trampolines->ctiStringLengthTrampoline = patchBuffer.trampolineAt(stringLengthBegin);

#if ENABLE(JIT_USE_SOFT_MODULO)
    trampolines->ctiSoftModulo = patchBuffer.trampolineAt(softModBegin);
#endif
    
    return executableMemory.release();
}

JIT::Label JIT::privateCompileCTINativeCall(JSGlobalData* globalData, bool isConstruct)
{
    int executableOffsetToFunction = isConstruct ? OBJECT_OFFSETOF(NativeExecutable, m_constructor) : OBJECT_OFFSETOF(NativeExecutable, m_function);

    Label nativeCallThunk = align();

    emitPutImmediateToCallFrameHeader(0, RegisterFile::CodeBlock);

#if CPU(X86)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT0);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    peek(regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ReturnPC);

    // Calling convention:      f(ecx, edx, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, X86Registers::ecx);

    subPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister); // Align stack after call.

    // call the function
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT1);
    loadPtr(Address(regT1, OBJECT_OFFSETOF(JSFunction, m_executable)), regT1);
    move(regT0, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    call(Address(regT1, executableOffsetToFunction));

    addPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister);

#elif CPU(ARM)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention:      f(r0 == regT0, r1 == regT1, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, ARMRegisters::r0);

    // call the function
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, ARMRegisters::r1);
    move(regT2, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    loadPtr(Address(ARMRegisters::r1, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    call(Address(regT2, executableOffsetToFunction));

    restoreReturnAddressBeforeReturn(regT3);
#elif CPU(SH4)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention: f(r0 == regT4, r1 == regT5, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, regT4);

    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT5);
    move(regT2, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    loadPtr(Address(regT5, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    call(Address(regT2, executableOffsetToFunction), regT0);
    restoreReturnAddressBeforeReturn(regT3);
#elif CPU(MIPS)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT0);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention:      f(a0, a1, a2, a3);
    // Host function signature: f(ExecState*);

    // Allocate stack space for 16 bytes (8-byte aligned)
    // 16 bytes (unused) for 4 arguments
    subPtr(TrustedImm32(16), stackPointerRegister);

    // Setup arg0
    move(callFrameRegister, MIPSRegisters::a0);

    // Call
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, MIPSRegisters::a2);
    loadPtr(Address(MIPSRegisters::a2, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    move(regT0, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    call(Address(regT2, executableOffsetToFunction));

    // Restore stack space
    addPtr(TrustedImm32(16), stackPointerRegister);

    restoreReturnAddressBeforeReturn(regT3);

#else
#error "JIT not supported on this platform."
    UNUSED_PARAM(executableOffsetToFunction);
    breakpoint();
#endif // CPU(X86)

    // Check for an exception
    Jump sawException = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(&globalData->exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));

    // Return.
    ret();

    // Handle an exception
    sawException.link(this);

    // Grab the return address.
    preserveReturnAddressAfterCall(regT1);

    move(TrustedImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));

    // Set the return address.
    move(TrustedImmPtr(FunctionPtr(ctiVMThrowTrampoline).value()), regT1);
    restoreReturnAddressBeforeReturn(regT1);

    ret();

    return nativeCallThunk;
}

JIT::CodeRef JIT::privateCompileCTINativeCall(JSGlobalData* globalData, NativeFunction func)
{
    Call nativeCall;

    emitPutImmediateToCallFrameHeader(0, RegisterFile::CodeBlock);

#if CPU(X86)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT0);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    peek(regT1);
    emitPutToCallFrameHeader(regT1, RegisterFile::ReturnPC);

    // Calling convention:      f(ecx, edx, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, X86Registers::ecx);

    subPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister); // Align stack after call.

    move(regT0, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.

    // call the function
    nativeCall = call();

    addPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister);

#elif CPU(ARM)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention:      f(r0 == regT0, r1 == regT1, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, ARMRegisters::r0);

    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, ARMRegisters::r1);
    move(regT2, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    loadPtr(Address(ARMRegisters::r1, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    // call the function
    nativeCall = call();

    restoreReturnAddressBeforeReturn(regT3);

#elif CPU(MIPS)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT0);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention:      f(a0, a1, a2, a3);
    // Host function signature: f(ExecState*);

    // Allocate stack space for 16 bytes (8-byte aligned)
    // 16 bytes (unused) for 4 arguments
    subPtr(TrustedImm32(16), stackPointerRegister);

    // Setup arg0
    move(callFrameRegister, MIPSRegisters::a0);

    // Call
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, MIPSRegisters::a2);
    loadPtr(Address(MIPSRegisters::a2, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);
    move(regT0, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    
    // call the function
    nativeCall = call();

    // Restore stack space
    addPtr(TrustedImm32(16), stackPointerRegister);

    restoreReturnAddressBeforeReturn(regT3);
#elif CPU(SH4)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetFromCallFrameHeaderPtr(RegisterFile::CallerFrame, regT2);
    emitGetFromCallFrameHeaderPtr(RegisterFile::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, RegisterFile::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutToCallFrameHeader(regT3, RegisterFile::ReturnPC);

    // Calling convention: f(r0 == regT4, r1 == regT5, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, regT4);

    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT5);
    move(regT2, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
    loadPtr(Address(regT5, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    // call the function
    nativeCall = call();

    restoreReturnAddressBeforeReturn(regT3);
#else
#error "JIT not supported on this platform."
    breakpoint();
#endif // CPU(X86)

    // Check for an exception
    Jump sawException = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(&globalData->exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));

    // Return.
    ret();

    // Handle an exception
    sawException.link(this);

    // Grab the return address.
    preserveReturnAddressAfterCall(regT1);

    move(TrustedImmPtr(&globalData->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof(void*));

    // Set the return address.
    move(TrustedImmPtr(FunctionPtr(ctiVMThrowTrampoline).value()), regT1);
    restoreReturnAddressBeforeReturn(regT1);

    ret();

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(*m_globalData, this);

    patchBuffer.link(nativeCall, FunctionPtr(func));
    return patchBuffer.finalizeCode();
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
        map(m_bytecodeOffset + OPCODE_LENGTH(op_mov), dst, regT1, regT0);
    }
}

void JIT::emit_op_end(Instruction* currentInstruction)
{
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

void JIT::emit_op_new_object(Instruction* currentInstruction)
{
    emitAllocateJSFinalObject(ImmPtr(m_codeBlock->globalObject()->emptyObjectStructure()), regT0, regT1);
    
    emitStoreCell(currentInstruction[1].u.operand, regT0);
}

void JIT::emitSlow_op_new_object(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITStubCall(this, cti_op_new_object).call(currentInstruction[1].u.operand);
}

void JIT::emit_op_check_has_instance(Instruction* currentInstruction)
{
    unsigned baseVal = currentInstruction[1].u.operand;

    emitLoadPayload(baseVal, regT0);

    // Check that baseVal is a cell.
    emitJumpSlowCaseIfNotJSCell(baseVal);
    
    // Check that baseVal 'ImplementsHasInstance'.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, Structure::typeInfoFlagsOffset()), TrustedImm32(ImplementsHasInstance)));
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

    // Check that proto are cells.  baseVal must be a cell - this is checked by op_check_has_instance.
    emitJumpSlowCaseIfNotJSCell(value);
    emitJumpSlowCaseIfNotJSCell(proto);
    
    // Check that prototype is an object
    loadPtr(Address(regT1, JSCell::structureOffset()), regT3);
    addSlowCase(emitJumpIfNotObject(regT3));

    // Fixme: this check is only needed because the JSC API allows HasInstance to be overridden; we should deprecate this.
    // Check that baseVal 'ImplementsDefaultHasInstance'.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, Structure::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance)));

    // Optimistically load the result true, and start looping.
    // Initially, regT1 still contains proto and regT2 still contains value.
    // As we loop regT2 will be updated with its prototype, recursively walking the prototype chain.
    move(TrustedImm32(1), regT0);
    Label loop(this);

    // Load the prototype of the cell in regT2.  If this is equal to regT1 - WIN!
    // Otherwise, check if we've hit null - if we have then drop out of the loop, if not go again.
    loadPtr(Address(regT2, JSCell::structureOffset()), regT2);
    load32(Address(regT2, Structure::prototypeOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT2);
    Jump isInstance = branchPtr(Equal, regT2, regT1);
    branchTest32(NonZero, regT2).linkTo(loop, this);

    // We get here either by dropping out of the loop, or if value was not an Object.  Result is false.
    move(TrustedImm32(0), regT0);

    // isInstance jumps right down to here, to skip setting the result to false (it has already set true).
    isInstance.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_check_has_instance(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned baseVal = currentInstruction[1].u.operand;

    linkSlowCaseIfNotJSCell(iter, baseVal);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_check_has_instance);
    stubCall.addArgument(baseVal);
    stubCall.call();
}

void JIT::emitSlow_op_instanceof(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned value = currentInstruction[2].u.operand;
    unsigned baseVal = currentInstruction[3].u.operand;
    unsigned proto = currentInstruction[4].u.operand;

    linkSlowCaseIfNotJSCell(iter, value);
    linkSlowCaseIfNotJSCell(iter, proto);
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITStubCall stubCall(this, cti_op_instanceof);
    stubCall.addArgument(value);
    stubCall.addArgument(baseVal);
    stubCall.addArgument(proto);
    stubCall.call(dst);
}

void JIT::emit_op_tear_off_activation(Instruction* currentInstruction)
{
    unsigned activation = currentInstruction[1].u.operand;
    unsigned arguments = currentInstruction[2].u.operand;
    Jump activationCreated = branch32(NotEqual, tagFor(activation), TrustedImm32(JSValue::EmptyValueTag));
    Jump argumentsNotCreated = branch32(Equal, tagFor(arguments), TrustedImm32(JSValue::EmptyValueTag));
    activationCreated.link(this);
    JITStubCall stubCall(this, cti_op_tear_off_activation);
    stubCall.addArgument(currentInstruction[1].u.operand);
    stubCall.addArgument(unmodifiedArgumentsRegister(currentInstruction[2].u.operand));
    stubCall.call();
    argumentsNotCreated.link(this);
}

void JIT::emit_op_tear_off_arguments(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;

    Jump argsNotCreated = branch32(Equal, tagFor(unmodifiedArgumentsRegister(dst)), TrustedImm32(JSValue::EmptyValueTag));
    JITStubCall stubCall(this, cti_op_tear_off_arguments);
    stubCall.addArgument(unmodifiedArgumentsRegister(dst));
    stubCall.call();
    argsNotCreated.link(this);
}

void JIT::emit_op_resolve(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.callWithValueProfiling(currentInstruction[1].u.operand, FirstProfilingSite);
}

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImm = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    addSlowCase(branchPtr(NotEqual, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr)));
    isImm.link(this);

    if (dst != src)
        emitStore(dst, regT1, regT0);
    map(m_bytecodeOffset + OPCODE_LENGTH(op_to_primitive), dst, regT1, regT0);
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
    JITStubCall stubCall(this, currentInstruction[3].u.operand ? cti_op_resolve_base_strict_put : cti_op_resolve_base);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.callWithValueProfiling(currentInstruction[1].u.operand, FirstProfilingSite);
}

void JIT::emit_op_ensure_property_exists(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_ensure_property_exists);
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_resolve_skip(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_skip);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(Imm32(currentInstruction[3].u.operand));
    stubCall.callWithValueProfiling(currentInstruction[1].u.operand, FirstProfilingSite);
}

void JIT::emit_op_resolve_global(Instruction* currentInstruction, bool dynamic)
{
    // FIXME: Optimize to use patching instead of so many memory accesses.

    unsigned dst = currentInstruction[1].u.operand;
    void* globalObject = m_codeBlock->globalObject();

    unsigned currentIndex = m_globalResolveInfoIndex++;
    GlobalResolveInfo* resolveInfoAddress = &m_codeBlock->globalResolveInfo(currentIndex);


    // Verify structure.
    move(TrustedImmPtr(globalObject), regT0);
    move(TrustedImmPtr(resolveInfoAddress), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(GlobalResolveInfo, structure)), regT1);
    addSlowCase(branchPtr(NotEqual, regT1, Address(regT0, JSCell::structureOffset())));

    // Load property.
    loadPtr(Address(regT0, OBJECT_OFFSETOF(JSGlobalObject, m_propertyStorage)), regT2);
    load32(Address(regT3, OBJECT_OFFSETOF(GlobalResolveInfo, offset)), regT3);
    load32(BaseIndex(regT2, regT3, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0); // payload
    load32(BaseIndex(regT2, regT3, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1); // tag
    emitValueProfilingSite(FirstProfilingSite);
    emitStore(dst, regT1, regT0);
    map(m_bytecodeOffset + (dynamic ? OPCODE_LENGTH(op_resolve_global_dynamic) : OPCODE_LENGTH(op_resolve_global)), dst, regT1, regT0);
}

void JIT::emitSlow_op_resolve_global(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    Identifier* ident = &m_codeBlock->identifier(currentInstruction[2].u.operand);

    unsigned currentIndex = m_globalResolveInfoIndex++;

    linkSlowCase(iter);
    JITStubCall stubCall(this, cti_op_resolve_global);
    stubCall.addArgument(TrustedImmPtr(ident));
    stubCall.addArgument(Imm32(currentIndex));
    stubCall.callWithValueProfiling(dst, SubsequentProfilingSite);
}

void JIT::emit_op_not(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoadTag(src, regT0);

    emitLoad(src, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::BooleanTag)));
    xor32(TrustedImm32(1), regT0);

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

    ASSERT((JSValue::BooleanTag + 1 == JSValue::Int32Tag) && !(JSValue::Int32Tag + 1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::BooleanTag)));
    addJump(branchTest32(Zero, regT0), target);
}

void JIT::emitSlow_op_jfalse(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);

    if (supportsFloatingPoint()) {
        // regT1 contains the tag from the hot path.
        Jump notNumber = branch32(Above, regT1, Imm32(JSValue::LowestTag));

        emitLoadDouble(cond, fpRegT0);
        emitJumpSlowToHot(branchDoubleZeroOrNaN(fpRegT0, fpRegT1), target);
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jfalse));

        notNumber.link(this);
    }

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

    ASSERT((JSValue::BooleanTag + 1 == JSValue::Int32Tag) && !(JSValue::Int32Tag + 1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::BooleanTag)));
    addJump(branchTest32(NonZero, regT0), target);
}

void JIT::emitSlow_op_jtrue(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);

    if (supportsFloatingPoint()) {
        // regT1 contains the tag from the hot path.
        Jump notNumber = branch32(Above, regT1, Imm32(JSValue::LowestTag));

        emitLoadDouble(cond, fpRegT0);
        emitJumpSlowToHot(branchDoubleNonZero(fpRegT0, fpRegT1), target);
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jtrue));

        notNumber.link(this);
    }

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

    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    addJump(branchTest8(NonZero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);

    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);

    ASSERT((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1));
    or32(TrustedImm32(1), regT1);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::NullTag)), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jneq_null(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    addJump(branchTest8(Zero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);

    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);

    ASSERT((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1));
    or32(TrustedImm32(1), regT1);
    addJump(branch32(NotEqual, regT1, TrustedImm32(JSValue::NullTag)), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jneq_ptr(Instruction* currentInstruction)
{
    unsigned src = currentInstruction[1].u.operand;
    JSCell* ptr = currentInstruction[2].u.jsCell.get();
    unsigned target = currentInstruction[3].u.operand;

    emitLoad(src, regT1, regT0);
    addJump(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)), target);
    addJump(branchPtr(NotEqual, regT0, TrustedImmPtr(ptr)), target);
}

void JIT::emit_op_jsr(Instruction* currentInstruction)
{
    int retAddrDst = currentInstruction[1].u.operand;
    int target = currentInstruction[2].u.operand;
    DataLabelPtr storeLocation = storePtrWithPatch(TrustedImmPtr(0), Address(callFrameRegister, sizeof(Register) * retAddrDst));
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
    addSlowCase(branch32(Equal, regT1, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(Equal, regT0, regT2, regT0);

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
    genericCase.append(branchPtr(NotEqual, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr)));
    genericCase.append(branchPtr(NotEqual, Address(regT2), TrustedImmPtr(m_globalData->jsStringVPtr)));

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
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_neq(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Equal, regT1, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(NotEqual, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_neq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchPtr(NotEqual, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr)));
    genericCase.append(branchPtr(NotEqual, Address(regT2), TrustedImmPtr(m_globalData->jsStringVPtr)));

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
    xor32(TrustedImm32(0x1), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src1 = currentInstruction[2].u.operand;
    unsigned src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings.
    Jump notCell = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    Jump firstNotString = branchPtr(NotEqual, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr));
    addSlowCase(branchPtr(Equal, Address(regT2), TrustedImmPtr(m_globalData->jsStringVPtr)));
    notCell.link(this);
    firstNotString.link(this);

    // Simply compare the payloads.
    if (type == OpStrictEq)
        compare32(Equal, regT0, regT2, regT0);
    else
        compare32(NotEqual, regT0, regT2, regT0);

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
    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    test8(NonZero, Address(regT1, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined), regT1);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    compare32(Equal, regT1, TrustedImm32(JSValue::NullTag), regT2);
    compare32(Equal, regT1, TrustedImm32(JSValue::UndefinedTag), regT1);
    or32(regT2, regT1);

    wasNotImmediate.link(this);

    emitStoreBool(dst, regT1);
}

void JIT::emit_op_neq_null(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    test8(Zero, Address(regT1, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined), regT1);

    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    compare32(NotEqual, regT1, TrustedImm32(JSValue::NullTag), regT2);
    compare32(NotEqual, regT1, TrustedImm32(JSValue::UndefinedTag), regT1);
    and32(regT2, regT1);

    wasNotImmediate.link(this);

    emitStoreBool(dst, regT1);
}

void JIT::emit_op_resolve_with_base(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_with_base);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.callWithValueProfiling(currentInstruction[2].u.operand, FirstProfilingSite);
}

void JIT::emit_op_resolve_with_this(Instruction* currentInstruction)
{
    JITStubCall stubCall(this, cti_op_resolve_with_this);
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[3].u.operand)));
    stubCall.addArgument(Imm32(currentInstruction[1].u.operand));
    stubCall.callWithValueProfiling(currentInstruction[2].u.operand, FirstProfilingSite);
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
        isNotObject.append(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    if (base != m_codeBlock->thisRegister() || m_codeBlock->isStrictMode()) {
        loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
        isNotObject.append(emitJumpIfNotObject(regT2));
    }

    // We could inline the case where you have a valid cache, but
    // this call doesn't seem to be hot.
    Label isObject(this);
    JITStubCall getPnamesStubCall(this, cti_op_get_pnames);
    getPnamesStubCall.addArgument(regT0);
    getPnamesStubCall.call(dst);
    load32(Address(regT0, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStringsSize)), regT3);
    store32(TrustedImm32(Int32Tag), intTagFor(i));
    store32(TrustedImm32(0), intPayloadFor(i));
    store32(TrustedImm32(Int32Tag), intTagFor(size));
    store32(regT3, payloadFor(size));
    Jump end = jump();

    isNotObject.link(this);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::NullTag)), breakTarget);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::UndefinedTag)), breakTarget);
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
    load32(intPayloadFor(i), regT0);
    Jump end = branch32(Equal, regT0, intPayloadFor(size));

    // Grab key @ i
    loadPtr(payloadFor(it), regT1);
    loadPtr(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStrings)), regT2);
    load32(BaseIndex(regT2, regT0, TimesEight), regT2);
    store32(TrustedImm32(JSValue::CellTag), tagFor(dst));
    store32(regT2, payloadFor(dst));

    // Increment i
    add32(TrustedImm32(1), regT0);
    store32(regT0, intPayloadFor(i));

    // Verify that i is valid:
    loadPtr(payloadFor(base), regT0);

    // Test base's structure
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure)))));

    // Test base's prototype chain
    loadPtr(Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedPrototypeChain))), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(StructureChain, m_vector)), regT3);
    addJump(branchTestPtr(Zero, Address(regT3)), target);

    Label checkPrototype(this);
    callHasProperty.append(branch32(Equal, Address(regT2, Structure::prototypeOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::NullTag)));
    loadPtr(Address(regT2, Structure::prototypeOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT2);
    loadPtr(Address(regT2, JSCell::structureOffset()), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(regT3)));
    addPtr(TrustedImm32(sizeof(Structure*)), regT3);
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

    Jump isInt32 = branch32(Equal, regT1, TrustedImm32(JSValue::Int32Tag));
    addSlowCase(branch32(AboveOrEqual, regT1, TrustedImm32(JSValue::EmptyValueTag)));
    isInt32.link(this);

    if (src != dst)
        emitStore(dst, regT1, regT0);
    map(m_bytecodeOffset + OPCODE_LENGTH(op_to_jsnumber), dst, regT1, regT0);
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
    stubCall.addArgument(TrustedImmPtr(&m_codeBlock->identifier(currentInstruction[2].u.operand)));
    stubCall.addArgument(currentInstruction[3].u.operand);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_catch(Instruction* currentInstruction)
{
    // cti_op_throw returns the callFrame for the handler.
    move(regT0, callFrameRegister);

    // Now store the exception returned by cti_op_throw.
    loadPtr(Address(stackPointerRegister, OBJECT_OFFSETOF(struct JITStackFrame, globalData)), regT3);
    load32(Address(regT3, OBJECT_OFFSETOF(JSGlobalData, exception) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    load32(Address(regT3, OBJECT_OFFSETOF(JSGlobalData, exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
    store32(TrustedImm32(JSValue().payload()), Address(regT3, OBJECT_OFFSETOF(JSGlobalData, exception) + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
    store32(TrustedImm32(JSValue().tag()), Address(regT3, OBJECT_OFFSETOF(JSGlobalData, exception) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

    unsigned exception = currentInstruction[1].u.operand;
    emitStore(exception, regT1, regT0);
    map(m_bytecodeOffset + OPCODE_LENGTH(op_catch), exception, regT1, regT0);
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
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset, SwitchRecord::Immediate));
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
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset, SwitchRecord::Character));
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
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset));

    JITStubCall stubCall(this, cti_op_switch_string);
    stubCall.addArgument(scrutinee);
    stubCall.addArgument(Imm32(tableIndex));
    stubCall.call();
    jump(regT0);
}

void JIT::emit_op_throw_reference_error(Instruction* currentInstruction)
{
    unsigned message = currentInstruction[1].u.operand;

    JITStubCall stubCall(this, cti_op_throw_reference_error);
    stubCall.addArgument(m_codeBlock->getConstant(message));
    stubCall.call();
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

void JIT::emit_op_create_activation(Instruction* currentInstruction)
{
    unsigned activation = currentInstruction[1].u.operand;
    
    Jump activationCreated = branch32(NotEqual, tagFor(activation), TrustedImm32(JSValue::EmptyValueTag));
    JITStubCall(this, cti_op_push_activation).call(activation);
    activationCreated.link(this);
}

void JIT::emit_op_create_arguments(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;

    Jump argsCreated = branch32(NotEqual, tagFor(dst), TrustedImm32(JSValue::EmptyValueTag));

    if (m_codeBlock->m_numParameters == 1)
        JITStubCall(this, cti_op_create_arguments_no_params).call();
    else
        JITStubCall(this, cti_op_create_arguments).call();

    emitStore(dst, regT1, regT0);
    emitStore(unmodifiedArgumentsRegister(dst), regT1, regT0);

    argsCreated.link(this);
}

void JIT::emit_op_init_lazy_reg(Instruction* currentInstruction)
{
    unsigned dst = currentInstruction[1].u.operand;

    emitStore(dst, JSValue());
}

void JIT::emit_op_get_callee(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    emitGetFromCallFrameHeaderPtr(RegisterFile::Callee, regT0);
    emitStoreCell(dst, regT0);
}

void JIT::emit_op_create_this(Instruction* currentInstruction)
{
    emitLoad(currentInstruction[2].u.operand, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(currentInstruction[2].u.operand, regT1);
    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    addSlowCase(emitJumpIfNotObject(regT1));
    
    // now we know that the prototype is an object, but we don't know if it's got an
    // inheritor ID
    
    loadPtr(Address(regT0, JSObject::offsetOfInheritorID()), regT2);
    addSlowCase(branchTestPtr(Zero, regT2));
    
    // now regT2 contains the inheritorID, which is the structure that the newly
    // allocated object will have.
    
    emitAllocateJSFinalObject(regT2, regT0, regT1);

    emitStoreCell(currentInstruction[1].u.operand, regT0);
}

void JIT::emitSlow_op_create_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCaseIfNotJSCell(iter, currentInstruction[2].u.operand); // not a cell
    linkSlowCase(iter); // not an object
    linkSlowCase(iter); // doesn't have an inheritor ID
    linkSlowCase(iter); // allocation failed
    unsigned protoRegister = currentInstruction[2].u.operand;
    emitLoad(protoRegister, regT1, regT0);
    JITStubCall stubCall(this, cti_op_create_this);
    stubCall.addArgument(regT1, regT0);
    stubCall.call(currentInstruction[1].u.operand);
}

void JIT::emit_op_convert_this(Instruction* currentInstruction)
{
    unsigned thisRegister = currentInstruction[1].u.operand;

    emitLoad(thisRegister, regT1, regT0);

    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branchPtr(Equal, Address(regT0), TrustedImmPtr(m_globalData->jsStringVPtr)));

    map(m_bytecodeOffset + OPCODE_LENGTH(op_convert_this), thisRegister, regT1, regT0);
}

void JIT::emitSlow_op_convert_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    void* globalThis = m_codeBlock->globalObject()->globalScopeChain()->globalThis.get();
    unsigned thisRegister = currentInstruction[1].u.operand;

    linkSlowCase(iter);
    Jump isNotUndefined = branch32(NotEqual, regT1, TrustedImm32(JSValue::UndefinedTag));
    move(TrustedImmPtr(globalThis), regT0);
    move(TrustedImm32(JSValue::CellTag), regT1);
    emitStore(thisRegister, regT1, regT0);
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_convert_this));

    isNotUndefined.link(this);
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

void JIT::emit_op_get_arguments_length(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int argumentsRegister = currentInstruction[2].u.operand;
    addSlowCase(branch32(NotEqual, tagFor(argumentsRegister), TrustedImm32(JSValue::EmptyValueTag)));
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT0);
    sub32(TrustedImm32(1), regT0);
    emitStoreInt32(dst, regT0);
}

void JIT::emitSlow_op_get_arguments_length(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int ident = currentInstruction[3].u.operand;
    
    JITStubCall stubCall(this, cti_op_get_by_id_generic);
    stubCall.addArgument(base);
    stubCall.addArgument(TrustedImmPtr(&(m_codeBlock->identifier(ident))));
    stubCall.call(dst);
}

void JIT::emit_op_get_argument_by_val(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int argumentsRegister = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    addSlowCase(branch32(NotEqual, tagFor(argumentsRegister), TrustedImm32(JSValue::EmptyValueTag)));
    emitLoad(property, regT1, regT2);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::Int32Tag)));
    add32(TrustedImm32(1), regT2);
    // regT2 now contains the integer index of the argument we want, including this
    emitGetFromCallFrameHeader32(RegisterFile::ArgumentCount, regT3);
    addSlowCase(branch32(AboveOrEqual, regT2, regT3));
    
    Jump skipOutofLineParams;
    int numArgs = m_codeBlock->m_numParameters;
    if (numArgs) {
        Jump notInInPlaceArgs = branch32(AboveOrEqual, regT2, Imm32(numArgs));
        addPtr(Imm32(static_cast<unsigned>(-(RegisterFile::CallFrameHeaderSize + numArgs) * sizeof(Register))), callFrameRegister, regT1);
        loadPtr(BaseIndex(regT1, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
        loadPtr(BaseIndex(regT1, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
        skipOutofLineParams = jump();
        notInInPlaceArgs.link(this);
    }

    addPtr(Imm32(static_cast<unsigned>(-(RegisterFile::CallFrameHeaderSize + numArgs) * sizeof(Register))), callFrameRegister, regT1);
    mul32(TrustedImm32(sizeof(Register)), regT3, regT3);
    subPtr(regT3, regT1);
    loadPtr(BaseIndex(regT1, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    loadPtr(BaseIndex(regT1, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
    if (numArgs)
        skipOutofLineParams.link(this);
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_argument_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    unsigned dst = currentInstruction[1].u.operand;
    unsigned arguments = currentInstruction[2].u.operand;
    unsigned property = currentInstruction[3].u.operand;

    linkSlowCase(iter);
    Jump skipArgumentsCreation = jump();

    linkSlowCase(iter);
    linkSlowCase(iter);
    if (m_codeBlock->m_numParameters == 1)
        JITStubCall(this, cti_op_create_arguments_no_params).call();
    else
        JITStubCall(this, cti_op_create_arguments).call();
    
    emitStore(arguments, regT1, regT0);
    emitStore(unmodifiedArgumentsRegister(arguments), regT1, regT0);
    
    skipArgumentsCreation.link(this);
    JITStubCall stubCall(this, cti_op_get_by_val);
    stubCall.addArgument(arguments);
    stubCall.addArgument(property);
    stubCall.call(dst);
}

#if ENABLE(JIT_USE_SOFT_MODULO)
void JIT::softModulo()
{
    move(regT2, regT3);
    move(regT0, regT2);
    move(TrustedImm32(0), regT1);
    JumpList exitBranch;

    // Check for negative result reminder
    Jump positiveRegT3 = branch32(GreaterThanOrEqual, regT3, TrustedImm32(0));
    neg32(regT3);
    xor32(TrustedImm32(1), regT1);
    positiveRegT3.link(this);

    Jump positiveRegT2 = branch32(GreaterThanOrEqual, regT2, TrustedImm32(0));
    neg32(regT2);
    xor32(TrustedImm32(2), regT1);
    positiveRegT2.link(this);

    // Save the condition for negative reminder
    push(regT1);

    exitBranch.append(branch32(LessThan, regT2, regT3));

    // Power of two fast case
    move(regT3, regT0);
    sub32(TrustedImm32(1), regT0);
    Jump notPowerOfTwo = branchTest32(NonZero, regT0, regT3);
    and32(regT0, regT2);
    exitBranch.append(jump());

    notPowerOfTwo.link(this);

#if CPU(X86) || CPU(X86_64)
    move(regT2, regT0);
    m_assembler.cdq();
    m_assembler.idivl_r(regT3);
    move(regT1, regT2);
#elif CPU(MIPS)
    m_assembler.div(regT2, regT3);
    m_assembler.mfhi(regT2);
#else
    countLeadingZeros32(regT2, regT0);
    countLeadingZeros32(regT3, regT1);
    sub32(regT0, regT1);

    Jump useFullTable = branch32(Equal, regT1, TrustedImm32(31));

    neg32(regT1);
    add32(TrustedImm32(31), regT1);

    int elementSizeByShift = -1;
#if CPU(ARM)
    elementSizeByShift = 3;
#else
#error "JIT_USE_SOFT_MODULO not yet supported on this platform."
#endif
    relativeTableJump(regT1, elementSizeByShift);

    useFullTable.link(this);
    // Modulo table
    for (int i = 31; i > 0; --i) {
#if CPU(ARM_TRADITIONAL)
        m_assembler.cmp_r(regT2, m_assembler.lsl(regT3, i));
        m_assembler.sub_r(regT2, regT2, m_assembler.lsl(regT3, i), ARMAssembler::CS);
#elif CPU(ARM_THUMB2)
        ShiftTypeAndAmount shift(SRType_LSL, i);
        m_assembler.sub_S(regT1, regT2, regT3, shift);
        m_assembler.it(ARMv7Assembler::ConditionCS);
        m_assembler.mov(regT2, regT1);
#else
#error "JIT_USE_SOFT_MODULO not yet supported on this platform."
#endif
    }

    Jump lower = branch32(Below, regT2, regT3);
    sub32(regT3, regT2);
    lower.link(this);
#endif

    exitBranch.link(this);

    // Check for negative reminder
    pop(regT1);
    Jump positiveResult = branch32(Equal, regT1, TrustedImm32(0));
    neg32(regT2);
    positiveResult.link(this);

    move(regT2, regT0);
    ret();
}
#endif // ENABLE(JIT_USE_SOFT_MODULO)

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
