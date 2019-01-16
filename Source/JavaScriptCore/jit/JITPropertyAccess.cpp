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
#include "JIT.h"

#include "CodeBlock.h"
#include "DirectArguments.h"
#include "GCAwareJITStubRoutine.h"
#include "GetterSetter.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "LinkBuffer.h"
#include "OpcodeInlines.h"
#include "ResultType.h"
#include "ScopedArguments.h"
#include "ScopedArgumentsTable.h"
#include "SlowPathCall.h"
#include "StructureStubInfo.h"
#include <wtf/ScopedLambda.h>
#include <wtf/StringPrintStream.h>


namespace JSC {
#if USE(JSVALUE64)

void JIT::emit_op_get_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    ArrayProfile* profile = &metadata.arrayProfile;
    ByValInfo* byValInfo = m_codeBlock->addByValInfo();

    emitGetVirtualRegister(base, regT0);
    bool propertyNameIsIntegerConstant = isOperandConstantInt(property);
    if (propertyNameIsIntegerConstant)
        move(Imm32(getOperandConstantInt(property)), regT1);
    else
        emitGetVirtualRegister(property, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, base);

    PatchableJump notIndex;
    if (!propertyNameIsIntegerConstant) {
        notIndex = emitPatchableJumpIfNotInt(regT1);
        addSlowCase(notIndex);

        // This is technically incorrect - we're zero-extending an int32. On the hot path this doesn't matter.
        // We check the value as if it was a uint32 against the m_vectorLength - which will always fail if
        // number was signed since m_vectorLength is always less than intmax (since the total allocation
        // size is always less than 4Gb). As such zero extending will have been correct (and extending the value
        // to 64-bits is necessary since it's used in the address calculation). We zero extend rather than sign
        // extending since it makes it easier to re-tag the value in the slow case.
        zeroExtend32ToPtr(regT1, regT1);
    }

    emitArrayProfilingSiteWithCell(regT0, regT2, profile);
    and32(TrustedImm32(IndexingShapeMask), regT2);

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
        break;
    }
    
    addSlowCase(badType);
    addSlowCase(slowCases);
    
    Label done = label();
    
    if (!ASSERT_DISABLED) {
        Jump resultOK = branchIfNotEmpty(regT0);
        abortWithReason(JITGetByValResultIsNotEmpty);
        resultOK.link(this);
    }

    emitValueProfilingSite(metadata);
    emitPutVirtualRegister(dst);

    Label nextHotPath = label();

    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeOffset, notIndex, badType, mode, profile, done, nextHotPath));
}

JITGetByIdGenerator JIT::emitGetByValWithCachedId(ByValInfo* byValInfo, OpGetByVal bytecode, const Identifier& propertyName, Jump& fastDoneCase, Jump& slowDoneCase, JumpList& slowCases)
{
    // base: regT0
    // property: regT1
    // scratch: regT3

    int dst = bytecode.dst.offset();

    slowCases.append(branchIfNotCell(regT1));
    emitByValIdentifierCheck(byValInfo, regT1, regT3, propertyName, slowCases);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        propertyName.impl(), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::Get);
    gen.generateFastPath(*this);

    fastDoneCase = jump();

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, dst, gen.stubInfo(), regT0, propertyName.impl());
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

    if (!isOperandConstantInt(property))
        linkSlowCase(iter); // property int32 check
    Jump nonCell = jump();
    linkSlowCase(iter); // base array check
    Jump notString = branchIfNotString(regT0);
    emitNakedCall(CodeLocationLabel<NoPtrTag>(m_vm->getCTIStub(stringGetByValGenerator).retaggedCode<NoPtrTag>()));
    Jump failed = branchTest64(Zero, regT0);
    emitPutVirtualRegister(dst, regT0);
    emitJumpSlowToHot(jump(), currentInstruction->size());
    failed.link(this);
    notString.link(this);
    nonCell.link(this);
    
    linkSlowCase(iter); // vector length check
    linkSlowCase(iter); // empty value
    
    Label slowPath = label();
    
    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    Call call = callOperation(operationGetByValOptimize, dst, regT0, regT1, byValInfo);

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

    emitGetVirtualRegister(base, regT0);
    bool propertyNameIsIntegerConstant = isOperandConstantInt(property);
    if (propertyNameIsIntegerConstant)
        move(Imm32(getOperandConstantInt(property)), regT1);
    else
        emitGetVirtualRegister(property, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, base);
    PatchableJump notIndex;
    if (!propertyNameIsIntegerConstant) {
        notIndex = emitPatchableJumpIfNotInt(regT1);
        addSlowCase(notIndex);
        // See comment in op_get_by_val.
        zeroExtend32ToPtr(regT1, regT1);
    }
    emitArrayProfilingSiteWithCell(regT0, regT2, profile);

    PatchableJump badType;
    JumpList slowCases;

    // FIXME: Maybe we should do this inline?
    addSlowCase(branchTest32(NonZero, regT2, TrustedImm32(CopyOnWrite)));
    and32(TrustedImm32(IndexingShapeMask), regT2);

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

