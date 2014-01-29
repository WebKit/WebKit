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
#include "JIT.h"

#include "Arguments.h"
#include "CopiedSpaceInlines.h"
#include "Debugger.h"
#include "Heap.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSCell.h"
#include "JSFunction.h"
#include "JSPropertyNameIterator.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "SlowPathCall.h"
#include "VirtualRegister.h"

namespace JSC {

#if USE(JSVALUE64)

JIT::CodeRef JIT::privateCompileCTINativeCall(VM* vm, NativeFunction)
{
    return vm->getCTIStub(nativeCallGenerator);
}

void JIT::emit_op_mov(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_captured_mov(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    emitNotifyWrite(regT0, regT1, currentInstruction[3].u.watchpointSet);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_end(Instruction* currentInstruction)
{
    RELEASE_ASSERT(returnValueGPR != callFrameRegister);
    emitGetVirtualRegister(currentInstruction[1].u.operand, returnValueGPR);
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
    emitPutVirtualRegister(currentInstruction[1].u.operand);
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

    emitGetVirtualRegister(baseVal, regT0);

    // Check that baseVal is a cell.
    emitJumpSlowCaseIfNotJSCell(regT0, baseVal);

    // Check that baseVal 'ImplementsHasInstance'.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT0);
    addSlowCase(branchTest8(Zero, Address(regT0, Structure::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance)));
}

void JIT::emit_op_instanceof(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    int proto = currentInstruction[3].u.operand;

    // Load the operands (baseVal, proto, and value respectively) into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitGetVirtualRegister(value, regT2);
    emitGetVirtualRegister(proto, regT1);

    // Check that proto are cells.  baseVal must be a cell - this is checked by op_check_has_instance.
    emitJumpSlowCaseIfNotJSCell(regT2, value);
    emitJumpSlowCaseIfNotJSCell(regT1, proto);

    // Check that prototype is an object
    loadPtr(Address(regT1, JSCell::structureOffset()), regT3);
    addSlowCase(emitJumpIfNotObject(regT3));
    
    // Optimistically load the result true, and start looping.
    // Initially, regT1 still contains proto and regT2 still contains value.
    // As we loop regT2 will be updated with its prototype, recursively walking the prototype chain.
    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);
    Label loop(this);

    // Load the prototype of the object in regT2.  If this is equal to regT1 - WIN!
    // Otherwise, check if we've hit null - if we have then drop out of the loop, if not go again.
    loadPtr(Address(regT2, JSCell::structureOffset()), regT2);
    load64(Address(regT2, Structure::prototypeOffset()), regT2);
    Jump isInstance = branchPtr(Equal, regT2, regT1);
    emitJumpIfJSCell(regT2).linkTo(loop, this);

    // We get here either by dropping out of the loop, or if value was not an Object.  Result is false.
    move(TrustedImm64(JSValue::encode(jsBoolean(false))), regT0);

    // isInstance jumps right down to here, to skip setting the result to false (it has already set true).
    isInstance.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_undefined(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitGetVirtualRegister(value, regT0);
    Jump isCell = emitJumpIfJSCell(regT0);

    compare64(Equal, regT0, TrustedImm32(ValueUndefined), regT0);
    Jump done = jump();
    
    isCell.link(this);
    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT1, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump notMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT1, Structure::globalObjectOffset()), regT1);
    comparePtr(Equal, regT0, regT1, regT0);

    notMasqueradesAsUndefined.link(this);
    done.link(this);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_boolean(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitGetVirtualRegister(value, regT0);
    xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), regT0);
    test64(Zero, regT0, TrustedImm32(static_cast<int32_t>(~1)), regT0);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_number(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitGetVirtualRegister(value, regT0);
    test64(NonZero, regT0, tagTypeNumberRegister, regT0);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_is_string(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    
    emitGetVirtualRegister(value, regT0);
    Jump isNotCell = emitJumpIfNotJSCell(regT0);
    
    loadPtr(Address(regT0, JSCell::structureOffset()), regT1);
    compare8(Equal, Address(regT1, Structure::typeInfoTypeOffset()), TrustedImm32(StringType), regT0);
    emitTagAsBoolImmediate(regT0);
    Jump done = jump();
    
    isNotCell.link(this);
    move(TrustedImm32(ValueFalse), regT0);
    
    done.link(this);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_tear_off_activation(Instruction* currentInstruction)
{
    int activation = currentInstruction[1].u.operand;
    Jump activationNotCreated = branchTest64(Zero, addressFor(activation));
    emitGetVirtualRegister(activation, regT0);
    callOperation(operationTearOffActivation, regT0);
    activationNotCreated.link(this);
}

void JIT::emit_op_tear_off_arguments(Instruction* currentInstruction)
{
    int arguments = currentInstruction[1].u.operand;
    int activation = currentInstruction[2].u.operand;

    Jump argsNotCreated = branchTest64(Zero, Address(callFrameRegister, sizeof(Register) * (unmodifiedArgumentsRegister(VirtualRegister(arguments)).offset())));
    emitGetVirtualRegister(unmodifiedArgumentsRegister(VirtualRegister(arguments)).offset(), regT0);
    emitGetVirtualRegister(activation, regT1);
    callOperation(operationTearOffArguments, regT0, regT1);
    argsNotCreated.link(this);
}

void JIT::emit_op_ret(Instruction* currentInstruction)
{
    ASSERT(callFrameRegister != regT1);
    ASSERT(regT1 != returnValueGPR);
    ASSERT(returnValueGPR != callFrameRegister);

    // Return the result in %eax.
    emitGetVirtualRegister(currentInstruction[1].u.operand, returnValueGPR);

    checkStackPointerAlignment();
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_ret_object_or_this(Instruction* currentInstruction)
{
    ASSERT(callFrameRegister != regT1);
    ASSERT(regT1 != returnValueGPR);
    ASSERT(returnValueGPR != callFrameRegister);

    // Return the result in %eax.
    emitGetVirtualRegister(currentInstruction[1].u.operand, returnValueGPR);
    Jump notJSCell = emitJumpIfNotJSCell(returnValueGPR);
    loadPtr(Address(returnValueGPR, JSCell::structureOffset()), regT2);
    Jump notObject = emitJumpIfNotObject(regT2);

    // Return.
    emitFunctionEpilogue();
    ret();

    // Return 'this' in %eax.
    notJSCell.link(this);
    notObject.link(this);
    emitGetVirtualRegister(currentInstruction[2].u.operand, returnValueGPR);

    // Return.
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_to_primitive(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    
    Jump isImm = emitJumpIfNotJSCell(regT0);
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::structureOffset()), TrustedImmPtr(m_vm->stringStructure.get())));
    isImm.link(this);

    if (dst != src)
        emitPutVirtualRegister(dst);

}

void JIT::emit_op_strcat(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_strcat);
    slowPathCall.call();
}

void JIT::emit_op_not(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);

