/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

#include "BasicBlockLocation.h"
#include "BytecodeGenerator.h"
#include "BytecodeStructs.h"
#include "CCallHelpers.h"
#include "Exception.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSCast.h"
#include "JSFunction.h"
#include "JSPropertyNameEnumerator.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "OpcodeInlines.h"
#include "SlowPathCall.h"
#include "TypeProfilerLog.h"
#include "VirtualRegister.h"

namespace JSC {

void JIT::emit_op_mov(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpMov>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;
    
    if (src.isConstant())
        emitStore(dst, getConstantOperand(src));
    else {
        emitLoad(src, regT1, regT0);
        emitStore(dst, regT1, regT0);
    }
}

void JIT::emit_op_end(const Instruction* currentInstruction)
{
    ASSERT(returnValueGPR != callFrameRegister);
    auto bytecode = currentInstruction->as<OpEnd>();
    emitLoad(bytecode.m_value, regT1, returnValueGPR);
    emitRestoreCalleeSaves();
    emitFunctionEpilogue();
    ret();
}

void JIT::emit_op_jmp(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJmp>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    addJump(jump(), target);
}

void JIT::emit_op_new_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNewObject>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    Structure* structure = metadata.m_objectAllocationProfile.structure();
    size_t allocationSize = JSFinalObject::allocationSize(structure->inlineCapacity());
    Allocator allocator = allocatorForNonVirtualConcurrently<JSFinalObject>(*m_vm, allocationSize, AllocatorForMode::AllocatorIfExists);

    RegisterID resultReg = returnValueGPR;
    RegisterID allocatorReg = regT1;
    RegisterID scratchReg = regT3;

    if (!allocator)
        addSlowCase(jump());
    else {
        JumpList slowCases;
        auto butterfly = TrustedImmPtr(nullptr);
        emitAllocateJSObject(resultReg, JITAllocator::constant(allocator), allocatorReg, TrustedImmPtr(structure), butterfly, scratchReg, slowCases);
        emitInitializeInlineStorage(resultReg, structure->inlineCapacity());
        addSlowCase(slowCases);
        emitStoreCell(bytecode.m_dst, resultReg);
    }
}

void JIT::emitSlow_op_new_object(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpNewObject>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    Structure* structure = metadata.m_objectAllocationProfile.structure();
    callOperation(operationNewObject, TrustedImmPtr(&vm()), structure);
    emitStoreCell(dst, returnValueGPR);
}

void JIT::emit_op_overrides_has_instance(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitLoadPayload(hasInstanceValue, regT0);
    // We don't jump if we know what Symbol.hasInstance would do.
    Jump hasInstanceValueNotCell = emitJumpIfNotJSCell(hasInstanceValue);
    Jump customhasInstanceValue = branchPtr(NotEqual, regT0, TrustedImmPtr(m_codeBlock->globalObject()->functionProtoHasInstanceSymbolFunction()));

    // We know that constructor is an object from the way bytecode is emitted for instanceof expressions.
    emitLoadPayload(constructor, regT0);

    // Check that constructor 'ImplementsDefaultHasInstance' i.e. the object is not a C-API user nor a bound function.
    test8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(ImplementsDefaultHasInstance), regT0);
    Jump done = jump();

    hasInstanceValueNotCell.link(this);
    customhasInstanceValue.link(this);
    move(TrustedImm32(1), regT0);

    done.link(this);
    emitStoreBool(dst, regT0);

}

void JIT::emit_op_instanceof(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInstanceof>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister proto = bytecode.m_prototype;

    // Load the operands into registers.
    // We use regT0 for baseVal since we will be done with this first, and we can then use it for the result.
    emitLoadPayload(value, regT2);
    emitLoadPayload(proto, regT1);

    // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
    emitJumpSlowCaseIfNotJSCell(value);
    emitJumpSlowCaseIfNotJSCell(proto);
    
    JITInstanceOfGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        RegisterSet::stubUnavailableRegisters(),
        regT0, // result
        regT2, // value
        regT1, // proto
        regT3, regT4); // scratch
    gen.generateFastPath(*this);
    m_instanceOfs.append(gen);
    
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_instanceof_custom(const Instruction*)
{
    // This always goes to slow path since we expect it to be rare.
    addSlowCase(jump());
}

