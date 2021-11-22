/*
 * Copyright (C) 2009-2021 Apple Inc. All rights reserved.
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

void JIT::emit_op_overrides_has_instance(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpOverridesHasInstance>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister constructor = bytecode.m_constructor;
    VirtualRegister hasInstanceValue = bytecode.m_hasInstanceValue;

    emitGetVirtualRegisterPayload(hasInstanceValue, regT0);
    // We don't jump if we know what Symbol.hasInstance would do.
    Jump hasInstanceValueNotCell = emitJumpIfNotJSCell(hasInstanceValue);
    loadGlobalObject(regT1);
    Jump customhasInstanceValue = branchPtr(NotEqual, regT0, Address(regT1, OBJECT_OFFSETOF(JSGlobalObject, m_functionProtoHasInstanceSymbolFunction)));

    // We know that constructor is an object from the way bytecode is emitted for instanceof expressions.
    emitGetVirtualRegisterPayload(constructor, regT0);

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

    using BaselineInstanceofRegisters::resultGPR;
    using BaselineInstanceofRegisters::valueJSR;
    using BaselineInstanceofRegisters::protoJSR;
    using BaselineInstanceofRegisters::stubInfoGPR;
    using BaselineInstanceofRegisters::scratch1GPR;
    using BaselineInstanceofRegisters::scratch2GPR;

    emitGetVirtualRegisterPayload(value, valueJSR.payloadGPR());
    emitGetVirtualRegisterPayload(proto, protoJSR.payloadGPR());

    // Check that proto are cells. baseVal must be a cell - this is checked by the get_by_id for Symbol.hasInstance.
    emitJumpSlowCaseIfNotJSCell(value);
    emitJumpSlowCaseIfNotJSCell(proto);
    
    JITInstanceOfGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        RegisterSet::stubUnavailableRegisters(),
        resultGPR,
        valueJSR.payloadGPR(),
        protoJSR.payloadGPR(),
        stubInfoGPR,
        scratch1GPR, scratch2GPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::InstanceOf;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_instanceOfs.append(gen);
    
    emitStoreBool(dst, resultGPR);
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

    using SlowOperation = decltype(operationInstanceOfOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    using BaselineInstanceofRegisters::valueJSR;
    using BaselineInstanceofRegisters::protoJSR;
    static_assert(valueJSR == preferredArgumentJSR<SlowOperation, 2>());
    // Proto will be passed on stack, just make sure no overlap
    static_assert(noOverlap(valueJSR, protoJSR, globalObjectGPR, stubInfoGPR));

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegisterTag(value, valueJSR.tagGPR());
    emitGetVirtualRegisterTag(proto, protoJSR.tagGPR());
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        dst,
        globalObjectGPR, stubInfoGPR, valueJSR, protoJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_is_empty(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsEmpty>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegister(value, regT1, regT0);
    compare32(Equal, regT1, TrustedImm32(JSValue::EmptyValueTag), regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_boolean(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsBoolean>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegisterTag(value, regT0);
    compare32(Equal, regT0, TrustedImm32(JSValue::BooleanTag), regT0);
    emitStoreBool(dst, regT0);
}

void JIT::emit_op_is_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpIsNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister value = bytecode.m_operand;
    
    emitGetVirtualRegisterTag(value, regT0);
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

    emitGetVirtualRegister(value, regT1, regT0);
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

    emitGetVirtualRegister(value, regT1, regT0);
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

    emitGetVirtualRegister(src, regT1, regT0);

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

    emitGetVirtualRegister(src, regT1, regT0);

    addSlowCase(branchIfNotCell(regT1));
    Jump done = branchIfSymbol(regT0);
    addSlowCase(branchIfNotString(regT0));

    done.link(this);
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_not(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpNot>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitGetVirtualRegisterTag(src, regT0);

    emitGetVirtualRegister(src, regT1, regT0);
    addSlowCase(branchIfNotBoolean(regT1, InvalidGPRReg));
    xor32(TrustedImm32(1), regT0);

    emitStoreBool(dst, regT0, (dst == src));
}

void JIT::emit_op_jundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegisterTag(value, regT0);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT0);
    addJump(branchIfNull(regT0), target);
}

void JIT::emit_op_jnundefined_or_null(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJnundefinedOrNull>();
    VirtualRegister value = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegisterTag(value, regT0);
    static_assert((JSValue::UndefinedTag + 1 == JSValue::NullTag) && (JSValue::NullTag & 0x1), "");
    or32(TrustedImm32(1), regT0);
    addJump(branchIfNotNull(regT0), target);
}

void JIT::emit_op_jeq_ptr(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeqPtr>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, regT1, regT0);
    Jump notCell = branchIfNotCell(regT1);
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, regT2);
    addJump(branchPtr(Equal, regT0, regT2), target);
    notCell.link(this);
}

void JIT::emit_op_jneq_ptr(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJneqPtr>();
    VirtualRegister src = bytecode.m_value;
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);

    emitGetVirtualRegister(src, regT1, regT0);
    Jump notCell = branchIfNotCell(regT1);
    loadCodeBlockConstantPayload(bytecode.m_specialPointer, regT2);
    Jump equal = branchPtr(Equal, regT0, regT2);
    notCell.link(this);
    store8ToMetadata(TrustedImm32(1), bytecode, OpJneqPtr::Metadata::offsetOfHasJumped());
    addJump(jump(), target);
    equal.link(this);
}

void JIT::emit_op_eq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEq>();

    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, regT1, regT0);
    emitGetVirtualRegister(src2, regT3, regT2);
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
    loadGlobalObject(regT1);
    callOperation(operationCompareStringEq, regT1, regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    loadGlobalObject(regT4);
    callOperation(operationCompareEq, regT4, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));

    storeResult.link(this);
    emitStoreBool(dst, returnValueGPR);
}

void JIT::emit_op_jeq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpJeq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, regT1, regT0);
    emitGetVirtualRegister(src2, regT3, regT2);
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
    loadGlobalObject(regT1);
    callOperation(operationCompareStringEq, regT1, regT0, regT2);
    emitJumpSlowToHot(branchTest32(type == CompileOpEqType::Eq ? NonZero : Zero, returnValueGPR), jumpTarget);
    done.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    loadGlobalObject(regT4);
    callOperation(operationCompareEq, regT4, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
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

    emitGetVirtualRegister(src1, regT1, regT0);
    emitGetVirtualRegister(src2, regT3, regT2);
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
    loadGlobalObject(regT1);
    callOperation(operationCompareStringEq, regT1, regT0, regT2);
    storeResult.append(jump());

    // Generic case.
    genericCase.append(getSlowCase(iter)); // doubles
    genericCase.link(this);
    loadGlobalObject(regT4);
    callOperation(operationCompareEq, regT4, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));

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

    emitGetVirtualRegister(src1, regT1, regT0);
    emitGetVirtualRegister(src2, regT3, regT2);
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
void JIT::compileOpStrictEq(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, regT1, regT0);
    emitGetVirtualRegister(src2, regT3, regT2);

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
    if constexpr (std::is_same<Op, OpStricteq>::value)
        compare32(Equal, regT0, regT2, regT0);
    else
        compare32(NotEqual, regT0, regT2, regT0);

    emitStoreBool(dst, regT0);
}

void JIT::emit_op_stricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpStricteq>(currentInstruction);
}

void JIT::emit_op_nstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEq<OpNstricteq>(currentInstruction);
}

template<typename Op>
void JIT::compileOpStrictEqJump(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    int target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    VirtualRegister src1 = bytecode.m_lhs;
    VirtualRegister src2 = bytecode.m_rhs;

    emitGetVirtualRegister(src1, jsRegT10);
    emitGetVirtualRegister(src2, jsRegT32);

    // Bail if the tags differ, or are double.
    addSlowCase(branch32(NotEqual, jsRegT10.tagGPR(), jsRegT32.tagGPR()));
    addSlowCase(branch32(Below, jsRegT10.tagGPR(), TrustedImm32(JSValue::LowestTag)));

    // Jump to a slow case if both are strings or symbols (non object).
    Jump notCell = branchIfNotCell(jsRegT10);
    Jump firstIsObject = branchIfObject(jsRegT10.payloadGPR());
    addSlowCase(branchIfNotObject(jsRegT32.payloadGPR()));
    notCell.link(this);
    firstIsObject.link(this);

    // Simply compare the payloads.
    if constexpr (std::is_same<Op, OpJstricteq>::value)
        addJump(branch32(Equal, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
    else
        addJump(branch32(NotEqual, jsRegT10.payloadGPR(), jsRegT32.payloadGPR()), target);
}

void JIT::emit_op_jstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJstricteq>(currentInstruction);
}

void JIT::emit_op_jnstricteq(const Instruction* currentInstruction)
{
    compileOpStrictEqJump<OpJnstricteq>(currentInstruction);
}

void JIT::emitSlow_op_jstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT4);
    callOperation(operationCompareStrictEq, regT4, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    emitJumpSlowToHot(branchTest32(NonZero, returnValueGPR), target);
}

void JIT::emitSlow_op_jnstricteq(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpJnstricteq>();
    unsigned target = jumpTarget(currentInstruction, bytecode.m_targetLabel);
    loadGlobalObject(regT4);
    callOperation(operationCompareStrictEq, regT4, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    emitJumpSlowToHot(branchTest32(Zero, returnValueGPR), target);
}

void JIT::emit_op_to_number(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumber>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitGetVirtualRegister(src, regT1, regT0);

    Jump isInt32 = branchIfInt32(regT1);
    addSlowCase(branch32(AboveOrEqual, regT1, TrustedImm32(JSValue::LowestTag)));
    isInt32.link(this);

    emitValueProfilingSite(bytecode, JSValueRegs(regT1, regT0));
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_numeric(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToNumeric>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;
    JSValueRegs argumentValueRegs(regT1, regT0);

    emitGetVirtualRegister(src, regT1, regT0);

    Jump isNotCell = branchIfNotCell(regT1);
    addSlowCase(branchIfNotHeapBigInt(regT0));
    Jump isBigInt = jump();

    isNotCell.link(this);
    addSlowCase(branchIfNotNumber(argumentValueRegs, regT2));
    isBigInt.link(this);

    emitValueProfilingSite(bytecode, JSValueRegs(regT1, regT0));
    if (src != dst)
        emitStore(dst, regT1, regT0);
}

void JIT::emit_op_to_string(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpToString>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister src = bytecode.m_operand;

    emitGetVirtualRegister(src, regT1, regT0);

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

    emitGetVirtualRegister(src, regT1, regT0);

    addSlowCase(branchIfNotCell(regT1));
    addSlowCase(branchIfNotObject(regT0));

    emitValueProfilingSite(bytecode, JSValueRegs(regT1, regT0));
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

    addPtr(TrustedImm32(stackPointerOffsetFor(m_unlinkedCodeBlock) * sizeof(Register)), callFrameRegister, stackPointerRegister);

    // When the LLInt throws an exception, there is a chance that we've already tiered up
    // the same CodeBlock to baseline, and we'll catch the exception in the baseline JIT (because
    // we updated the exception handlers to point here). Because the LLInt uses a different value
    // inside s_constantsGPR, the callee saves we restore above may not contain the correct register.
    // So we replenish it here.
    {
        loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
        loadPtr(Address(regT0, CodeBlock::offsetOfJITData()), regT0);
        loadPtr(Address(regT0, CodeBlock::JITData::offsetOfJITConstantPool()), s_constantsGPR);
    }

    callOperationNoExceptionCheck(operationRetrieveAndClearExceptionIfCatchable, &vm());
    Jump isCatchableException = branchTest32(NonZero, returnValueGPR);
    jumpToExceptionHandler(vm());
    isCatchableException.link(this);

    // Now store the exception returned by operationThrow.
    move(returnValueGPR, regT2);
    move(TrustedImm32(JSValue::CellTag), regT1);

    emitStore(bytecode.m_exception, regT1, regT2);

    load32(Address(regT2, Exception::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), regT0);
    load32(Address(regT2, Exception::valueOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), regT1);

    emitStore(bytecode.m_thrownValue, regT1, regT0);

#if ENABLE(DFG_JIT)
    // FIXME: consider inline caching the process of doing OSR entry, including
    // argument type proofs, storing locals to the buffer, etc
    // https://bugs.webkit.org/show_bug.cgi?id=175598

    callOperationNoExceptionCheck(operationTryOSREnterAtCatchAndValueProfile, &vm(), m_bytecodeIndex.asBits());
    auto skipOSREntry = branchTestPtr(Zero, returnValueGPR);
    emitRestoreCalleeSaves();
    farJump(returnValueGPR, NoPtrTag);
    skipOSREntry.link(this);
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
    emitGetVirtualRegisterPayload(currentScope, regT0);
    loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitStoreCell(bytecode.m_dst, regT0);
}

void JIT::emit_op_enter(const Instruction* currentInstruction)
{
    emitEnterOptimizationCheck();
    
    // Even though JIT code doesn't use them, we initialize our constant
    // registers to zap stale pointers, to avoid unnecessarily prolonging
    // object lifetime and increasing GC pressure.
    size_t count = m_unlinkedCodeBlock->numVars();
    for (size_t i = CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters(); i < count; ++i)
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

void JIT::emit_op_check_tdz(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckTdz>();
    emitGetVirtualRegisterTag(bytecode.m_targetVirtualRegister, regT0);
    addSlowCase(branchIfEmpty(regT0));
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
