/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "DirectArguments.h"
#include "GCAwareJITStubRoutine.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "LinkBuffer.h"
#include "OpcodeInlines.h"
#include "ResultType.h"
#include "SlowPathCall.h"
#include "StructureStubInfo.h"
#include <wtf/StringPrintStream.h>


namespace JSC {
    
void JIT::emit_op_put_getter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    int base = bytecode.base.offset();
    int property = bytecode.property;
    int options = bytecode.attributes;
    int getter = bytecode.accessor.offset();

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterById, regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    int base = bytecode.base.offset();
    int property = bytecode.property;
    int options = bytecode.attributes;
    int setter = bytecode.accessor.offset();

    emitLoadPayload(base, regT1);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterById, regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    int base = bytecode.base.offset();
    int property = bytecode.property;
    int attributes = bytecode.attributes;
    int getter = bytecode.getter.offset();
    int setter = bytecode.setter.offset();

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    emitLoadPayload(setter, regT4);
    callOperation(operationPutGetterSetter, regT1, m_codeBlock->identifier(property).impl(), attributes, regT3, regT4);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    int32_t attributes = bytecode.attributes;
    int getter = bytecode.accessor.offset();

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterByVal, regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    int32_t attributes = bytecode.attributes;
    int setter = bytecode.accessor.offset();

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterByVal, regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_del_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property;
    emitLoad(base, regT1, regT0);
    callOperation(operationDeleteByIdJSResult, dst, JSValueRegs(regT1, regT0), m_codeBlock->identifier(property).impl());
}

void JIT::emit_op_del_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    callOperation(operationDeleteByValJSResult, dst, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
}

void JIT::emit_op_get_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    ArrayProfile* profile = &metadata.arrayProfile;
    ByValInfo* byValInfo = m_codeBlock->addByValInfo();

    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    PatchableJump notIndex = patchableBranch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag));
    addSlowCase(notIndex);
    emitArrayProfilingSiteWithCell(regT0, regT1, profile);
    and32(TrustedImm32(IndexingShapeMask), regT1);

    PatchableJump badType;
    JumpList slowCases;
    
    JITArrayMode mode = chooseArrayMode(profile);
    switch (mode) {
    case JITInt32:
        slowCases = emitInt32GetByVal(currentInstruction, badType);
        break;
    case JITDouble:
        slowCases = emitDoubleGetByVal(currentInstruction, badType);
        break;
    case JITContiguous:
        slowCases = emitContiguousGetByVal(currentInstruction, badType);
        break;
    case JITArrayStorage:
        slowCases = emitArrayStorageGetByVal(currentInstruction, badType);
        break;
    default:
        CRASH();
    }
    
    addSlowCase(badType);
    addSlowCase(slowCases);
    
    Label done = label();

    if (!ASSERT_DISABLED) {
        Jump resultOK = branchIfNotEmpty(regT1);
        abortWithReason(JITGetByValResultIsNotEmpty);
        resultOK.link(this);
    }

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);

    Label nextHotPath = label();
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeOffset, notIndex, badType, mode, profile, done, nextHotPath));
}

JITGetByIdGenerator JIT::emitGetByValWithCachedId(ByValInfo* byValInfo, OpGetByVal bytecode, const Identifier& propertyName, Jump& fastDoneCase, Jump& slowDoneCase, JumpList& slowCases)
{
    // base: tag(regT1), payload(regT0)
    // property: tag(regT3), payload(regT2)
    // scratch: regT4

    int dst = bytecode.dst.offset();

    slowCases.append(branchIfNotCell(regT3));
    emitByValIdentifierCheck(byValInfo, regT2, regT4, propertyName, slowCases);

    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        propertyName.impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::Get);
    gen.generateFastPath(*this);

    fastDoneCase = jump();

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, dst, gen.stubInfo(), JSValueRegs(regT1, regT0), propertyName.impl());
    gen.reportSlowPathCall(coldPathBegin, call);
    slowDoneCase = jump();

    return gen;
}