void JIT::emitSlow_op_instanceof(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    
    auto bytecode = currentInstruction->as<OpInstanceof>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister proto = bytecode.m_prototype;
    
    JITInstanceOfGenerator& gen = m_instanceOfs[m_instanceOfIndex++];
    
    Label coldPathBegin = label();
    emitLoadTag(value, regT0);
    emitLoadTag(proto, regT3);
    Call call = callOperation(operationInstanceOfOptimize, dst, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT0, regT2), JSValueRegs(regT3, regT1));
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emitSlow_op_instanceof_custom(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInstanceofCustom>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitLoad(value, regT1, regT0);
    emitLoadPayload(constructor, regT2);
    emitLoad(hasInstanceValue, regT4, regT3);
    callOperation(operationInstanceOfCustom, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), regT2, JSValueRegs(regT4, regT3));
    emitStoreBool(dst, returnValueGPR);
}
    
void JIT::emit_op_is_empty(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsEmpty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitLoad(value, regT1, regT0);
    compare32(Equal, regT1, TrustedImm32(JSValue::EmptyValueTag), regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_undefined(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefined>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitLoad(value, regT1, regT0);
    Jump isCell = branchIfCell(regT1);

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

void JIT::emit_op_is_undefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsUndefinedOrNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitLoadTag(value, regT0);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT0);
    compare32(Equal, regT0, TrustedImm32(JSValue::NullTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_boolean(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBoolean>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitLoadTag(value, regT0);
    compare32(Equal, regT0, TrustedImm32(JSValue::BooleanTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitLoadTag(value, regT0);
    add32(TrustedImm32(1), regT0);
    compare32(Below, regT0, TrustedImm32(JSValue::LowestTag + 1), regT0);
    emitStoreBool(dst, regT0);
}

NO_RETURN void JIT::emit_op_is_big_int(const Instruction*)
{
    // We emit is_cell_with_type instead, since BigInt32 is not supported on 32-bit platforms.
    RELEASE_ASSERT_NOT_REACHED();
}

void JIT::emit_op_is_cell_with_type(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsCellWithType>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    int type = bytecode.m_type;

    emitLoad(value, regT1, regT0);
    Jump isNotCell = branchIfNotCell(regT1);

    compare8(Equal, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(type), regT0);
    Jump done = jump();

    isNotCell.link(this);
    move(TrustedImm32(0), regT0);

    done.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsObject>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;

    emitLoad(value, regT1, regT0);
    Jump isNotCell = branchIfNotCell(regT1);

    compare8(AboveOrEqual, Address(regT0, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType), regT0);
    Jump done = jump();

    isNotCell.link(this);
    move(TrustedImm32(0), regT0);

    done.link(this);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_to_primitive(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPrimitive>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitLoad(src, regT1, regT0);

    Jump isImm = branchIfNotCell(regT1);
    addSlowCase(branchIfObject(regT0));
    isImm.link(this);

    if (dst != src)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_property_key(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToPropertyKey>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_src;

    emitLoad(src, regT1, regT0);

    addSlowCase(branchIfNotCell(regT1));
    Jump done = branchIfSymbol(regT0);
    addSlowCase(branchIfNotString(regT0));

    done.link(this);
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_set_function_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetFunctionName>();
    VirtualRegister func = bytecode.m_function;
    VirtualRegister name = bytecode.m_name;
    emitLoadPayload(func, regT1);
    emitLoad(name, regT3, regT2);
    callOperation(operationSetFunctionName, m_codeBlock->globalObject(), regT1, JSValueRegs(regT3, regT2));
}

void JIT::emit_op_not(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNot>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoadTag(src, regT0);

    emitLoad(src, regT1, regT0);
    addSlowCase(branchIfNotBoolean(regT1, InvalidGPRReg));
    xor32(TrustedImm32(1), regT0);

    emitStoreBool(dst, regT0, (dst == src));
}

void JIT::emit_op_jfalse(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJfalse>();
    VirtualRegister cond = bytecode.m_condition;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoad(cond, regT1, regT0);

    JSValueRegs value(regT1, regT0);
    GPRReg scratch1 = regT2;
    GPRReg scratch2 = regT3;
    bool shouldCheckMasqueradesAsUndefined = true;
    addJump(branchIfFalsey(vm(), value, scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, m_codeBlock->globalObject()), target);
}

void JIT::emit_op_jtrue(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJtrue>();
    VirtualRegister cond = bytecode.m_condition;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoad(cond, regT1, regT0);
    bool shouldCheckMasqueradesAsUndefined = true;
    JSValueRegs value(regT1, regT0);
    GPRReg scratch1 = regT2;
    GPRReg scratch2 = regT3;
    addJump(branchIfTruthy(vm(), value, scratch1, scratch2, fpRegT0, fpRegT1, shouldCheckMasqueradesAsUndefined, m_codeBlock->globalObject()), target);
}

void JIT::emit_op_jeq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branchIfNotCell(regT1);

    Jump isNotMasqueradesAsUndefined = branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined));
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(Equal, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump masqueradesGlobalObjectIsForeign = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT1);
    addJump(branchIfNull(regT1), target);

    isNotMasqueradesAsUndefined.link(this);
    masqueradesGlobalObjectIsForeign.link(this);
}

void JIT::emit_op_jneq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqNull>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoad(src, regT1, regT0);

    Jump isImmediate = branchIfNotCell(regT1);

    addJump(branchTest8(Zero, Address(regT0, JSCell::typeInfoFlagsOffset()), TrustedImm32(MasqueradesAsUndefined)), target);
    loadPtr(Address(regT0, JSCell::structureIDOffset()), regT2);
    move(TrustedImmPtr(m_codeBlock->globalObject()), regT0);
    addJump(branchPtr(NotEqual, Address(regT2, Structure::globalObjectOffset()), regT0), target);
    Jump wasNotImmediate = jump();

    // Now handle the immediate cases - undefined & null
    isImmediate.link(this);

    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT1);
    addJump(branchIfNotNull(regT1), target);

    wasNotImmediate.link(this);
}

void JIT::emit_op_jundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoadTag(value, regT0);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT0);
    addJump(branchIfNull(regT0), target);
}

void JIT::emit_op_jnundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoadTag(value, regT0);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT0);
    addJump(branchIfNotNull(regT0), target);
}