    // Invert against JSValue(false); if the value was tagged as a boolean, then all bits will be
    // clear other than the low bit (which will be 0 or 1 for false or true inputs respectively).
    // Then invert against JSValue(true), which will add the tag back in, and flip the low bit.
    xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), regT0);
    addSlowCase(branchTestPtr(NonZero, regT0, TrustedImm32(static_cast<int32_t>(~1))));
    xor64(TrustedImm32(static_cast<int32_t>(ValueTrue)), regT0);

    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_jfalse(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[2].u.operand;
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsNumber(0)))), target);
    Jump isNonZero = emitJumpIfImmediateInteger(regT0);

    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsBoolean(false)))), target);
    addSlowCase(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(jsBoolean(true)))));

    isNonZero.link(this);
}

void JIT::emit_op_jeq_null(Instruction* currentInstruction)
{
    int src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(Equal, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    and64(TrustedImm32(~TagBitUndefined), regT0);
    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);            

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);
};
void JIT::emit_op_jneq_null(Instruction* currentInstruction)
{
    int src = currentInstruction[1].u.operand;
    unsigned target = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    // First, handle JSCell cases - check MasqueradesAsUndefined bit on the structure.
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    addJump(branchTest8(Zero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(NotEqual, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    and64(TrustedImm32(~TagBitUndefined), regT0);
    addJump(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(jsNull()))), target);            

    wasNotImmediate.link(this);
}

void JIT::emit_op_jneq_ptr(Instruction* currentInstruction)
{
    int src = currentInstruction[1].u.operand;
    Special::Pointer ptr = currentInstruction[2].u.specialPointer;
    unsigned target = currentInstruction[3].u.operand;
    
    emitGetVirtualRegister(src, regT0);
    addJump(branchPtr(NotEqual, regT0, TrustedImmPtr(actualPointerFor(m_codeBlock, ptr))), target);
}

void JIT::emit_op_eq(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    compare32(Equal, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_jtrue(Instruction* currentInstruction)
{
    unsigned target = currentInstruction[2].u.operand;
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);

    Jump isZero = branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsNumber(0))));
    addJump(emitJumpIfImmediateInteger(regT0), target);

    addJump(branch64(Equal, regT0, TrustedImm64(JSValue::encode(jsBoolean(true)))), target);
    addSlowCase(branch64(NotEqual, regT0, TrustedImm64(JSValue::encode(jsBoolean(false)))));

    isZero.link(this);
}