template<typename Op>
JIT::JumpList JIT::emitGenericContiguousPutByVal(Op bytecode, PatchableJump& badType, IndexingType indexingShape)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    int value = bytecode.value.offset();
    ArrayProfile* profile = &metadata.arrayProfile;
    
    JumpList slowCases;

    badType = patchableBranch32(NotEqual, regT2, TrustedImm32(indexingShape));
    
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT2);
    Jump outOfBounds = branch32(AboveOrEqual, regT1, Address(regT2, Butterfly::offsetOfPublicLength()));

    Label storeResult = label();
    emitGetVirtualRegister(value, regT3);
    switch (indexingShape) {
    case Int32Shape:
        slowCases.append(branchIfNotInt32(regT3));
        store64(regT3, BaseIndex(regT2, regT1, TimesEight));
        break;
    case DoubleShape: {
        Jump notInt = branchIfNotInt32(regT3);
        convertInt32ToDouble(regT3, fpRegT0);
        Jump ready = jump();
        notInt.link(this);
        add64(tagTypeNumberRegister, regT3);
        move64ToDouble(regT3, fpRegT0);
        slowCases.append(branchIfNaN(fpRegT0));
        ready.link(this);
        storeDouble(fpRegT0, BaseIndex(regT2, regT1, TimesEight));
        break;
    }
    case ContiguousShape:
        store64(regT3, BaseIndex(regT2, regT1, TimesEight));
        emitWriteBarrier(bytecode.base.offset(), value, ShouldFilterValue);
        break;
    default:
        CRASH();
        break;
    }
    
    Jump done = jump();
    outOfBounds.link(this);
    
    slowCases.append(branch32(AboveOrEqual, regT1, Address(regT2, Butterfly::offsetOfVectorLength())));
    
    emitArrayProfileStoreToHoleSpecialCase(profile);
    
    add32(TrustedImm32(1), regT1, regT3);
    store32(regT3, Address(regT2, Butterfly::offsetOfPublicLength()));
    jump().linkTo(storeResult, this);
    
    done.link(this);
    
    return slowCases;
}

template<typename Op>
JIT::JumpList JIT::emitArrayStoragePutByVal(Op bytecode, PatchableJump& badType)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    int value = bytecode.value.offset();
    ArrayProfile* profile = &metadata.arrayProfile;
    
    JumpList slowCases;
    
    badType = patchableBranch32(NotEqual, regT2, TrustedImm32(ArrayStorageShape));
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT2);
    slowCases.append(branch32(AboveOrEqual, regT1, Address(regT2, ArrayStorage::vectorLengthOffset())));

    Jump empty = branchTest64(Zero, BaseIndex(regT2, regT1, TimesEight, ArrayStorage::vectorOffset()));

    Label storeResult(this);
    emitGetVirtualRegister(value, regT3);
    store64(regT3, BaseIndex(regT2, regT1, TimesEight, ArrayStorage::vectorOffset()));
    emitWriteBarrier(bytecode.base.offset(), value, ShouldFilterValue);
    Jump end = jump();
    
    empty.link(this);
    emitArrayProfileStoreToHoleSpecialCase(profile);
    add32(TrustedImm32(1), Address(regT2, ArrayStorage::numValuesInVectorOffset()));
    branch32(Below, regT1, Address(regT2, ArrayStorage::lengthOffset())).linkTo(storeResult, this);

    add32(TrustedImm32(1), regT1);
    store32(regT1, Address(regT2, ArrayStorage::lengthOffset()));
    sub32(TrustedImm32(1), regT1);
    jump().linkTo(storeResult, this);

    end.link(this);
    
    return slowCases;
}

template<typename Op>
JITPutByIdGenerator JIT::emitPutByValWithCachedId(ByValInfo* byValInfo, Op bytecode, PutKind putKind, const Identifier& propertyName, JumpList& doneCases, JumpList& slowCases)
{
    // base: regT0
    // property: regT1
    // scratch: regT2

    int base = bytecode.base.offset();
    int value = bytecode.value.offset();

    slowCases.append(branchIfNotCell(regT1));
    emitByValIdentifierCheck(byValInfo, regT1, regT1, propertyName, slowCases);

    // Write barrier breaks the registers. So after issuing the write barrier,
    // reload the registers.
    emitGetVirtualRegisters(base, regT0, value, regT1);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        JSValueRegs(regT0), JSValueRegs(regT1), regT2, m_codeBlock->ecmaMode(), putKind);
    gen.generateFastPath(*this);
    emitWriteBarrier(base, value, ShouldFilterBase);
    doneCases.append(jump());

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    Call call = callOperation(gen.slowPathFunction(), gen.stubInfo(), regT1, regT0, propertyName.impl());
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

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    emitGetVirtualRegister(value, regT2);
    Call call = callOperation(isDirect ? operationDirectPutByValOptimize : operationPutByValOptimize, regT0, regT1, regT2, byValInfo);

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_put_getter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    emitGetVirtualRegister(bytecode.base.offset(), regT0);
    int32_t options = bytecode.attributes;
    emitGetVirtualRegister(bytecode.accessor.offset(), regT1);
    callOperation(operationPutGetterById, regT0, m_codeBlock->identifier(bytecode.property).impl(), options, regT1);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    emitGetVirtualRegister(bytecode.base.offset(), regT0);
    int32_t options = bytecode.attributes;
    emitGetVirtualRegister(bytecode.accessor.offset(), regT1);
    callOperation(operationPutSetterById, regT0, m_codeBlock->identifier(bytecode.property).impl(), options, regT1);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    emitGetVirtualRegister(bytecode.base.offset(), regT0);
    int32_t attribute = bytecode.attributes;
    emitGetVirtualRegister(bytecode.getter.offset(), regT1);
    emitGetVirtualRegister(bytecode.setter.offset(), regT2);
    callOperation(operationPutGetterSetter, regT0, m_codeBlock->identifier(bytecode.property).impl(), attribute, regT1, regT2);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    emitGetVirtualRegister(bytecode.base.offset(), regT0);
    emitGetVirtualRegister(bytecode.property.offset(), regT1);
    int32_t attributes = bytecode.attributes;
    emitGetVirtualRegister(bytecode.accessor, regT2);
    callOperation(operationPutGetterByVal, regT0, regT1, attributes, regT2);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    emitGetVirtualRegister(bytecode.base.offset(), regT0);
    emitGetVirtualRegister(bytecode.property.offset(), regT1);
    int32_t attributes = bytecode.attributes;
    emitGetVirtualRegister(bytecode.accessor.offset(), regT2);
    callOperation(operationPutSetterByVal, regT0, regT1, attributes, regT2);
}