void JIT::emit_op_jneq_ptr(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqPtr>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister src = bytecode.m_value;
    JSValue specialPointer = getConstantOperand(bytecode.m_specialPointer);
    ASSERT(specialPointer.isCell());
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitLoad(src, regT1, regT0);
    Jump notCell = branchIfNotCell(regT1);
    Jump equal = branchPtr(Equal, regT0, TrustedImmPtr(specialPointer.asCell()));
    notCell.link(this);
    store8(TrustedImm32(1), &metadata.m_hasJumped);
    addJump(jump(), target);
    equal.link(this);
}

void JIT::emit_op_eq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();

    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branchIfCell(regT1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(Equal, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_eq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpEq>();
    VirtualRegister dst = bytecode.m_dst;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchIfNotString(regT0));
    genericCase.append(branchIfNotString(regT2));

    // String case.
    callOperation(operationCompareStringEq, m_codeBlock->globalObject(), regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    callOperation(operationCompareEq, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));

    storeResult.link(this);
    emitStoreBool(dst, returnValueGPR);
}

void JIT::emit_op_jeq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branchIfCell(regT1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    addJump(branch32(Equal, regT0, regT2), target);
}

void JIT::compileOpEqJumpSlow(Vector<SlowCaseEntry>::iterator& iter, CompileOpEqType type, int jumpTarget)
{
    JumpList done;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchIfNotString(regT0));
    genericCase.append(branchIfNotString(regT2));

    // String case.
    callOperation(operationCompareStringEq, m_codeBlock->globalObject(), regT0, regT2);
    emitJumpSlowToHot(branchTest32(type == CompileOpEqType::Eq ? NonZero : Zero, returnValueGPR), jumpTarget);
    done.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    callOperation(operationCompareEq, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    emitJumpSlowToHot(branchTest32(type == CompileOpEqType::Eq ? NonZero : Zero, returnValueGPR), jumpTarget);

    done.link(this);
}

