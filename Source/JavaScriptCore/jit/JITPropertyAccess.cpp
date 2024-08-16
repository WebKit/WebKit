/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#include "BaselineJITRegisters.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DirectArguments.h"
#include "JITInlines.h"
#include "JITThunks.h"
#include "JSLexicalEnvironment.h"
#include "LinkBuffer.h"
#include "PrivateFieldPutKind.h"
#include "ProbeContext.h"
#include "SlowPathCall.h"
#include "StructureStubInfo.h"
#include "ThunkGenerators.h"
#include <wtf/ScopedLambda.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

void JIT::emit_op_get_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::resultJSR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::profileGPR;
    using BaselineJITRegisters::GetByVal::scratch1GPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpGetByVal::Metadata::offsetOfArrayProfile(), profileGPR);

    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, profileGPR, stubInfoGPR);
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;

    if (bytecode.metadata(m_profiledCodeBlock).m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers())
        stubInfo->canBeMegamorphic = true;

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCellAndProfile(baseJSR.payloadGPR(), profileGPR, scratch1GPR);

    gen.generateDataICFastPath(*this);

    addSlowCase();
    m_getByVals.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

template<typename OpcodeType>
void JIT::generateGetByValSlowCase(const OpcodeType&, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emitSlow_op_get_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generateGetByValSlowCase(currentInstruction->as<OpGetByVal>(), iter);
}

void JIT::emit_op_get_private_name(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetPrivateName>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::resultJSR;
    using BaselineJITRegisters::GetByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetPrivateName,
        RegisterSetBuilder::stubUnavailableRegisters(), baseJSR, propertyJSR, resultJSR, InvalidGPRReg, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_getByVals.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_get_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_set_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, propertyJSR);
    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    JITPrivateBrandAccessGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::SetPrivateBrand, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_privateBrandAccesses.append(gen);

    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_set_private_brand(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    UNUSED_PARAM(currentInstruction);
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_check_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::propertyJSR;
    using BaselineJITRegisters::PrivateBrand::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITPrivateBrandAccessGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::CheckPrivateBrand, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_privateBrandAccesses.append(gen);
}

void JIT::emitSlow_op_check_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

template<typename Op>
void JIT::emit_op_put_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    VirtualRegister value = bytecode.m_value;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::profileGPR; // Keep in mind that this can be a metadataTable register in ARMv7.
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::scratch1GPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfArrayProfile(), profileGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCellAndProfile(baseJSR.payloadGPR(), profileGPR, scratch1GPR);

    ECMAMode ecmaMode = this->ecmaMode(bytecode);
    bool isDirect = std::is_same_v<Op, OpPutByValDirect>;
    JITPutByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        isDirect ? (ecmaMode.isStrict() ? AccessType::PutByValDirectStrict : AccessType::PutByValDirectSloppy) : (ecmaMode.isStrict() ? AccessType::PutByValStrict : AccessType::PutByValSloppy),
        RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, profileGPR, stubInfoGPR);
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
#if CPU(ARM_THUMB2)
    // ARMv7 clobbers metadataTable register. Thus we need to restore them back here.
    emitMaterializeMetadataAndConstantPoolRegisters();
#endif
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
    setFastPathResumePoint();
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const JSInstruction*);

void JIT::emit_op_put_by_val_direct(const JSInstruction* currentInstruction)
{
    emit_op_put_by_val<OpPutByValDirect>(currentInstruction);
}

template<typename OpcodeType>
void JIT::generatePutByValSlowCase(const OpcodeType&, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emitSlow_op_put_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generatePutByValSlowCase(currentInstruction->as<OpPutByVal>(), iter);
}

void JIT::emitSlow_op_put_by_val_direct(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generatePutByValSlowCase(currentInstruction->as<OpPutByValDirect>(), iter);
}

void JIT::emit_op_put_private_name(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutPrivateName>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    VirtualRegister value = bytecode.m_value;

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITPutByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), bytecode.m_putKind.isDefine() ? AccessType::DefinePrivateNameByVal : AccessType::SetPrivateNameByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, InvalidGPRReg, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_put_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_put_getter_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    emitGetVirtualRegisterPayload(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegisterPayload(bytecode.m_accessor, regT1);
    loadGlobalObject(regT2);
    callOperation(operationPutGetterById, regT2, regT0, TrustedImmPtr(m_unlinkedCodeBlock->identifier(bytecode.m_property).impl()), options, regT1);
}

void JIT::emit_op_put_setter_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    emitGetVirtualRegisterPayload(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegisterPayload(bytecode.m_accessor, regT1);
    loadGlobalObject(regT2);
    callOperation(operationPutSetterById, regT2, regT0, TrustedImmPtr(m_unlinkedCodeBlock->identifier(bytecode.m_property).impl()), options, regT1);
}

void JIT::emit_op_put_getter_setter_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    emitGetVirtualRegisterPayload(bytecode.m_base, regT0);
    int32_t attribute = bytecode.m_attributes;
    emitGetVirtualRegisterPayload(bytecode.m_getter, regT1);
    emitGetVirtualRegisterPayload(bytecode.m_setter, regT2);
    loadGlobalObject(regT3);
    callOperation(operationPutGetterSetter, regT3, regT0, TrustedImmPtr(m_unlinkedCodeBlock->identifier(bytecode.m_property).impl()), attribute, regT1, regT2);
}

void JIT::emit_op_put_getter_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();

    using SlowOperation = decltype(operationPutGetterByVal);
    constexpr GPRReg globalObjectGRP = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg baseGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 2>();
    // Attributes in argument 3
    constexpr GPRReg setterGPR = preferredArgumentGPR<SlowOperation, 4>();

    emitGetVirtualRegisterPayload(bytecode.m_base, baseGPR);
    emitGetVirtualRegister(bytecode.m_property, propertyJSR);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegisterPayload(bytecode.m_accessor, setterGPR);
    loadGlobalObject(globalObjectGRP);
    callOperation(operationPutGetterByVal, globalObjectGRP, baseGPR, propertyJSR, attributes, setterGPR);
}

void JIT::emit_op_put_setter_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();

    using SlowOperation = decltype(operationPutSetterByVal);
    constexpr GPRReg globalObjectGRP = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg baseGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 2>();
    // Attributes in argument 3
    constexpr GPRReg setterGPR = preferredArgumentGPR<SlowOperation, 4>();

    emitGetVirtualRegisterPayload(bytecode.m_base, baseGPR);
    emitGetVirtualRegister(bytecode.m_property, propertyJSR);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegisterPayload(bytecode.m_accessor, setterGPR);
    loadGlobalObject(globalObjectGRP);
    callOperation(operationPutSetterByVal, globalObjectGRP, baseGPR, propertyJSR, attributes, setterGPR);
}

