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
#include "JIT.h"

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DirectArguments.h"
#include "JITInlines.h"
#include "JSLexicalEnvironment.h"
#include "LinkBuffer.h"
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
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    ArrayProfile* profile = &metadata.m_arrayProfile;

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);

    if (metadata.m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers()) {
        auto notCell = branchIfNotCell(regT0);
        emitArrayProfilingSiteWithCell(regT0, regT2, profile);
        notCell.link(this);
        callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByVal, dst, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1);
    } else {
        emitJumpSlowCaseIfNotJSCell(regT0, base);
        emitArrayProfilingSiteWithCell(regT0, regT2, profile);

        JITGetByValGenerator gen(
            m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
            JSValueRegs(regT0), JSValueRegs(regT1), JSValueRegs(regT0));
        if (isOperandConstantInt(property))
            gen.stubInfo()->propertyIsInt32 = true;
        gen.generateFastPath(*this);
        addSlowCase(gen.slowPathJump());
        m_getByVals.append(gen);

        emitValueProfilingSite(bytecode.metadata(m_codeBlock));
        emitPutVirtualRegister(dst);
    }

}

void JIT::emitSlow_op_get_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    if (hasAnySlowCases(iter)) {
        auto bytecode = currentInstruction->as<OpGetByVal>();
        VirtualRegister dst = bytecode.m_dst;
        auto& metadata = bytecode.metadata(m_codeBlock);
        ArrayProfile* profile = &metadata.m_arrayProfile;

        linkAllSlowCases(iter);

        JITGetByValGenerator& gen = m_getByVals[m_getByValIndex];
        ++m_getByValIndex;
        Label coldPathBegin = label();
        Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByValOptimize, dst, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), profile, regT0, regT1);
        gen.reportSlowPathCall(coldPathBegin, call);
    }
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
    
    m_byValCompilationInfo.append(ByValCompilationInfo(byValInfo, m_bytecodeIndex, notIndex, badType, mode, profile, done, done));
}

template<typename Op>
JIT::JumpList JIT::emitGenericContiguousPutByVal(Op bytecode, PatchableJump& badType, IndexingType indexingShape)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister value = bytecode.m_value;
    ArrayProfile* profile = &metadata.m_arrayProfile;
    
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
        add64(numberTagRegister, regT3);
        move64ToDouble(regT3, fpRegT0);
        slowCases.append(branchIfNaN(fpRegT0));
        ready.link(this);
        storeDouble(fpRegT0, BaseIndex(regT2, regT1, TimesEight));
        break;
    }
    case ContiguousShape:
        store64(regT3, BaseIndex(regT2, regT1, TimesEight));
        emitWriteBarrier(bytecode.m_base, value, ShouldFilterValue);
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
    VirtualRegister value = bytecode.m_value;
    ArrayProfile* profile = &metadata.m_arrayProfile;
    
    JumpList slowCases;
    
    badType = patchableBranch32(NotEqual, regT2, TrustedImm32(ArrayStorageShape));
    loadPtr(Address(regT0, JSObject::butterflyOffset()), regT2);
    slowCases.append(branch32(AboveOrEqual, regT1, Address(regT2, ArrayStorage::vectorLengthOffset())));

    Jump empty = branchTest64(Zero, BaseIndex(regT2, regT1, TimesEight, ArrayStorage::vectorOffset()));

    Label storeResult(this);
    emitGetVirtualRegister(value, regT3);
    store64(regT3, BaseIndex(regT2, regT1, TimesEight, ArrayStorage::vectorOffset()));
    emitWriteBarrier(bytecode.m_base, value, ShouldFilterValue);
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
JITPutByIdGenerator JIT::emitPutByValWithCachedId(Op bytecode, PutKind putKind, CacheableIdentifier propertyName, JumpList& doneCases, JumpList& slowCases)
{
    // base: regT0
    // property: regT1
    // scratch: regT2

    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;

    slowCases.append(branchIfNotCell(regT1));
    emitByValIdentifierCheck(regT1, regT1, propertyName, slowCases);

    // Write barrier breaks the registers. So after issuing the write barrier,
    // reload the registers.
    emitGetVirtualRegisters(base, regT0, value, regT1);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(), propertyName,
        JSValueRegs(regT0), JSValueRegs(regT1), regT2, ecmaMode(bytecode), putKind, privateFieldAccessKind(bytecode));
    gen.generateFastPath(*this);
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
    doneCases.append(jump());

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    Call call = callOperation(gen.slowPathFunction(), TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT1, regT0, propertyName.rawBits());
    gen.reportSlowPathCall(coldPathBegin, call);
    doneCases.append(jump());

    return gen;
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

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    emitGetVirtualRegister(value, regT2);
    Call call = callOperation(isDirect ? operationDirectPutByValOptimize : operationPutByValOptimize, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1, regT2, byValInfo, TrustedImm32(ecmaMode.value()));

    m_byValCompilationInfo[m_byValInstructionIndex].slowPathTarget = slowPath;
    m_byValCompilationInfo[m_byValInstructionIndex].returnAddress = call;
    m_byValInstructionIndex++;
}