void JIT::emitSlow_op_jeq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    compileOpEqJumpSlow(iter, CompileOpEqType::Eq, target);
}

void JIT::emit_op_neq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeq>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branchIfCell(regT1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    compare32(NotEqual, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emitSlow_op_neq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpNeq>();
    VirtualRegister dst = bytecode.m_dst;

    JumpList storeResult;
    JumpList genericCase;

    genericCase.append(getSlowCase(iter)); // tags not equal

    linkSlowCase(iter); // tags equal and JSCell
    genericCase.append(branchIfNotString(regT0));
    genericCase.append(branchIfNotString(regT2));

    // String case.
    callOperation(operationCompareStringEq, m_codeBlock->globalObject(), regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    callOperation(operationCompareEq, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));

    storeResult.link(this);
    xor32(TrustedImm32(0x1), returnValueGPR);
    emitStoreBool(dst, returnValueGPR);
}

void JIT::emit_op_jneq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branchIfCell(regT1));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    addJump(branch32(NotEqual, regT0, regT2), target);
}

void JIT::emitSlow_op_jneq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpJneq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    compileOpEqJumpSlow(iter, CompileOpEqType::NEq, target);
}

template <typename Op>
void JIT::compileOpStrictEq(const Instruction* currentInstruction, CompileOpStrictEqType type)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings or symbols (non object).
    Jump notCell = branchIfNotCell(regT1);
    Jump firstIsObject = branchIfObject(regT0);
    addSlowCase(branchIfNotObject(regT2));
    notCell.link(this);
    firstIsObject.link(this);

    // Simply compare the payloads.
    if (type == CompileOpStrictEqType::StrictEq)
        compare32(Equal, regT0, regT2, regT0);
    else
        compare32(NotEqual, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emit_op_stricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpStricteq>(currentInstruction, CompileOpStrictEqType::StrictEq);
}

void JIT::emit_op_nstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpNstricteq>(currentInstruction, CompileOpStrictEqType::NStrictEq);
}

template<typename Op>
void JIT::compileOpStrictEqJump(const Instruction* currentInstruction, CompileOpStrictEqType type)
{
    auto bytecode = currentInstruction->as<Op>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitLoad2(src1, regT1, regT0, src2, regT3, regT2);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, regT1, regT3));
    addSlowCase(branch32(Below, regT1, TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings or symbols (non object).
    Jump notCell = branchIfNotCell(regT1);
    Jump firstIsObject = branchIfObject(regT0);
    addSlowCase(branchIfNotObject(regT2));
    notCell.link(this);
    firstIsObject.link(this);

    // Simply compare the payloads.
    if (type == CompileOpStrictEqType::StrictEq)
        addJump(branch32(Equal, regT0, regT2), target);
    else
        addJump(branch32(NotEqual, regT0, regT2), target);
}

void JIT::emit_op_jstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJstricteq>(currentInstruction, CompileOpStrictEqType::StrictEq);
}

void JIT::emit_op_jnstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJnstricteq>(currentInstruction, CompileOpStrictEqType::NStrictEq);
}

void JIT::emitSlow_op_jstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareStrictEq, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jnstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJnstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    callOperation(operationCompareStrictEq, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

void JIT::emit_op_eq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branchIfNotCell(regT1);

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

void JIT::emit_op_neq_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNeqNull>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoad(src, regT1, regT0);
    Jump isImmediate = branchIfNotCell(regT1);

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

void JIT::emit_op_throw(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpThrow>();
    ASSERT(regT0 == returnValueGPR);
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(vm().topEntryFrame);
    emitLoad(bytecode.m_value, regT1, regT0);
    callOperationNoExceptionCheck(operationThrow, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0));
    jumpToExceptionHandler(vm());
}

