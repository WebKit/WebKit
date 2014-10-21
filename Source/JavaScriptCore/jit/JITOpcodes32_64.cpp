/*
 * Copyright (C) 2009, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "Debugger.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSEnvironmentRecord.h"
#include "JSFunction.h"
#include "JSPropertyNameEnumerator.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "RepatchBuffer.h"
#include "SlowPathCall.h"
#include "TypeProfilerLog.h"
#include "VirtualRegister.h"

namespace JSC {

JIT::CodeRef JIT::privateCompileCTINativeCall(VM* vm, NativeFunction func)
{
    Call nativeCall;

    emitFunctionPrologue();
    emitPutImmediateToCallFrameHeader(0, JSStack::CodeBlock);
    storePtr(callFrameRegister, &m_vm->topCallFrame);

#if CPU(X86)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetCallerFrameFromCallFrameHeaderPtr(regT0);
    emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, JSStack::ScopeChain);

    // Calling convention:      f(ecx, edx, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, X86Registers::ecx);

    subPtr(TrustedImm32(8), stackPointerRegister); // Align stack for call.
    storePtr(X86Registers::ecx, Address(stackPointerRegister));

    // call the function
    nativeCall = call();

    addPtr(TrustedImm32(8), stackPointerRegister);

#elif CPU(ARM) || CPU(SH4) || CPU(MIPS)
    // Load caller frame's scope chain into this callframe so that whatever we call can get to its global data.
    emitGetCallerFrameFromCallFrameHeaderPtr(regT2);
    emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, JSStack::ScopeChain);

#if CPU(MIPS)
    // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    subPtr(TrustedImm32(16), stackPointerRegister);
#endif

    // Calling convention is f(argumentGPR0, argumentGPR1, ...).
    // Host function signature is f(ExecState*).
    move(callFrameRegister, argumentGPR0);

    emitGetFromCallFrameHeaderPtr(JSStack::Callee, argumentGPR1);
    loadPtr(Address(argumentGPR1, OBJECT_OFFSETOF(JSFunction, m_executable)), regT2);

    // call the function
    nativeCall = call();

#if CPU(MIPS)
    // Restore stack space
    addPtr(TrustedImm32(16), stackPointerRegister);
#endif

    restoreReturnAddressBeforeReturn(regT3);
#else
#error "JIT not supported on this platform."
    abortWithReason(JITNotSupported);
#endif // CPU(X86)

    // Check for an exception
    Jump sawException = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(vm->addressOfException()) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag)); 

    emitFunctionEpilogue();
    // Return.
    ret();

    // Handle an exception
    sawException.link(this);

    storePtr(callFrameRegister, &m_vm->topCallFrame);

#if CPU(X86)
    addPtr(TrustedImm32(-4), stackPointerRegister);
    loadPtr(Address(callFrameRegister), X86Registers::ecx);
    push(X86Registers::ecx);
#else
    loadPtr(Address(callFrameRegister), argumentGPR0);
#endif
    move(TrustedImmPtr(FunctionPtr(operationVMHandleException).value()), regT3);
    call(regT3);

#if CPU(X86)
    addPtr(TrustedImm32(8), stackPointerRegister);
#endif

    jumpToExceptionHandler();

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(*m_vm, *this, GLOBAL_THUNK_ID);

    patchBuffer.link(nativeCall, FunctionPtr(func));
    return FINALIZE_CODE(patchBuffer, ("JIT CTI native call"));
}

void JIT::emit_op_mov(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;
    
    if (m_codeBlock->isConstantRegisterIndex(src))
        emitStore(dst, getConstantOperand(src));
    else {
        emitLoad(src, regT1, regT0);
        emitStore(dst, regT1, regT0);
    }
}

void JIT::emit_op_captured_mov(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    emitNotifyWrite(regT1, regT0, regT2, currentInstruction[3].u.watchpointSet);
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_end(Instruction* currentInstruction)
{
    ASSERT(returnValueGPR != callFrameRegister);
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_jmp(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[1].u.operand;
    addJump(jump(), target);
}

void JIT::emit_op_new_object(Instruction* currentInstruction)
{
    Structure* structure = currentInstruction[3].u.objectAllocationProfile->structure();
    size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
    MarkedAllocator* allocator = &m_vm->heap.allocatorForObjectWithoutDestructor(allocationSize);

    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID scratchReg = regT2;

    move(TrustedImmPtr(allocator), allocatorReg);
    emitAllocateJSObject(allocatorReg, TrustedImmPtr(structure), resultReg, scratchReg);
    emitStoreCell(currentInstruction[1].u.operand, resultReg);
}

void JIT::emitSlow_op_new_object(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    int dst = currentInstruction[1].u.operand;
    Structure* structure = currentInstruction[3].u.objectAllocationProfile->structure();
    callOperation(operationNewObject, structure);
    emitStoreCell(dst, returnValueGPR);
}

void JIT::emit_op_check_has_instance(Instruction* currentInstruction)
{
    int baseVal = currentInstruction[3].u.operand;

    emitLoadPayload(baseVal, regT0);

    // Check that baseVal is a cell.
    emitJumpSlowCaseIfNotJSCell(baseVal);
    
    // Check that baseVal 'ImplementsHasInstance'.
    addSlowCase(branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance)));
}

void JIT::emit_op_instanceof(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    int proto = currentInstruction[3].u.operand;

    // Load the operands into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitLoadPayload(value, regT2);
    emitLoadPayload(proto, regT1);

    // Check that proto are cells.  baseVal must be a cell - this is checked by op_check_has_instance.
    emitJumpSlowCaseIfNotJSCell(value);
    emitJumpSlowCaseIfNotJSCell(proto);
    
    // Check that prototype is an object
    addSlowCase(emitJumpIfCellNotObject(regT1));

    // Optimistically load the result true, and start looping.
    // Initially, regT1 still contains proto and regT2 still contains value.
    // As we loop regT2 will be updated with its prototype, recursively walking the prototype chain.
    move(TrustedImm32(1), regT0);
    Label loop(this);

    // Load the prototype of the cell in regT2.  If this is equal to regT1 - WIN!
    // Otherwise, check if we've hit null - if we have then drop out of the loop, if not go again.
    loadPtr(Address(regT2, JSCell::structureIDOffset()), regT2);
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
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    int baseVal = currentInstruction[3].u.operand;

    linkSlowCaseIfNotJSCell(iter, baseVal);
    linkSlowCase(iter);

    emitLoad(value, regT1, regT0);
    emitLoad(baseVal, regT3, regT2);
    callOperation(operationCheckHasInstance, dst, regT1, regT0, regT3, regT2);

    emitJumpSlowToHot(jump(), currentInstruction[4].u.operand);
}

void JIT::emitSlow_op_instanceof(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    int proto = currentInstruction[3].u.operand;

    linkSlowCaseIfNotJSCell(iter, value);
    linkSlowCaseIfNotJSCell(iter, proto);
    linkSlowCase(iter);

    emitLoad(value, regT1, regT0);
    emitLoad(proto, regT3, regT2);
    callOperation(operationInstanceOf, dst, regT1, regT0, regT3, regT2);
}

void JIT::emit_op_is_undefined(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitLoad(value, regT1, regT0);
    Jump isCell = branch32(Equal, regT1, TrustedImm32(JSValue::CellTag));

    compare32(Equal, regT1, TrustedImm32(JSValue::UndefinedTag), regT0);
    Jump done = jump();
    
    isCell.link(this);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump notMasqueradesAsUndefined = jump();
    
    isMasqueradesAsUndefined.link(this);
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT1);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT1, Structure::globalObjectOffset()), regT1);
    compare32(Equal, regT0, regT1, regT0);

    notMasqueradesAsUndefined.link(this);
    done.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_boolean(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitLoadTag(value, regT0);
    compare32(Equal, regT0, TrustedImm32(JSValue::BooleanTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_number(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitLoadTag(value, regT0);
    add32(TrustedImm32(1), regT0);
    compare32(Below, regT0, TrustedImm32(JSValue::LowestTag + 1), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_string(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitLoad(value, regT1, regT0);
    Jump isNotCell = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    
    compare8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(StringType), regT0);
    Jump done = jump();
    
    isNotCell.link(this);
    move(TrustedImm32(0), regT0);
    
    done.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_tear_off_lexical_environment(Instruction* currentInstruction)
{
    int lexicalEnvironment = currentInstruction[1].u.operand;
    Jump activationNotCreated = branch32(Equal, tagFor(lexicalEnvironment), TrustedImm32(JSValue::EmptyValueTag));
    emitLoadPayload(lexicalEnvironment, regT0);
    callOperation(operationTearOffActivation, regT0);
    activationNotCreated.link(this);
}

void JIT::emit_op_tear_off_arguments(Instruction* currentInstruction)
{
    VirtualRegister arguments = VirtualRegister(currentInstruction[1].u.operand);
    int lexicalEnvironment = currentInstruction[2].u.operand;

    Jump argsNotCreated = branch32(Equal, tagFor(arguments.offset()), TrustedImm32(JSValue::EmptyValueTag));
    emitLoadPayload(arguments.offset(), regT0);
    emitLoadPayload(lexicalEnvironment, regT1);
    callOperation(operationTearOffArguments, regT0, regT1);
    argsNotCreated.link(this);
}

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImm = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    isImm.link(this);

    if (dst != src)
        emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_to_primitive(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_primitive);
    slowPathCall.call();
}

void JIT::emit_op_strcat(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_strcat);
    slowPathCall.call();
}

void JIT::emit_op_not(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoadTag(src, regT0);

    emitLoad(src, regT1, regT0);
    addSlowCase(branch32(NotEqual, regT1, TrustedImm32(JSValue::BooleanTag)));
    xor32(TrustedImm32(1), regT0);

    emitStoreBool(dst, regT0, (dst == src));
}

void JIT::emitSlow_op_not(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_not);
    slowPathCall.call();
}

void JIT::emit_op_jfalse(Instruction* currentInstruction)
{
    int cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(cond, regT1, regT0);

    ASSERT((JSValue::BooleanTag + 1 == JSValue::Int32Tag) && !(JSValue::Int32Tag + 1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::BooleanTag)));
    addJump(branchTest32(Zero, regT0), target);
}

void JIT::emitSlow_op_jfalse(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);

    if (supportsFloatingPoint()) {
        // regT1 contains the tag from the hot path.
        Jump notNumber = branch32(Above, regT1, TrustedImm32(JSValue::LowestTag));

        emitLoadDouble(cond, fpRegT0);
        emitJumpSlowToHot(branchDoubleZeroOrNaN(fpRegT0, fpRegT1), target);
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jfalse));

        notNumber.link(this);
    }

    callOperation(operationConvertJSValueToBoolean, regT1, regT0);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target); // Inverted.
}

void JIT::emit_op_jtrue(Instruction* currentInstruction)
{
    int cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(cond, regT1, regT0);

    ASSERT((JSValue::BooleanTag + 1 == JSValue::Int32Tag) && !(JSValue::Int32Tag + 1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::BooleanTag)));
    addJump(branchTest32(NonZero, regT0), target);
}

void JIT::emitSlow_op_jtrue(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int cond = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    linkSlowCase(iter);

    if (supportsFloatingPoint()) {
        // regT1 contains the tag from the hot path.
        Jump notNumber = branch32(Above, regT1, TrustedImm32(JSValue::LowestTag));

        emitLoadDouble(cond, fpRegT0);
        emitJumpSlowToHot(branchDoubleNonZero(fpRegT0, fpRegT1), target);
        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_jtrue));

        notNumber.link(this);
    }

    callOperation(operationConvertJSValueToBoolean, regT1, regT0);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emit_op_jeq_null(Instruction* currentInstruction)
{
    int src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(Equal, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    ASSERT((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1));
    or32(TrustedImm32(1), regT1);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::NullTag)), target);

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);
}

void JIT::emit_op_jneq_null(Instruction* currentInstruction)
{
    int src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    addJump(branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(NotEqual, Address(regT2, Structure::globalObjectOffset()), regT0), target);
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
    int src = currentInstruction[1].u.operand;
    Special::Pointer ptr = currentInstruction[2].u.specialPointer;
    unsigned target = currentInstruction[3].u.operand;

    emitLoad(src, regT1, regT0);
    addJump(branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag)), target);
    addJump(branchPtr(NotEqual, regT0, TrustedImmPtr(actualPointerFor(m_codeBlock, ptr))), target);
}

void JIT::emit_op_eq(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;
    int src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Equal, regT1, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(Equal, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_eq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int op1 = currentInstruction[2].u.operand;
    int op2 = currentInstruction[3].u.operand;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    genericCase.append(branchPtr(NotEqual, Address(regT2, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));

    // String case.
    callOperation(operationCompareStringEq, regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    emitLoad(op1, regT1, regT0);
    emitLoad(op2, regT3, regT2);
    callOperation(operationCompareEq, regT1, regT0, regT3, regT2);

    storeResult.link(this);
    emitStoreBool(dst, returnValueGPR);
}

void JIT::emit_op_neq(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;
    int src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Equal, regT1, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(NotEqual, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_neq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    genericCase.append(branchPtr(NotEqual, Address(regT2, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));

    // String case.
    callOperation(operationCompareStringEq, regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    callOperation(operationCompareEq, regT1, regT0, regT3, regT2);

    storeResult.link(this);
    xor32(TrustedImm32(0x1), returnValueGPR);
    emitStoreBool(dst, returnValueGPR);
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;
    int src2 = currentInstruction[3].u.operand;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings.
    Jump notCell = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    Jump firstNotString = branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get()));
    addSlowCase(branchPtr(Equal, Address(regT2, JSCell::structureIDOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
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
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_stricteq);
    slowPathCall.call();
}

void JIT::emit_op_nstricteq(Instruction* currentInstruction)
{
    compileOpStrictEq(currentInstruction, OpNStrictEq);
}

void JIT::emitSlow_op_nstricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_nstricteq);
    slowPathCall.call();
}

void JIT::emit_op_eq_null(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT1);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    compare32(Equal, regT0, regT2, regT1);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    compare32(Equal, regT1, TrustedImm32(JSValue::NullTag), regT2);
    compare32(Equal, regT1, TrustedImm32(JSValue::UndefinedTag), regT1);
    or32(regT2, regT1);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    emitStoreBool(dst, regT1);
}

void JIT::emit_op_neq_null(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));

    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(1), regT1);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    compare32(NotEqual, regT0, regT2, regT1);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    compare32(NotEqual, regT1, TrustedImm32(JSValue::NullTag), regT2);
    compare32(NotEqual, regT1, TrustedImm32(JSValue::UndefinedTag), regT1);
    and32(regT2, regT1);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    emitStoreBool(dst, regT1);
}

void JIT::emit_op_throw(Instruction* currentInstruction)
{
    ASSERT(regT0 == returnValueGPR);
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperationNoExceptionCheck(operationThrow, regT1, regT0);
    jumpToExceptionHandler();
}

void JIT::emit_op_push_with_scope(Instruction* currentInstruction)
{
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperation(operationPushWithScope, regT1, regT0);
}

void JIT::emit_op_pop_scope(Instruction*)
{
    callOperation(operationPopScope);
}

void JIT::emit_op_to_number(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isInt32 = branch32(Equal, regT1, TrustedImm32(JSValue::Int32Tag));
    addSlowCase(branch32(AboveOrEqual, regT1, TrustedImm32(JSValue::LowestTag)));
    isInt32.link(this);

    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_to_number(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_number);
    slowPathCall.call();
}

void JIT::emit_op_push_name_scope(Instruction* currentInstruction)
{
    emitLoad(currentInstruction[2].u.operand, regT1, regT0);
    callOperation(operationPushNameScope, &m_codeBlock->identifier(currentInstruction[1].u.operand), regT1, regT0, currentInstruction[3].u.operand);
}

void JIT::emit_op_catch(Instruction* currentInstruction)
{
    move(TrustedImmPtr(m_vm), regT3);
    // operationThrow returns the callFrame for the handler.
    load32(Address(regT3, VM::callFrameForThrowOffset()), callFrameRegister);
    load32(Address(regT3, VM::vmEntryFrameForThrowOffset()), regT0);
    store32(regT0, Address(regT3, VM::topVMEntryFrameOffset()));

    addPtr(TrustedImm32(stackPointerOffsetFor(codeBlock()) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    // Now store the exception returned by operationThrow.
    load32(Address(regT3, VM::exceptionOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    load32(Address(regT3, VM::exceptionOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
    store32(TrustedImm32(JSValue().payload()), Address(regT3, VM::exceptionOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
    store32(TrustedImm32(JSValue().tag()), Address(regT3, VM::exceptionOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

    unsigned exception = currentInstruction[1].u.operand;
    emitStore(exception, regT1, regT0);
}

void JIT::emit_op_switch_imm(Instruction* currentInstruction)
{
    size_t tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset, SwitchRecord::Immediate));
    jumpTable->ensureCTITable();

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchImmWithUnknownKeyType, regT1, regT0, tableIndex);
    jump(returnValueGPR);
}

void JIT::emit_op_switch_char(Instruction* currentInstruction)
{
    size_t tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset, SwitchRecord::Character));
    jumpTable->ensureCTITable();

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchCharWithUnknownKeyType, regT1, regT0, tableIndex);
    jump(returnValueGPR);
}

void JIT::emit_op_switch_string(Instruction* currentInstruction)
{
    size_t tableIndex = currentInstruction[1].u.operand;
    unsigned defaultOffset = currentInstruction[2].u.operand;
    unsigned scrutinee = currentInstruction[3].u.operand;

    // create jump table for switch destinations, track this switch statement.
    StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeOffset, defaultOffset));

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchStringWithUnknownKeyType, regT1, regT0, tableIndex);
    jump(returnValueGPR);
}

void JIT::emit_op_throw_static_error(Instruction* currentInstruction)
{
    emitLoad(m_codeBlock->getConstant(currentInstruction[1].u.operand), regT1, regT0);
    callOperation(operationThrowStaticError, regT1, regT0, currentInstruction[2].u.operand);
}

void JIT::emit_op_debug(Instruction* currentInstruction)
{
    load32(codeBlock()->debuggerRequestsAddress(), regT0);
    Jump noDebuggerRequests = branchTest32(Zero, regT0);
    callOperation(operationDebug, currentInstruction[1].u.operand);
    noDebuggerRequests.link(this);
}


void JIT::emit_op_enter(Instruction* currentInstruction)
{
    emitEnterOptimizationCheck();
    
    // Even though JIT code doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    for (int i = 0; i < m_codeBlock->m_numVars; ++i)
        emitStore(virtualRegisterForLocal(i).offset(), jsUndefined());

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enter);
    slowPathCall.call();
}

void JIT::emit_op_create_lexical_environment(Instruction* currentInstruction)
{
    int lexicalEnvironment = currentInstruction[1].u.operand;

    callOperation(operationCreateActivation, 0);
    emitStoreCell(lexicalEnvironment, returnValueGPR);
}

void JIT::emit_op_create_arguments(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;

    Jump argsCreated = branch32(NotEqual, tagFor(dst), TrustedImm32(JSValue::EmptyValueTag));
    callOperation(operationCreateArguments);
    emitStoreCell(dst, returnValueGPR);
    emitStoreCell(unmodifiedArgumentsRegister(VirtualRegister(dst)).offset(), returnValueGPR);
    argsCreated.link(this);
}

void JIT::emit_op_init_lazy_reg(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;

    emitStore(dst, JSValue());
}

void JIT::emit_op_get_callee(Instruction* currentInstruction)
{
    int result = currentInstruction[1].u.operand;
    WriteBarrierBase<JSCell>* cachedFunction = &currentInstruction[2].u.jsCell;
    emitGetFromCallFrameHeaderPtr(JSStack::Callee, regT0);

    loadPtr(cachedFunction, regT2);
    addSlowCase(branchPtr(NotEqual, regT0, regT2));

    move(TrustedImm32(JSValue::CellTag), regT1);
    emitStore(result, regT1, regT0);
}

void JIT::emitSlow_op_get_callee(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_callee);
    slowPathCall.call();
}

void JIT::emit_op_create_this(Instruction* currentInstruction)
{
    int callee = currentInstruction[2].u.operand;
    RegisterID calleeReg = regT0;
    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID structureReg = regT2;
    RegisterID scratchReg = regT3;

    emitLoadPayload(callee, calleeReg);
    loadPtr(Address(calleeReg, JSFunction::offsetOfAllocationProfile() + ObjectAllocationProfile::offsetOfAllocator()), allocatorReg);
    loadPtr(Address(calleeReg, JSFunction::offsetOfAllocationProfile() + ObjectAllocationProfile::offsetOfStructure()), structureReg);
    addSlowCase(branchTestPtr(Zero, allocatorReg));

    emitAllocateJSObject(allocatorReg, structureReg, resultReg, scratchReg);
    emitStoreCell(currentInstruction[1].u.operand, resultReg);
}

void JIT::emitSlow_op_create_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter); // doesn't have an allocation profile
    linkSlowCase(iter); // allocation failed

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_create_this);
    slowPathCall.call();
}

void JIT::emit_op_to_this(Instruction* currentInstruction)
{
    WriteBarrierBase<Structure>* cachedStructure = &currentInstruction[2].u.structure;
    int thisRegister = currentInstruction[1].u.operand;

    emitLoad(thisRegister, regT3, regT2);

    addSlowCase(branch32(NotEqual, regT3, TrustedImm32(JSValue::CellTag)));
    addSlowCase(branch8(NotEqual, Address(regT2, JSCell::typeInfoTypeOffset()), TrustedImm32(FinalObjectType)));
    loadPtr(Address(regT2, JSCell::structureIDOffset()), regT0);
    loadPtr(cachedStructure, regT2);
    addSlowCase(branchPtr(NotEqual, regT0, regT2));
}

void JIT::emitSlow_op_to_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_this);
    slowPathCall.call();
}

void JIT::emit_op_profile_will_call(Instruction* currentInstruction)
{
    load32(m_vm->enabledProfilerAddress(), regT0);
    Jump profilerDone = branchTestPtr(Zero, regT0);
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperation(operationProfileWillCall, regT1, regT0);
    profilerDone.link(this);
}

void JIT::emit_op_profile_did_call(Instruction* currentInstruction)
{
    load32(m_vm->enabledProfilerAddress(), regT0);
    Jump profilerDone = branchTestPtr(Zero, regT0);
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperation(operationProfileDidCall, regT1, regT0);
    profilerDone.link(this);
}

void JIT::emit_op_get_arguments_length(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int argumentsRegister = currentInstruction[2].u.operand;
    addSlowCase(branch32(NotEqual, tagFor(argumentsRegister), TrustedImm32(JSValue::EmptyValueTag)));
    load32(payloadFor(JSStack::ArgumentCount), regT0);
    sub32(TrustedImm32(1), regT0);
    emitStoreInt32(dst, regT0);
}

void JIT::emitSlow_op_get_arguments_length(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    callOperation(operationGetArgumentsLength, dst, base);
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
    load32(payloadFor(JSStack::ArgumentCount), regT3);
    addSlowCase(branch32(AboveOrEqual, regT2, regT3));
    
    loadPtr(BaseIndex(callFrameRegister, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload) + CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))), regT0);
    loadPtr(BaseIndex(callFrameRegister, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag) + CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))), regT1);
    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_argument_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int arguments = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;

    linkSlowCase(iter);
    Jump skipArgumentsCreation = jump();

    linkSlowCase(iter);
    linkSlowCase(iter);

    callOperation(operationCreateArguments);
    emitStoreCell(arguments, returnValueGPR);
    emitStoreCell(unmodifiedArgumentsRegister(VirtualRegister(arguments)).offset(), returnValueGPR);
    
    skipArgumentsCreation.link(this);
    emitLoad(arguments, regT1, regT0);
    emitLoad(property, regT3, regT2);
    callOperation(WithProfile, operationGetByValGeneric, dst, regT1, regT0, regT3, regT2);
}

void JIT::emit_op_get_enumerable_length(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_enumerable_length);
    slowPathCall.call();
}

void JIT::emit_op_has_structure_property(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int enumerator = currentInstruction[4].u.operand;

    emitLoadPayload(base, regT0);
    emitJumpSlowCaseIfNotJSCell(base);

    emitLoadPayload(enumerator, regT1);

    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    addSlowCase(branch32(NotEqual, regT0, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));
    
    move(TrustedImm32(1), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_has_structure_property(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_has_structure_property);
    slowPathCall.call();
}

void JIT::emit_op_has_generic_property(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_has_generic_property);
    slowPathCall.call();
}

void JIT::privateCompileHasIndexedProperty(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
{
    Instruction* currentInstruction = m_codeBlock->instructions().begin() + byValInfo->bytecodeIndex;
    
    PatchableJump badType;
    
    // FIXME: Add support for other types like TypedArrays and Arguments.
    // See https://bugs.webkit.org/show_bug.cgi?id=135033 and https://bugs.webkit.org/show_bug.cgi?id=135034.
    JumpList slowCases = emitLoadForArrayMode(currentInstruction, arrayMode, badType);
    move(TrustedImm32(1), regT0);
    Jump done = jump();

    LinkBuffer patchBuffer(*m_vm, *this, m_codeBlock);
    
    patchBuffer.link(badType, CodeLocationLabel(MacroAssemblerCodePtr::createFromExecutableAddress(returnAddress.value())).labelAtOffset(byValInfo->returnAddressToSlowPath));
    patchBuffer.link(slowCases, CodeLocationLabel(MacroAssemblerCodePtr::createFromExecutableAddress(returnAddress.value())).labelAtOffset(byValInfo->returnAddressToSlowPath));
    
    patchBuffer.link(done, byValInfo->badTypeJump.labelAtOffset(byValInfo->badTypeJumpToDone));
    
    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer,
        ("Baseline has_indexed_property stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value()));
    
    RepatchBuffer repatchBuffer(m_codeBlock);
    repatchBuffer.relink(byValInfo->badTypeJump, CodeLocationLabel(byValInfo->stubRoutine->code().code()));
    repatchBuffer.relinkCallerToFunction(returnAddress, FunctionPtr(operationHasIndexedPropertyGeneric));
}

void JIT::emit_op_has_indexed_property(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    ArrayProfile* profile = currentInstruction[4].u.arrayProfile;
    
    emitLoadPayload(base, regT0);
    emitJumpSlowCaseIfNotJSCell(base);

    emitLoadPayload(property, regT1);

    // This is technically incorrect - we're zero-extending an int32. On the hot path this doesn't matter.
    // We check the value as if it was a uint32 against the m_vectorLength - which will always fail if
    // number was signed since m_vectorLength is always less than intmax (since the total allocation
    // size is always less than 4Gb). As such zero extending will have been correct (and extending the value
    // to 64-bits is necessary since it's used in the address calculation. We zero extend rather than sign
    // extending since it makes it easier to re-tag the value in the slow case.
    zeroExtend32ToPtr(regT1, regT1);

    emitArrayProfilingSiteWithCell(regT0, regT2, profile);
    and32(TrustedImm32(IndexingShapeMask), regT2);

    JITArrayMode mode = chooseArrayMode(profile);
    PatchableJump badType;

    // FIXME: Add support for other types like TypedArrays and Arguments.
    // See https://bugs.webkit.org/show_bug.cgi?id=135033 and https://bugs.webkit.org/show_bug.cgi?id=135034.
    JumpList slowCases = emitLoadForArrayMode(currentInstruction, mode, badType);
    move(TrustedImm32(1), regT0);

    addSlowCase(badType);
    addSlowCase(slowCases);
    
    Label done = label();
    
    emitStoreBool(dst, regT0);
    
    m_byValCompilationInfo.append(ByValCompilationInfo(m_bytecodeOffset, badType, mode, done));
}

void JIT::emitSlow_op_has_indexed_property(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    ArrayProfile* profile = currentInstruction[4].u.arrayProfile;
    
    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // base array check
    
    Jump skipProfiling = jump();
    
    linkSlowCase(iter); // vector length check
    linkSlowCase(iter); // empty value
    
    emitArrayProfileOutOfBoundsSpecialCase(profile);
    
    skipProfiling.link(this);
    
    Label slowPath = label();
    
    emitLoad(base, regT1, regT0);
    emitLoad(property, regT3, regT2);
    Call call = callOperation(operationHasIndexedPropertyDefault, dst, regT1, regT0, regT3, regT2);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_get_direct_pname(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int index = currentInstruction[4].u.operand;
    int enumerator = currentInstruction[5].u.operand;

    // Check that base is a cell
    emitLoadPayload(base, regT0);
    emitJumpSlowCaseIfNotJSCell(base);

    // Check the structure
    emitLoadPayload(enumerator, regT1);
    load32(Address(regT0, JSCell::structureIDOffset()), regT2);
    addSlowCase(branch32(NotEqual, regT2, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));

    // Compute the offset
    emitLoadPayload(index, regT2);
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    Jump outOfLineAccess = branch32(AboveOrEqual, regT2, Address(regT1, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));
    addPtr(TrustedImm32(JSObject::offsetOfInlineStorage()), regT0);
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
    load32(BaseIndex(regT0, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    
    Jump done = jump();

    // Otherwise it's out of line
    outOfLineAccess.link(this);
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT0);
    sub32(Address(regT1, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), regT2);
    neg32(regT2);
    int32_t offsetOfFirstProperty = static_cast<int32_t>(offsetInButterfly(firstOutOfLineOffset)) * sizeof(EncodedJSValue);
    load32(BaseIndex(regT0, regT2, TimesEight, offsetOfFirstProperty + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);
    load32(BaseIndex(regT0, regT2, TimesEight, offsetOfFirstProperty + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    
    done.link(this);
    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_direct_pname(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int base = currentInstruction[2].u.operand;
    linkSlowCaseIfNotJSCell(iter, base);
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_direct_pname);
    slowPathCall.call();
}

void JIT::emit_op_get_structure_property_enumerator(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_structure_property_enumerator);
    slowPathCall.call();
}

void JIT::emit_op_get_generic_property_enumerator(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_generic_property_enumerator);
    slowPathCall.call();
}

void JIT::emit_op_next_enumerator_pname(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int enumerator = currentInstruction[2].u.operand;
    int index = currentInstruction[3].u.operand;

    emitLoadPayload(index, regT0);
    emitLoadPayload(enumerator, regT1);
    Jump inBounds = branch32(Below, regT0, Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesLengthOffset()));

    move(TrustedImm32(JSValue::NullTag), regT2);
    move(TrustedImm32(0), regT0);

    Jump done = jump();
    inBounds.link(this);

    loadPtr(Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), regT1);
    loadPtr(BaseIndex(regT1, regT0, timesPtr()), regT0);
    move(TrustedImm32(JSValue::CellTag), regT2);

    done.link(this);
    emitStore(dst, regT2, regT0);
}

void JIT::emit_op_to_index_string(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_index_string);
    slowPathCall.call();
}

void JIT::emit_op_profile_type(Instruction* currentInstruction)
{
    TypeLocation* cachedTypeLocation = currentInstruction[2].u.location;
    int valueToProfile = currentInstruction[1].u.operand;

    // Load payload in T0. Load tag in T3.
    emitLoadPayload(valueToProfile, regT0);
    emitLoadTag(valueToProfile, regT3);

    JumpList jumpToEnd;

    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 32-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(branch32(Equal, regT3, TrustedImm32(JSValue::UndefinedTag)));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(branch32(Equal, regT3, TrustedImm32(JSValue::NullTag)));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(branch32(Equal, regT3, TrustedImm32(JSValue::BooleanTag)));
    else if (cachedTypeLocation->m_lastSeenType == TypeMachineInt)
        jumpToEnd.append(branch32(Equal, regT3, TrustedImm32(JSValue::Int32Tag)));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber) {
        jumpToEnd.append(branch32(Below, regT3, TrustedImm32(JSValue::LowestTag)));
        jumpToEnd.append(branch32(Equal, regT3, TrustedImm32(JSValue::Int32Tag)));
    } else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        Jump isNotCell = branch32(NotEqual, regT3, TrustedImm32(JSValue::CellTag));
        jumpToEnd.append(branch8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(StringType)));
        isNotCell.link(this);
    }

    // Load the type profiling log into T2.
    TypeProfilerLog* cachedTypeProfilerLog = m_vm->typeProfilerLog();
    move(TrustedImmPtr(cachedTypeProfilerLog), regT2);

    // Load the next log entry into T1.
    loadPtr(Address(regT2, TypeProfilerLog::currentLogEntryOffset()), regT1);

    // Store the JSValue onto the log entry.
    store32(regT0, Address(regT1, TypeProfilerLog::LogEntry::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
    store32(regT3, Address(regT1, TypeProfilerLog::LogEntry::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)));

    // Store the structureID of the cell if argument is a cell, otherwise, store 0 on the log entry.
    Jump notCell = branch32(NotEqual, regT3, TrustedImm32(JSValue::CellTag));
    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    store32(regT0, Address(regT1, TypeProfilerLog::LogEntry::structureIDOffset()));
    Jump skipNotCell = jump();
    notCell.link(this);
    store32(TrustedImm32(0), Address(regT1, TypeProfilerLog::LogEntry::structureIDOffset()));
    skipNotCell.link(this);

    // Store the typeLocation on the log entry.
    move(TrustedImmPtr(cachedTypeLocation), regT0);
    store32(regT0, Address(regT1, TypeProfilerLog::LogEntry::locationOffset()));

    // Increment the current log entry.
    addPtr(TrustedImm32(sizeof(TypeProfilerLog::LogEntry)), regT1);
    store32(regT1, Address(regT2, TypeProfilerLog::currentLogEntryOffset()));
    jumpToEnd.append(branchPtr(NotEqual, regT1, TrustedImmPtr(cachedTypeProfilerLog->logEndPtr())));
    // Clear the log if we're at the end of the log.
    callOperation(operationProcessTypeProfilerLog);

    jumpToEnd.link(this);
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
