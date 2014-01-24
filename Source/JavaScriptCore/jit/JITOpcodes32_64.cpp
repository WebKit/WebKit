/*
 * Copyright (C) 2009, 2012, 2013 Apple Inc. All rights reserved.
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
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "JSVariableObject.h"
#include "LinkBuffer.h"
#include "SlowPathCall.h"
#include "VirtualRegister.h"

namespace JSC {

JIT::CodeRef JIT::privateCompileCTINativeCall(VM* vm, NativeFunction func)
{
    Call nativeCall;

    emitPutImmediateToCallFrameHeader(0, JSStack::CodeBlock);
    storePtr(callFrameRegister, &m_vm->topCallFrame);

#if CPU(X86)
    // Load caller frame's scope chain into this callframe so that whatever we call can
    // get to its global data.
    emitGetCallerFrameFromCallFrameHeaderPtr(regT0);
    emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, regT1, regT0);
    emitPutCellToCallFrameHeader(regT1, JSStack::ScopeChain);

    peek(regT1);
    emitPutReturnPCToCallFrameHeader(regT1);

    // Calling convention:      f(ecx, edx, ...);
    // Host function signature: f(ExecState*);
    move(callFrameRegister, X86Registers::ecx);

    subPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister); // Align stack after call.

    move(regT0, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.

    // call the function
    nativeCall = call();

    addPtr(TrustedImm32(16 - sizeof(void*)), stackPointerRegister);

#elif CPU(ARM) || CPU(SH4) || CPU(MIPS)
    // Load caller frame's scope chain into this callframe so that whatever we call can get to its global data.
    emitGetCallerFrameFromCallFrameHeaderPtr(regT2);
    emitGetFromCallFrameHeaderPtr(JSStack::ScopeChain, regT1, regT2);
    emitPutCellToCallFrameHeader(regT1, JSStack::ScopeChain);

    preserveReturnAddressAfterCall(regT3); // Callee preserved
    emitPutReturnPCToCallFrameHeader(regT3);

#if CPU(MIPS)
    // Allocate stack space for (unused) 16 bytes (8-byte aligned) for 4 arguments.
    subPtr(TrustedImm32(16), stackPointerRegister);
#endif

    // Calling convention is f(argumentGPR0, argumentGPR1, ...).
    // Host function signature is f(ExecState*).
    move(callFrameRegister, argumentGPR0);

    emitGetFromCallFrameHeaderPtr(JSStack::Callee, argumentGPR1);
    move(regT2, callFrameRegister); // Eagerly restore caller frame register to avoid loading from stack.
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
    breakpoint();
#endif // CPU(X86)

    // Check for an exception
    Jump sawException = branch32(NotEqual, AbsoluteAddress(reinterpret_cast<char*>(vm->addressOfException()) + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag)); 

    // Return.
    ret();

    // Handle an exception
    sawException.link(this);

    // Grab the return address.
    preserveReturnAddressAfterCall(regT1);

    move(TrustedImmPtr(&vm->exceptionLocation), regT2);
    storePtr(regT1, regT2);
    storePtr(callFrameRegister, &m_vm->topCallFrame);

#if CPU(X86)
    addPtr(TrustedImm32(-12), stackPointerRegister);
    push(callFrameRegister);
#else
    move(callFrameRegister, argumentGPR0);
#endif
    move(TrustedImmPtr(FunctionPtr(operationVMHandleException).value()), regT3);
    call(regT3);

#if CPU(X86)
    addPtr(TrustedImm32(16), stackPointerRegister);
#endif

    jumpToExceptionHandler();

    // All trampolines constructed! copy the code, link up calls, and set the pointers on the Machine object.
    LinkBuffer patchBuffer(*m_vm, this, GLOBAL_THUNK_ID);

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
    restoreReturnAddressBeforeReturn(Address(callFrameRegister, CallFrame::returnPCOffset()));
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
    loadPtr(Address(regT0, JSCell::structureOffset()), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, Structure::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance)));
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
    loadPtr(Address(regT1, JSCell::structureOffset()), regT3);
    addSlowCase(emitJumpIfNotObject(regT3));

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
    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT1, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump notMasqueradesAsUndefined = jump();
    
    isMasqueradesAsUndefined.link(this);
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
    
    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    compare8(Equal, Address(regT1, Structure::typeInfoTypeOffset()), TrustedImm32(StringType), regT0);
    Jump done = jump();
    
    isNotCell.link(this);
    move(TrustedImm32(0), regT0);
    
    done.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_tear_off_activation(Instruction* currentInstruction)
{
    int activation = currentInstruction[1].u.operand;
    Jump activationNotCreated = branch32(Equal, tagFor(activation), TrustedImm32(JSValue::EmptyValueTag));
    emitLoadPayload(activation, regT0);
    callOperation(operationTearOffActivation, regT0);
    activationNotCreated.link(this);
}

void JIT::emit_op_tear_off_arguments(Instruction* currentInstruction)
{
    VirtualRegister arguments = VirtualRegister(currentInstruction[1].u.operand);
    int activation = currentInstruction[2].u.operand;

    Jump argsNotCreated = branch32(Equal, tagFor(unmodifiedArgumentsRegister(arguments).offset()), TrustedImm32(JSValue::EmptyValueTag));
    emitLoadPayload(unmodifiedArgumentsRegister(VirtualRegister(arguments)).offset(), regT0);
    emitLoadPayload(activation, regT1);
    callOperation(operationTearOffArguments, regT0, regT1);
    argsNotCreated.link(this);
}

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitLoad(src, regT1, regT0);

    Jump isImm = branch32(NotEqual, regT1, TrustedImm32(JSValue::CellTag));
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
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

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
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

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    addJump(branchTest8(Zero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
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
    genericCase.append(branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    genericCase.append(branchPtr(NotEqual, Address(regT2, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));

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
    genericCase.append(branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    genericCase.append(branchPtr(NotEqual, Address(regT2, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));

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
    Jump firstNotString = branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get()));
    addSlowCase(branchPtr(Equal, Address(regT2, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
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

    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT1);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
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

    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(1), regT1);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
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
    if (VirtualRegister(base) != m_codeBlock->thisRegister() || m_codeBlock->isStrictMode()) {
        loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
        isNotObject.append(emitJumpIfNotObject(regT2));
    }

    // We could inline the case where you have a valid cache, but
    // this call doesn't seem to be hot.
    Label isObject(this);
    callOperation(operationGetPNames, regT0);
    emitStoreCell(dst, returnValueGPR);
    load32(Address(regT0, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStringsSize)), regT3);
    store32(TrustedImm32(Int32Tag), intTagFor(i));
    store32(TrustedImm32(0), intPayloadFor(i));
    store32(TrustedImm32(Int32Tag), intTagFor(size));
    store32(regT3, payloadFor(size));
    Jump end = jump();

    isNotObject.link(this);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::NullTag)), breakTarget);
    addJump(branch32(Equal, regT1, TrustedImm32(JSValue::UndefinedTag)), breakTarget);
    callOperation(operationToObject, base, regT1, regT0);
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
    callOperation(operationHasProperty, regT0, regT1);

    // Test for valid key.
    addJump(branchTest32(NonZero, regT0), target);
    jump().linkTo(begin, this);

    // End of loop.
    end.link(this);
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
    jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

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
    jumpTable->ctiOffsets.grow(jumpTable->branchOffsets.size());

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

void JIT::emit_op_create_activation(Instruction* currentInstruction)
{
    int activation = currentInstruction[1].u.operand;
    
    Jump activationCreated = branch32(NotEqual, tagFor(activation), TrustedImm32(JSValue::EmptyValueTag));
    callOperation(operationCreateActivation, 0);
    emitStoreCell(activation, returnValueGPR);
    activationCreated.link(this);
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
    loadPtr(Address(regT2, JSCell::structureOffset()), regT0);
    addSlowCase(branch8(NotEqual, Address(regT0, Structure::typeInfoTypeOffset()), TrustedImm32(FinalObjectType)));
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
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperation(operationProfileWillCall, regT1, regT0);
}

void JIT::emit_op_profile_did_call(Instruction* currentInstruction)
{
    emitLoad(currentInstruction[1].u.operand, regT1, regT0);
    callOperation(operationProfileDidCall, regT1, regT0);
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

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
