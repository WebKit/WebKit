/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DirectArguments.h"
#include "GCAwareJITStubRoutine.h"
#include "InterpreterInlines.h"
#include "JITInlines.h"
#include "JSArray.h"
#include "JSFunction.h"
#include "JSLexicalEnvironment.h"
#include "JSPromise.h"
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
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int options = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_accessor;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterById, m_codeBlock->globalObject(), regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int options = bytecode.m_attributes;
    VirtualRegister setter = bytecode.m_accessor;

    emitLoadPayload(base, regT1);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterById, m_codeBlock->globalObject(), regT1, m_codeBlock->identifier(property).impl(), options, regT3);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    VirtualRegister base = bytecode.m_base;
    int property = bytecode.m_property;
    int attributes = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_getter;
    VirtualRegister setter = bytecode.m_setter;

    emitLoadPayload(base, regT1);
    emitLoadPayload(getter, regT3);
    emitLoadPayload(setter, regT4);
    callOperation(operationPutGetterSetter, m_codeBlock->globalObject(), regT1, m_codeBlock->identifier(property).impl(), attributes, regT3, regT4);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    int32_t attributes = bytecode.m_attributes;
    VirtualRegister getter = bytecode.m_accessor;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(getter, regT3);
    callOperation(operationPutGetterByVal, m_codeBlock->globalObject(), regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    int32_t attributes = bytecode.m_attributes;
    VirtualRegister setter = bytecode.m_accessor;

    emitLoadPayload(base, regT2);
    emitLoad(property, regT1, regT0);
    emitLoadPayload(setter, regT3);
    callOperation(operationPutSetterByVal, m_codeBlock->globalObject(), regT2, JSValueRegs(regT1, regT0), attributes, regT3);
}

void JIT::emit_op_del_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JSValueRegs baseRegs = JSValueRegs(regT3, regT2);
    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);

    emitLoad(base, baseRegs.tagGPR(), baseRegs.payloadGPR());
    emitJumpSlowCaseIfNotJSCell(base, baseRegs.tagGPR());
    JITDelByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident),
        baseRegs, resultRegs, regT4);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_delByIds.append(gen);

    boxBoolean(regT0, resultRegs);

    emitPutVirtualRegister(dst, resultRegs);

    // IC can write new Structure without write-barrier if a base is cell.
    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emit_op_del_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    JSValueRegs baseRegs = JSValueRegs(regT3, regT2);
    JSValueRegs propertyRegs = JSValueRegs(regT1, regT0);
    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);

    emitLoad2(base, baseRegs.tagGPR(), baseRegs.payloadGPR(), property, propertyRegs.tagGPR(), propertyRegs.payloadGPR());

    emitJumpSlowCaseIfNotJSCell(base, baseRegs.tagGPR());
    emitJumpSlowCaseIfNotJSCell(property, propertyRegs.tagGPR());

    JITDelByValGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        baseRegs, propertyRegs, resultRegs, regT4);

    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_delByVals.append(gen);

    boxBoolean(regT0, resultRegs);
    emitPutVirtualRegister(dst, resultRegs);

    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    JITDelByValGenerator& gen = m_delByVals[m_delByValIndex++];

    Label coldPathBegin = label();

    JSValueRegs baseRegs = JSValueRegs(regT3, regT2);
    JSValueRegs propertyRegs = JSValueRegs(regT1, regT0);
    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);

    emitLoad2(base, baseRegs.tagGPR(), baseRegs.payloadGPR(), property, propertyRegs.tagGPR(), propertyRegs.payloadGPR());

    Call call = callOperation(operationDeleteByValOptimize, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), JSValueRegs(baseRegs.tagGPR(), baseRegs.payloadGPR()), JSValueRegs(propertyRegs.tagGPR(), propertyRegs.payloadGPR()), TrustedImm32(bytecode.m_ecmaMode.value()));
    gen.reportSlowPathCall(coldPathBegin, call);

    boxBoolean(regT0, resultRegs);
    emitPutVirtualRegister(dst, resultRegs);
}