void JIT::emit_op_put_getter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT1);
    callOperation(operationPutGetterById, TrustedImmPtr(m_codeBlock->globalObject()), regT0, m_codeBlock->identifier(bytecode.m_property).impl(), options, regT1);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT1);
    callOperation(operationPutSetterById, TrustedImmPtr(m_codeBlock->globalObject()), regT0, m_codeBlock->identifier(bytecode.m_property).impl(), options, regT1);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t attribute = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_getter, regT1);
    emitGetVirtualRegister(bytecode.m_setter, regT2);
    callOperation(operationPutGetterSetter, TrustedImmPtr(m_codeBlock->globalObject()), regT0, m_codeBlock->identifier(bytecode.m_property).impl(), attribute, regT1, regT2);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    emitGetVirtualRegister(bytecode.m_property, regT1);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT2);
    callOperation(operationPutGetterByVal, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1, attributes, regT2);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    emitGetVirtualRegister(bytecode.m_property, regT1);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT2);
    callOperation(operationPutSetterByVal, TrustedImmPtr(m_codeBlock->globalObject()), regT0, regT1, attributes, regT2);
}

void JIT::emit_op_del_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(base, regT1);
    emitJumpSlowCaseIfNotJSCell(regT1, base);
    JITDelByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident),
        JSValueRegs(regT1), JSValueRegs(regT0), regT2);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_delByIds.append(gen);

    boxBoolean(regT0, JSValueRegs(regT0));
    emitPutVirtualRegister(dst, JSValueRegs(regT0));

    // IC can write new Structure without write-barrier if a base is cell.
    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITDelByIdGenerator& gen = m_delByIds[m_delByIdIndex++];

    Label coldPathBegin = label();

    emitGetVirtualRegister(base, regT0);
    Call call = callOperation(operationDeleteByIdOptimize, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits(), TrustedImm32(bytecode.m_ecmaMode.value()));
    gen.reportSlowPathCall(coldPathBegin, call);

    boxBoolean(regT0, JSValueRegs(regT0));
    emitPutVirtualRegister(dst, JSValueRegs(regT0));
}

void JIT::emit_op_del_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    emitGetVirtualRegister(base, regT1);
    emitJumpSlowCaseIfNotJSCell(regT1, base);
    emitGetVirtualRegister(property, regT0);
    emitJumpSlowCaseIfNotJSCell(regT0, property);
    JITDelByValGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        JSValueRegs(regT1), JSValueRegs(regT0), JSValueRegs(regT0), regT2);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_delByVals.append(gen);

    boxBoolean(regT0, JSValueRegs(regT0));
    emitPutVirtualRegister(dst, JSValueRegs(regT0));

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

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(property, regT1);
    Call call = callOperation(operationDeleteByValOptimize, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, regT1, TrustedImm32(bytecode.m_ecmaMode.value()));
    gen.reportSlowPathCall(coldPathBegin, call);

    boxBoolean(regT0, JSValueRegs(regT0));
    emitPutVirtualRegister(dst, JSValueRegs(regT0));
}