void JIT::emit_op_neq(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    compare32(NotEqual, regT1, regT0, regT0);
    emitTagAsBoolImmediate(regT0);

    emitPutVirtualRegister(currentInstruction[1].u.operand);

}

void JIT::emit_op_bitxor(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    xor64(regT1, regT0);
    emitFastArithReTagImmediate(regT0, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_bitor(Instruction* currentInstruction)
{
    emitGetVirtualRegisters(currentInstruction[2].u.operand, regT0, currentInstruction[3].u.operand, regT1);
    emitJumpSlowCaseIfNotImmediateIntegers(regT0, regT1, regT2);
    or64(regT1, regT0);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_throw(Instruction* currentInstruction)
{
    ASSERT(regT0 == returnValueGPR);
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);
    callOperationNoExceptionCheck(operationThrow, regT0);
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

    emitGetVirtualRegister(base, regT0);
    if (!m_codeBlock->isKnownNotImmediate(base))
        isNotObject.append(emitJumpIfNotJSCell(regT0));
    if (base != m_codeBlock->thisRegister().offset() || m_codeBlock->isStrictMode()) {
        loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
        isNotObject.append(emitJumpIfNotObject(regT2));
    }

    // We could inline the case where you have a valid cache, but
    // this call doesn't seem to be hot.
    Label isObject(this);
    callOperation(operationGetPNames, regT0);
    emitStoreCell(dst, returnValueGPR);
    load32(Address(regT0, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStringsSize)), regT3);
    store64(tagTypeNumberRegister, addressFor(i));
    store32(TrustedImm32(Int32Tag), intTagFor(size));
    store32(regT3, intPayloadFor(size));
    Jump end = jump();

    isNotObject.link(this);
    move(regT0, regT1);
    and32(TrustedImm32(~TagBitUndefined), regT1);
    addJump(branch32(Equal, regT1, TrustedImm32(ValueNull)), breakTarget);
    callOperation(operationToObject, base, regT0);
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
    loadPtr(addressFor(it), regT1);
    loadPtr(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_jsStrings)), regT2);

    load64(BaseIndex(regT2, regT0, TimesEight), regT2);

    emitPutVirtualRegister(dst, regT2);

    // Increment i
    add32(TrustedImm32(1), regT0);
    store32(regT0, intPayloadFor(i));

    // Verify that i is valid:
    emitGetVirtualRegister(base, regT0);

    // Test base's structure
    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedStructure)))));

    // Test base's prototype chain
    loadPtr(Address(Address(regT1, OBJECT_OFFSETOF(JSPropertyNameIterator, m_cachedPrototypeChain))), regT3);
    loadPtr(Address(regT3, OBJECT_OFFSETOF(StructureChain, m_vector)), regT3);
    addJump(branchTestPtr(Zero, Address(regT3)), target);

    Label checkPrototype(this);
    load64(Address(regT2, Structure::prototypeOffset()), regT2);
    callHasProperty.append(emitJumpIfNotJSCell(regT2));
    loadPtr(Address(regT2, JSCell::structureOffset()), regT2);
    callHasProperty.append(branchPtr(NotEqual, regT2, Address(regT3)));
    addPtr(TrustedImm32(sizeof(Structure*)), regT3);
    branchTestPtr(NonZero, Address(regT3)).linkTo(checkPrototype, this);

    // Continue loop.
    addJump(jump(), target);

    // Slow case: Ask the object if i is valid.
    callHasProperty.link(this);
    emitGetVirtualRegister(dst, regT1);
    callOperation(operationHasProperty, regT0, regT1);

    // Test for valid key.
    addJump(branchTest32(NonZero, regT0), target);
    jump().linkTo(begin, this);

    // End of loop.
    end.link(this);
}

