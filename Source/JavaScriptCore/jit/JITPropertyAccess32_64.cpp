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
    
void JIT::emit_op_put_getter_by_id(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int options = currentInstruction[3].u.operand;
    int getter = currentInstruction[4].u.operand;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterById, regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_setter_by_id(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int options = currentInstruction[3].u.operand;
    int setter = currentInstruction[4].u.operand;

    emitLoadPayload(base, regT1);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterById, regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_getter_setter_by_id(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int attribute = currentInstruction[3].u.operand;
    int getter = currentInstruction[4].u.operand;
    int setter = currentInstruction[5].u.operand;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    emitLoadPayload(setter, regT4);
    callOperation(operationPutGetterSetter, regT1, m_codeBlock->identifier(property).impl(), attribute, regT3, regT4);
}

void JIT::emit_op_put_getter_by_val(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int32_t attributes = currentInstruction[3].u.operand;
    int getter = currentInstruction[4].u.operand;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterByVal, regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_put_setter_by_val(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int32_t attributes = currentInstruction[3].u.operand;
    int getter = currentInstruction[4].u.operand;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutSetterByVal, regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_del_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    emitLoad(base, regT1, regT0);
    callOperation(operationDeleteByIdJSResult, dst, JSValueRegs(regT1, regT0), m_codeBlock->identifier(property).impl());
}

void JIT::emit_op_del_by_val(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    emitLoad2(base, regT1, regT0, property, regT3, regT2);
    callOperation(operationDeleteByValJSResult, dst, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
}

void JIT::emit_op_get_by_val(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    ArrayProfile* profile = arrayProfileFor<OpGetByValShape>(currentInstruction);
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

    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);

    Label nextHotPath = label();
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeOffset, notIndex, badType, mode, profile, done, nextHotPath));
}

JITGetByIdGenerator JIT::emitGetByValWithCachedId(ByValInfo* byValInfo, Instruction* currentInstruction, const Identifier& propertyName, Jump& fastDoneCase, Jump& slowDoneCase, JumpList& slowCases)
{
    int dst = currentInstruction[1].u.operand;

    // base: tag(regT1), payload(regT0)
    // property: tag(regT3), payload(regT2)
    // scratch: regT4

    slowCases.append(branchIfNotCell(regT3));
    emitByValIdentifierCheck(byValInfo, regT2, regT4, propertyName, slowCases);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        propertyName.impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::Get);
    gen.generateFastPath(*this);

    fastDoneCase = jump();

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    Call call = callOperationWithProfile(operationGetByIdOptimize, dst, gen.stubInfo(), JSValueRegs(regT1, regT0), propertyName.impl());
    gen.reportSlowPathCall(coldPathBegin, call);
    slowDoneCase = jump();

    return gen;
}

void JIT::emitSlow_op_get_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int property = currentInstruction[3].u.operand;
    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;

    linkSlowCaseIfNotJSCell(iter, base); // base cell check
    linkSlowCase(iter); // property int32 check

    Jump nonCell = jump();
    linkSlowCase(iter); // base array check
    Jump notString = branchIfNotString(regT0);
    emitNakedCall(CodeLocationLabel<NoPtrTag>(m_vm->getCTIStub(stringGetByValGenerator).retaggedCode<NoPtrTag>()));
    Jump failed = branchTestPtr(Zero, regT0);
    emitStoreCell(dst, regT0);
    emitJumpSlowToHot(jump(), OPCODE_LENGTH(op_get_by_val));
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

    emitValueProfilingSite();
}

void JIT::emit_op_put_by_val(Instruction* currentInstruction)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    ArrayProfile* profile = arrayProfileFor<OpPutByValShape>(currentInstruction);
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
        slowCases = emitInt32PutByVal(currentInstruction, badType);
        break;
    case JITDouble:
        slowCases = emitDoublePutByVal(currentInstruction, badType);
        break;
    case JITContiguous:
        slowCases = emitContiguousPutByVal(currentInstruction, badType);
        break;
    case JITArrayStorage:
        slowCases = emitArrayStoragePutByVal(currentInstruction, badType);
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