void JIT::emit_op_try_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::TryGetById);
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationTryGetByIdOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());
    
    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id_direct(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::GetByIdDirect);
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdDirectOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(baseVReg, regT0);
    
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);
    
    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling()) {
        Jump notArrayLengthMode = branch8(NotEqual, AbsoluteAddress(&metadata.m_modeMetadata.mode), TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)));
        emitArrayProfilingSiteWithCell(regT0, regT1, &metadata.m_modeMetadata.arrayLengthMode.arrayProfile);
        notArrayLengthMode.link(this);
    }

    JITGetByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT0), JSValueRegs(regT0), AccessType::GetById);
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(resultVReg);
}

void JIT::emit_op_get_by_id_with_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister thisVReg = bytecode.m_thisValue;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(baseVReg, regT0);
    emitGetVirtualRegister(thisVReg, regT1);
    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);
    emitJumpSlowCaseIfNotJSCell(regT1, thisVReg);

    JITGetByIdWithThisGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT0), JSValueRegs(regT0), JSValueRegs(regT1));
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
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emitSlow_op_get_by_id_with_this(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();

    Call call = callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetByIdWithThisOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, regT1, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_put_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister valueVReg = bytecode.m_value;
    bool direct = bytecode.m_flags.isDirect();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    emitGetVirtualRegisters(baseVReg, regT0, valueVReg, regT1);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident),
        JSValueRegs(regT0), JSValueRegs(regT1), regT2, ecmaMode(bytecode),
        direct ? Direct : NotDirect, privateFieldAccessKind(bytecode));
    
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_putByIds.append(gen);
    
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(baseVReg, ShouldFilterBase);
}

void JIT::emitSlow_op_put_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutById>();
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    Label coldPathBegin(this);
    
    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];

    Call call = callOperation(gen.slowPathFunction(), TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT1, regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

    gen.reportSlowPathCall(coldPathBegin, call);
}

void JIT::emit_op_in_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    emitGetVirtualRegister(baseVReg, regT0);

    emitJumpSlowCaseIfNotJSCell(regT0, baseVReg);

    JITInByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident), JSValueRegs(regT0), JSValueRegs(regT0));
    gen.generateFastPath(*this);
    addSlowCase(gen.slowPathJump());
    m_inByIds.append(gen);

    emitPutVirtualRegister(resultVReg);
}

void JIT::emitSlow_op_in_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_codeBlock->identifier(bytecode.m_property));

    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    Label coldPathBegin = label();

    Call call = callOperation(operationInByIdOptimize, resultVReg, TrustedImmPtr(m_codeBlock->globalObject()), gen.stubInfo(), regT0, CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_codeBlock, *ident).rawBits());

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
    emitGetVirtualRegister(scope, regT0);
    for (unsigned i = 0; i < depth; ++i)
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitPutVirtualRegister(dst);
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
            move(TrustedImmPtr(constantScope), regT0);
            emitPutVirtualRegister(dst);
            break;
        }

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
            move(TrustedImmPtr(metadata.m_lexicalEnvironment.get()), regT0);
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

void JIT::emitGetClosureVar(VirtualRegister scope, uintptr_t operand)
{
    emitGetVirtualRegister(scope, regT0);
    loadPtr(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register)), regT0);
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
            emitLoadWithStructureCheck(scope, structureSlot); // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            GPRReg base = regT0;
            GPRReg result = regT0;
            GPRReg offset = regT1;
            GPRReg scratch = regT2;

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                return branchPtr(Equal, base, TrustedImmPtr(m_codeBlock->globalObject()));
            }));

            load32(operandSlot, offset);
            if (ASSERT_ENABLED) {
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
    emitPutVirtualRegister(dst);
    emitValueProfilingSite(metadata);
}

void JIT::emitSlow_op_get_from_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    callOperationWithProfile(bytecode.metadata(m_codeBlock), operationGetFromScope, dst, TrustedImmPtr(m_codeBlock->globalObject()), currentInstruction);
}