void JIT::emit_op_del_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    ECMAMode ecmaMode = bytecode.m_ecmaMode;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::resultJSR;
    using BaselineJITRegisters::DelById::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITDelByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), ecmaMode.isStrict() ? AccessType::DeleteByIdStrict : AccessType::DeleteByIdSloppy, RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, resultJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_delByIds.append(gen);

    setFastPathResumePoint();
    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);

    // IC can write new Structure without write-barrier if a base is cell.
    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITDelByIdGenerator& gen = m_delByIds[m_delByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_del_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::resultJSR;
    using BaselineJITRegisters::DelByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitJumpSlowCaseIfNotJSCell(propertyJSR, property);

    JITDelByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex),
        bytecode.m_ecmaMode.isStrict() ? AccessType::DeleteByValStrict : AccessType::DeleteByValSloppy,
        RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_delByVals.append(gen);

    setFastPathResumePoint();
    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);

    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITDelByValGenerator& gen = m_delByVals[m_delByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_try_get_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::TryGetById, CacheType::GetByIdPrototype);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_try_get_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_get_by_id_direct(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetByIdDirect, CacheType::GetByIdSelf);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id_direct(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_get_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    GetByIdModeMetadata modeMetadata = bytecode.metadata(m_profiledCodeBlock).m_modeMetadata;

    CacheType cacheType = CacheType::GetByIdSelf;
    if (modeMetadata.mode == GetByIdMode::ProtoLoad)
        cacheType = CacheType::GetByIdPrototype;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById, cacheType);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emit_op_get_length(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetLength>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &vm().propertyNames->length;

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;

    emitGetVirtualRegister(baseVReg, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    if (shouldEmitProfiling())
        emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratch1GPR);

    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromImmortalIdentifier(ident->impl()), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById, CacheType::ArrayLength);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emitSlow_op_get_length(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_get_by_id_with_this(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister thisVReg = bytecode.m_thisValue;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::thisJSR;
    using BaselineJITRegisters::GetByIdWithThis::resultJSR;
    using BaselineJITRegisters::GetByIdWithThis::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(thisVReg, thisJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);
    emitJumpSlowCaseIfNotJSCell(thisJSR, thisVReg);

    JITGetByIdWithThisGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), resultJSR, baseJSR, thisJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIdsWithThis.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id_with_this(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_put_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister valueVReg = bytecode.m_value;
    bool direct = bytecode.m_flags.isDirect();
    ECMAMode ecmaMode = this->ecmaMode(bytecode);
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(valueVReg, valueJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITPutByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, valueJSR, stubInfoGPR, scratch1GPR, direct ? (ecmaMode.isStrict() ? AccessType::PutByIdDirectStrict : AccessType::PutByIdDirectSloppy) : (ecmaMode.isStrict() ? AccessType::PutByIdStrict : AccessType::PutByIdSloppy));

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_putByIds.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(baseVReg, ShouldFilterBase);
}

void JIT::emitSlow_op_put_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_in_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::InById::baseJSR;
    using BaselineJITRegisters::InById::resultJSR;
    using BaselineJITRegisters::InById::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITInByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_inByIds.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_in_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_in_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::InByVal::baseJSR;
    using BaselineJITRegisters::InByVal::propertyJSR;
    using BaselineJITRegisters::InByVal::resultJSR;
    using BaselineJITRegisters::InByVal::stubInfoGPR;
    using BaselineJITRegisters::InByVal::profileGPR;
    using BaselineJITRegisters::InByVal::scratch1GPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpInByVal::Metadata::offsetOfArrayProfile(), profileGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCellAndProfile(baseJSR.payloadGPR(), profileGPR, scratch1GPR);

    JITInByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::InByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, profileGPR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_inByVals.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_in_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emitHasPrivate(VirtualRegister dst, VirtualRegister base, VirtualRegister propertyOrBrand, AccessType type)
{
    using BaselineJITRegisters::InByVal::baseJSR;
    using BaselineJITRegisters::InByVal::propertyJSR;
    using BaselineJITRegisters::InByVal::resultJSR;
    using BaselineJITRegisters::InByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(propertyOrBrand, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITInByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), type, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, InvalidGPRReg, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    addSlowCase();
    m_inByVals.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitHasPrivateSlow(AccessType type, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT_UNUSED(type, type == AccessType::HasPrivateName || type == AccessType::HasPrivateBrand);
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_has_private_name(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateName>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_property, AccessType::HasPrivateName);
}

void JIT::emitSlow_op_has_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    emitHasPrivateSlow(AccessType::HasPrivateName, iter);
}

void JIT::emit_op_has_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateBrand>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_brand, AccessType::HasPrivateBrand);
}

void JIT::emitSlow_op_has_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    emitHasPrivateSlow(AccessType::HasPrivateBrand, iter);
}

void JIT::emit_op_resolve_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    using BaselineJITRegisters::ResolveScope::metadataGPR;
    using BaselineJITRegisters::ResolveScope::scopeGPR;
    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR;
    using BaselineJITRegisters::ResolveScope::scratch1GPR;

    // If we profile certain resolve types, we're guaranteed all linked code will have the same
    // resolve type.

    if (profiledResolveType == ModuleVar)
        loadPtrFromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLexicalEnvironment(), returnValueGPR);
    else if (profiledResolveType == ClosureVar) {
        emitGetVirtualRegisterPayload(scope, scopeGPR);
        static_assert(scopeGPR == returnValueGPR);
        unsigned localScopeDepth = bytecode.metadata(m_profiledCodeBlock).m_localScopeDepth;
        if (localScopeDepth < 8) {
            for (unsigned index = 0; index < localScopeDepth; ++index)
                loadPtr(Address(returnValueGPR, JSScope::offsetOfNext()), returnValueGPR);
        } else {
            ASSERT(localScopeDepth >= 8);
            load32FromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLocalScopeDepth(), scratch1GPR);
            auto loop = label();
            loadPtr(Address(returnValueGPR, JSScope::offsetOfNext()), returnValueGPR);
            branchSub32(NonZero, scratch1GPR, TrustedImm32(1), scratch1GPR).linkTo(loop, this);
        }
    } else {
        // Inlined fast path for common types.
        uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
        addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, metadataGPR);

        using Metadata = OpResolveScope::Metadata;
        switch (profiledResolveType) {
        case GlobalProperty: {
            addSlowCase(branch32(NotEqual, Address(metadataGPR, Metadata::offsetOfResolveType()), TrustedImm32(profiledResolveType)));
            loadGlobalObject(returnValueGPR);
            load32(Address(metadataGPR, Metadata::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR);
            addSlowCase(branch32(NotEqual, Address(returnValueGPR, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR));
            break;
        }
        case GlobalVar: {
            addSlowCase(branch32(NotEqual, Address(metadataGPR, Metadata::offsetOfResolveType()), TrustedImm32(profiledResolveType)));
            loadGlobalObject(returnValueGPR);
            break;
        }
        case GlobalLexicalVar: {
            addSlowCase(branch32(NotEqual, Address(metadataGPR, Metadata::offsetOfResolveType()), TrustedImm32(profiledResolveType)));
            loadGlobalObject(returnValueGPR);
            loadPtr(Address(returnValueGPR, JSGlobalObject::offsetOfGlobalLexicalEnvironment()), returnValueGPR);
            break;
        }
        default: {
            MacroAssemblerCodeRef<JITThunkPtrTag> code;
            if (profiledResolveType == ClosureVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<ClosureVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalPropertyWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalPropertyWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
            else
                code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);

            emitGetVirtualRegisterPayload(scope, scopeGPR);
            move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
            nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
            break;
        }
        }
    }

    setFastPathResumePoint();
    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
}

