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
    using BaselineJITRegisters::GetByVal::FastPath::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::FastPath::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    if (bytecode.metadata(m_profiledCodeBlock).m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers()) {
        stubInfo->tookSlowPath = true;

        auto notCell = branchIfNotCell(baseJSR);
        emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);
        notCell.link(this);
        loadGlobalObject(scratchGPR);
        callOperationWithResult(operationGetByVal, resultJSR, scratchGPR, baseJSR, propertyJSR);

        gen.generateEmptyPath(*this);
    } else {
        emitJumpSlowCaseIfNotJSCell(baseJSR, base);
        emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    }

    addSlowCase();
    m_getByVals.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

template<typename OpcodeType>
void JIT::generateGetByValSlowCase(const OpcodeType& bytecode, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];

    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::profileGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    if (!gen.m_unlinkedStubInfo->tookSlowPath) {
        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
        materializePointerIntoMetadata(bytecode, OpcodeType::Metadata::offsetOfArrayProfile(), profileGPR);

        emitNakedNearCall(vm().getCTIStub(slow_op_get_by_val_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());
    }

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emitSlow_op_get_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generateGetByValSlowCase(currentInstruction->as<OpGetByVal>(), iter);
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_val_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValOptimize);

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::profileGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, profileGPR, baseJSR, propertyJSR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_val_callSlowOperationThenCheckException");
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
    using BaselineJITRegisters::GetByVal::FastPath::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetPrivateName,
        RegisterSetBuilder::stubUnavailableRegisters(), baseJSR, propertyJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByVals.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_get_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];

    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_private_name_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::GetByVal::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_private_name_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetPrivateNameOptimize);

    using BaselineJITRegisters::GetByVal::baseJSR;
    using BaselineJITRegisters::GetByVal::propertyJSR;
    using BaselineJITRegisters::GetByVal::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, baseJSR, propertyJSR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_private_name_callSlowOperationThenCheckException");
}

void JIT::emit_op_set_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::brandJSR;
    using BaselineJITRegisters::PrivateBrand::FastPath::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, brandJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITPrivateBrandAccessGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::SetPrivateBrand, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, brandJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
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

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];

    using BaselineJITRegisters::PrivateBrand::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::PrivateBrand::SlowPath::stubInfoGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);

    static_assert(std::is_same<FunctionTraits<decltype(operationSetPrivateBrandOptimize)>::ArgumentTypes, FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_private_name_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_check_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselineJITRegisters::PrivateBrand::baseJSR;
    using BaselineJITRegisters::PrivateBrand::brandJSR;
    using BaselineJITRegisters::PrivateBrand::FastPath::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, brandJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITPrivateBrandAccessGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::CheckPrivateBrand, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, brandJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_privateBrandAccesses.append(gen);
}

void JIT::emitSlow_op_check_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];

    using BaselineJITRegisters::PrivateBrand::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::PrivateBrand::SlowPath::stubInfoGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);

    static_assert(std::is_same<FunctionTraits<decltype(operationCheckPrivateBrandOptimize)>::ArgumentTypes, FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_private_name_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    gen.reportSlowPathCall(coldPathBegin, Call());
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
    using BaselineJITRegisters::PutByVal::profileGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);
    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfArrayProfile(), profileGPR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), /* scratchGPR: */ stubInfoGPR);

    PutKind putKind = std::is_same_v<Op, OpPutByValDirect> ? PutKind::Direct : PutKind::NotDirect;
    ECMAMode ecmaMode = this->ecmaMode(bytecode);
    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITPutByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::PutByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, profileGPR, stubInfoGPR, putKind, ecmaMode, PrivateFieldPutKind::none());
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const JSInstruction*);

void JIT::emit_op_put_by_val_direct(const JSInstruction* currentInstruction)
{
    emit_op_put_by_val<OpPutByValDirect>(currentInstruction);
}