void JIT::emit_op_del_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property;
    emitGetVirtualRegister(base, regT0);
    callOperation(operationDeleteByIdJSResult, dst, regT0, m_codeBlock->identifier(property).impl());
}

void JIT::emit_op_del_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    int dst = bytecode.dst.offset();
    int base = bytecode.base.offset();
    int property = bytecode.property.offset();
    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    callOperation(operationDeleteByValJSResult, dst, regT0, regT1);
}

void JIT::emit_op_try_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    int resultVReg = bytecode.dst.offset();
    int baseVReg = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::TryGet);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);
    
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(resultVReg);
}

void JIT::emitSlow_op_try_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpTryGetById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationTryGetByIdOptimize, resultVReg, gen.stubInfo(), regT0, ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id_direct(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    int resultVReg = bytecode.dst.offset();
    int baseVReg = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::GetDirect);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(resultVReg);
}

void JIT::emitSlow_op_get_by_id_direct(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdDirectOptimize, resultVReg, gen.stubInfo(), regT0, ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int resultVReg = bytecode.dst.offset();
    int baseVReg = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitGetVirtualRegister(baseVReg, regT0);
    
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);
    
    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling()) {
        Jump notArrayLengthMode = branch8(NotEqual, AbsoluteAddress(&metadata.mode), TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
        emitArrayProfilingSiteWithCell(regT0, regT1, &metadata.modeMetadata.arrayLengthMode.arrayProfile);
        notArrayLengthMode.link(this);
    }

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::Get);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(resultVReg);
}

void JIT::emit_op_get_by_id_with_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    int resultVReg = bytecode.dst.offset();
    int baseVReg = bytecode.base.offset();
    int thisVReg = bytecode.thisValue.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitGetVirtualRegister(baseVReg, regT0);
    emitGetVirtualRegister(thisVReg, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);
    emitJumpSlowCaseIfNotJSCell(regT1, thisVReg);

    JITGetByIdWithThisGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT0), JSValueRegs(regT0), JSValueRegs(regT1), AccessType::GetWithThis);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIdsWithThis.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(resultVReg);
}

void JIT::emitSlow_op_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, resultVReg, gen.stubInfo(), regT0, ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emitSlow_op_get_by_id_with_this(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdWithThisOptimize, resultVReg, gen.stubInfo(), regT0, regT1, ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_put_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    int baseVReg = bytecode.base.offset();
    int valueVReg = bytecode.value.offset();
    unsigned direct = metadata.flags & PutByIdIsDirect;

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        JSValueRegs(regT0), JSValueRegs(regT1), regT2, m_codeBlock->ecmaMode(),
        direct ? Direct : NotDirect);
    
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    
    emitWriteBarrier(baseVReg, valueVReg, ShouldFilterBase);

    m_putByIds.append(gen);
}

void JIT::emitSlow_op_put_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutById>();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    Label coldPathBegin(this);
    
    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];

    Call call = callOperation(gen.slowPathFunction(), gen.stubInfo(), regT1, regT0, ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_in_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    int resultVReg = bytecode.dst.offset();
    int baseVReg = bytecode.base.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITInByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(m_bytecodeOffset), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs(regT0), JSValueRegs(regT0));
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_inByIds.append(gen);

    emitPutVirtualRegister(resultVReg);
}

void JIT::emitSlow_op_in_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInById>();
    int resultVReg = bytecode.dst.offset();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.property));

    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationInByIdOptimize, resultVReg, gen.stubInfo(), regT0, ident->impl());

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
    emitGetVirtualRegister(scope, regT0);
    for (unsigned i = 0; i < depth; ++i)
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitPutVirtualRegister(dst);
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
            move(TrustedImmPtr(constantScope), regT0);
            emitPutVirtualRegister(dst);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(dst, scope, needsVarInjectionChecks(resolveType), depth);
            break;
        case ModuleVar:
            move(TrustedImmPtr(metadata.lexicalEnvironment.get()), regT0);
            emitPutVirtualRegister(dst);
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
        loadPtr(constantScopeSlot, regT0);
        emitPutVirtualRegister(dst);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.resolveType, regT0);

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
    loadPtr(structureSlot, regT1);
    emitGetVirtualRegister(scope, regT0);
    addSlowCase(branchTestPtr(Zero, regT1));
    load32(Address(regT1, Structure::structureIDOffset()), regT1);
    addSlowCase(branch32(NotEqual, Address(regT0, JSCell::structureIDOffset()), regT1));
}

void JIT::emitGetVarFromPointer(JSValue* operand, GPRReg reg)
{
    loadPtr(operand, reg);
}

void JIT::emitGetVarFromIndirectPointer(JSValue** operand, GPRReg reg)
{
    loadPtr(operand, reg);
    loadPtr(reg, reg);
}