void JIT::emitSlow_op_resolve_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpResolveScope>();
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    using BaselineJITRegisters::ResolveScope::metadataGPR;
    using BaselineJITRegisters::ResolveScope::scopeGPR;
    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR;

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<ClosureVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalVar)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);
    else if (profiledResolveType == GlobalProperty)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalProperty>);
    else if (profiledResolveType == GlobalLexicalVar)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVar>);
    else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalPropertyWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalPropertyWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
    else
        code = vm().getCTIStub(generateOpResolveScopeThunk<GlobalVar>);

    emitGetVirtualRegisterPayload(scope, scopeGPR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
}

template <ResolveType profiledResolveType>
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::generateOpResolveScopeThunk(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().

    CCallHelpers jit;

    using Metadata = OpResolveScope::Metadata;
    using BaselineJITRegisters::ResolveScope::metadataGPR; // Incoming
    using BaselineJITRegisters::ResolveScope::scopeGPR; // Incoming
    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR; // Incoming - pass through to slow path.
    using BaselineJITRegisters::ResolveScope::scratch1GPR;
    UNUSED_PARAM(bytecodeOffsetGPR);
    static_assert(scopeGPR == returnValueGPR); // emitResolveClosure assumes this

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks, GPRReg globalObjectGPR = InvalidGPRReg) {
        if (!needsVarInjectionChecks)
            return;
        if (globalObjectGPR == InvalidGPRReg) {
            globalObjectGPR = scratch1GPR;
            loadGlobalObject(jit, globalObjectGPR);
        }
        jit.loadPtr(Address(globalObjectGPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratch1GPR);
        slowCase.append(jit.branch8(Equal, Address(scratch1GPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitResolveClosure = [&] (bool needsVarInjectionChecks) {
        doVarInjectionCheck(needsVarInjectionChecks);
        jit.load32(Address(metadataGPR, Metadata::offsetOfLocalScopeDepth()), scratch1GPR);
        RELEASE_ASSERT(scopeGPR == returnValueGPR);

        Label loop = jit.label();
        Jump done = jit.branchTest32(Zero, scratch1GPR);
        jit.loadPtr(Address(returnValueGPR, JSScope::offsetOfNext()), returnValueGPR);
        jit.sub32(TrustedImm32(1), scratch1GPR);
        jit.jump().linkTo(loop, &jit);
        done.link(&jit);
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject().
            loadGlobalObject(jit, returnValueGPR);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), returnValueGPR);
            jit.load32(Address(metadataGPR, Metadata::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR);
            slowCase.append(jit.branch32(NotEqual, Address(returnValueGPR, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), scratch1GPR));
            break;
        }

        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject() for GlobalVar*,
            // and codeBlock->globalObject()->globalLexicalEnvironment() for GlobalLexicalVar*.
            loadGlobalObject(jit, returnValueGPR);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), returnValueGPR);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                jit.loadPtr(Address(returnValueGPR, JSGlobalObject::offsetOfGlobalLexicalEnvironment()), returnValueGPR);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(needsVarInjectionChecks(resolveType));
            break;
        case Dynamic:
            slowCase.append(jit.jump());
            break;
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (profiledResolveType == ClosureVar)
        emitCode(ClosureVar);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(ClosureVarWithVarInjectionChecks);
    else {
        JumpList skipToEnd;
        jit.load32(Address(metadataGPR, Metadata::offsetOfResolveType()), regT1);

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, regT1, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jit.jump());
            notCase.link(&jit);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (resolveType != profiledResolveType)
                emitCaseWithoutCheck(resolveType);
        };

        // Check that we're the profiled resolve type first.
        switch (profiledResolveType) {
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);
        slowCase.append(jit.jump());

        skipToEnd.link(&jit);
    }

    jit.ret();

    slowCase.linkThunk(CodeLocationLabel { vm.getCTIStub(slow_op_resolve_scopeGenerator).retaggedCode<NoPtrTag>() }, &jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "resolve_scope"_s, "Baseline: resolve_scope");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_resolve_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR; // Incoming

    constexpr GPRReg scratch1GPR = regT2;
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;
    static_assert(noOverlap(bytecodeOffsetGPR, scratch1GPR , globalObjectGPR, instructionGPR));

    jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true); // Return address tagged in 'generateOpResolveScopeThunk'

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), scratch1GPR);
    jit.loadPtr(Address(scratch1GPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(scratch1GPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(bytecodeOffsetGPR, instructionGPR);
    jit.setupArguments<decltype(operationResolveScopeForBaseline)>(globalObjectGPR, instructionGPR);
    jit.callOperation<OperationPtrTag>(operationResolveScopeForBaseline);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    jit.jumpThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::CheckException).retaggedCode<NoPtrTag>()));

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "slow_op_resolve_scope"_s, "Baseline: slow_op_resolve_scope");
}