void JIT::emit_op_to_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoad(src, regT1, regT0);

    Jump isInt32 = branchIfInt32(regT1);
    addSlowCase(branch32(AboveOrEqual, regT1, TrustedImm32(JSValue::LowestTag)));
    isInt32.link(this);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_numeric(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumeric>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;
    JSValueRegs argumentValueRegs(regT1, regT0);

    emitLoad(src, regT1, regT0);

    Jump isNotCell = branchIfNotCell(regT1);
    addSlowCase(branchIfNotHeapBigInt(regT0));
    Jump isBigInt = jump();

    isNotCell.link(this);
    addSlowCase(branchIfNotNumber(argumentValueRegs, regT2));
    isBigInt.link(this);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_string(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToString>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoad(src, regT1, regT0);

    addSlowCase(branchIfNotCell(regT1));
    addSlowCase(branchIfNotString(regT0));

    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_object(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToObject>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitLoad(src, regT1, regT0);

    addSlowCase(branchIfNotCell(regT1));
    addSlowCase(branchIfNotObject(regT0));

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_catch(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCatch>();

    restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(vm().topEntryFrame);

    move(TrustedImmPtr(m_vm), regT3);
    // operationThrow returns the callFrame for the handler.
    load32(Address(regT3, VM::callFrameForCatchOffset()), callFrameRegister);
    storePtr(TrustedImmPtr(nullptr), Address(regT3, VM::callFrameForCatchOffset()));

    addPtr(TrustedImm32(stackPointerOffsetFor(codeBlock()) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    callOperationNoExceptionCheck(operationCheckIfExceptionIsUncatchableAndNotifyProfiler, TrustedImmPtr(&vm()));
    Jump isCatchableException = branchTest32(Zero, returnValueGPR);
    jumpToExceptionHandler(vm());
    isCatchableException.link(this);

    move(TrustedImmPtr(m_vm), regT3);

    // Now store the exception returned by operationThrow.
    load32(Address(regT3, VM::exceptionOffset()), regT2);
    move(TrustedImm32(JSValue::CellTag), regT1);

    store32(TrustedImm32(0), Address(regT3, VM::exceptionOffset()));

    emitStore(bytecode.m_exception, regT1, regT2);

    load32(Address(regT2, Exception::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    load32(Address(regT2, Exception::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);

    emitStore(bytecode.m_thrownValue, regT1, regT0);

#if ENABLE(DFG_JIT)
    // FIXME: consider inline caching the process of doing OSR entry, including
    // argument type proofs, storing locals to the buffer, etc
    // https://bugs.webkit.org/show_bug.cgi?id=175598

    auto& metadata = bytecode.metadata(m_codeBlock);
    ValueProfileAndVirtualRegisterBuffer* buffer = metadata.m_buffer;
    if (buffer || !shouldEmitProfiling())
        callOperation(operationTryOSREnterAtCatch, &vm(), m_bytecodeIndex.asBits());
    else
        callOperation(operationTryOSREnterAtCatchAndValueProfile, &vm(), m_bytecodeIndex.asBits());
    auto skipOSREntry = branchTestPtr(Zero, returnValueGPR);
    emitRestoreCalleeSaves();
    farJump(returnValueGPR, NoPtrTag);
    skipOSREntry.link(this);
    if (buffer && shouldEmitProfiling()) {
        buffer->forEach([&] (ValueProfileAndVirtualRegister& profile) {
            JSValueRegs regs(regT1, regT0);
            emitGetVirtualRegister(profile.m_operand, regs);
            emitValueProfilingSite(static_cast<ValueProfile&>(profile));
        });
    }
#endif // ENABLE(DFG_JIT)
}

void JIT::emit_op_identity_with_profile(const Instruction*)
{
    // We don't need to do anything here...
}

void JIT::emit_op_get_parent_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetParentScope>();
    VirtualRegister currentScope = bytecode.m_scope;
    emitLoadPayload(currentScope, regT0);
    loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitStoreCell(bytecode.m_dst, regT0);
}

void JIT::emit_op_switch_imm(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchImm>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Immediate));
    jumpTable->ensureCTITable();

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchImmWithUnknownKeyType, TrustedImmPtr(&vm()), JSValueRegs(regT1, regT0), tableIndex);
    farJump(returnValueGPR, NoPtrTag);
}

void JIT::emit_op_switch_char(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchChar>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    SimpleJumpTable* jumpTable = &m_codeBlock->switchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset, SwitchRecord::Character));
    jumpTable->ensureCTITable();

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchCharWithUnknownKeyType, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), tableIndex);
    farJump(returnValueGPR, NoPtrTag);
}