void JIT::emitSlow_op_del_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JSValueRegs baseRegs = JSValueRegs(regT1, regT0);
    JSValueRegs resultRegs = JSValueRegs(regT1, regT0);

    JITDelByIdGenerator& gen = m_delByIds[m_delByIdIndex++];

    Label coldPathBegin = label();

    emitLoad(base, baseRegs.tagGPR(), baseRegs.payloadGPR());

    Call call = callOperation(operationDeleteByIdOptimize, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), baseRegs, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits(), TrustedImm32(bytecode.m_ecmaMode.value()));
    gen.reportSlowPathCall(coldPathBegin, call);

    boxBoolean(regT0, resultRegs);
    emitPutVirtualRegister(dst, resultRegs);
}

void JIT::emit_op_get_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ArrayProfile* profile = &metadata.m_arrayProfile;

    emitLoad2(base, regT1, regT0, property, regT3, regT2);

    if (metadata.m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers()) {
        auto notCell = branchIfNotCell(regT1);
        emitArrayProfilingSiteWithCell(regT0, regT4, profile);
        notCell.link(this);
        callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByVal, dst, TrustedImmPtr(m_codeBlock->globalObject()), JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
    } else {
        emitJumpSlowCaseIfNotJSCell(base, regT1);
        emitArrayProfilingSiteWithCell(regT0, regT4, profile);

        JITGetByValGenerator gen(
            m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSet::stubUnavailableRegisters(),
            JSValueRegs::payloadOnly(regT0), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0));
        if (isOperandConstantInt(property))
            gen.stubInfo()->propertyIsInt32 = true;
        gen.generateFastPath(*this);
        addSlowCase(gen.slowPathJump());
        m_getByVals.append(gen);

        emitValueProfilingSite(bytecode.metadata(m_codeBlock));
        emitStore(dst, regT1, regT0);
    }
}

void JIT::emitSlow_op_get_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    if (hasAnySlowCases(iter)) {
        auto bytecode = currentInstruction->as<OpGetByVal>();
        VirtualRegister dst = bytecode.m_dst;
        auto& metadata = bytecode.metadata(m_codeBlock);
        ArrayProfile* profile = &metadata.m_arrayProfile;

        JITGetByValGenerator& gen = m_getByVals[m_getByValIndex];
        ++m_getByValIndex;

        linkAllSlowCases(iter);

        Label coldPathBegin = label();
        Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByValOptimize, dst, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), profile, JSValueRegs(regT1, regT0), JSValueRegs(regT3, regT2));
        gen.reportSlowPathCall(coldPathBegin, call);
    }
}

void JIT::emit_op_get_private_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetPrivateName>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    auto baseGPR = JSValueRegs::payloadOnly(regT0);
    auto propertyGPR = JSValueRegs(regT3, regT2);

    emitLoad2(base, regT1, regT0, property, regT3, regT2);

    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByValGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetPrivateName,
        RegisterSet::stubUnavailableRegisters(), baseGPR, propertyGPR, JSValueRegs(regT1, regT0));
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByVals.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}


void JIT::emitSlow_op_get_private_name(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    auto bytecode = currentInstruction->as<OpGetPrivateName>();
    VirtualRegister dst = bytecode.m_dst;

    linkAllSlowCases(iter);

    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex];
    ++m_getByValIndex;
    Label coldPathBegin = label();

    auto baseGPR = JSValueRegs(regT1, regT0);
    auto propertyGPR = JSValueRegs(regT3, regT2);
    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetPrivateNameOptimize, dst, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), baseGPR, propertyGPR);
    gen.reportSlowPathCall(coldPathBegin, call);
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
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ArrayProfile* profile = &metadata.m_arrayProfile;
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
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeIndex, notIndex, badType, mode, profile, done, done));
}

template <typename Op>
JIT::JumpList JIT::emitGenericContiguousPutByVal(Op bytecode, PatchableJump& badType, IndexingType indexingShape)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    ArrayProfile* profile = &metadata.m_arrayProfile;

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
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    ArrayProfile* profile = &metadata.m_arrayProfile;

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