void JIT::emitGetClosureVar(int scope, uintptr_t operand)
{
    emitGetVirtualRegister(scope, regT0);
    loadPtr(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register)), regT0);
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
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            GPRReg base = regT0;
            GPRReg result = regT0;
            GPRReg offset = regT1;
            GPRReg scratch = regT2;

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                return branchPtr(Equal, base, TrustedImmPtr(m_codeBlock->globalObject()));
            }));

            load32(operandSlot, offset);
            if (!ASSERT_DISABLED) {
                Jump isOutOfLine = branch32(GreaterThanOrEqual, offset, TrustedImm32(firstOutOfLineOffset));
                abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(this);
            }
            loadPtr(Address(base, JSObject::butterflyOffset()), scratch);
            neg32(offset);
            signExtend32ToPtr(offset, offset);
            load64(BaseIndex(scratch, offset, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), result);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            if (indirectLoadForOperand)
                emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT0);
            else
                emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                addSlowCase(branchIfEmpty(regT0));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitGetClosureVar(scope, *operandSlot);
            break;
        case Dynamic:
            addSlowCase(jump());
            break;
        case LocalClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.getPutInfo, regT0);
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
        load32(&metadata.getPutInfo, regT0);
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
    emitPutVirtualRegister(dst);
    emitValueProfilingSite(metadata);
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
    emitGetVirtualRegister(value, regT0);
    emitNotifyWrite(set);
    storePtr(regT0, operand);
}
void JIT::emitPutGlobalVariableIndirect(JSValue** addressOfOperand, int value, WatchpointSet** indirectWatchpointSet)
{
    emitGetVirtualRegister(value, regT0);
    loadPtr(indirectWatchpointSet, regT1);
    emitNotifyWrite(regT1);
    loadPtr(addressOfOperand, regT1);
    storePtr(regT0, regT1);
}

void JIT::emitPutClosureVar(int scope, uintptr_t operand, int value, WatchpointSet* set)
{
    emitGetVirtualRegister(value, regT1);
    emitGetVirtualRegister(scope, regT0);
    emitNotifyWrite(set);
    storePtr(regT1, Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register)));
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
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            emitGetVirtualRegister(value, regT2);

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                return branchPtr(Equal, regT0, TrustedImmPtr(m_codeBlock->globalObject()));
            }));

            loadPtr(Address(regT0, JSObject::butterflyOffset()), regT0);
            loadPtr(operandSlot, regT1);
            negPtr(regT1);
            storePtr(regT2, BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)));
            emitWriteBarrier(m_codeBlock->globalObject(), value, ShouldFilterValue);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_codeBlock);
            RELEASE_ASSERT(constantScope);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            if (!isInitialization(getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                if (indirectLoadForOperand)
                    emitGetVarFromIndirectPointer(bitwise_cast<JSValue**>(operandSlot), regT0);
                else
                    emitGetVarFromPointer(bitwise_cast<JSValue*>(*operandSlot), regT0);
                addSlowCase(branchIfEmpty(regT0));
            }
            if (indirectLoadForOperand)
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, &metadata.watchpointSet);
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, metadata.watchpointSet);
            emitWriteBarrier(constantScope, value, ShouldFilterValue);
            break;
        }
        case LocalClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitPutClosureVar(scope, *operandSlot, value, metadata.watchpointSet);
            emitWriteBarrier(scope, value, ShouldFilterValue);
            break;
        case ModuleVar:
        case Dynamic:
            addSlowCase(jump());
            break;
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    };

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.getPutInfo, regT0);
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
        load32(&metadata.getPutInfo, regT0);
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
    
    emitGetVirtualRegister(arguments, regT0);
    load64(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)), regT0);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_put_to_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    int arguments = bytecode.arguments.offset();
    int index = bytecode.index;
    int value = bytecode.value.offset();
    
    emitGetVirtualRegister(arguments, regT0);
    emitGetVirtualRegister(value, regT1);
    store64(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)));

    emitWriteBarrier(arguments, value, ShouldFilterValue);
}

void JIT::emitWriteBarrier(unsigned owner, unsigned value, WriteBarrierMode mode)
{
    Jump valueNotCell;
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) {
        emitGetVirtualRegister(value, regT0);
        valueNotCell = branchIfNotCell(regT0);
    }
    
    emitGetVirtualRegister(owner, regT0);
    Jump ownerNotCell;
    if (mode == ShouldFilterBaseAndValue || mode == ShouldFilterBase)
        ownerNotCell = branchIfNotCell(regT0);

    Jump ownerIsRememberedOrInEden = barrierBranch(*vm(), regT0, regT1);
    callOperation(operationWriteBarrierSlowPath, regT0);
    ownerIsRememberedOrInEden.link(this);

    if (mode == ShouldFilterBaseAndValue || mode == ShouldFilterBase)
        ownerNotCell.link(this);
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) 
        valueNotCell.link(this);
}

void JIT::emitWriteBarrier(JSCell* owner, unsigned value, WriteBarrierMode mode)
{
    emitGetVirtualRegister(value, regT0);
    Jump valueNotCell;
    if (mode == ShouldFilterValue)
        valueNotCell = branchIfNotCell(regT0);

    emitWriteBarrier(owner);

    if (mode == ShouldFilterValue) 
        valueNotCell.link(this);
}

#else // USE(JSVALUE64)