void JIT::emit_op_push_with_scope(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);
    callOperation(operationPushWithScope, regT0);
}

void JIT::emit_op_pop_scope(Instruction*)
{
    callOperation(operationPopScope);
}

void JIT::compileOpStrictEq(Instruction* currentInstruction, CompileOpStrictEqType type)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;
    int src2 = currentInstruction[3].u.operand;

    emitGetVirtualRegisters(src1, regT0, src2, regT1);
    
    // Jump slow if both are cells (to cover strings).
    move(regT0, regT2);
    or64(regT1, regT2);
    addSlowCase(emitJumpIfJSCell(regT2));
    
    // Jump slow if either is a double. First test if it's an integer, which is fine, and then test
    // if it's a double.
    Jump leftOK = emitJumpIfImmediateInteger(regT0);
    addSlowCase(emitJumpIfImmediateNumber(regT0));
    leftOK.link(this);
    Jump rightOK = emitJumpIfImmediateInteger(regT1);
    addSlowCase(emitJumpIfImmediateNumber(regT1));
    rightOK.link(this);

    if (type == OpStrictEq)
        compare64(Equal, regT1, regT0, regT0);
    else
        compare64(NotEqual, regT1, regT0, regT0);
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

void JIT::emit_op_to_number(Instruction* currentInstruction)
{
    int srcVReg = currentInstruction[2].u.operand;
    emitGetVirtualRegister(srcVReg, regT0);
    
    addSlowCase(emitJumpIfNotImmediateNumber(regT0));

    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emit_op_push_name_scope(Instruction* currentInstruction)
{
    emitGetVirtualRegister(currentInstruction[2].u.operand, regT0);
    callOperation(operationPushNameScope, &m_codeBlock->identifier(currentInstruction[1].u.operand), regT0, currentInstruction[3].u.operand);
}

void JIT::emit_op_catch(Instruction* currentInstruction)
{
    move(TrustedImmPtr(m_vm), regT3);
    load64(Address(regT3, VM::callFrameForThrowOffset()), callFrameRegister);

    addPtr(TrustedImm32(stackPointerOffsetFor(codeBlock()) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    load64(Address(regT3, VM::exceptionOffset()), regT0);
    store64(TrustedImm64(JSValue::encode(JSValue())), Address(regT3, VM::exceptionOffset()));
    emitPutVirtualRegister(currentInstruction[1].u.operand);
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

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchImmWithUnknownKeyType, regT0, tableIndex);
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

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchCharWithUnknownKeyType, regT0, tableIndex);
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

    emitGetVirtualRegister(scrutinee, regT0);
    callOperation(operationSwitchStringWithUnknownKeyType, regT0, tableIndex);
    jump(returnValueGPR);
}

void JIT::emit_op_throw_static_error(Instruction* currentInstruction)
{
    move(TrustedImm64(JSValue::encode(m_codeBlock->getConstant(currentInstruction[1].u.operand))), regT0);
    callOperation(operationThrowStaticError, regT0, currentInstruction[2].u.operand);
}

void JIT::emit_op_debug(Instruction* currentInstruction)
{
    load32(codeBlock()->debuggerRequestsAddress(), regT0);
    Jump noDebuggerRequests = branchTest32(Zero, regT0);
    callOperation(operationDebug, currentInstruction[1].u.operand);
    noDebuggerRequests.link(this);
}

void JIT::emit_op_eq_null(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(0), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(Equal, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    and64(TrustedImm32(~TagBitUndefined), regT0);
    compare64(Equal, regT0, TrustedImm32(ValueNull), regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);

}

void JIT::emit_op_neq_null(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int src1 = currentInstruction[2].u.operand;

    emitGetVirtualRegister(src1, regT0);
    Jump isImmediate = emitJumpIfNotJSCell(regT0);

    loadPtr(Address(regT0, JSCell::structureOffset()), regT2);
    Jump isMasqueradesAsUndefined = branchTest8(NonZero, Address(regT2, Structure::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    move(TrustedImm32(1), regT0);
    Jump wasNotMasqueradesAsUndefined = jump();

    isMasqueradesAsUndefined.link(this);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    loadPtr(Address(regT2, Structure::globalObjectOffset()), regT2);
    comparePtr(NotEqual, regT0, regT2, regT0);
    Jump wasNotImmediate = jump();

    isImmediate.link(this);

    and64(TrustedImm32(~TagBitUndefined), regT0);
    compare64(NotEqual, regT0, TrustedImm32(ValueNull), regT0);

    wasNotImmediate.link(this);
    wasNotMasqueradesAsUndefined.link(this);

    emitTagAsBoolImmediate(regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_enter(Instruction* currentInstruction)
{
    emitEnterOptimizationCheck();
    
    // Even though CTI doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    size_t count = m_codeBlock->m_numVars;
    for (size_t j = 0; j < count; ++j)
        emitInitRegister(virtualRegisterForLocal(j).offset());

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enter);
    slowPathCall.call();
}

void JIT::emit_op_create_activation(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    
    Jump activationCreated = branchTest64(NonZero, Address(callFrameRegister, sizeof(Register) * dst));
    callOperation(operationCreateActivation, 0);
    emitStoreCell(dst, returnValueGPR);
    activationCreated.link(this);
}

void JIT::emit_op_create_arguments(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;

    Jump argsCreated = branchTest64(NonZero, Address(callFrameRegister, sizeof(Register) * dst));

    callOperation(operationCreateArguments);
    emitStoreCell(dst, returnValueGPR);
    emitStoreCell(unmodifiedArgumentsRegister(VirtualRegister(dst)), returnValueGPR);

    argsCreated.link(this);
}

void JIT::emit_op_init_lazy_reg(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;

    store64(TrustedImm64((int64_t)0), Address(callFrameRegister, sizeof(Register) * dst));
}

void JIT::emit_op_to_this(Instruction* currentInstruction)
{
    WriteBarrierBase<Structure>* cachedStructure = &currentInstruction[2].u.structure;
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT1);

    emitJumpSlowCaseIfNotJSCell(regT1);
    loadPtr(Address(regT1, JSCell::structureOffset()), regT0);

    addSlowCase(branch8(NotEqual, Address(regT0, Structure::typeInfoTypeOffset()), TrustedImm32(FinalObjectType)));
    loadPtr(cachedStructure, regT2);
    addSlowCase(branchPtr(NotEqual, regT0, regT2));
}

void JIT::emit_op_get_callee(Instruction* currentInstruction)
{
    int result = currentInstruction[1].u.operand;
    WriteBarrierBase<JSCell>* cachedFunction = &currentInstruction[2].u.jsCell;
    emitGetFromCallFrameHeaderPtr(JSStack::Callee, regT0);

    loadPtr(cachedFunction, regT2);
    addSlowCase(branchPtr(NotEqual, regT0, regT2));

    emitPutVirtualRegister(result);
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

    emitGetVirtualRegister(callee, calleeReg);
    loadPtr(Address(calleeReg, JSFunction::offsetOfAllocationProfile() + ObjectAllocationProfile::offsetOfAllocator()), allocatorReg);
    loadPtr(Address(calleeReg, JSFunction::offsetOfAllocationProfile() + ObjectAllocationProfile::offsetOfStructure()), structureReg);
    addSlowCase(branchTestPtr(Zero, allocatorReg));

    emitAllocateJSObject(allocatorReg, structureReg, resultReg, scratchReg);
    emitPutVirtualRegister(currentInstruction[1].u.operand);
}

void JIT::emitSlow_op_create_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter); // doesn't have an allocation profile
    linkSlowCase(iter); // allocation failed

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_create_this);
    slowPathCall.call();
}

void JIT::emit_op_profile_will_call(Instruction* currentInstruction)
{
    Jump profilerDone = branchTestPtr(Zero, AbsoluteAddress(m_vm->enabledProfilerAddress()));
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);
    callOperation(operationProfileWillCall, regT0);
    profilerDone.link(this);
}