void JIT::emit_op_get_from_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    using Metadata = OpGetFromScope::Metadata;

    using BaselineJITRegisters::GetFromScope::metadataGPR;
    using BaselineJITRegisters::GetFromScope::scopeGPR;
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetFromScope::scratch1GPR;

    if (profiledResolveType == ClosureVar) {
        emitGetVirtualRegisterPayload(scope, scopeGPR);
        loadPtrFromMetadata(bytecode, Metadata::offsetOfOperand(), scratch1GPR);
        loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, JSLexicalEnvironment::offsetOfVariables()), returnValueJSR);
    } else {
        // Inlined fast path for common types.
        uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
        addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, metadataGPR);
        load32(Address(metadataGPR, Metadata::offsetOfGetPutInfo()), scratch1GPR);
        and32(TrustedImm32(GetPutInfo::typeBits), scratch1GPR); // Load ResolveType into scratch1GPR

        switch (profiledResolveType) {
        case GlobalProperty: {
            addSlowCase(branch32(NotEqual, scratch1GPR, TrustedImm32(profiledResolveType)));
            loadPtr(Address(metadataGPR, Metadata::offsetOfStructure()), scratch1GPR);
            addSlowCase(branchTestPtr(Zero, scratch1GPR));
            emitEncodeStructureID(scratch1GPR, scratch1GPR);
            emitGetVirtualRegisterPayload(scope, scopeGPR);
            addSlowCase(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratch1GPR));
            loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);
            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scopeGPR);
            negPtr(scratch1GPR);
            loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), returnValueJSR);
            break;
        }
        case GlobalVar: {
            addSlowCase(branch32(NotEqual, scratch1GPR, TrustedImm32(profiledResolveType)));
            loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);
            loadValue(Address(scratch1GPR), returnValueJSR);
            break;
        }
        case GlobalLexicalVar: {
            addSlowCase(branch32(NotEqual, scratch1GPR, TrustedImm32(profiledResolveType)));
            loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);
            loadValue(Address(scratch1GPR), returnValueJSR);
            addSlowCase(branchIfEmpty(returnValueJSR));
            break;
        }
        default: {
            MacroAssemblerCodeRef<JITThunkPtrTag> code;
            if (profiledResolveType == ClosureVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<ClosureVarWithVarInjectionChecks>);
            if (profiledResolveType == GlobalProperty)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalProperty>);
            if (profiledResolveType == GlobalVar)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);
            if (profiledResolveType == GlobalLexicalVar)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVar>);
            else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVarWithVarInjectionChecks>);
            else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
            else
                code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);

            emitGetVirtualRegisterPayload(scope, scopeGPR);
            move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
            nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
            break;
        }
        }
    }

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
}

void JIT::emitSlow_op_get_from_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();

    using BaselineJITRegisters::GetFromScope::metadataGPR;
    using BaselineJITRegisters::GetFromScope::scopeGPR;
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetFromScope::scratch1GPR;

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<ClosureVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalVar)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);
    else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalProperty)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalProperty>);
    else if (profiledResolveType == GlobalLexicalVar)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVar>);
    else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
    else
        code = vm().getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);

    uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);
    emitGetVirtualRegisterPayload(scope, scopeGPR);
    addPtr(TrustedImm32(metadataOffset), GPRInfo::metadataTableRegister, metadataGPR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    nearCallThunk(CodeLocationLabel { code.retaggedCode<NoPtrTag>() });
}

template <ResolveType profiledResolveType>
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::generateOpGetFromScopeThunk(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    using Metadata = OpGetFromScope::Metadata;

    using BaselineJITRegisters::GetFromScope::metadataGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::scopeGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR; // Incoming - pass through to slow path.
    using BaselineJITRegisters::GetFromScope::scratch1GPR;
    UNUSED_PARAM(bytecodeOffsetGPR);

    CCallHelpers jit;

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks) {
        if (!needsVarInjectionChecks)
            return;
        loadGlobalObject(jit, scratch1GPR);
        jit.loadPtr(Address(scratch1GPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratch1GPR);
        slowCase.append(jit.branch8(Equal, Address(scratch1GPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            jit.loadPtr(Address(metadataGPR, OpGetFromScope::Metadata::offsetOfStructure()), scratch1GPR);
            slowCase.append(jit.branchTestPtr(Zero, scratch1GPR));
            jit.emitEncodeStructureID(scratch1GPR, scratch1GPR);
            slowCase.append(jit.branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratch1GPR));

            jit.jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(jit, scratch1GPR);
                return jit.branchPtr(Equal, scopeGPR, scratch1GPR);
            }));

            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);

            if (ASSERT_ENABLED) {
                Jump isOutOfLine = jit.branch32(GreaterThanOrEqual, scratch1GPR, TrustedImm32(firstOutOfLineOffset));
                jit.abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(&jit);
            }

            jit.loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scopeGPR);
            jit.negPtr(scratch1GPR);
            jit.loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), returnValueJSR);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratch1GPR);
            jit.loadValue(Address(scratch1GPR), returnValueJSR);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                slowCase.append(jit.branchIfEmpty(returnValueJSR));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR,  Metadata::offsetOfOperand()), scratch1GPR);
            jit.loadValue(BaseIndex(scopeGPR, scratch1GPR, TimesEight, JSLexicalEnvironment::offsetOfVariables()), returnValueJSR);
            break;
        case Dynamic:
            slowCase.append(jit.jump());
            break;
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (profiledResolveType == ClosureVar || profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(profiledResolveType);
    else {
        JumpList skipToEnd;
        jit.load32(Address(metadataGPR, Metadata::offsetOfGetPutInfo()), scratch1GPR);
        jit.and32(TrustedImm32(GetPutInfo::typeBits), scratch1GPR); // Load ResolveType into scratch1GPR

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, scratch1GPR, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jit.jump());
            notCase.link(&jit);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (profiledResolveType != resolveType)
                emitCaseWithoutCheck(resolveType);
        };

        switch (profiledResolveType) {
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);

        slowCase.append(jit.jump());
        skipToEnd.link(&jit);
    }

    jit.ret();

    slowCase.linkThunk(CodeLocationLabel { vm.getCTIStub(slow_op_get_from_scopeGenerator).retaggedCode<NoPtrTag>() }, &jit);

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_from_scope"_s, "Baseline: get_from_scope");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_from_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::GetFromScope::metadataGPR; // Incoming
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR; // Incoming
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;
    static_assert(noOverlap(metadataGPR, bytecodeOffsetGPR, globalObjectGPR, instructionGPR));
    static_assert(noOverlap(metadataGPR, returnValueGPR));

    jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true); // Return address tagged in 'generateOpGetFromScopeThunk'

    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), instructionGPR);
    jit.loadPtr(Address(instructionGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(instructionGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(bytecodeOffsetGPR, instructionGPR);

    // save metadataGPR (arguments to call below are in registers on all platforms, so ok to stack this).
    // Note: we will do a call, so can't use pushToSave, as it does not maintain ABI stack alignment.
    jit.subPtr(TrustedImmPtr(16), stackPointerRegister);
    jit.storePtr(metadataGPR, Address(stackPointerRegister));

    jit.callOperation<OperationPtrTag>(operationGetFromScope);
    Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

    jit.loadPtr(Address(stackPointerRegister), metadataGPR); // Restore metadataGPR
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    jit.emitCTIThunkEpilogue();
    jit.ret();

    exceptionCheck.link(&jit);
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    jit.jumpThunk(CodeLocationLabel { vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator).retaggedCode<NoPtrTag>() });

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "slow_op_get_from_scope"_s, "Baseline: slow_op_get_from_scope");
}