void JIT::emitSlow_op_get_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;

    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // property int32 check

    Jump nonCell = jump();
    linkSlowCase(iter); // base array check
    Jump notString = branchIfNotString(regT0);
    emitNakedCall(CodeLocationLabel<NoPtrTag>(m_vm->getCTIStub(stringGetByValGenerator).retaggedCode<NoPtrTag>()));
    Jump failed = branchTestPtr(Zero, regT0);
    emitStoreCell(dst, regT0);
    emitJumpSlowToHot(jump(), currentInstruction->size());
    failed.link(this);
    notString.link(this);
    nonCell.link(this);

    linkSlowCase(iter); // vector length check
    linkSlowCase(iter); // empty value
    
    Label slowPath = label();
    
    emitLoad(base, regT1, regT0);
    emitLoad(property, regT3, regT2);
    Call call = callOperation(operationGetByValOptimize, dst, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2), byValInfo);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
}

void JIT::emit_op_put_by_val_direct(const Instruction* currentInstruction)
{
    emit_op_put_by_val<OpPutByValDirect>(currentInstruction);
}

template<typename Op>
void JIT::emit_op_put_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    ArrayProfile* profile = &metadata.arrayProfile;
    ByValInfo* byValInfo = m_codeBlock->addByValInfo();
    
    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    PatchableJump notIndex = patchableBranch32(NotEqual, regT3, TrustedImm32(JSValue::Int32Tag));
    addSlowCase(notIndex);
    emitArrayProfilingSiteWithCell(regT0, regT1, profile);
    
    PatchableJump badType;
    JumpList slowCases;

    // FIXME: Maybe we should do this inline?
    addSlowCase(branchTest32(NonZero, regT1, TrustedImm32(CopyOnWrite)));
    and32(TrustedImm32(IndexingShapeMask), regT1);
    
    JITArrayMode mode = chooseArrayMode(profile);
    switch (mode) {
    case JITInt32:
        slowCases = emitInt32PutByVal(bytecode, badType);
        break;
    case JITDouble:
        slowCases = emitDoublePutByVal(bytecode, badType);
        break;
    case JITContiguous:
        slowCases = emitContiguousPutByVal(bytecode, badType);
        break;
    case JITArrayStorage:
        slowCases = emitArrayStoragePutByVal(bytecode, badType);
        break;
    default:
        CRASH();
        break;
    }
    
    addSlowCase(badType);
    addSlowCase(slowCases);
    
    Label done = label();
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeOffset, notIndex, badType, mode, profile, done, done));
}

template <typename Op>
JIT::JumpList JIT::emitGenericContiguousPutByVal(Op bytecode, PatchableJump& badType, IndexingType indexingShape)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    int base = bytecode.base.offset();
    int value = bytecode.value.offset();
    ArrayProfile* profile = &metadata.arrayProfile;

    JumpList slowCases;
    
    badType = patchableBranch32(NotEqual, regT1, TrustedImm32(ContiguousShape));
    
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT3);
    Jump outOfBounds = branch32(AboveOrEqual, regT2, Address(regT3, Butterfly::offsetOfPublicLength()));
    
    Label storeResult = label();
    emitLoad(value, regT1, regT0);
    switch (indexingShape) {
    case Int32Shape:
        slowCases.append(branchIfNotInt32(regT1));
        store32(regT0, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        store32(regT1, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        break;
    case ContiguousShape:
        store32(regT0, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload)));
        store32(regT1, BaseIndex(regT3, regT2, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag)));
        emitLoad(base, regT2, regT3);
        emitWriteBarrier(base, value, ShouldFilterValue);
        break;
    case DoubleShape: {
        Jump notInt = branchIfNotInt32(regT1);
        convertInt32ToDouble(regT0, fpRegT0);
        Jump ready = jump();
        notInt.link(this);
        moveIntsToDouble(regT0, regT1, fpRegT0, fpRegT1);
        slowCases.append(branchIfNaN(fpRegT0));
        ready.link(this);
        storeDouble(fpRegT0, BaseIndex(regT3, regT2, TimesEight));
        break;
    }
    default:
        CRASH();
        break;
    }
        
    Jump done = jump();
    
    outOfBounds.link(this);
    slowCases.append(branch32(AboveOrEqual, regT2, Address(regT3, Butterfly::offsetOfVectorLength())));
    
    emitArrayProfileStoreToHoleSpecialCase(profile);
    
    add32(TrustedImm32(1), regT2, regT1);
    store32(regT1, Address(regT3, Butterfly::offsetOfPublicLength()));
    jump().linkTo(storeResult, this);
    
    done.link(this);
    
    return slowCases;
}