void JIT::emit_op_switch_string(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSwitchString>();
    size_t tableIndex = bytecode.m_tableIndex;
    unsigned defaultOffset = jumpTarget(currentInstruction, bytecode.m_defaultOffset);
    VirtualRegister scrutinee = bytecode.m_scrutinee;

    // create jump table for switch destinations, track this switch statement.
    StringJumpTable* jumpTable = &m_codeBlock->stringSwitchJumpTable(tableIndex);
    m_switches.append(SwitchRecord(jumpTable, m_bytecodeIndex, defaultOffset));

    emitLoad(scrutinee, regT1, regT0);
    callOperation(operationSwitchStringWithUnknownKeyType, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), tableIndex);
    farJump(returnValueGPR, NoPtrTag);
}

void JIT::emit_op_debug(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDebug>();
    load32(codeBlock()->debuggerRequestsAddress(), regT0);
    Jump noDebuggerRequests = branchTest32(Zero, regT0);
    callOperation(operationDebug, &vm(), static_cast<int>(bytecode.m_debugHookType));
    noDebuggerRequests.link(this);
}


void JIT::emit_op_enter(const Instruction* currentInstruction)
{
    emitEnterOptimizationCheck();
    
    // Even though JIT code doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    for (int i = CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters(); i < m_codeBlock->numVars(); ++i)
        emitStore(virtualRegisterForLocal(i), jsUndefined());

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enter);
    slowPathCall.call();
}

void JIT::emit_op_get_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetScope>();
    VirtualRegister dst = bytecode.m_dst;
    emitGetFromCallFrameHeaderPtr(CallFrameSlot::callee, regT0);
    loadPtr(Address(regT0, JSFunction::offsetOfScopeChain()), regT0);
    emitStoreCell(dst, regT0);
}

void JIT::emit_op_create_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCreateThis>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister callee = bytecode.m_callee;
    WriteBarrierBase<JSCell>* cachedFunction = &metadata.m_cachedCallee;
    RegisterID calleeReg = regT0;
    RegisterID rareDataReg = regT4;
    RegisterID resultReg = regT0;
    RegisterID allocatorReg = regT1;
    RegisterID structureReg = regT2;
    RegisterID cachedFunctionReg = regT4;
    RegisterID scratchReg = regT3;

    emitLoadPayload(callee, calleeReg);
    addSlowCase(branchIfNotFunction(calleeReg));
    loadPtr(Address(calleeReg, JSFunction::offsetOfExecutableOrRareData()), rareDataReg);
    addSlowCase(branchTestPtr(Zero, rareDataReg, TrustedImm32(JSFunction::rareDataTag)));
    load32(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator() - JSFunction::rareDataTag), allocatorReg);
    loadPtr(Address(rareDataReg, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure() - JSFunction::rareDataTag), structureReg);

    loadPtr(cachedFunction, cachedFunctionReg);
    Jump hasSeenMultipleCallees = branchPtr(Equal, cachedFunctionReg, TrustedImmPtr(JSCell::seenMultipleCalleeObjects()));
    addSlowCase(branchPtr(NotEqual, calleeReg, cachedFunctionReg));
    hasSeenMultipleCallees.link(this);

    JumpList slowCases;
    auto butterfly = TrustedImmPtr(nullptr);
    emitAllocateJSObject(resultReg, JITAllocator::variable(), allocatorReg, structureReg, butterfly, scratchReg, slowCases);
    load8(Address(structureReg, Structure::inlineCapacityOffset()), scratchReg);
    emitInitializeInlineStorage(resultReg, scratchReg);
    addSlowCase(slowCases);
    emitStoreCell(bytecode.m_dst, resultReg);
}