void JIT::emitWriteBarrier(unsigned owner, unsigned value, WriteBarrierMode mode)
{
    Jump valueNotCell;
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) {
        emitLoadTag(value, regT0);
        valueNotCell = branchIfNotCell(regT0);
    }
    
    emitLoad(owner, regT0, regT1);
    Jump ownerNotCell;
    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell = branchIfNotCell(regT0);

    Jump ownerIsRememberedOrInEden = barrierBranch(*vm(), regT1, regT2);
    callOperation(operationWriteBarrierSlowPath, regT1);
    ownerIsRememberedOrInEden.link(this);

    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell.link(this);
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) 
        valueNotCell.link(this);
}

void JIT::emitWriteBarrier(JSCell* owner, unsigned value, WriteBarrierMode mode)
{
    Jump valueNotCell;
    if (mode == ShouldFilterValue) {
        emitLoadTag(value, regT0);
        valueNotCell = branchIfNotCell(regT0);
    }

    emitWriteBarrier(owner);

    if (mode == ShouldFilterValue) 
        valueNotCell.link(this);
}

#endif // USE(JSVALUE64)

void JIT::emitWriteBarrier(JSCell* owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(*vm(), owner, regT0);
    callOperation(operationWriteBarrierSlowPath, owner);
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitByValIdentifierCheck(ByValInfo* byValInfo, RegisterID cell, RegisterID scratch, const Identifier& propertyName, JumpList& slowCases)
{
    if (propertyName.isSymbol())
        slowCases.append(branchPtr(NotEqual, cell, TrustedImmPtr(byValInfo->cachedSymbol.get())));
    else {
        slowCases.append(branchIfNotString(cell));
        loadPtr(Address(cell, JSString::offsetOfValue()), scratch);
        slowCases.append(branchPtr(NotEqual, scratch, TrustedImmPtr(propertyName.impl())));
    }
}

void JIT::privateCompileGetByVal(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
{
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    
    PatchableJump badType;
    JumpList slowCases;
    
    switch (arrayMode) {
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
    case JITDirectArguments:
        slowCases = emitDirectArgumentsGetByVal(currentInstruction, badType);
        break;
    case JITScopedArguments:
        slowCases = emitScopedArgumentsGetByVal(currentInstruction, badType);
        break;
    default:
        TypedArrayType type = typedArrayTypeForJITArrayMode(arrayMode);
        if (isInt(type))
            slowCases = emitIntTypedArrayGetByVal(currentInstruction, badType, type);
        else 
            slowCases = emitFloatTypedArrayGetByVal(currentInstruction, badType, type);
        break;
    }
    
    Jump done = jump();

    LinkBuffer patchBuffer(*this, m_codeBlock);

    patchBuffer.link(badType, byValInfo->slowPathTarget);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);

    patchBuffer.link(done, byValInfo->badTypeDoneTarget);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline get_by_val stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value());
    
    MacroAssembler::repatchJump(byValInfo->badTypeJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(operationGetByValGeneric));
}

void JIT::privateCompileGetByValWithCachedId(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, const Identifier& propertyName)
{
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    auto bytecode = currentInstruction->as<OpGetByVal>();

    Jump fastDoneCase;
    Jump slowDoneCase;
    JumpList slowCases;

    JITGetByIdGenerator gen = emitGetByValWithCachedId(byValInfo, bytecode, propertyName, fastDoneCase, slowDoneCase, slowCases);

    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    LinkBuffer patchBuffer(*this, m_codeBlock);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);
    patchBuffer.link(fastDoneCase, byValInfo->badTypeDoneTarget);
    patchBuffer.link(slowDoneCase, byValInfo->badTypeNextHotPathTarget);
    if (!m_exceptionChecks.empty())
        patchBuffer.link(m_exceptionChecks, byValInfo->exceptionHandler);

    for (const auto& callSite : m_calls) {
        if (callSite.callee)
            patchBuffer.link(callSite.from, callSite.callee);
    }
    gen.finalize(patchBuffer, patchBuffer);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline get_by_val with cached property name '%s' stub for %s, return point %p", propertyName.impl()->utf8().data(), toCString(*m_codeBlock).data(), returnAddress.value());
    byValInfo->stubInfo = gen.stubInfo();

    MacroAssembler::repatchJump(byValInfo->notIndexJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(operationGetByValGeneric));
}

template<typename Op>
void JIT::privateCompilePutByVal(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
{
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    auto bytecode = currentInstruction->as<Op>();
    
    PatchableJump badType;
    JumpList slowCases;

    bool needsLinkForWriteBarrier = false;

    switch (arrayMode) {
    case JITInt32:
        slowCases = emitInt32PutByVal(bytecode, badType);
        break;
    case JITDouble:
        slowCases = emitDoublePutByVal(bytecode, badType);
        break;
    case JITContiguous:
        slowCases = emitContiguousPutByVal(bytecode, badType);
        needsLinkForWriteBarrier = true;
        break;
    case JITArrayStorage:
        slowCases = emitArrayStoragePutByVal(bytecode, badType);
        needsLinkForWriteBarrier = true;
        break;
    default:
        TypedArrayType type = typedArrayTypeForJITArrayMode(arrayMode);
        if (isInt(type))
            slowCases = emitIntTypedArrayPutByVal(bytecode, badType, type);
        else 
            slowCases = emitFloatTypedArrayPutByVal(bytecode, badType, type);
        break;
    }
    
    Jump done = jump();

    LinkBuffer patchBuffer(*this, m_codeBlock);
    patchBuffer.link(badType, byValInfo->slowPathTarget);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);
    patchBuffer.link(done, byValInfo->badTypeDoneTarget);
    if (needsLinkForWriteBarrier) {
        ASSERT(removeCodePtrTag(m_calls.last().callee.executableAddress()) == removeCodePtrTag(operationWriteBarrierSlowPath));
        patchBuffer.link(m_calls.last().from, m_calls.last().callee);
    }
    
    bool isDirect = currentInstruction->opcodeID() == op_put_by_val_direct;
    if (!isDirect) {
        byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
            m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
            "Baseline put_by_val stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value());
        
    } else {
        byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
            m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
            "Baseline put_by_val_direct stub for %s, return point %p", toCString(*m_codeBlock).data(), returnAddress.value());
    }
    MacroAssembler::repatchJump(byValInfo->badTypeJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(isDirect ? operationDirectPutByValGeneric : operationPutByValGeneric));
}