void JIT::emitSlow_op_put_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    bool isDirect = currentInstruction->opcodeID() == op_put_by_val_direct;
    VirtualRegister base;
    VirtualRegister property;
    VirtualRegister value;
    ECMAMode ecmaMode = ECMAMode::strict();

    auto load = [&](auto bytecode) {
        base = bytecode.m_base;
        property = bytecode.m_property;
        value = bytecode.m_value;
        ecmaMode = JIT::ecmaMode(bytecode);
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
    Call call = callOperation(isDirect ? operationDirectPutByValOptimize : operationPutByValOptimize, m_codeBlock->globalObject(), JSValueRegs(regT2, regT1), JSValueRegs(regT3, regT0), JSValueRegs(regT5, regT4), byValInfo, TrustedImm32(ecmaMode.value()));

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_try_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::TryGetById);
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationTryGetByIdOptimize, resultVReg, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT1, regT0), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id_direct(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::GetByIdDirect);
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdDirectOptimize, resultVReg, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT1, regT0), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}


void JIT::emit_op_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));
    
    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling()) {
        Jump notArrayLengthMode = branch8(NotEqual, AbsoluteAddress(&metadata.m_modeMetadata.mode), TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
        emitArrayProfilingSiteWithCell(regT0, regT2, &metadata.m_modeMetadata.arrayLengthMode.arrayProfile);
        notArrayLengthMode.link(this);
    }

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0), AccessType::GetById);
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, resultVReg, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT1, regT0), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id_with_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister thisVReg = bytecode.m_thisValue;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));
    
    emitLoad(base, regT1, regT0);
    emitLoad(thisVReg, regT4, regT3);
    emitJumpSlowCaseIfNotJSCell(base, regT1);
    emitJumpSlowCaseIfNotJSCell(thisVReg, regT4);

    JITGetByIdWithThisGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT1, regT0), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT4, regT3));
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();
    
    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdWithThisOptimize, resultVReg, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT1, regT0), JSValueRegs(regT4, regT3), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_put_by_id(const Instruction* currentInstruction)
{
    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.
    
    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    bool direct = bytecode.m_flags.isDirect();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));
    
    emitLoad2(base, regT1, regT0, value, regT3, regT2);
    
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident),
        JSValueRegs::payloadOnly(regT0), JSValueRegs(regT3, regT2),
        regT1, bytecode.m_flags.ecmaMode(), direct ? Direct : NotDirect, PrivateFieldAccessKind::None);
    
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_putByIds.append(gen);
    
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_put_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    Label coldPathBegin(this);

    // JITPutByIdGenerator only preserve the value and the base's payload, we have to reload the tag.
    emitLoadTag(base, regT1);

    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];
    
    Call call = callOperation(
        gen.slowPathFunction(), m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_in_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitLoad(base, regT1, regT0);
    emitJumpSlowCaseIfNotJSCell(base, regT1);

    JITInByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs::payloadOnly(regT0), JSValueRegs(regT1, regT0));
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_inByIds.append(gen);

    emitStore(dst, regT1, regT0);
}

void JIT::emitSlow_op_in_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationInByIdOptimize, resultVReg, m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT1, regT0), CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emitVarInjectionCheck(bool needsVarInjectionChecks)
{
    if (!needsVarInjectionChecks)
        return;
    addSlowCase(branch8(Equal, AbsoluteAddress(m_codeBlock->globalObject()->varInjectionWatchpoint()->addressOfState()), TrustedImm32(IsInvalidated)));
}

void JIT::emitResolveClosure(VirtualRegister dst, VirtualRegister scope, bool needsVarInjectionChecks, unsigned depth)
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
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType resolveType = metadata.m_resolveType;
    unsigned depth = metadata.m_localScopeDepth;

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            JSScope* constantScope = JSScope::constantScopeForCodeBlock(resolveType, m_codeBlock);
            RELEASE_ASSERT(constantScope);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            load32(&metadata.m_globalLexicalBindingEpoch, regT1);
            addSlowCase(branch32(NotEqual, AbsoluteAddress(m_codeBlock->globalObject()->addressOfGlobalLexicalBindingEpoch()), regT1));
            move(TrustedImm32(JSValue::CellTag), regT1);
            move(TrustedImmPtr(constantScope), regT0);
            emitStore(dst, regT1, regT0);
            break;
        }

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
            move(TrustedImmPtr(metadata.m_lexicalEnvironment.get()), regT0);
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
        JumpList skipToEnd;
        load32(&metadata.m_resolveType, regT0);

        Jump notGlobalProperty = branch32(NotEqual, regT0, TrustedImm32(resolveType));
        emitCode(resolveType);
        skipToEnd.append(jump());

        notGlobalProperty.link(this);
        emitCode(needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar);

        skipToEnd.link(this);
        break;
    }
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        JumpList skipToEnd;
        load32(&metadata.m_resolveType, regT0);

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