template <typename Op>
JIT::JumpList JIT::emitArrayStoragePutByVal(Op bytecode, PatchableJump& badType)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    int base = bytecode.base.offset();
    int value = bytecode.value.offset();
    ArrayProfile* profile = &metadata.arrayProfile;

    JumpList slowCases;
    
    badType = patchableBranch32(NotEqual, regT1, TrustedImm32(ArrayStorageShape));
    
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT3);
    slowCases.append(branch32(AboveOrEqual, regT2, Address(regT3, ArrayStorage::vectorLengthOffset())));

    Jump empty = branch32(Equal, BaseIndex(regT3, regT2, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag)), TrustedImm32(JSValue::EmptyValueTag));
    
    Label storeResult(this);
    emitLoad(value, regT1, regT0);
    store32(regT0, BaseIndex(regT3, regT2, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.payload))); // payload
    store32(regT1, BaseIndex(regT3, regT2, TimesEight, ArrayStorage::vectorOffset() + OBJECT_OFFSETOF(JSValue, u.asBits.tag))); // tag
    Jump end = jump();
    
    empty.link(this);
    emitArrayProfileStoreToHoleSpecialCase(profile);
    add32(TrustedImm32(1), Address(regT3, OBJECT_OFFSETOF(ArrayStorage, m_numValuesInVector)));
    branch32(Below, regT2, Address(regT3, ArrayStorage::lengthOffset())).linkTo(storeResult, this);
    
    add32(TrustedImm32(1), regT2, regT0);
    store32(regT0, Address(regT3, ArrayStorage::lengthOffset()));
    jump().linkTo(storeResult, this);
    
    end.link(this);
    
    emitWriteBarrier(base, value, ShouldFilterValue);
    
    return slowCases;
}

template <typename Op>
JITPutByIdGenerator JIT::emitPutByValWithCachedId(ByValInfo* byValInfo, Op bytecode, PutKind putKind, const Identifier& propertyName, JumpList& doneCases, JumpList& slowCases)
{
    // base: tag(regT1), payload(regT0)
    // property: tag(regT3), payload(regT2)

    int base = bytecode.base.offset();
    int value = bytecode.value.offset();

    slowCases.append(branchIfNotCell(regT3));
    emitByValIdentifierCheck(byValInfo, regT2, regT2, propertyName, slowCases);

    // Write barrier breaks the registers. So after issuing the write barrier,
    // reload the registers.
    emitWriteBarrier(base, value, ShouldFilterBase);
    emitLoadPayload(base, regT0);
    emitLoad(value, regT3, regT2);

    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        JSValueRegs::payloadOnly(regT0), JSValueRegs(regT3, regT2), regT1, m_codeBlock->ecmaMode(), putKind);
    gen.generateFastPath(*this);
    doneCases.append(jump());

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    // JITPutByIdGenerator only preserve the value and the base's payload, we have to reload the tag.
    emitLoadTag(base, regT1);

    Call call = callOperation(gen.slowPathFunction(), gen.stubInfo(), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0), propertyName.impl());
    gen.reportSlowPathCall(coldPathBegin, call);
    doneCases.append(jump());

    return gen;
}