void JIT::emit_op_put_to_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToScope>();
    VirtualRegister scope = bytecode.m_scope;
    VirtualRegister value = bytecode.m_value;

    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject.
            // Additionally, resolve_scope handles checking for the var injection.
            constexpr JSValueRegs valueJSR = jsRegT10;
            constexpr GPRReg scopeGPR = regT2;
            constexpr GPRReg scratch1GPR1 = regT3;
            constexpr GPRReg scratch1GPR2 = regT4;
            static_assert(noOverlap(valueJSR, scopeGPR, scratch1GPR1, scratch1GPR2));
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfStructure(), scratch1GPR1);
            emitGetVirtualRegisterPayload(scope, scopeGPR);
            addSlowCase(branchTestPtr(Zero, scratch1GPR1));
            emitEncodeStructureID(scratch1GPR1, scratch1GPR1);
            addSlowCase(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratch1GPR1));

            emitGetVirtualRegister(value, valueJSR);

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(scratch1GPR2);
                return branchPtr(Equal, scopeGPR, scratch1GPR2);
            }));

            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scratch1GPR2);
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), scratch1GPR1);
            negPtr(scratch1GPR1);
            storeValue(valueJSR, BaseIndex(scratch1GPR2, scratch1GPR1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)));
            emitWriteBarrier(scope, value, ShouldFilterValue);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            static_assert(noOverlap(jsRegT10, regT2, regT3));
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT2);
            emitVarReadOnlyCheck(resolveType, regT2);

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), regT2);

            if (!isInitialization(bytecode.m_getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                loadValue(Address(regT2), jsRegT10);
                addSlowCase(branchIfEmpty(jsRegT10));
            }

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfWatchpointSet(), regT3);
            emitNotifyWriteWatchpoint(regT3);

            emitGetVirtualRegister(value, jsRegT10);
            storeValue(jsRegT10, Address(regT2));

            emitWriteBarrier(scope, value, ShouldFilterValue);
            break;
        }
        case ResolvedClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            static_assert(noOverlap(jsRegT10, regT2, regT3));
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT3);

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfWatchpointSet(), regT3);
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), regT2);
            emitNotifyWriteWatchpoint(regT3);
            emitGetVirtualRegister(value, jsRegT10);
            emitGetVirtualRegisterPayload(scope, regT3);
            storeValue(jsRegT10, BaseIndex(regT3, regT2, TimesEight, JSLexicalEnvironment::offsetOfVariables()));

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

    // If any linked CodeBlock sees ClosureVar/ ClosureVarWithVarInjectionChecks, then we can compile things
    // that way for all CodeBlocks, since we've proven that is the type we will be. If we're a ClosureVar,
    // all CodeBlocks will be ClosureVar. If we're ClosureVarWithVarInjectionChecks, we're always ClosureVar
    // if the var injection watchpoint isn't fired. If it is fired, then we take the slow path, so it doesn't
    // matter what type we are dynamically.
    if (profiledResolveType == ClosureVar)
        emitCode(ClosureVar);
    else if (profiledResolveType == ResolvedClosureVar)
        emitCode(ResolvedClosureVar);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        emitCode(ClosureVarWithVarInjectionChecks);
    else {
        JumpList skipToEnd;
        load32FromMetadata(bytecode, OpPutToScope::Metadata::offsetOfGetPutInfo(), regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = branch32(NotEqual, regT0, TrustedImm32(resolveType));
            emitCode(resolveType);
            skipToEnd.append(jump());
            notCase.link(this);
        };

        auto emitCase = [&] (ResolveType resolveType) {
            if (profiledResolveType != resolveType)
                emitCaseWithoutCheck(resolveType);
        };

        switch (profiledResolveType) {
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            break;
        default:
            emitCaseWithoutCheck(profiledResolveType);
            break;
        }

        emitCase(GlobalVar);
        emitCase(GlobalProperty);
        emitCase(GlobalLexicalVar);
        emitCase(GlobalVarWithVarInjectionChecks);
        emitCase(GlobalPropertyWithVarInjectionChecks);
        emitCase(GlobalLexicalVarWithVarInjectionChecks);

        addSlowCase(jump());
        skipToEnd.link(this);
    }
}

void JIT::emitSlow_op_put_to_scope(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutToScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    if (profiledResolveType == ModuleVar) {
        // If any linked CodeBlock saw a ModuleVar, then all linked CodeBlocks are guaranteed
        // to also see ModuleVar.
        JITSlowPathCall slowPathCall(this, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else {
        uint32_t bytecodeOffset = m_bytecodeIndex.offset();
        ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
        ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

        using BaselineJITRegisters::PutToScope::bytecodeOffsetGPR;

        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
        nearCallThunk(CodeLocationLabel { vm().getCTIStub(slow_op_put_to_scopeGenerator).retaggedCode<NoPtrTag>() });
    }
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_to_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;
    using BaselineJITRegisters::PutToScope::bytecodeOffsetGPR; // Incoming
    constexpr GPRReg codeBlockGPR = argumentGPR3; // Only used as scratch register
    static_assert(noOverlap(globalObjectGPR, instructionGPR, bytecodeOffsetGPR, codeBlockGPR));

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(bytecodeOffsetGPR, instructionGPR);
    jit.setupArguments<decltype(operationPutToScope)>(globalObjectGPR, instructionGPR);
    jit.callOperation<OperationPtrTag>(operationPutToScope);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    jit.jumpThunk(CodeLocationLabel(vm.getCTIStub(CommonJITThunkID::CheckException).retaggedCode<NoPtrTag>()));

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "slow_op_put_to_scope"_s, "Baseline: slow_op_put_to_scope");
}

void JIT::emit_op_get_from_arguments(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;

    emitGetVirtualRegisterPayload(arguments, regT0);
    loadValue(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)), jsRegT10);
    emitValueProfilingSite(bytecode, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_put_to_arguments(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutToArguments>();
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    VirtualRegister value = bytecode.m_value;

    static_assert(noOverlap(regT2, jsRegT10));
    emitGetVirtualRegisterPayload(arguments, regT2);
    emitGetVirtualRegister(value, jsRegT10);
    storeValue(jsRegT10, Address(regT2, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)));

    emitWriteBarrier(arguments, value, ShouldFilterValue);
}

void JIT::emit_op_get_internal_field(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetInternalField>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    unsigned index = bytecode.m_index;

    emitGetVirtualRegisterPayload(base, regT0);
    loadValue(Address(regT0, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)), jsRegT10);

    emitValueProfilingSite(bytecode, jsRegT10);
    emitPutVirtualRegister(dst, jsRegT10);
}

void JIT::emit_op_put_internal_field(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutInternalField>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister value = bytecode.m_value;
    unsigned index = bytecode.m_index;

    static_assert(noOverlap(regT2, jsRegT10));
    emitGetVirtualRegisterPayload(base, regT2);
    emitGetVirtualRegister(value, jsRegT10);
    storeValue(jsRegT10, Address(regT2, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)));
    emitWriteBarrier(base, value, ShouldFilterValue);
}

#if USE(JSVALUE64)