template<typename Op>
void JIT::privateCompilePutByValWithCachedId(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, PutKind putKind, const Identifier& propertyName)
{
    ASSERT((putKind == Direct && Op::opcodeID == op_put_by_val_direct) || (putKind == NotDirect && Op::opcodeID == op_put_by_val));
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    auto bytecode = currentInstruction->as<Op>();

    JumpList doneCases;
    JumpList slowCases;

    JITPutByIdGenerator gen = emitPutByValWithCachedId(byValInfo, bytecode, putKind, propertyName, doneCases, slowCases);

    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    LinkBuffer patchBuffer(*this, m_codeBlock);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);
    patchBuffer.link(doneCases, byValInfo->badTypeDoneTarget);
    if (!m_exceptionChecks.empty())
        patchBuffer.link(m_exceptionChecks, byValInfo->exceptionHandler);

    for (const auto& callSite : m_calls) {
        if (callSite.callee)
            patchBuffer.link(callSite.from, callSite.callee);
    }
    gen.finalize(patchBuffer, patchBuffer);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline put_by_val%s with cached property name '%s' stub for %s, return point %p", (putKind == Direct) ? "_direct" : "", propertyName.impl()->utf8().data(), toCString(*m_codeBlock).data(), returnAddress.value());
    byValInfo->stubInfo = gen.stubInfo();

    MacroAssembler::repatchJump(byValInfo->notIndexJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(putKind == Direct ? operationDirectPutByValGeneric : operationPutByValGeneric));
}

JIT::JumpList JIT::emitDoubleLoad(const Instruction*, PatchableJump& badType)
{
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    RegisterID indexing = regT2;
    RegisterID scratch = regT3;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    RegisterID indexing = regT1;
    RegisterID scratch = regT3;
#endif

    JumpList slowCases;

    badType = patchableBranch32(NotEqual, indexing, TrustedImm32(DoubleShape));
    loadPtr(Address(base, JSObject::butterflyOffset()), scratch);
    slowCases.append(branch32(AboveOrEqual, property, Address(scratch, Butterfly::offsetOfPublicLength())));
    loadDouble(BaseIndex(scratch, property, TimesEight), fpRegT0);
    slowCases.append(branchIfNaN(fpRegT0));

    return slowCases;
}

JIT::JumpList JIT::emitContiguousLoad(const Instruction*, PatchableJump& badType, IndexingType expectedShape)
{
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    RegisterID indexing = regT2;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    RegisterID indexing = regT1;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
#endif

    JumpList slowCases;

    badType = patchableBranch32(NotEqual, indexing, TrustedImm32(expectedShape));
    loadPtr(Address(base, JSObject::butterflyOffset()), scratch);
    slowCases.append(branch32(AboveOrEqual, property, Address(scratch, Butterfly::offsetOfPublicLength())));
    loadValue(BaseIndex(scratch, property, TimesEight), result);
    slowCases.append(branchIfEmpty(result));

    return slowCases;
}

JIT::JumpList JIT::emitArrayStorageLoad(const Instruction*, PatchableJump& badType)
{
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    RegisterID indexing = regT2;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    RegisterID indexing = regT1;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
#endif

    JumpList slowCases;

    add32(TrustedImm32(-ArrayStorageShape), indexing, scratch);
    badType = patchableBranch32(Above, scratch, TrustedImm32(SlowPutArrayStorageShape - ArrayStorageShape));

    loadPtr(Address(base, JSObject::butterflyOffset()), scratch);
    slowCases.append(branch32(AboveOrEqual, property, Address(scratch, ArrayStorage::vectorLengthOffset())));

    loadValue(BaseIndex(scratch, property, TimesEight, ArrayStorage::vectorOffset()), result);
    slowCases.append(branchIfEmpty(result));

    return slowCases;
}

JIT::JumpList JIT::emitDirectArgumentsGetByVal(const Instruction*, PatchableJump& badType)
{
    JumpList slowCases;
    
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#endif

    load8(Address(base, JSCell::typeInfoTypeOffset()), scratch);
    badType = patchableBranch32(NotEqual, scratch, TrustedImm32(DirectArgumentsType));
    
    load32(Address(base, DirectArguments::offsetOfLength()), scratch2);
    slowCases.append(branch32(AboveOrEqual, property, scratch2));
    slowCases.append(branchTestPtr(NonZero, Address(base, DirectArguments::offsetOfMappedArguments())));
    
    loadValue(BaseIndex(base, property, TimesEight, DirectArguments::storageOffset()), result);
    
    return slowCases;
}