void JIT::emit_op_profile_did_call(Instruction* currentInstruction)
{
    Jump profilerDone = branchTestPtr(Zero, AbsoluteAddress(m_vm->enabledProfilerAddress()));
    emitGetVirtualRegister(currentInstruction[1].u.operand, regT0);
    callOperation(operationProfileDidCall, regT0);
    profilerDone.link(this);
}


// Slow cases

void JIT::emitSlow_op_to_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_this);
    slowPathCall.call();
}

void JIT::emitSlow_op_to_primitive(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_primitive);
    slowPathCall.call();
}

void JIT::emitSlow_op_not(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_not);
    slowPathCall.call();
}

void JIT::emitSlow_op_jfalse(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    callOperation(operationConvertJSValueToBoolean, regT0);
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), currentInstruction[2].u.operand); // inverted!
}

void JIT::emitSlow_op_jtrue(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    callOperation(operationConvertJSValueToBoolean, regT0);
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), currentInstruction[2].u.operand);
}

void JIT::emitSlow_op_bitxor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_bitxor);
    slowPathCall.call();
}

void JIT::emitSlow_op_bitor(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_bitor);
    slowPathCall.call();
}

void JIT::emitSlow_op_eq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    callOperation(operationCompareEq, regT0, regT1);
    emitTagAsBoolImmediate(returnValueGPR);
    emitPutVirtualRegister(currentInstruction[1].u.operand, returnValueGPR);
}