void JIT::emitLoadWithStructureCheck(VirtualRegister scope, Structure** structureSlot)
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

void JIT::emitGetClosureVar(VirtualRegister scope, uintptr_t operand)
{
    emitLoad(scope, regT1, regT0);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + TagOffset), regT1);
    load32(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register) + PayloadOffset), regT0);
}

void JIT::emit_op_get_from_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType resolveType = metadata.m_getPutInfo.resolveType();
    Structure** structureSlot = metadata.m_structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.m_operand);
    
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
            if (ASSERT_ENABLED) {
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
        load32(&metadata.m_getPutInfo, regT0);
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
        load32(&metadata.m_getPutInfo, regT0);
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
    VirtualRegister dst = bytecode.m_dst;
    callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetFromScope, dst, m_codeBlock->globalObject(), currentInstruction);
}

void JIT::emitPutGlobalVariable(JSValue* operand, VirtualRegister value, WatchpointSet* set)
{
    emitLoad(value, regT1, regT0);
    emitNotifyWrite(set);
    uintptr_t rawAddress = bitwise_cast<uintptr_t>(operand);
    store32(regT1, bitwise_cast<void*>(rawAddress + TagOffset));
    store32(regT0, bitwise_cast<void*>(rawAddress + PayloadOffset));
}

void JIT::emitPutGlobalVariableIndirect(JSValue** addressOfOperand, VirtualRegister value, WatchpointSet** indirectWatchpointSet)
{
    emitLoad(value, regT1, regT0);
    loadPtr(indirectWatchpointSet, regT2);
    emitNotifyWrite(regT2);
    loadPtr(addressOfOperand, regT2);
    store32(regT1, Address(regT2, TagOffset));
    store32(regT0, Address(regT2, PayloadOffset));
}

void JIT::emitPutClosureVar(VirtualRegister scope, uintptr_t operand, VirtualRegister value, WatchpointSet* set)
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
    VirtualRegister scope = bytecode.m_scope;
    VirtualRegister value = bytecode.m_value;
    GetPutInfo getPutInfo = copiedGetPutInfo(bytecode);
    ResolveType resolveType = getPutInfo.resolveType();
    Structure** structureSlot = metadata.m_structure.slot();
    uintptr_t* operandSlot = reinterpret_cast<uintptr_t*>(&metadata.m_operand);
    
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
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, &metadata.m_watchpointSet);
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, metadata.m_watchpointSet);
            break;
        }
        case LocalClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitWriteBarrier(scope, value, ShouldFilterValue);
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitPutClosureVar(scope, *operandSlot, value, metadata.m_watchpointSet);
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
        load32(&metadata.m_getPutInfo, regT0);
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
        load32(&metadata.m_getPutInfo, regT0);
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
        callOperation(operationPutToScope, m_codeBlock->globalObject(), currentInstruction);
}

void JIT::emit_op_get_from_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    
    emitLoadPayload(arguments, regT0);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset), regT1);
    load32(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset), regT0);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_put_to_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    VirtualRegister value = bytecode.m_value;
    
    emitWriteBarrier(arguments, value, ShouldFilterValue);
    
    emitLoadPayload(arguments, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + TagOffset));
    store32(regT2, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>) + PayloadOffset));
}

void JIT::emit_op_get_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetInternalField>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    unsigned index = bytecode.m_index;

    emitLoadPayload(base, regT2);
    load32(Address(regT2, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + TagOffset), regT1);
    load32(Address(regT2, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + PayloadOffset), regT0);
    emitValueProfilingSite(metadata);
    emitStore(dst, regT1, regT0);
}

void JIT::emit_op_put_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutInternalField>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    unsigned index = bytecode.m_index;

    emitLoadPayload(base, regT0);
    emitLoad(value, regT1, regT2);
    store32(regT1, Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + TagOffset));
    store32(regT2, Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index) + PayloadOffset));
    emitWriteBarrier(base, value, ShouldFilterValue);
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const Instruction*);

} // namespace JSC

#endif // USE(JSVALUE32_64)
#endif // ENABLE(JIT)