JIT::JumpList JIT::emitScopedArgumentsGetByVal(const Instruction*, PatchableJump& badType)
{
    JumpList slowCases;
    
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
    RegisterID scratch3 = regT5;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
    RegisterID scratch3 = regT5;
#endif

    load8(Address(base, JSCell::typeInfoTypeOffset()), scratch);
    badType = patchableBranch32(NotEqual, scratch, TrustedImm32(ScopedArgumentsType));
    loadPtr(Address(base, ScopedArguments::offsetOfStorage()), scratch3);
    xorPtr(TrustedImmPtr(ScopedArgumentsPoison::key()), scratch3);
    slowCases.append(branch32(AboveOrEqual, property, Address(scratch3, ScopedArguments::offsetOfTotalLengthInStorage())));
    
    loadPtr(Address(base, ScopedArguments::offsetOfTable()), scratch);
    xorPtr(TrustedImmPtr(ScopedArgumentsPoison::key()), scratch);
    load32(Address(scratch, ScopedArgumentsTable::offsetOfLength()), scratch2);
    Jump overflowCase = branch32(AboveOrEqual, property, scratch2);
    loadPtr(Address(base, ScopedArguments::offsetOfScope()), scratch2);
    xorPtr(TrustedImmPtr(ScopedArgumentsPoison::key()), scratch2);
    loadPtr(Address(scratch, ScopedArgumentsTable::offsetOfArguments()), scratch);
    load32(BaseIndex(scratch, property, TimesFour), scratch);
    slowCases.append(branch32(Equal, scratch, TrustedImm32(ScopeOffset::invalidOffset)));
    loadValue(BaseIndex(scratch2, scratch, TimesEight, JSLexicalEnvironment::offsetOfVariables()), result);
    Jump done = jump();
    overflowCase.link(this);
    sub32(property, scratch2);
    neg32(scratch2);
    loadValue(BaseIndex(scratch3, scratch2, TimesEight), result);
    slowCases.append(branchIfEmpty(result));
    done.link(this);
    
    load32(Address(scratch3, ScopedArguments::offsetOfTotalLengthInStorage()), scratch);
    emitPreparePreciseIndexMask32(property, scratch, scratch2);
    andPtr(scratch2, result.payloadGPR());
    
    return slowCases;
}

JIT::JumpList JIT::emitIntTypedArrayGetByVal(const Instruction*, PatchableJump& badType, TypedArrayType type)
{
    ASSERT(isInt(type));
    
    // The best way to test the array type is to use the classInfo. We need to do so without
    // clobbering the register that holds the indexing type, base, and property.

#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#endif
    RegisterID resultPayload = result.payloadGPR();
    
    JumpList slowCases;
    
    load8(Address(base, JSCell::typeInfoTypeOffset()), scratch);
    badType = patchableBranch32(NotEqual, scratch, TrustedImm32(typeForTypedArrayType(type)));
    slowCases.append(branch32(AboveOrEqual, property, Address(base, JSArrayBufferView::offsetOfLength())));
    loadPtr(Address(base, JSArrayBufferView::offsetOfVector()), scratch);
    cageConditionally(Gigacage::Primitive, scratch, scratch2);

    switch (elementSize(type)) {
    case 1:
        if (JSC::isSigned(type))
            load8SignedExtendTo32(BaseIndex(scratch, property, TimesOne), resultPayload);
        else
            load8(BaseIndex(scratch, property, TimesOne), resultPayload);
        break;
    case 2:
        if (JSC::isSigned(type))
            load16SignedExtendTo32(BaseIndex(scratch, property, TimesTwo), resultPayload);
        else
            load16(BaseIndex(scratch, property, TimesTwo), resultPayload);
        break;
    case 4:
        load32(BaseIndex(scratch, property, TimesFour), resultPayload);
        break;
    default:
        CRASH();
    }
    
    Jump done;
    if (type == TypeUint32) {
        Jump canBeInt = branch32(GreaterThanOrEqual, resultPayload, TrustedImm32(0));
        
        convertInt32ToDouble(resultPayload, fpRegT0);
        addDouble(AbsoluteAddress(&twoToThe32), fpRegT0);
        boxDouble(fpRegT0, result);
        done = jump();
        canBeInt.link(this);
    }

    boxInt32(resultPayload, result);
    if (done.isSet())
        done.link(this);
    return slowCases;
}

JIT::JumpList JIT::emitFloatTypedArrayGetByVal(const Instruction*, PatchableJump& badType, TypedArrayType type)
{
    ASSERT(isFloat(type));
    
#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    JSValueRegs result = JSValueRegs(regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    JSValueRegs result = JSValueRegs(regT1, regT0);
    RegisterID scratch = regT3;
    RegisterID scratch2 = regT4;
#endif
    
    JumpList slowCases;

    load8(Address(base, JSCell::typeInfoTypeOffset()), scratch);
    badType = patchableBranch32(NotEqual, scratch, TrustedImm32(typeForTypedArrayType(type)));
    slowCases.append(branch32(AboveOrEqual, property, Address(base, JSArrayBufferView::offsetOfLength())));
    loadPtr(Address(base, JSArrayBufferView::offsetOfVector()), scratch);
    cageConditionally(Gigacage::Primitive, scratch, scratch2);
    
    switch (elementSize(type)) {
    case 4:
        loadFloat(BaseIndex(scratch, property, TimesFour), fpRegT0);
        convertFloatToDouble(fpRegT0, fpRegT0);
        break;
    case 8: {
        loadDouble(BaseIndex(scratch, property, TimesEight), fpRegT0);
        break;
    }
    default:
        CRASH();
    }
    
    purifyNaN(fpRegT0);
    
    boxDouble(fpRegT0, result);
    return slowCases;    
}