void JIT::emitSlow_op_neq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    callOperation(operationCompareEq, regT0, regT1);
    xor32(TrustedImm32(0x1), regT0);
    emitTagAsBoolImmediate(returnValueGPR);
    emitPutVirtualRegister(currentInstruction[1].u.operand, returnValueGPR);
}

void JIT::emitSlow_op_stricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_stricteq);
    slowPathCall.call();
}

void JIT::emitSlow_op_nstricteq(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);
    linkSlowCase(iter);
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_nstricteq);
    slowPathCall.call();
}

void JIT::emitSlow_op_check_has_instance(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int value = currentInstruction[2].u.operand;
    int baseVal = currentInstruction[3].u.operand;

    linkSlowCaseIfNotJSCell(iter, baseVal);
    linkSlowCase(iter);
    emitGetVirtualRegister(value, regT0);
    emitGetVirtualRegister(baseVal, regT1);
    callOperation(operationCheckHasInstance, dst, regT0, regT1);

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
    emitGetVirtualRegister(value, regT0);
    emitGetVirtualRegister(proto, regT1);
    callOperation(operationInstanceOf, dst, regT0, regT1);
}

void JIT::emitSlow_op_to_number(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkSlowCase(iter);

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_to_number);
    slowPathCall.call();
}

void JIT::emit_op_get_arguments_length(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int argumentsRegister = currentInstruction[2].u.operand;
    addSlowCase(branchTest64(NonZero, addressFor(argumentsRegister)));
    emitGetFromCallFrameHeader32(JSStack::ArgumentCount, regT0);
    sub32(TrustedImm32(1), regT0);
    emitFastArithReTagImmediate(regT0, regT0);
    emitPutVirtualRegister(dst, regT0);
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
    addSlowCase(branchTest64(NonZero, addressFor(argumentsRegister)));
    emitGetVirtualRegister(property, regT1);
    addSlowCase(emitJumpIfNotImmediateInteger(regT1));
    add32(TrustedImm32(1), regT1);
    // regT1 now contains the integer index of the argument we want, including this
    emitGetFromCallFrameHeader32(JSStack::ArgumentCount, regT2);
    addSlowCase(branch32(AboveOrEqual, regT1, regT2));

    signExtend32ToPtr(regT1, regT1);
    load64(BaseIndex(callFrameRegister, regT1, TimesEight, CallFrame::thisArgumentOffset() * static_cast<int>(sizeof(Register))), regT0);
    emitValueProfilingSite();
    emitPutVirtualRegister(dst, regT0);
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
    emitStoreCell(unmodifiedArgumentsRegister(VirtualRegister(arguments)), returnValueGPR);
    
    skipArgumentsCreation.link(this);
    emitGetVirtualRegister(arguments, regT0);
    emitGetVirtualRegister(property, regT1);
    callOperation(WithProfile, operationGetByValGeneric, dst, regT0, regT1);
}

#endif // USE(JSVALUE64)

void JIT::emit_op_touch_entry(Instruction* currentInstruction)
{
    if (m_codeBlock->symbolTable()->m_functionEnteredOnce.hasBeenInvalidated())
        return;
    
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_touch_entry);
    slowPathCall.call();
}