void JIT::emitPutGlobalVariable(JSValue* operand, VirtualRegister value, WatchpointSet* set)
{
    emitGetVirtualRegister(value, regT0);
    emitNotifyWrite(set);
    storePtr(regT0, operand);
}
void JIT::emitPutGlobalVariableIndirect(JSValue** addressOfOperand, VirtualRegister value, WatchpointSet** indirectWatchpointSet)
{
    emitGetVirtualRegister(value, regT0);
    loadPtr(indirectWatchpointSet, regT1);
    emitNotifyWrite(regT1);
    loadPtr(addressOfOperand, regT1);
    storePtr(regT0, regT1);
}

void JIT::emitPutClosureVar(VirtualRegister scope, uintptr_t operand, VirtualRegister value, WatchpointSet* set)
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
                emitPutGlobalVariableIndirect(bitwise_cast<JSValue**>(operandSlot), value, &metadata.m_watchpointSet);
            else
                emitPutGlobalVariable(bitwise_cast<JSValue*>(*operandSlot), value, metadata.m_watchpointSet);
            emitWriteBarrier(constantScope, value, ShouldFilterValue);
            break;
        }
        case LocalClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType));
            emitPutClosureVar(scope, *operandSlot, value, metadata.m_watchpointSet);
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
        callOperation(operationPutToScope, TrustedImmPtr(m_codeBlock->globalObject()), currentInstruction);
}

void JIT::emit_op_get_from_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    
    emitGetVirtualRegister(arguments, regT0);
    load64(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)), regT0);
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_put_to_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    VirtualRegister value = bytecode.m_value;
    
    emitGetVirtualRegister(arguments, regT0);
    emitGetVirtualRegister(value, regT1);
    store64(regT1, Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)));

    emitWriteBarrier(arguments, value, ShouldFilterValue);
}

void JIT::emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode mode)
{
    // value may be invalid VirtualRegister if mode is UnconditionalWriteBarrier or ShouldFilterBase.
    Jump valueNotCell;
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) {
        emitGetVirtualRegister(value, regT0);
        valueNotCell = branchIfNotCell(regT0);
    }
    
    emitGetVirtualRegister(owner, regT0);
    Jump ownerNotCell;
    if (mode == ShouldFilterBaseAndValue || mode == ShouldFilterBase)
        ownerNotCell = branchIfNotCell(regT0);

    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), regT0, regT1);
    callOperation(operationWriteBarrierSlowPath, &vm(), regT0);
    ownerIsRememberedOrInEden.link(this);

    if (mode == ShouldFilterBaseAndValue || mode == ShouldFilterBase)
        ownerNotCell.link(this);
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) 
        valueNotCell.link(this);
}

void JIT::emitWriteBarrier(JSCell* owner, VirtualRegister value, WriteBarrierMode mode)
{
    emitGetVirtualRegister(value, regT0);
    Jump valueNotCell;
    if (mode == ShouldFilterValue)
        valueNotCell = branchIfNotCell(regT0);

    emitWriteBarrier(owner);

    if (mode == ShouldFilterValue) 
        valueNotCell.link(this);
}

void JIT::emit_op_get_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetInternalField>();
    auto& metadata = bytecode.metadata(m_codeBlock);
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    unsigned index = bytecode.m_index;

    emitGetVirtualRegister(base, regT1);
    loadPtr(Address(regT1, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)), regT0);

    emitValueProfilingSite(metadata);
    emitPutVirtualRegister(dst);
}

void JIT::emit_op_put_internal_field(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutInternalField>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    unsigned index = bytecode.m_index;

    emitGetVirtualRegister(base, regT0);
    emitGetVirtualRegister(value, regT1);
    storePtr(regT1, Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)));
    emitWriteBarrier(base, value, ShouldFilterValue);
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const Instruction*);

#else // USE(JSVALUE64)