void JIT::emit_op_to_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToThis>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    StructureID* cachedStructureID = &metadata.m_cachedStructureID;
    VirtualRegister thisRegister = bytecode.m_srcDst;

    emitLoad(thisRegister, regT3, regT2);

    addSlowCase(branchIfNotCell(regT3));
    addSlowCase(branchIfNotType(regT2, FinalObjectType));
    loadPtr(Address(regT2, JSCell::structureIDOffset()), regT0);
    load32(cachedStructureID, regT2);
    addSlowCase(branchPtr(NotEqual, regT0, regT2));
}

void JIT::emit_op_check_tdz(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckTdz>();
    emitLoadTag(bytecode.m_targetVirtualRegister, regT0);
    addSlowCase(branchIfEmpty(regT0));
}

template <typename OpCodeType>
void JIT::emit_op_has_structure_propertyImpl(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCodeType>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister enumerator = bytecode.m_enumerator;

    emitLoadPayload(base, regT0);
    emitJumpSlowCaseIfNotJSCell(base);

    emitLoadPayload(enumerator, regT1);

    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    addSlowCase(branch32(NotEqual, regT0, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));
    
    move(TrustedImm32(1), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_has_structure_property(const Instruction* currentInstruction)
{
    emit_op_has_structure_propertyImpl<OpHasStructureProperty>(currentInstruction);
}

void JIT::emit_op_has_own_structure_property(const Instruction* currentInstruction)
{
    emit_op_has_structure_propertyImpl<OpHasOwnStructureProperty>(currentInstruction);
}

void JIT::emit_op_in_structure_property(const Instruction* currentInstruction)
{
    emit_op_has_structure_propertyImpl<OpInStructureProperty>(currentInstruction);
}

void JIT::privateCompileHasIndexedProperty(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
{
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    
    PatchableJump badType;
    
    // FIXME: Add support for other types like TypedArrays and Arguments.
    // See https://bugs.webkit.org/show_bug.cgi?id=135033 and https://bugs.webkit.org/show_bug.cgi?id=135034.
    JumpList slowCases = emitLoadForArrayMode(currentInstruction, arrayMode, badType);
    move(TrustedImm32(1), regT0);
    Jump done = jump();

    LinkBuffer patchBuffer(*this, m_codeBlock);
    
    patchBuffer.link(badType, byValInfo->slowPathTarget);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);

    patchBuffer.link(done, byValInfo->doneTarget);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline has_indexed_property stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value());
    
    MacroAssembler::repatchJump(byValInfo->badTypeJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(operationHasIndexedPropertyGeneric));
}

void JIT::emit_op_has_indexed_property(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasIndexedProperty>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ArrayProfile* profile = &metadata.m_arrayProfile;
    ByValInfo* byValInfo = m_codeBlock->addByValInfo();
    
    emitLoadPayload(base, regT0);
    emitJumpSlowCaseIfNotJSCell(base);

    emitLoad(property, regT3, regT1);
    addSlowCase(branchIfNotInt32(regT3));

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

    Label nextHotPath = label();
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeIndex, PatchableJump(), badType, mode, profile, done, nextHotPath));
}

void JIT::emitSlow_op_has_indexed_property(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpHasIndexedProperty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;

    Label slowPath = label();
    
    emitLoad(base, regT1, regT0);
    emitLoad(property, regT3, regT2);
    Call call = callOperation(operationHasIndexedPropertyDefault, dst, m_codeBlock->globalObject(), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2), byValInfo);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_get_direct_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetDirectPname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister enumerator = bytecode.m_enumerator;

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
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_enumerator_structure_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorStructurePname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister enumerator = bytecode.m_enumerator;
    VirtualRegister index = bytecode.m_index;

    emitLoadPayload(index, regT0);
    emitLoadPayload(enumerator, regT1);
    Jump inBounds = branch32(Below, regT0, Address(regT1, JSPropertyNameEnumerator::endStructurePropertyIndexOffset()));

    move(TrustedImm32(JSValue::NullTag), regT2);
    move(TrustedImm32(0), regT0);

    Jump done = jump();
    inBounds.link(this);

    loadPtr(Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), regT1);
    loadPtr(BaseIndex(regT1, regT0, ScalePtr), regT0);
    move(TrustedImm32(JSValue::CellTag), regT2);

    done.link(this);
    emitStore(dst, regT2, regT0);
}