void JIT::emitSlow_op_put_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    bool isDirect = currentInstruction->opcodeID() == op_put_by_val_direct;
    int base;
    int property;
    int value;

    auto load = [&](auto bytecode) {
        base = bytecode.base.offset();
        property = bytecode.property.offset();
        value = bytecode.value.offset();
    };

    if (isDirect)
        load(currentInstruction->as<OpPutByValDirect>());
    else
        load(currentInstruction->as<OpPutByVal>());

    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;
    
    linkAllSlowCases(iter);
    Label slowPath = label();
    
    // The register selection below is chosen to reduce register swapping on ARM.
    // Swapping shouldn't happen on other platforms.
    emitLoad(base, regT2, regT1);
    emitLoad(property, regT3, regT0);
    emitLoad(value, regT5, regT4);
    Call call = callOperation(isDirect ? operationDirectPutByValOptimize : operationPutByValOptimize, JSValueRegs(regT2, regT1), JSValueRegs(regT3, regT0), JSValueRegs(regT5, regT4), byValInfo);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_try_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::TryGet);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);
    
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_try_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpTryGetById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationTryGetByIdOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id_direct(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::GetDirect);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id_direct(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdDirectOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling()) {
        Jump notArrayLengthMode = branch8(NotEqual, AbsoluteAddress(&metadata.mode), TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
        emitArrayProfilingSiteWithCell(regT0, regT2, &metadata.modeMetadata.arrayLengthMode.arrayProfile);
        notArrayLengthMode.link(this);
    }

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::Get);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id_with_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int thisVReg = bytecode.thisValue.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));
    
    emitLoad(base, regT1, regT0);
    emitLoad(thisVReg, regT4, regT3);
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    emitJumpSlowCaseIfNotJSCell(thisVReg, regT4);

    JITGetByIdWithThisGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT1, regT0), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT4, regT3), AccessType::GetWithThis);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIdsWithThis.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id_with_this(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdWithThisOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), JSValueRegs(regT4, regT3), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_put_by_id(const Instruction* currentInstruction)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.
    
    auto bytecode = currentInstruction->as<OpPutById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int base = bytecode.base.offset();
    int value = bytecode.value.offset();
    int direct = metadata.flags & PutByIdIsDirect;
    
    emitLoad2(base, regT1, regT0, value, regT3, regT2);
    
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        JSValueRegs::payloadOnly(regT0), JSValueRegs(regT3, regT2),
        regT1, m_codeBlock->ecmaMode(), direct ? Direct : NotDirect);
    
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    
    emitWriteBarrier(base, value, ShouldFilterBase);

    m_putByIds.append(gen);
}

void JIT::emitSlow_op_put_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutById>();
    int base = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    Label coldPathBegin(this);

    // JITPutByIdGenerator only preserve the value and the base's payload, we have to reload the tag.
    emitLoadTag(base, regT1);

    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];
    
    Call call = callOperation(
        gen.slowPathFunction(), gen.stubInfo(), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_in_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITInByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0));
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_inByIds.append(gen);

    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_in_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationInByIdOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emitVarInjectionCheck(bool needsVarInjectionChecks)
{
    if (!needsVarInjectionChecks)
        return;
    addSlowCase(branch8(Equal, AbsoluteAddress(m_codeBlock->globalObject()->varInjectionWatchpoint()->addressOfState()), TrustedImm32(IsInvalidated)));
}

void JIT::emitResolveClosure(int dst, int scope, bool needsVarInjectionChecks, unsigned depth)
{
    emitVarInjectionCheck(needsVarInjectionChecks);
    move(TrustedImm32(JSValue::CellTag), regT1);
    emitLoadPayload(scope, regT0);
    for (unsigned i = 0; i < depth; ++i)
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_resolve_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int dst = bytecode.dst.offset();
    int scope = bytecode.scope.offset();
    ResolveType resolveType = metadata.resolveType;
    unsigned depth = metadata.localScopeDepth;

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks:
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks: 
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_codeBlock);
            RELEASE_ASSERT(constantScope);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(constantScope), regT0);
            emitStore(dst, regT1, regT0);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(dst, scope, needsVarInjectionChecks(resolveType), depth);
            break;
        case ModuleVar:
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(metadata.lexicalEnvironment.get()), regT0);
            emitStore(dst, regT1, regT0);
            break;
        case Dynamic:
            addSlowCase(jump());
            break;
        case LocalClosureVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };
    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        // Since these GlobalProperty can be changed to GlobalLexicalVar, we should load the value from metadata.
        JSScope** constantScopeSlot = metadata.constantScope.slot();
        emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
        move(TrustedImm32(JSValue::CellTag), regT1);
        loadPtr(constantScopeSlot, regT0);
        emitStore(dst, regT1, regT0);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&currentInstruction[4], regT0);

        Jump notGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(GlobalProperty));
        emitCode(GlobalProperty);
        skipToEnd.append(jump());
        notGlobalProperty.link(this);

        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        emitCode(GlobalPropertyWithVarInjectionChecks);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());
        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType);
        break;
    }
}