template<typename Op>
JIT::JumpList JIT::emitIntTypedArrayPutByVal(Op bytecode, PatchableJump& badType, TypedArrayType type)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    ArrayProfile* profile = &metadata.arrayProfile;
    ASSERT(isInt(type));
    
    int value = bytecode.value.offset();

#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    RegisterID earlyScratch = regT3;
    RegisterID lateScratch = regT2;
    RegisterID lateScratch2 = regT4;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    RegisterID earlyScratch = regT3;
    RegisterID lateScratch = regT1;
    RegisterID lateScratch2 = regT4;
#endif
    
    JumpList slowCases;
    
    load8(Address(base, JSCell::typeInfoTypeOffset()), earlyScratch);
    badType = patchableBranch32(NotEqual, earlyScratch, TrustedImm32(typeForTypedArrayType(type)));
    Jump inBounds = branch32(Below, property, Address(base, JSArrayBufferView::offsetOfLength()));
    emitArrayProfileOutOfBoundsSpecialCase(profile);
    slowCases.append(jump());
    inBounds.link(this);
    
#if USE(JSVALUE64)
    emitGetVirtualRegister(value, earlyScratch);
    slowCases.append(branchIfNotInt32(earlyScratch));
#else
    emitLoad(value, lateScratch, earlyScratch);
    slowCases.append(branchIfNotInt32(lateScratch));
#endif
    
    // We would be loading this into base as in get_by_val, except that the slow
    // path expects the base to be unclobbered.
    loadPtr(Address(base, JSArrayBufferView::offsetOfVector()), lateScratch);
    cageConditionally(Gigacage::Primitive, lateScratch, lateScratch2);
    
    if (isClamped(type)) {
        ASSERT(elementSize(type) == 1);
        ASSERT(!JSC::isSigned(type));
        Jump inBounds = branch32(BelowOrEqual, earlyScratch, TrustedImm32(0xff));
        Jump tooBig = branch32(GreaterThan, earlyScratch, TrustedImm32(0xff));
        xor32(earlyScratch, earlyScratch);
        Jump clamped = jump();
        tooBig.link(this);
        move(TrustedImm32(0xff), earlyScratch);
        clamped.link(this);
        inBounds.link(this);
    }
    
    switch (elementSize(type)) {
    case 1:
        store8(earlyScratch, BaseIndex(lateScratch, property, TimesOne));
        break;
    case 2:
        store16(earlyScratch, BaseIndex(lateScratch, property, TimesTwo));
        break;
    case 4:
        store32(earlyScratch, BaseIndex(lateScratch, property, TimesFour));
        break;
    default:
        CRASH();
    }
    
    return slowCases;
}

template<typename Op>
JIT::JumpList JIT::emitFloatTypedArrayPutByVal(Op bytecode, PatchableJump& badType, TypedArrayType type)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    ArrayProfile* profile = &metadata.arrayProfile;
    ASSERT(isFloat(type));
    
    int value = bytecode.value.offset();

#if USE(JSVALUE64)
    RegisterID base = regT0;
    RegisterID property = regT1;
    RegisterID earlyScratch = regT3;
    RegisterID lateScratch = regT2;
    RegisterID lateScratch2 = regT4;
#else
    RegisterID base = regT0;
    RegisterID property = regT2;
    RegisterID earlyScratch = regT3;
    RegisterID lateScratch = regT1;
    RegisterID lateScratch2 = regT4;
#endif
    
    JumpList slowCases;
    
    load8(Address(base, JSCell::typeInfoTypeOffset()), earlyScratch);
    badType = patchableBranch32(NotEqual, earlyScratch, TrustedImm32(typeForTypedArrayType(type)));
    Jump inBounds = branch32(Below, property, Address(base, JSArrayBufferView::offsetOfLength()));
    emitArrayProfileOutOfBoundsSpecialCase(profile);
    slowCases.append(jump());
    inBounds.link(this);
    
#if USE(JSVALUE64)
    emitGetVirtualRegister(value, earlyScratch);
    Jump doubleCase = branchIfNotInt32(earlyScratch);
    convertInt32ToDouble(earlyScratch, fpRegT0);
    Jump ready = jump();
    doubleCase.link(this);
    slowCases.append(branchIfNotNumber(earlyScratch));
    add64(tagTypeNumberRegister, earlyScratch);
    move64ToDouble(earlyScratch, fpRegT0);
    ready.link(this);
#else
    emitLoad(value, lateScratch, earlyScratch);
    Jump doubleCase = branchIfNotInt32(lateScratch);
    convertInt32ToDouble(earlyScratch, fpRegT0);
    Jump ready = jump();
    doubleCase.link(this);
    slowCases.append(branch32(Above, lateScratch, TrustedImm32(JSValue::LowestTag)));
    moveIntsToDouble(earlyScratch, lateScratch, fpRegT0, fpRegT1);
    ready.link(this);
#endif
    
    // We would be loading this into base as in get_by_val, except that the slow
    // path expects the base to be unclobbered.
    loadPtr(Address(base, JSArrayBufferView::offsetOfVector()), lateScratch);
    cageConditionally(Gigacage::Primitive, lateScratch, lateScratch2);
    
    switch (elementSize(type)) {
    case 4:
        convertDoubleToFloat(fpRegT0, fpRegT0);
        storeFloat(fpRegT0, BaseIndex(lateScratch, property, TimesFour));
        break;
    case 8:
        storeDouble(fpRegT0, BaseIndex(lateScratch, property, TimesEight));
        break;
    default:
        CRASH();
    }
    
    return slowCases;
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const Instruction*);

} // namespace JSC

#endif // ENABLE(JIT)