void JIT::emit_op_get_by_val_with_this(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByValWithThis>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister thisValue = bytecode.m_thisValue;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::GetByValWithThis::baseJSR;
    using BaselineJITRegisters::GetByValWithThis::propertyJSR;
    using BaselineJITRegisters::GetByValWithThis::thisJSR;
    using BaselineJITRegisters::GetByValWithThis::resultJSR;
    using BaselineJITRegisters::GetByValWithThis::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::profileGPR;
    using BaselineJITRegisters::GetByValWithThis::scratch1GPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(thisValue, thisJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpGetByValWithThis::Metadata::offsetOfArrayProfile(), profileGPR);

    JITGetByValWithThisGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByValWithThis, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, thisJSR, resultJSR, profileGPR, stubInfoGPR);
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCellAndProfile(baseJSR.payloadGPR(), profileGPR, scratch1GPR);

    gen.generateDataICFastPath(*this);

    addSlowCase();
    m_getByValsWithThis.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_get_by_val_with_this(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    ASSERT(BytecodeIndex(m_bytecodeIndex.offset()) == m_bytecodeIndex);
    JITGetByValWithThisGenerator& gen = m_getByValsWithThis[m_getByValWithThisIndex++];
    linkAllSlowCases(iter);
    gen.reportBaselineDataICSlowPathBegin(label());
    nearCallThunk(CodeLocationLabel { InlineCacheCompiler::generateSlowPathCode(vm(), gen.accessType()).retaggedCode<NoPtrTag>() });
}

void JIT::emit_op_get_property_enumerator(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetPropertyEnumerator>();

    VirtualRegister base = bytecode.m_base;
    VirtualRegister dst = bytecode.m_dst;

    JumpList doneCases;
    JumpList genericCases;

    emitGetVirtualRegister(base, regT0);
    genericCases.append(branchIfNotCell(regT0));
    load8(Address(regT0, JSCell::indexingTypeAndMiscOffset()), regT1);
    and32(TrustedImm32(IndexingTypeMask), regT1);
    genericCases.append(branch32(Above, regT1, TrustedImm32(ArrayWithUndecided)));

    emitLoadStructure(vm(), regT0, regT1);
    loadPtr(Address(regT1, Structure::previousOrRareDataOffset()), regT1);
    genericCases.append(branchTestPtr(Zero, regT1));
    genericCases.append(branchIfStructure(regT1));
    loadPtr(Address(regT1, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag()), regT1);

    genericCases.append(branchTestPtr(Zero, regT1));
    genericCases.append(branchTestPtr(NonZero, regT1, TrustedImm32(StructureRareData::cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag)));
    emitPutVirtualRegister(dst, regT1);
    doneCases.append(jump());

    genericCases.link(this);
    JITSlowPathCall slowPathCall(this, slow_path_get_property_enumerator);
    slowPathCall.call();

    doneCases.link(this);
}

void JIT::emit_op_enumerator_next(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorNext>();

    VirtualRegister base = bytecode.m_base;
    VirtualRegister mode = bytecode.m_mode;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister propertyName = bytecode.m_propertyName;
    VirtualRegister enumerator = bytecode.m_enumerator;

    JumpList done;
    JumpList operationCases;

    GPRReg modeGPR = regT0;
    GPRReg indexGPR = regT1;
    GPRReg baseGPR = regT2;

    // This is the most common mode set we tend to see, so special case it if we profile it in the LLInt.
    if (bytecode.metadata(m_profiledCodeBlock).m_enumeratorMetadata == JSPropertyNameEnumerator::OwnStructureMode) {
        GPRReg enumeratorGPR = regT3;
        GPRReg scratch1GPR = regT4;
        emitGetVirtualRegister(enumerator, enumeratorGPR);
        operationCases.append(branchTest32(NonZero, Address(enumeratorGPR, JSPropertyNameEnumerator::flagsOffset()), TrustedImm32((~JSPropertyNameEnumerator::OwnStructureMode) & JSPropertyNameEnumerator::enumerationModeMask)));
        emitGetVirtualRegister(base, baseGPR);

        load8FromMetadata(bytecode, OpEnumeratorNext::Metadata::offsetOfEnumeratorMetadata(), scratch1GPR);
        or32(TrustedImm32(JSPropertyNameEnumerator::OwnStructureMode), scratch1GPR);
        store8ToMetadata(scratch1GPR, bytecode, OpEnumeratorNext::Metadata::offsetOfEnumeratorMetadata());

        load32(Address(enumeratorGPR, JSPropertyNameEnumerator::cachedStructureIDOffset()), indexGPR);
        operationCases.append(branch32(NotEqual, indexGPR, Address(baseGPR, JSCell::structureIDOffset())));

        emitGetVirtualRegister(mode, modeGPR);
        emitGetVirtualRegister(index, indexGPR);
        Jump notInit = branchTest32(Zero, modeGPR);
        // Need to use add64 since this is a JSValue int32.
        add64(TrustedImm32(1), indexGPR);
        emitPutVirtualRegister(index, indexGPR);
        notInit.link(this);
        storeTrustedValue(jsNumber(static_cast<uint8_t>(JSPropertyNameEnumerator::OwnStructureMode)), addressFor(mode));

        Jump outOfBounds = branch32(AboveOrEqual, indexGPR, Address(enumeratorGPR, JSPropertyNameEnumerator::endStructurePropertyIndexOffset()));
        loadPtr(Address(enumeratorGPR, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset()), enumeratorGPR);
        // We need to clear the high bits from the number encoding.
        and32(TrustedImm32(-1), indexGPR);
        loadPtr(BaseIndex(enumeratorGPR, indexGPR, ScalePtr), enumeratorGPR);

        emitPutVirtualRegister(propertyName, enumeratorGPR);
        done.append(jump());

        outOfBounds.link(this);
        storeTrustedValue(vm().smallStrings.sentinelString(), addressFor(propertyName));
        done.append(jump());
    }

    operationCases.link(this);

    JITSlowPathCall slowPathCall(this, slow_path_enumerator_next);
    slowPathCall.call();

    done.link(this);
}

template <typename Bytecode, typename SlowPathFunctionType>
void JIT::emit_enumerator_has_propertyImpl(const Bytecode& bytecode, SlowPathFunctionType generalCase)
{
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister enumerator = bytecode.m_enumerator;
    VirtualRegister mode = bytecode.m_mode;

    JumpList slowCases;

    emitGetVirtualRegister(mode, regT0);
    load8FromMetadata(bytecode, Bytecode::Metadata::offsetOfEnumeratorMetadata(), regT1);
    or32(regT0, regT1);
    store8ToMetadata(regT1, bytecode, Bytecode::Metadata::offsetOfEnumeratorMetadata());

    slowCases.append(branchTest32(Zero, regT0, TrustedImm32(JSPropertyNameEnumerator::OwnStructureMode)));

    emitGetVirtualRegister(base, regT0);

    slowCases.append(branchIfNotCell(regT0));

    emitGetVirtualRegister(enumerator, regT1);
    load32(Address(regT0, JSCell::structureIDOffset()), regT0);
    slowCases.append(branch32(NotEqual, regT0, Address(regT1, JSPropertyNameEnumerator::cachedStructureIDOffset())));

    move(TrustedImm64(JSValue::encode(jsBoolean(true))), regT0);
    emitPutVirtualRegister(dst, regT0);
    Jump done = jump();

    slowCases.link(this);

    JITSlowPathCall slowPathCall(this, generalCase);
    slowPathCall.call();

    done.link(this);
}