void JIT::emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode mode)
{
    // value may be invalid VirtualRegister if mode is UnconditionalWriteBarrier or ShouldFilterBase.
    Jump valueNotCell;
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) {
        emitLoadTag(value, regT0);
        valueNotCell = branchIfNotCell(regT0);
    }
    
    emitLoad(owner, regT0, regT1);
    Jump ownerNotCell;
    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell = branchIfNotCell(regT0);

    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), regT1, regT2);
    callOperation(operationWriteBarrierSlowPath, &vm(), regT1);
    ownerIsRememberedOrInEden.link(this);

    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell.link(this);
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) 
        valueNotCell.link(this);
}

void JIT::emitWriteBarrier(JSCell* owner, VirtualRegister value, WriteBarrierMode mode)
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

template <typename Op>
JITPutByIdGenerator JIT::emitPutByValWithCachedId(Op bytecode, PutKind putKind, CacheableIdentifier propertyName, JumpList& doneCases, JumpList& slowCases)
{
    // base: tag(regT1), payload(regT0)
    // property: tag(regT3), payload(regT2)

    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;

    slowCases.append(branchIfNotCell(regT3));
    emitByValIdentifierCheck(regT2, regT2, propertyName, slowCases);

    // Write barrier breaks the registers. So after issuing the write barrier,
    // reload the registers.
    //
    // IC can write new Structure without write-barrier if a base is cell.
    // We are emitting write-barrier before writing here but this is OK since 32bit JSC does not have concurrent GC.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
    emitLoadPayload(base, regT0);
    emitLoad(value, regT3, regT2);

    JITPutByIdGenerator gen(
        m_codeBlock, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(), propertyName,
        JSValueRegs::payloadOnly(regT0), JSValueRegs(regT3, regT2), regT1, ecmaMode(bytecode), putKind, privateFieldAccessKind(bytecode));
    gen.generateFastPath(*this);
    doneCases.append(jump());

    Label coldPathBegin = label();
    gen.slowPathJump().link(this);

    // JITPutByIdGenerator only preserve the value and the base's payload, we have to reload the tag.
    emitLoadTag(base, regT1);

    Call call = callOperation(gen.slowPathFunction(), m_codeBlock->globalObject(), gen.stubInfo(), JSValueRegs(regT3, regT2), JSValueRegs(regT1, regT0), propertyName.rawBits());
    gen.reportSlowPathCall(coldPathBegin, call);
    doneCases.append(jump());

    return gen;
}

#endif // USE(JSVALUE64)

void JIT::emitWriteBarrier(VirtualRegister owner, WriteBarrierMode mode)
{
    ASSERT(mode == UnconditionalWriteBarrier || mode == ShouldFilterBase);
    emitWriteBarrier(owner, VirtualRegister(), mode);
}

void JIT::emitWriteBarrier(JSCell* owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), owner, regT0);
    callOperation(operationWriteBarrierSlowPath, &vm(), owner);
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitByValIdentifierCheck(RegisterID cell, RegisterID scratch, CacheableIdentifier propertyName, JumpList& slowCases)
{
    if (propertyName.isSymbolCell())
        slowCases.append(branchPtr(NotEqual, cell, TrustedImmPtr(propertyName.cell())));
    else {
        slowCases.append(branchIfNotString(cell));
        loadPtr(Address(cell, JSString::offsetOfValue()), scratch);
        slowCases.append(branchPtr(NotEqual, scratch, TrustedImmPtr(propertyName.uid())));
    }
}

template<typename Op>
void JIT::privateCompilePutByVal(const ConcurrentJSLocker&, ByValInfo* byValInfo, ReturnAddressPtr returnAddress, JITArrayMode arrayMode)
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
    patchBuffer.link(done, byValInfo->doneTarget);
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
// This function is only consumed from another translation unit (JITOperations.cpp),
// so we list off the two expected specializations in advance.
template void JIT::privateCompilePutByVal<OpPutByVal>(const ConcurrentJSLocker&, ByValInfo*, ReturnAddressPtr, JITArrayMode);
template void JIT::privateCompilePutByVal<OpPutByValDirect>(const ConcurrentJSLocker&, ByValInfo*, ReturnAddressPtr, JITArrayMode);