void JIT::emit_op_enumerator_generic_pname(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorGenericPname>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister enumerator = bytecode.m_enumerator;
    VirtualRegister index = bytecode.m_index;

    emitLoadPayload(index, regT0);
    emitLoadPayload(enumerator, regT1);
    Jump inBounds = branch32(Below, regT0, Address(regT1, JSPropertyNameEnumerator::endGenericPropertyIndexOffset()));

    move(TrustedImm32(JSValue::NullTag), regT2);
    move(TrustedImm32(0), regT0);

    Jump done = jump();
    inBounds.link(this);

    loadPtr(Address(regT1, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), regT1);
    loadPtr(BaseIndex(regT1, regT0, ScalePtr), regT0);
    move(TrustedImm32(JSValue::CellTag), regT2);
    
    done.link(this);
    emitStore(dst, regT2, regT0);
}

void JIT::emit_op_profile_type(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpProfileType>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    TypeLocation* cachedTypeLocation = metadata.m_typeLocation;
    VirtualRegister valueToProfile = bytecode.m_targetVirtualRegister;

    // Load payload in T0. Load tag in T3.
    emitLoadPayload(valueToProfile, regT0);
    emitLoadTag(valueToProfile, regT3);

    JumpList jumpToEnd;

    jumpToEnd.append(branchIfEmpty(regT3));

    // Compile in a predictive type check, if possible, to see if we can skip writing to the log.
    // These typechecks are inlined to match those of the 32-bit JSValue type checks.
    if (cachedTypeLocation->m_lastSeenType == TypeUndefined)
        jumpToEnd.append(branchIfUndefined(regT3));
    else if (cachedTypeLocation->m_lastSeenType == TypeNull)
        jumpToEnd.append(branchIfNull(regT3));
    else if (cachedTypeLocation->m_lastSeenType == TypeBoolean)
        jumpToEnd.append(branchIfBoolean(regT3, InvalidGPRReg));
    else if (cachedTypeLocation->m_lastSeenType == TypeAnyInt)
        jumpToEnd.append(branchIfInt32(regT3));
    else if (cachedTypeLocation->m_lastSeenType == TypeNumber) {
        jumpToEnd.append(branchIfNumber(JSValueRegs(regT3, regT0), regT1));
    } else if (cachedTypeLocation->m_lastSeenType == TypeString) {
        Jump isNotCell = branchIfNotCell(regT3);
        jumpToEnd.append(branchIfString(regT0));
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
    Jump notCell = branchIfNotCell(regT3);
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
    callOperation(operationProcessTypeProfilerLog, &vm());

    jumpToEnd.link(this);
}

void JIT::emit_op_log_shadow_chicken_prologue(const Instruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(nonArgGPR0 != regT0 && nonArgGPR0 != regT2, "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenPrologue>();
    GPRReg shadowPacketReg = regT0;
    GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
    GPRReg scratch2Reg = regT2;
    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);

    scratch1Reg = regT4;
    emitLoadPayload(bytecode.m_scope, regT3);
    logShadowChickenProloguePacket(shadowPacketReg, scratch1Reg, regT3);
}

void JIT::emit_op_log_shadow_chicken_tail(const Instruction* currentInstruction)
{
    RELEASE_ASSERT(vm().shadowChicken());
    updateTopCallFrame();
    static_assert(nonArgGPR0 != regT0 && nonArgGPR0 != regT2, "we will have problems if this is true.");
    auto bytecode = currentInstruction->as<OpLogShadowChickenTail>();
    GPRReg shadowPacketReg = regT0;
    GPRReg scratch1Reg = nonArgGPR0; // This must be a non-argument register.
    GPRReg scratch2Reg = regT2;
    ensureShadowChickenPacket(vm(), shadowPacketReg, scratch1Reg, scratch2Reg);
    emitLoadPayload(bytecode.m_thisValue, regT2);
    emitLoadTag(bytecode.m_thisValue, regT1);
    JSValueRegs thisRegs(regT1, regT2);
    emitLoadPayload(bytecode.m_scope, regT3);
    logShadowChickenTailPacket(shadowPacketReg, thisRegs, regT3, m_codeBlock, CallSiteIndex(m_bytecodeIndex));
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