void JIT::emit_op_loop_hint(Instruction*)
{
    // Emit the JIT optimization check: 
    if (canBeOptimized()) {
        addSlowCase(branchAdd32(PositiveOrZero, TrustedImm32(Options::executionCounterIncrementForLoop()),
            AbsoluteAddress(m_codeBlock->addressOfJITExecuteCounter())));
    }

    // Emit the watchdog timer check:
    if (m_vm->watchdog.isEnabled())
        addSlowCase(branchTest8(NonZero, AbsoluteAddress(m_vm->watchdog.timerDidFireAddress())));
}

void JIT::emitSlow_op_loop_hint(Instruction*, Vector<SlowCaseEntry>::iterator& iter)
{
#if ENABLE(DFG_JIT)
    // Emit the slow path for the JIT optimization check:
    if (canBeOptimized()) {
        linkSlowCase(iter);
        
        callOperation(operationOptimize, m_bytecodeOffset);
        Jump noOptimizedEntry = branchTestPtr(Zero, returnValueGPR);
        move(returnValueGPR2, stackPointerRegister);
        jump(returnValueGPR);
        noOptimizedEntry.link(this);

        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_loop_hint));
    }
#endif

    // Emit the slow path of the watchdog timer check:
    if (m_vm->watchdog.isEnabled()) {
        linkSlowCase(iter);
        callOperation(operationHandleWatchdogTimer);

        emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_loop_hint));
    }

}

void JIT::emit_op_new_regexp(Instruction* currentInstruction)
{
    callOperation(operationNewRegexp, currentInstruction[1].u.operand, m_codeBlock->regexp(currentInstruction[2].u.operand));
}

void JIT::emit_op_new_func(Instruction* currentInstruction)
{
    Jump lazyJump;
    int dst = currentInstruction[1].u.operand;
    if (currentInstruction[3].u.operand) {
#if USE(JSVALUE32_64)
        lazyJump = branch32(NotEqual, tagFor(dst), TrustedImm32(JSValue::EmptyValueTag));
#else
        lazyJump = branchTest64(NonZero, addressFor(dst));
#endif
    }

    FunctionExecutable* funcExec = m_codeBlock->functionDecl(currentInstruction[2].u.operand);
    callOperation(operationNewFunction, dst, funcExec);

    if (currentInstruction[3].u.operand)
        lazyJump.link(this);
}

void JIT::emit_op_new_captured_func(Instruction* currentInstruction)
{
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_new_captured_func);
    slowPathCall.call();
}

void JIT::emit_op_new_func_exp(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    FunctionExecutable* funcExpr = m_codeBlock->functionExpr(currentInstruction[2].u.operand);
    callOperation(operationNewFunction, dst, funcExpr);
}

void JIT::emit_op_new_array(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int valuesIndex = currentInstruction[2].u.operand;
    int size = currentInstruction[3].u.operand;
    addPtr(TrustedImm32(valuesIndex * sizeof(Register)), callFrameRegister, regT0);
    callOperation(operationNewArrayWithProfile, dst,
        currentInstruction[4].u.arrayAllocationProfile, regT0, size);
}

void JIT::emit_op_new_array_with_size(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int sizeIndex = currentInstruction[2].u.operand;
#if USE(JSVALUE64)
    emitGetVirtualRegister(sizeIndex, regT0);
    callOperation(operationNewArrayWithSizeAndProfile, dst,
        currentInstruction[3].u.arrayAllocationProfile, regT0);
#else
    emitLoad(sizeIndex, regT1, regT0);
    callOperation(operationNewArrayWithSizeAndProfile, dst,
        currentInstruction[3].u.arrayAllocationProfile, regT1, regT0);
#endif
}

void JIT::emit_op_new_array_buffer(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int valuesIndex = currentInstruction[2].u.operand;
    int size = currentInstruction[3].u.operand;
    const JSValue* values = codeBlock()->constantBuffer(valuesIndex);
    callOperation(operationNewArrayBufferWithProfile, dst, currentInstruction[4].u.arrayAllocationProfile, values, size);
}

void JIT::emitSlow_op_captured_mov(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    VariableWatchpointSet* set = currentInstruction[3].u.watchpointSet;
    if (!set || set->state() == IsInvalidated)
        return;
    linkSlowCase(iter);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_captured_mov);
    slowPathCall.call();
}

} // namespace JSC

#endif // ENABLE(JIT)