void JIT::emit_op_enumerator_in_by_val(const JSInstruction* currentInstruction)
{
    emit_enumerator_has_propertyImpl(currentInstruction->as<OpEnumeratorInByVal>(), slow_path_enumerator_in_by_val);
}

void JIT::emit_op_enumerator_has_own_property(const JSInstruction* currentInstruction)
{
    emit_enumerator_has_propertyImpl(currentInstruction->as<OpEnumeratorHasOwnProperty>(), slow_path_enumerator_has_own_property);
}

void JIT::emit_op_enumerator_get_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorGetByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister mode = bytecode.m_mode;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister propertyName = bytecode.m_propertyName;
    VirtualRegister enumerator = bytecode.m_enumerator;

    JumpList doneCases;

    constexpr GPRReg resultGPR = BaselineJITRegisters::EnumeratorGetByVal::resultJSR.payloadGPR();
    constexpr GPRReg baseGPR = BaselineJITRegisters::EnumeratorGetByVal::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineJITRegisters::EnumeratorGetByVal::propertyJSR.payloadGPR();
    using BaselineJITRegisters::EnumeratorGetByVal::profileGPR;
    using BaselineJITRegisters::EnumeratorGetByVal::stubInfoGPR;
    using BaselineJITRegisters::EnumeratorGetByVal::scratch1GPR;
    using BaselineJITRegisters::EnumeratorGetByVal::scratch2GPR;
    using BaselineJITRegisters::EnumeratorGetByVal::scratch3GPR;

    emitGetVirtualRegister(base, baseGPR);
    emitGetVirtualRegister(mode, scratch3GPR);
    emitGetVirtualRegister(propertyName, propertyGPR);

    load8FromMetadata(bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata(), scratch2GPR);
    or32(scratch3GPR, scratch2GPR);
    store8ToMetadata(scratch2GPR, bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata());

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpEnumeratorGetByVal::Metadata::offsetOfArrayProfile(), profileGPR);

    addSlowCase(branchIfNotCell(baseGPR));
    // This is always an int32 encoded value.
    Jump isNotOwnStructureMode = branchTest32(NonZero, scratch3GPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode | JSPropertyNameEnumerator::GenericMode));

    // Check the structure
    emitGetVirtualRegister(enumerator, scratch1GPR);
    load32(Address(baseGPR, JSCell::structureIDOffset()), scratch2GPR);
    Jump structureMismatch = branch32(NotEqual, scratch2GPR, Address(scratch1GPR, JSPropertyNameEnumerator::cachedStructureIDOffset()));

    // Compute the offset.
    emitGetVirtualRegister(index, scratch2GPR);
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    Jump outOfLineAccess = branch32(AboveOrEqual, scratch2GPR, Address(scratch1GPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));
    signExtend32ToPtr(scratch2GPR, scratch2GPR);
    load64(BaseIndex(baseGPR, scratch2GPR, TimesEight, JSObject::offsetOfInlineStorage()), resultGPR);
    doneCases.append(jump());

    // Otherwise it's out of line
    outOfLineAccess.link(this);
    sub32(Address(scratch1GPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratch2GPR);
    neg32(scratch2GPR);
    signExtend32ToPtr(scratch2GPR, scratch2GPR);
    loadPtr(Address(baseGPR, JSObject::butterflyOffset()), scratch1GPR);
    constexpr intptr_t offsetOfFirstProperty = offsetInButterfly(firstOutOfLineOffset) * static_cast<intptr_t>(sizeof(EncodedJSValue));
    load64(BaseIndex(scratch1GPR, scratch2GPR, TimesEight, offsetOfFirstProperty), resultGPR);
    doneCases.append(jump());

    structureMismatch.link(this);
    store8ToMetadata(TrustedImm32(JSPropertyNameEnumerator::HasSeenOwnStructureModeStructureMismatch), bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata());

    isNotOwnStructureMode.link(this);
    Jump isNotIndexed = branchTest32(Zero, scratch3GPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode));
    // Replace the string with the index.
    emitGetVirtualRegister(index, propertyGPR);

    isNotIndexed.link(this);
    emitArrayProfilingSiteWithCellAndProfile(baseGPR, profileGPR, scratch1GPR);

    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        JSValueRegs(baseGPR), JSValueRegs(propertyGPR), JSValueRegs(resultGPR), profileGPR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByVals.append(gen);

    doneCases.link(this);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
}

void JIT::emitSlow_op_enumerator_get_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generateGetByValSlowCase(currentInstruction->as<OpEnumeratorGetByVal>(), iter);
}