template<typename Op>
void JIT::privateCompilePutByValWithCachedId(ByValInfo* byValInfo, ReturnAddressPtr returnAddress, PutKind putKind, CacheableIdentifier propertyName)
{
    ASSERT((putKind == Direct && Op::opcodeID == op_put_by_val_direct) || (putKind == NotDirect && Op::opcodeID == op_put_by_val));
    const Instruction* currentInstruction = m_codeBlock->instructions().at(byValInfo->bytecodeIndex).ptr();
    auto bytecode = currentInstruction->as<Op>();

    JumpList doneCases;
    JumpList slowCases;

    JITPutByIdGenerator gen = emitPutByValWithCachedId(bytecode, putKind, propertyName, doneCases, slowCases);

    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    LinkBuffer patchBuffer(*this, m_codeBlock);
    patchBuffer.link(slowCases, byValInfo->slowPathTarget);
    patchBuffer.link(doneCases, byValInfo->doneTarget);
    if (!m_exceptionChecks.empty())
        patchBuffer.link(m_exceptionChecks, byValInfo->exceptionHandler);

    for (const auto& callSite : m_calls) {
        if (callSite.callee)
            patchBuffer.link(callSite.from, callSite.callee);
    }
    gen.finalize(patchBuffer, patchBuffer);

    byValInfo->stubRoutine = FINALIZE_CODE_FOR_STUB(
        m_codeBlock, patchBuffer, JITStubRoutinePtrTag,
        "Baseline put_by_val%s with cached property name '%s' stub for %s, return point %p", (putKind == Direct) ? "_direct" : "", propertyName.uid()->utf8().data(), toCString(*m_codeBlock).data(), returnAddress.value());
    byValInfo->stubInfo = gen.stubInfo();

    MacroAssembler::repatchJump(byValInfo->notIndexJump, CodeLocationLabel<JITStubRoutinePtrTag>(byValInfo->stubRoutine->code().code()));
    MacroAssembler::repatchCall(CodeLocationCall<NoPtrTag>(MacroAssemblerCodePtr<NoPtrTag>(returnAddress)), FunctionPtr<OperationPtrTag>(putKind == Direct ? operationDirectPutByValGeneric : operationPutByValGeneric));
}
// This function is only consumed from another translation unit (JITOperations.cpp),
// so we list off the two expected specializations in advance.
template void JIT::privateCompilePutByValWithCachedId<OpPutByVal>(ByValInfo*, ReturnAddressPtr, PutKind, CacheableIdentifier);
template void JIT::privateCompilePutByValWithCachedId<OpPutByValDirect>(ByValInfo*, ReturnAddressPtr, PutKind, CacheableIdentifier);

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

template<typename Op>
JIT::JumpList JIT::emitIntTypedArrayPutByVal(Op bytecode, PatchableJump& badType, TypedArrayType type)
{
    auto& metadata = bytecode.metadata(m_codeBlock);
    ArrayProfile* profile = &metadata.m_arrayProfile;
    ASSERT(isInt(type));
    
    VirtualRegister value = bytecode.m_value;

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
    load32(Address(base, JSArrayBufferView::offsetOfLength()), lateScratch2);
    Jump inBounds = branch32(Below, property, lateScratch2);
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
    cageConditionally(Gigacage::Primitive, lateScratch, lateScratch2, lateScratch2);
    
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
    ArrayProfile* profile = &metadata.m_arrayProfile;
    ASSERT(isFloat(type));
    
    VirtualRegister value = bytecode.m_value;

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
    load32(Address(base, JSArrayBufferView::offsetOfLength()), lateScratch2);
    Jump inBounds = branch32(Below, property, lateScratch2);
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
    add64(numberTagRegister, earlyScratch);
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
    cageConditionally(Gigacage::Primitive, lateScratch, lateScratch2, lateScratch2);
    
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

} // namespace JSC

#endif // ENABLE(JIT)