void JIT::emitLoadWithStructureCheck(int scope, Structure** structureSlot)
{
    emitLoad(scope, regT1, regT0);
    loadPtr(structureSlot, regT2);
    addSlowCase(branchPtr(NotEqual, Address(regT0, JSCell::structureIDOffset()), regT2));
}

void JIT::emitGetVarFromPointer(JSValue* operand, GPRReg tag, GPRReg payload)
{
    uintptr_t rawAddress = bitwise_cast<uintptr_t>(operand);
    load32(bitwise_cast<void*>(rawAddress + TagOffset), tag);
    load32(bitwise_cast<void*>(rawAddress + PayloadOffset), payload);
}
void JIT::emitGetVarFromIndirectPointer(JSValue** operand, GPRReg tag, GPRReg payload)
{
    loadPtr(operand, payload);
    load32(Address(payload, TagOffset), tag);
    load32(Address(payload, PayloadOffset), payload);
}

void JIT::emitGetClosureVar(int scope, uintptr_t operand)
{
    emitLoad(scope, regT1, regT0);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + TagOffset), regT1);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + PayloadOffset), regT0);
}

void JIT::emit_op_get_from_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int dst = bytecode.dst.offset();
    int scope = bytecode.scope.offset();
    ResolveType resolveType = metadata.getPutInfo.resolveType();
    Structure** structureSlot = metadata.structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.operand);
    
    auto emitCode = [&] (ResolveType resolveType, bool indirectLoadForOperand) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection.
            GPRReg base = regT2;
            GPRReg resultTag = regT1;
            GPRReg resultPayload = regT0;
            GPRReg offset = regT3;
            
            move(regT0, base);
            load32(operandSlot, offset);
            if (!ASSERT_DISABLED) {
                Jump isOutOfLine = branch32(GreaterThanOrEqual, offset, TrustedImm32(firstOutOfLineOffset));
                abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(this);
            }
            loadPtr(Address(base, JSObject::butterflyOffset()), base);
            neg32(offset);
            load32(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.payload) + (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), resultPayload);
            load32(BaseIndex(base, offset, TimesEight, OBJECT_OFFSETOF(JSValue, u.asBits.tag) + (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), resultTag);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            if (indirectLoadForOperand)
                emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT1, regT0);
            else
                emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT1, regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                addSlowCase(branchIfEmpty(regT1));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitGetClosureVar(scope, *operandSlot);
            break;
        case Dynamic:
            addSlowCase(jump());
            break;
        case ModuleVar:
        case LocalClosureVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&currentInstruction[4], regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isNotGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(resolveType));
        emitCode(resolveType, false);
        skipToEnd.append(jump());

        isNotGlobalProperty.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar, true);
        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&currentInstruction[4], regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(GlobalProperty));
        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        isGlobalProperty.link(this);
        emitCode(GlobalProperty, false);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar, true);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks, true);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());

        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType, false);
        break;
    }
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_from_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetFromScope>();
    int dst = bytecode.dst.offset();
    callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetFromScope, dst, currentInstruction);
}

void JIT::emitPutGlobalVariable(JSValue* operand, int value, WatchpointSet* set)
{
    emitLoad(value, regT1, regT0);
    emitNotifyWrite(set);
    uintptr_t rawAddress = bitwise_cast<uintptr_t>(operand);
    store32(regT1, bitwise_cast<void*>(rawAddress + TagOffset));
    store32(regT0, bitwise_cast<void*>(rawAddress + PayloadOffset));
}