void JIT::emit_op_enumerator_put_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorPutByVal>();
    VirtualRegister mode = bytecode.m_mode;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister propertyName = bytecode.m_propertyName;
    VirtualRegister value = bytecode.m_value;
    VirtualRegister enumerator = bytecode.m_enumerator;

    JumpList doneCases;

    constexpr GPRReg valueGPR = BaselineJITRegisters::EnumeratorPutByVal::valueJSR.payloadGPR();
    constexpr GPRReg baseGPR = BaselineJITRegisters::EnumeratorPutByVal::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineJITRegisters::EnumeratorPutByVal::propertyJSR.payloadGPR();
    using BaselineJITRegisters::EnumeratorPutByVal::profileGPR;
    using BaselineJITRegisters::EnumeratorPutByVal::stubInfoGPR;
    using BaselineJITRegisters::EnumeratorPutByVal::scratch1GPR;
    using BaselineJITRegisters::EnumeratorPutByVal::scratch2GPR;

    // These four registers need to be set up before jumping to SlowPath code.
    emitGetVirtualRegister(base, baseGPR);
    emitGetVirtualRegister(value, valueGPR);
    emitGetVirtualRegister(propertyName, propertyGPR);
    materializePointerIntoMetadata(bytecode, OpEnumeratorPutByVal::Metadata::offsetOfArrayProfile(), profileGPR);

    emitGetVirtualRegister(mode, scratch2GPR);

    load8FromMetadata(bytecode, OpEnumeratorPutByVal::Metadata::offsetOfEnumeratorMetadata(), scratch1GPR);
    or32(scratch2GPR, scratch1GPR);
    store8ToMetadata(scratch1GPR, bytecode, OpEnumeratorPutByVal::Metadata::offsetOfEnumeratorMetadata());

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    loadStructureStubInfo(stubInfoIndex, stubInfoGPR);

    addSlowCase(branchIfNotCell(baseGPR));
    // This is always an int32 encoded value.
    Jump isNotOwnStructureMode = branchTest32(NonZero, scratch2GPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode | JSPropertyNameEnumerator::GenericMode));

    // Check the structure
    JumpList structureMismatch;
    emitGetVirtualRegister(enumerator, scratch1GPR);
    load32(Address(baseGPR, JSCell::structureIDOffset()), scratch2GPR);
    structureMismatch.append(branch32(NotEqual, scratch2GPR, Address(scratch1GPR, JSPropertyNameEnumerator::cachedStructureIDOffset())));
    emitNonNullDecodeZeroExtendedStructureID(scratch2GPR, scratch2GPR);
    structureMismatch.append(branchTest32(NonZero, Address(scratch2GPR, Structure::bitFieldOffset()), TrustedImm32(Structure::s_isWatchingReplacementBits)));

    // Compute the offset.
    emitGetVirtualRegister(index, scratch2GPR);
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    Jump outOfLineAccess = branch32(AboveOrEqual, scratch2GPR, Address(scratch1GPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));
    signExtend32ToPtr(scratch2GPR, scratch2GPR);
    store64(valueGPR, BaseIndex(baseGPR, scratch2GPR, TimesEight, JSObject::offsetOfInlineStorage()));
    doneCases.append(jump());

    // Otherwise it's out of line
    outOfLineAccess.link(this);
    sub32(Address(scratch1GPR, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratch2GPR);
    neg32(scratch2GPR);
    signExtend32ToPtr(scratch2GPR, scratch2GPR);
    constexpr intptr_t offsetOfFirstProperty = offsetInButterfly(firstOutOfLineOffset) * static_cast<intptr_t>(sizeof(EncodedJSValue));
    loadPtr(Address(baseGPR, JSObject::butterflyOffset()), scratch1GPR);
    store64(valueGPR, BaseIndex(scratch1GPR, scratch2GPR, TimesEight, offsetOfFirstProperty));
    doneCases.append(jump());

    structureMismatch.link(this);
    store8ToMetadata(TrustedImm32(JSPropertyNameEnumerator::HasSeenOwnStructureModeStructureMismatch), bytecode, OpEnumeratorPutByVal::Metadata::offsetOfEnumeratorMetadata());

    isNotOwnStructureMode.link(this);
    Jump isNotIndexed = branchTest32(Zero, scratch2GPR, TrustedImm32(JSPropertyNameEnumerator::IndexedMode));
    // Replace the string with the index.
    emitGetVirtualRegister(index, propertyGPR);

    isNotIndexed.link(this);
    emitArrayProfilingSiteWithCellAndProfile(baseGPR, profileGPR, scratch1GPR);

    ECMAMode ecmaMode = bytecode.m_ecmaMode;
    JITPutByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), ecmaMode.isStrict() ? AccessType::PutByValStrict : AccessType::PutByValSloppy, RegisterSetBuilder::stubUnavailableRegisters(),
        JSValueRegs(baseGPR), JSValueRegs(propertyGPR), JSValueRegs(valueGPR), profileGPR, stubInfoGPR);

    gen.generateDataICFastPath(*this);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_putByVals.append(gen);

    doneCases.link(this);

    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_enumerator_put_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generatePutByValSlowCase(currentInstruction->as<OpEnumeratorPutByVal>(), iter);
}

#elif USE(JSVALUE32_64)

void JIT::emit_op_get_by_val_with_this(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_get_by_val_with_this);
    slowPathCall.call();
}

void JIT::emitSlow_op_get_by_val_with_this(const JSInstruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

void JIT::emit_op_get_property_enumerator(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_get_property_enumerator);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_next(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_enumerator_next);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_get_by_val(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_enumerator_get_by_val);
    slowPathCall.call();
}

void JIT::emitSlow_op_enumerator_get_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

void JIT::emit_op_enumerator_in_by_val(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_enumerator_in_by_val);
    slowPathCall.call();
}

void JIT::emit_op_enumerator_put_by_val(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_enumerator_put_by_val);
    slowPathCall.call();
}

void JIT::emitSlow_op_enumerator_put_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&)
{
    UNREACHABLE_FOR_PLATFORM();
}

void JIT::emit_op_enumerator_has_own_property(const JSInstruction*)
{
    JITSlowPathCall slowPathCall(this, slow_path_enumerator_has_own_property);
    slowPathCall.call();
}

#endif

void JIT::emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode mode)
{
    // value may be invalid VirtualRegister if mode is UnconditionalWriteBarrier or ShouldFilterBase.
    Jump valueNotCell;
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue) {
#if USE(JSVALUE64)
        emitGetVirtualRegister(value, regT0);
#elif USE(JSVALUE32_64)
        emitGetVirtualRegisterTag(value, regT0);
#endif
        valueNotCell = branchIfNotCell(regT0);
    }

    constexpr GPRReg arg1GPR = preferredArgumentGPR<decltype(operationWriteBarrierSlowPath), 1>();
#if USE(JSVALUE64)
    constexpr JSValueRegs tmpJSR { arg1GPR };
#elif USE(JSVALUE32_64)
    constexpr JSValueRegs tmpJSR { regT0, arg1GPR };
#endif
    static_assert(noOverlap(regT0, arg1GPR, regT2));

    emitGetVirtualRegister(owner, tmpJSR);
    Jump ownerNotCell;
    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell = branchIfNotCell(tmpJSR);

    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), tmpJSR.payloadGPR(), regT2);
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, TrustedImmPtr(&vm()), tmpJSR.payloadGPR());
    ownerIsRememberedOrInEden.link(this);

    if (mode == ShouldFilterBase || mode == ShouldFilterBaseAndValue)
        ownerNotCell.link(this);
    if (mode == ShouldFilterValue || mode == ShouldFilterBaseAndValue)
        valueNotCell.link(this);
}

void JIT::emitWriteBarrier(VirtualRegister owner, WriteBarrierMode mode)
{
    ASSERT(mode == UnconditionalWriteBarrier || mode == ShouldFilterBase);
    emitWriteBarrier(owner, VirtualRegister(), mode);
}

void JIT::emitWriteBarrier(JSCell* owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), owner, regT0);
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, TrustedImmPtr(&vm()), TrustedImmPtr(owner));
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitWriteBarrier(GPRReg owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), owner, selectScratchGPR(owner));
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, TrustedImmPtr(&vm()), owner);
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitVarInjectionCheck(bool needsVarInjectionChecks, GPRReg scratch1GPR)
{
    if (!needsVarInjectionChecks)
        return;

    loadGlobalObject(scratch1GPR);
    loadPtr(Address(scratch1GPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratch1GPR);
    addSlowCase(branch8(Equal, Address(scratch1GPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
}

} // namespace JSC

#endif // ENABLE(JIT)