void JIT::emitSlow_op_put_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];

    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::SlowPath::bytecodeOffsetGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_put_by_val_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_by_val_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperatoin = decltype(operationPutByValStrictOptimize);

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::profileGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::PutByVal::SlowPath::bytecodeOffsetGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArgumentsForIndirectCall<SlowOperatoin>(stubInfoGPR,
        globalObjectGPR, baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR);
    jit.call(Address(nonArgGPR0, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_by_val_callSlowOperationThenCheckException");
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

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITPutByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::PutPrivateName, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, InvalidGPRReg, stubInfoGPR, PutKind::Direct, ECMAMode::sloppy(), bytecode.m_putKind);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_put_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];

    using BaselineJITRegisters::PutByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_put_private_name_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_private_name_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationPutByValDefinePrivateFieldOptimize);

    using BaselineJITRegisters::PutByVal::baseJSR;
    using BaselineJITRegisters::PutByVal::propertyJSR;
    using BaselineJITRegisters::PutByVal::valueJSR;
    using BaselineJITRegisters::PutByVal::profileGPR;
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    using BaselineJITRegisters::PutByVal::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::PutByVal::SlowPath::bytecodeOffsetGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    // Loading nullptr to this register is necessary for setupArgumentsForIndirectCall
    // to not clobber globalObjectGPR on ARM_THUMB2, and is otherwise harmless.
    jit.move(TrustedImmPtr(nullptr), profileGPR);
    jit.setupArgumentsForIndirectCall<SlowOperation>(stubInfoGPR,
        globalObjectGPR, baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR);
    jit.call(Address(nonArgGPR0, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_private_name_callSlowOperationThenCheckException");
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
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::FastPath::resultJSR;
    using BaselineJITRegisters::DelById::FastPath::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITDelByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_delByIds.append(gen);

    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);

    // IC can write new Structure without write-barrier if a base is cell.
    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_id(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITDelByIdGenerator& gen = m_delByIds[m_delByIdIndex++];

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::DelById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::DelById::SlowPath::propertyGPR;
    using BaselineJITRegisters::DelById::SlowPath::ecmaModeGPR;

    emitGetVirtualRegister(base, baseJSR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    move(TrustedImm32(bytecode.m_ecmaMode.value()), ecmaModeGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_del_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(dst, returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_del_by_id_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByIdOptimize);

    using BaselineJITRegisters::DelById::baseJSR;
    using BaselineJITRegisters::DelById::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::DelById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::DelById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::DelById::SlowPath::propertyGPR;
    using BaselineJITRegisters::DelById::SlowPath::ecmaModeGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, baseJSR, propertyGPR, ecmaModeGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    jit.boxBoolean(returnValueGPR, returnValueJSR);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_del_by_id_callSlowOperationThenCheckException");
}

void JIT::emit_op_del_by_val(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::FastPath::resultJSR;
    using BaselineJITRegisters::DelByVal::FastPath::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitGetVirtualRegister(property, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(propertyJSR, property);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITDelByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_delByVals.append(gen);

    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);

    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_del_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    JITDelByValGenerator& gen = m_delByVals[m_delByValIndex++];

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::DelByVal::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::DelByVal::SlowPath::ecmaModeGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImm32(bytecode.m_ecmaMode.value()), ecmaModeGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_del_by_val_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(dst, returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_del_by_val_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationDeleteByValOptimize);

    using BaselineJITRegisters::DelByVal::baseJSR;
    using BaselineJITRegisters::DelByVal::propertyJSR;
    using BaselineJITRegisters::DelByVal::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::DelByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::DelByVal::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::DelByVal::SlowPath::ecmaModeGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, baseJSR, propertyJSR, ecmaModeGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    jit.boxBoolean(returnValueGPR, returnValueJSR);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_del_by_val_prepareCall");
}

void JIT::emit_op_try_get_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::TryGetById);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);

    setFastPathResumePoint();
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_try_get_by_id(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpTryGetById>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    using BaselineJITRegisters::GetById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::SlowPath::propertyGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(JIT::TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    static_assert(std::is_same<decltype(operationTryGetByIdOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_get_by_id_direct(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetByIdDirect);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id_direct(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    using BaselineJITRegisters::GetById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::SlowPath::propertyGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    static_assert(std::is_same<decltype(operationGetByIdDirectOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_get_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::FastPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::FastPath::scratchGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    if (*ident == vm().propertyNames->length && shouldEmitProfiling()) {
        load8FromMetadata(bytecode, OpGetById::Metadata::offsetOfModeMetadata() + GetByIdModeMetadata::offsetOfMode(), scratchGPR);
        Jump notArrayLengthMode = branch32(NotEqual, TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)), scratchGPR);
        emitArrayProfilingSiteWithCell(
            bytecode,
            OpGetById::Metadata::offsetOfModeMetadata() + GetByIdModeMetadataArrayLength::offsetOfArrayProfile(),
            baseJSR.payloadGPR(), scratchGPR);
        notArrayLengthMode.link(this);
    }

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpGetById>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    using BaselineJITRegisters::GetById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::SlowPath::propertyGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::GetById::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdOptimize);

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::GetById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::SlowPath::propertyGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, baseJSR, propertyGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_id_callSlowOperationThenCheckException");
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
    using BaselineJITRegisters::GetByIdWithThis::FastPath::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(thisVReg, thisJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);
    emitJumpSlowCaseIfNotJSCell(thisJSR, thisVReg);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByIdWithThisGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), resultJSR, baseJSR, thisJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIdsWithThis.append(gen);

    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id_with_this(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];

    using BaselineJITRegisters::GetByIdWithThis::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::propertyGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_id_with_this_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::GetByIdWithThis::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_id_with_this_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByIdWithThisOptimize);

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::thisJSR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByIdWithThis::SlowPath::propertyGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, baseJSR, thisJSR, propertyGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_id_with_this_callSlowOperationThenCheckException");
}

void JIT::emit_op_put_by_id(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister valueVReg = bytecode.m_value;
    bool direct = bytecode.m_flags.isDirect();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::FastPath::stubInfoGPR;
    using BaselineJITRegisters::PutById::FastPath::scratchGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(valueVReg, valueJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITPutByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, valueJSR, stubInfoGPR, scratchGPR, ecmaMode(bytecode),
        direct ? PutKind::Direct : PutKind::NotDirect);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_putByIds.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(baseVReg, ShouldFilterBase);
}

void JIT::emitSlow_op_put_by_id(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpPutById>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];

    using BaselineJITRegisters::PutById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::PutById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::PutById::SlowPath::propertyGPR;

    Label coldPathBegin(this);

    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    emitNakedNearCall(vm().getCTIStub(slow_op_put_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_by_id_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationPutByIdStrictOptimize);

    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::PutById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::PutById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::PutById::SlowPath::propertyGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, valueJSR, baseJSR, propertyGPR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_by_id_callSlowOperationThenCheckException");
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
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITInByIdGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSetBuilder::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByIds.append(gen);

    setFastPathResumePoint();
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_in_by_id(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpInById>();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));
    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    using BaselineJITRegisters::GetById::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetById::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetById::SlowPath::propertyGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);

    // slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator will do exactly what we need.
    // So, there's no point in creating a duplicate thunk just to give it a different name.
    static_assert(std::is_same<decltype(operationInByIdOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_id_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::InById::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
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
    using BaselineJITRegisters::InByVal::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITInByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::InByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByVals.append(gen);

    setFastPathResumePoint();
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_in_by_val(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    auto bytecode = currentInstruction->as<OpInByVal>();
    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];

    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::profileGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpInByVal::Metadata::offsetOfArrayProfile(), profileGPR);

    // slow_op_get_by_val_callSlowOperationThenCheckExceptionGenerator will do exactly what we need.
    // So, there's no point in creating a duplicate thunk just to give it a different name.
    static_assert(std::is_same<decltype(operationInByValOptimize), decltype(operationGetByValOptimize)>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_by_val_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::InByVal::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emitHasPrivate(VirtualRegister dst, VirtualRegister base, VirtualRegister propertyOrBrand, AccessType type)
{
    using BaselineJITRegisters::InByVal::baseJSR;
    using BaselineJITRegisters::InByVal::propertyJSR;
    using BaselineJITRegisters::InByVal::resultJSR;
    using BaselineJITRegisters::InByVal::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(propertyOrBrand, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITInByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), type, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByVals.append(gen);

    setFastPathResumePoint();
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitHasPrivateSlow(AccessType type)
{
    ASSERT_UNUSED(type, type == AccessType::HasPrivateName || type == AccessType::HasPrivateBrand);

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];

    using BaselineJITRegisters::GetByVal::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByVal::SlowPath::stubInfoGPR;

    Label coldPathBegin = label();

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);

    static_assert(std::is_same<decltype(operationHasPrivateNameOptimize), decltype(operationGetPrivateNameOptimize)>::value);
    static_assert(std::is_same<decltype(operationHasPrivateBrandOptimize), decltype(operationGetPrivateNameOptimize)>::value);
    emitNakedNearCall(vm().getCTIStub(slow_op_get_private_name_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());

    static_assert(BaselineJITRegisters::InByVal::resultJSR == returnValueJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_has_private_name(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateName>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_property, AccessType::HasPrivateName);
}

void JIT::emitSlow_op_has_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    emitHasPrivateSlow(AccessType::HasPrivateName);
}

void JIT::emit_op_has_private_brand(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateBrand>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_brand, AccessType::HasPrivateBrand);
}

void JIT::emitSlow_op_has_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    emitHasPrivateSlow(AccessType::HasPrivateBrand);
}

void JIT::emit_op_resolve_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    // If we profile certain resolve types, we're guaranteed all linked code will have the same
    // resolve type.

    if (profiledResolveType == ModuleVar)
        loadPtrFromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLexicalEnvironment(), returnValueGPR);
    else {
        uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);

        using BaselineJITRegisters::ResolveScope::metadataGPR;
        using BaselineJITRegisters::ResolveScope::scopeGPR;
        using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR;

        emitGetVirtualRegisterPayload(scope, scopeGPR);
        addPtr(TrustedImm32(metadataOffset), s_metadataGPR, metadataGPR);
        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

        MacroAssemblerCodeRef<JITThunkPtrTag> code;
        if (profiledResolveType == ClosureVar)
            code = vm().getCTIStub(generateOpResolveScopeThunk<ClosureVar>);
        else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
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

        emitNakedNearCall(code.retaggedCode<NoPtrTag>());
    }

    boxCell(returnValueGPR, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
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
    constexpr GPRReg scratchGPR = regT5; // local temporary
    UNUSED_PARAM(bytecodeOffsetGPR);
    static_assert(noOverlap(metadataGPR, scopeGPR, bytecodeOffsetGPR, scratchGPR));
    static_assert(scopeGPR == returnValueGPR); // emitResolveClosure assumes this

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks, GPRReg globalObjectGPR = InvalidGPRReg) {
        if (!needsVarInjectionChecks)
            return;
        if (globalObjectGPR == InvalidGPRReg) {
            globalObjectGPR = scratchGPR;
            loadGlobalObject(jit, globalObjectGPR);
        }
        jit.loadPtr(Address(globalObjectGPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratchGPR);
        slowCase.append(jit.branch8(Equal, Address(scratchGPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitResolveClosure = [&] (bool needsVarInjectionChecks) {
        doVarInjectionCheck(needsVarInjectionChecks);
        jit.load32(Address(metadataGPR, Metadata::offsetOfLocalScopeDepth()), scratchGPR);
        RELEASE_ASSERT(scopeGPR == returnValueGPR);

        Label loop = jit.label();
        Jump done = jit.branchTest32(Zero, scratchGPR);
        jit.loadPtr(Address(returnValueGPR, JSScope::offsetOfNext()), returnValueGPR);
        jit.sub32(TrustedImm32(1), scratchGPR);
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
            jit.load32(Address(metadataGPR, Metadata::offsetOfGlobalLexicalBindingEpoch()), scratchGPR);
            slowCase.append(jit.branch32(NotEqual, Address(returnValueGPR, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), scratchGPR));
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

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    patchBuffer.link(slowCase, CodeLocationLabel(vm.getCTIStub(slow_op_resolve_scopeGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "resolve_scope thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_resolve_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using BaselineJITRegisters::ResolveScope::bytecodeOffsetGPR; // Incoming

    constexpr GPRReg scratchGPR = regT2;
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;
    static_assert(noOverlap(bytecodeOffsetGPR, scratchGPR , globalObjectGPR, instructionGPR));

    jit.emitCTIThunkPrologue(/* returnAddressAlreadyTagged: */ true); // Return address tagged in 'generateOpResolveScopeThunk'

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), scratchGPR);
    jit.loadPtr(Address(scratchGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(scratchGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(bytecodeOffsetGPR, instructionGPR);
    jit.setupArguments<decltype(operationResolveScopeForBaseline)>(globalObjectGPR, instructionGPR);
    Call operation = jit.call(OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    patchBuffer.link<OperationPtrTag>(operation, operationResolveScopeForBaseline);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_resolve_scope");
}

void JIT::emit_op_get_from_scope(const JSInstruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    uint32_t metadataOffset = m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode);

    using BaselineJITRegisters::GetFromScope::metadataGPR;
    using BaselineJITRegisters::GetFromScope::scopeGPR;
    using BaselineJITRegisters::GetFromScope::bytecodeOffsetGPR;

    emitGetVirtualRegisterPayload(scope, scopeGPR);
    addPtr(TrustedImm32(metadataOffset), s_metadataGPR, metadataGPR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVar)
        code = vm().getCTIStub(generateOpGetFromScopeThunk<ClosureVar>);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
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

    emitNakedNearCall(code.retaggedCode<NoPtrTag>());
    emitPutVirtualRegister(dst, returnValueJSR);
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
    constexpr GPRReg scratchGPR = regT5;
    UNUSED_PARAM(bytecodeOffsetGPR);
    static_assert(noOverlap(returnValueJSR, metadataGPR, scopeGPR, bytecodeOffsetGPR, scratchGPR));

    CCallHelpers jit;

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks) {
        if (!needsVarInjectionChecks)
            return;
        loadGlobalObject(jit, scratchGPR);
        jit.loadPtr(Address(scratchGPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratchGPR);
        slowCase.append(jit.branch8(Equal, Address(scratchGPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            jit.loadPtr(Address(metadataGPR, OpGetFromScope::Metadata::offsetOfStructure()), scratchGPR);
            slowCase.append(jit.branchTestPtr(Zero, scratchGPR));
            jit.emitEncodeStructureID(scratchGPR, scratchGPR);
            slowCase.append(jit.branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratchGPR));

            jit.jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(jit, scratchGPR);
                return jit.branchPtr(Equal, scopeGPR, scratchGPR);
            }));

            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratchGPR);

            if (ASSERT_ENABLED) {
                Jump isOutOfLine = jit.branch32(GreaterThanOrEqual, scratchGPR, TrustedImm32(firstOutOfLineOffset));
                jit.abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(&jit);
            }

            jit.loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scopeGPR);
            jit.negPtr(scratchGPR);
            jit.loadValue(BaseIndex(scopeGPR, scratchGPR, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), returnValueJSR);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), scratchGPR);
            jit.loadValue(Address(scratchGPR), returnValueJSR);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                slowCase.append(jit.branchIfEmpty(returnValueJSR));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR,  Metadata::offsetOfOperand()), scratchGPR);
            jit.loadValue(BaseIndex(scopeGPR, scratchGPR, TimesEight, JSLexicalEnvironment::offsetOfVariables()), returnValueJSR);
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
        jit.load32(Address(metadataGPR, Metadata::offsetOfGetPutInfo()), scratchGPR);
        jit.and32(TrustedImm32(GetPutInfo::typeBits), scratchGPR); // Load ResolveType into scratchGPR

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, scratchGPR, TrustedImm32(resolveType));
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

    static_assert(ValueProfile::numberOfBuckets == 1);
    jit.storeValue(returnValueJSR, Address(metadataGPR, Metadata::offsetOfProfile() + ValueProfile::offsetOfFirstBucket()));
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    patchBuffer.link(slowCase, CodeLocationLabel(vm.getCTIStub(slow_op_get_from_scopeGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "get_from_scope thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_from_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using Metadata = OpGetFromScope::Metadata;

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

    Call operation = jit.call(OperationPtrTag);
    Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

    jit.loadPtr(Address(stackPointerRegister), metadataGPR); // Restore metadataGPR
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer
    jit.storeValue(returnValueJSR, Address(metadataGPR, Metadata::offsetOfProfile() + ValueProfile::offsetOfFirstBucket()));

    jit.emitCTIThunkEpilogue();
    jit.ret();

    exceptionCheck.link(&jit);
    jit.addPtr(TrustedImmPtr(16), stackPointerRegister); // Restore stack pointer

    Jump jumpToHandler = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationGetFromScope);
    auto handler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);
    patchBuffer.link(jumpToHandler, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_from_scope");
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
            constexpr GPRReg scratchGPR1 = regT3;
            constexpr GPRReg scratchGPR2 = regT4;
            static_assert(noOverlap(valueJSR, scopeGPR, scratchGPR1, scratchGPR2));
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfStructure(), scratchGPR1);
            emitGetVirtualRegisterPayload(scope, scopeGPR);
            addSlowCase(branchTestPtr(Zero, scratchGPR1));
            emitEncodeStructureID(scratchGPR1, scratchGPR1);
            addSlowCase(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), scratchGPR1));

            emitGetVirtualRegister(value, valueJSR);

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(scratchGPR2);
                return branchPtr(Equal, scopeGPR, scratchGPR2);
            }));

            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), scratchGPR2);
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), scratchGPR1);
            negPtr(scratchGPR1);
            storeValue(valueJSR, BaseIndex(scratchGPR2, scratchGPR1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)));
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
        ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
        ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

        using BaselineJITRegisters::PutToScope::bytecodeOffsetGPR;

        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
        emitNakedNearCall(vm().getCTIStub(slow_op_put_to_scopeGenerator).retaggedCode<NoPtrTag>());
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
    Call operation = jit.call(OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link<OperationPtrTag>(operation, operationPutToScope);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_to_scope");
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
    using BaselineJITRegisters::GetByValWithThis::FastPath::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::FastPath::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(thisValue, thisJSR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByValWithThisGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByValWithThis, RegisterSetBuilder::stubUnavailableRegisters(),
        baseJSR, propertyJSR, thisJSR, resultJSR, stubInfoGPR);
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);

    addSlowCase();
    m_getByValsWithThis.append(gen);

    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    setFastPathResumePoint();
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_get_by_val_with_this(const JSInstruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    auto bytecode = currentInstruction->as<OpGetByValWithThis>();

    ASSERT(hasAnySlowCases(iter));

    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    JITGetByValWithThisGenerator& gen = m_getByValsWithThis[m_getByValWithThisIndex++];

    using BaselineJITRegisters::GetByValWithThis::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::profileGPR;

    Label coldPathBegin = label();
    linkAllSlowCases(iter);

    if (!gen.m_unlinkedStubInfo->tookSlowPath) {
        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
        loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
        materializePointerIntoMetadata(bytecode, OpGetByValWithThis::Metadata::offsetOfArrayProfile(), profileGPR);

        emitNakedNearCall(vm().getCTIStub(slow_op_get_by_val_with_this_callSlowOperationThenCheckExceptionGenerator).retaggedCode<NoPtrTag>());
    }

    gen.reportSlowPathCall(coldPathBegin, Call());
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_val_with_this_callSlowOperationThenCheckExceptionGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    using SlowOperation = decltype(operationGetByValWithThisOptimize);

    using BaselineJITRegisters::GetByValWithThis::baseJSR;
    using BaselineJITRegisters::GetByValWithThis::propertyJSR;
    using BaselineJITRegisters::GetByValWithThis::thisJSR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::globalObjectGPR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::bytecodeOffsetGPR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::stubInfoGPR;
    using BaselineJITRegisters::GetByValWithThis::SlowPath::profileGPR;

    jit.emitCTIThunkPrologue();

    // Call slow operation
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.prepareCallOperation(vm);
    loadGlobalObject(jit, globalObjectGPR);
    jit.setupArguments<SlowOperation>(globalObjectGPR, stubInfoGPR, profileGPR, baseJSR, propertyJSR, thisJSR);
    static_assert(preferredArgumentGPR<SlowOperation, 1>() == argumentGPR1, "Needed for branch to slow operation via StubInfo");
    jit.call(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);

    jit.emitCTIThunkEpilogue();

    // Tail call to exception check thunk
    Jump exceptionCheck = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_val_with_this_callSlowOperationThenCheckException");
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
        GPRReg scratchGPR = regT4;
        emitGetVirtualRegister(enumerator, enumeratorGPR);
        operationCases.append(branchTest32(NonZero, Address(enumeratorGPR, JSPropertyNameEnumerator::flagsOffset()), TrustedImm32((~JSPropertyNameEnumerator::OwnStructureMode) & JSPropertyNameEnumerator::enumerationModeMask)));
        emitGetVirtualRegister(base, baseGPR);

        load8FromMetadata(bytecode, OpEnumeratorNext::Metadata::offsetOfEnumeratorMetadata(), scratchGPR);
        or32(TrustedImm32(JSPropertyNameEnumerator::OwnStructureMode), scratchGPR);
        store8ToMetadata(scratchGPR, bytecode, OpEnumeratorNext::Metadata::offsetOfEnumeratorMetadata());

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
    constexpr GPRReg stubInfoGPR = BaselineJITRegisters::EnumeratorGetByVal::stubInfoGPR;
    constexpr GPRReg scratch1 = BaselineJITRegisters::EnumeratorGetByVal::scratch1;
    constexpr GPRReg scratch2 = BaselineJITRegisters::EnumeratorGetByVal::scratch2;
    constexpr GPRReg scratch3 = BaselineJITRegisters::EnumeratorGetByVal::scratch3;

    emitGetVirtualRegister(base, baseGPR);
    emitGetVirtualRegister(mode, scratch3);
    emitGetVirtualRegister(propertyName, propertyGPR);

    load8FromMetadata(bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata(), scratch2);
    or32(scratch3, scratch2);
    store8ToMetadata(scratch2, bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata());

    addSlowCase(branchIfNotCell(baseGPR));
    // This is always an int32 encoded value.
    Jump isNotOwnStructureMode = branchTest32(NonZero, scratch3, TrustedImm32(JSPropertyNameEnumerator::IndexedMode | JSPropertyNameEnumerator::GenericMode));

    // Check the structure
    emitGetVirtualRegister(enumerator, scratch1);
    load32(Address(baseGPR, JSCell::structureIDOffset()), scratch2);
    Jump structureMismatch = branch32(NotEqual, scratch2, Address(scratch1, JSPropertyNameEnumerator::cachedStructureIDOffset()));

    // Compute the offset.
    emitGetVirtualRegister(index, scratch2);
    // If index is less than the enumerator's cached inline storage, then it's an inline access
    Jump outOfLineAccess = branch32(AboveOrEqual, scratch2, Address(scratch1, JSPropertyNameEnumerator::cachedInlineCapacityOffset()));
    signExtend32ToPtr(scratch2, scratch2);
    load64(BaseIndex(baseGPR, scratch2, TimesEight, JSObject::offsetOfInlineStorage()), resultGPR);
    doneCases.append(jump());

    // Otherwise it's out of line
    outOfLineAccess.link(this);
    loadPtr(Address(baseGPR, JSObject::butterflyOffset()), baseGPR);
    sub32(Address(scratch1, JSPropertyNameEnumerator::cachedInlineCapacityOffset()), scratch2);
    neg32(scratch2);
    signExtend32ToPtr(scratch2, scratch2);
    constexpr intptr_t offsetOfFirstProperty = offsetInButterfly(firstOutOfLineOffset) * static_cast<intptr_t>(sizeof(EncodedJSValue));
    load64(BaseIndex(baseGPR, scratch2, TimesEight, offsetOfFirstProperty), resultGPR);
    doneCases.append(jump());

    structureMismatch.link(this);
    store8ToMetadata(TrustedImm32(JSPropertyNameEnumerator::HasSeenOwnStructureModeStructureMismatch), bytecode, OpEnumeratorGetByVal::Metadata::offsetOfEnumeratorMetadata());

    isNotOwnStructureMode.link(this);
    Jump isNotIndexed = branchTest32(Zero, scratch3, TrustedImm32(JSPropertyNameEnumerator::IndexedMode));
    // Replace the string with the index.
    emitGetVirtualRegister(index, propertyGPR);

    isNotIndexed.link(this);
    emitArrayProfilingSiteWithCell(bytecode, baseGPR, scratch1);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    JITGetByValGenerator gen(
        nullptr, stubInfo, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSetBuilder::stubUnavailableRegisters(),
        JSValueRegs(baseGPR), JSValueRegs(propertyGPR), JSValueRegs(resultGPR), stubInfoGPR);
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
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

void JIT::emitVarInjectionCheck(bool needsVarInjectionChecks, GPRReg scratchGPR)
{
    if (!needsVarInjectionChecks)
        return;

    loadGlobalObject(scratchGPR);
    loadPtr(Address(scratchGPR, JSGlobalObject::offsetOfVarInjectionWatchpoint()), scratchGPR);
    addSlowCase(branch8(Equal, Address(scratchGPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
}

} // namespace JSC

#endif // ENABLE(JIT)