void JIT::emitPutGlobalVariableIndirect(JSValue** addressOfOperand, int value, WatchpointSet** indirectWatchpointSet)
{
    emitLoad(value, regT1, regT0);
    loadPtr(indirectWatchpointSet, regT2);
    emitNotifyWrite(regT2);
    loadPtr(addressOfOperand, regT2);
    store32(regT1, Address(regT2, TagOffset));
    store32(regT0, Address(regT2, PayloadOffset));
}

void JIT::emitPutClosureVar(int scope, uintptr_t operand, int value, WatchpointSet* set)
{
    emitLoad(value, regT3, regT2);
    emitLoad(scope, regT1, regT0);
    emitNotifyWrite(set);
    store32(regT3, Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + TagOffset));
    store32(regT2, Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + PayloadOffset));
}

void JIT::emit_op_put_to_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToScope>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int scope = bytecode.scope.offset();
    int value = bytecode.value.offset();
    GetPutInfo getPutInfo = copiedGetPutInfo(bytecode);
    ResolveType resolveType = getPutInfo.resolveType();
    Structure** structureSlot = metadata.structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.operand);
    
    auto emitCode = [&] (ResolveType resolveType, bool indirectLoadForOperand) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            emitWriteBarrier(m_codeBlock->globalObject(), value, ShouldFilterValue);
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection.
            emitLoad(value, regT3, regT2);
            
            loadPtr(Address(regT0, JSObject::butterflyOffset()), regT0);
            loadPtr(operandSlot, regT1);
            negPtr(regT1);
            store32(regT3, BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
            store32(regT2, BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_codeBlock);
            RELEASE_ASSERT(constantScope);
            emitWriteBarrier(constantScope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            if (!isInitialization(getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                if (indirectLoadForOperand)
                    emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT1, regT0);
                else
                    emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT1, regT0);
                addSlowCase(branchIfEmpty(regT1));
            }
            if (indirectLoadForOperand)
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, &metadata.watchpointSet);
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, metadata.watchpointSet);
            break;
        }
        case LocalClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitWriteBarrier(scope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitPutClosureVar(scope, *operandSlot, value, metadata.watchpointSet);
            break;
        case ModuleVar:
        case Dynamic:
            addSlowCase(jump());
            break;
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&currentInstruction[4], regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(resolveType));
        Jump isGlobalLexicalVar = branch32(Equal, regT0, TrustedImm32(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar));
        addSlowCase(jump()); // Dynamic, it can happen if we attempt to put a value to already-initialized const binding.

        isGlobalLexicalVar.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar, true);
        skipToEnd.append(jump());

        isGlobalProperty.link(this);
        emitCode(resolveType, false);
        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&currentInstruction[4], regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(GlobalProperty));
        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        isGlobalProperty.link(this);
        emitCode(GlobalProperty, false);
        skipToEnd.append(jump());
        notGlobalPropertyWithVarInjections.link(this);

        Jump notGlobalLexicalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVar));
        emitCode(GlobalLexicalVar, true);
        skipToEnd.append(jump());
        notGlobalLexicalVar.link(this);

        Jump notGlobalLexicalVarWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalLexicalVarWithVarInjectionChecks));
        emitCode(GlobalLexicalVarWithVarInjectionChecks, true);
        skipToEnd.append(jump());
        notGlobalLexicalVarWithVarInjections.link(this);

        addSlowCase(jump());

        skipToEnd.link(this);
        break;
    }

    default:
        emitCode(resolveType, false);
        break;
    }
}

void JIT::emitSlow_op_put_to_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutToScope>();
    ResolveType resolveType = copiedGetPutInfo(bytecode).resolveType();
    if (resolveType == ModuleVar) {
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else
        callOperation(operationPutToScope, currentInstruction);
}

void JIT::emit_op_get_from_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    int dst = bytecode.dst.offset();
    int arguments = bytecode.arguments.offset();
    int index = bytecode.index;
    
    emitLoadPayload(arguments, regT0);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset), regT1);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset), regT0);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_put_to_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    int arguments = bytecode.arguments.offset();
    int index = bytecode.index;
    int value = bytecode.value.offset();
    
    emitWriteBarrier(arguments, value, ShouldFilterValue);
    
    emitLoadPayload(arguments, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset));
    store32(regT2, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset));
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