JIT::JumpList JIT::emitGenericContiguousPutByVal(Instruction* currentInstruction, PatchableJump& badType, IndexingType indexingShape)
{
    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;
    ArrayProfile* profile = arrayProfileFor<OpPutByValShape>(currentInstruction);

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

JIT::JumpList JIT::emitArrayStoragePutByVal(Instruction* currentInstruction, PatchableJump& badType)
{
    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;
    ArrayProfile* profile = arrayProfileFor<OpPutByValShape>(currentInstruction);

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

JITPutByIdGenerator JIT::emitPutByValWithCachedId(ByValInfo* byValInfo, Instruction* currentInstruction, PutKind putKind, const Identifier& propertyName, JumpList& doneCases, JumpList& slowCases)
{
    // base: tag(regT1), payload(regT0)
    // property: tag(regT3), payload(regT2)

    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;

    slowCases.append(branchIfNotCell(regT3));
    emitByValIdentifierCheck(byValInfo, regT2, regT2, propertyName, slowCases);

    // Write barrier breaks the registers. So after issuing the write barrier,
    // reload the registers.
    emitWriteBarrier(base, value, ShouldFilterBase);
    emitLoadPayload(base, regT0);
    emitLoad(value, regT3, regT2);

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

void JIT::emitSlow_op_put_by_val(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    int base = currentInstruction[1].u.operand;
    int property = currentInstruction[2].u.operand;
    int value = currentInstruction[3].u.operand;
    ByValInfo* byValInfo = m_byValCompilationInfo[m_byValInstructionIndex].byValInfo;
    
    linkAllSlowCases(iter);
    Label slowPath = label();
    
    bool isDirect = Interpreter::getOpcodeID(currentInstruction->u.opcode) == op_put_by_val_direct;

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

void JIT::emit_op_try_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::TryGet);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);
    
    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_try_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int resultVReg = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationTryGetByIdOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id_direct(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::GetDirect);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id_direct(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int resultVReg = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperationWithProfile(operationGetByIdDirectOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());

    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling())
        emitArrayProfilingSiteForBytecodeIndexWithCell(regT0, regT2, m_bytecodeOffset);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeOffset), CallSiteIndex(currentInstruction), RegisterSet::stubUnavailableRegisters(),
        ident->impl(), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::Get);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int resultVReg = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(operationGetByIdOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id_with_this(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    int thisVReg = currentInstruction[3].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[4].u.operand));
    
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

    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_by_id_with_this(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int resultVReg = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[4].u.operand));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(operationGetByIdWithThisOptimize, resultVReg, gen.stubInfo(), JSValueRegs(regT1, regT0), JSValueRegs(regT4, regT3), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_put_by_id(Instruction* currentInstruction)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.
    
    int base = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;
    int direct = currentInstruction[8].u.putByIdFlags & PutByIdIsDirect;
    
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

void JIT::emitSlow_op_put_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int base = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[2].u.operand));

    Label coldPathBegin(this);

    // JITPutByIdGenerator only preserve the value and the base's payload, we have to reload the tag.
    emitLoadTag(base, regT1);

    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];
    
    Call call = callOperation(
        gen.slowPathFunction(), gen.stubInfo(), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0), ident->impl());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_in_by_id(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int base = currentInstruction[2].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

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

void JIT::emitSlow_op_in_by_id(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int resultVReg = currentInstruction[1].u.operand;
    const Identifier* ident = &(m_codeBlock->identifier(currentInstruction[3].u.operand));

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

void JIT::emit_op_resolve_scope(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int scope = currentInstruction[2].u.operand;
    ResolveType resolveType = static_cast<ResolveType>(currentInstruction[4].u.operand);
    unsigned depth = currentInstruction[5].u.operand;
    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalVar:
        case GlobalLexicalVar:
        case GlobalPropertyWithVarInjectionChecks:
        case GlobalVarWithVarInjectionChecks: 
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
            move(TrustedImmPtr(currentInstruction[6].u.jsCell.get()), regT0);
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

void JIT::emit_op_get_from_scope(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int scope = currentInstruction[2].u.operand;
    ResolveType resolveType = GetPutInfo(currentInstruction[4].u.operand).resolveType();
    Structure** structureSlot = currentInstruction[5].u.structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&currentInstruction[6].u.pointer);
    
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
    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_get_from_scope(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    int dst = currentInstruction[1].u.operand;
    callOperationWithProfile(operationGetFromScope, dst, currentInstruction);
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

void JIT::emit_op_put_to_scope(Instruction* currentInstruction)
{
    int scope = currentInstruction[1].u.operand;
    int value = currentInstruction[3].u.operand;
    GetPutInfo getPutInfo = GetPutInfo(currentInstruction[4].u.operand);
    ResolveType resolveType = getPutInfo.resolveType();
    Structure** structureSlot = currentInstruction[5].u.structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&currentInstruction[6].u.pointer);
    
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
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, bitwise_cast<WatchpointSet**>(&currentInstruction[5]));
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, currentInstruction[5].u.watchpointSet);
            break;
        }
        case LocalClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitWriteBarrier(scope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitPutClosureVar(scope, *operandSlot, value, currentInstruction[5].u.watchpointSet);
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

void JIT::emitSlow_op_put_to_scope(Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    GetPutInfo getPutInfo = GetPutInfo(currentInstruction[4].u.operand);
    ResolveType resolveType = getPutInfo.resolveType();
    if (resolveType == ModuleVar) {
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else
        callOperation(operationPutToScope, currentInstruction);
}

void JIT::emit_op_get_from_arguments(Instruction* currentInstruction)
{
    int dst = currentInstruction[1].u.operand;
    int arguments = currentInstruction[2].u.operand;
    int index = currentInstruction[3].u.operand;
    
    emitLoadPayload(arguments, regT0);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset), regT1);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset), regT0);
    emitValueProfilingSite();
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_put_to_arguments(Instruction* currentInstruction)
{
    int arguments = currentInstruction[1].u.operand;
    int index = currentInstruction[2].u.operand;
    int value = currentInstruction[3].u.operand;
    
    emitWriteBarrier(arguments, value, ShouldFilterValue);
    
    emitLoadPayload(arguments, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset));
    store32(regT2, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset));
}

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
