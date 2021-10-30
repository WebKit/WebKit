/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

void JIT::emit_op_get_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineGetByValRegisters::baseJSR;
    using BaselineGetByValRegisters::propertyJSR;
    using BaselineGetByValRegisters::resultJSR;
    using BaselineGetByValRegisters::stubInfoGPR;
    using BaselineGetByValRegisters::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    if (bytecode.metadata(m_profiledCodeBlock).m_seenIdentifiers.count() > Options::getByValICMaxNumberOfIdentifiers()) {
        auto notCell = branchIfNotCell(baseJSR);
        emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);
        notCell.link(this);
        loadGlobalObject(scratchGPR);
        callOperationWithProfile(bytecode, operationGetByVal, dst, scratchGPR, baseJSR, propertyJSR);
    } else {
        emitJumpSlowCaseIfNotJSCell(baseJSR, base);
        emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);

        JITGetByValGenerator gen(
            nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSet::stubUnavailableRegisters(),
            baseJSR, propertyJSR, resultJSR, stubInfoGPR);

        auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
        stubInfo->accessType = AccessType::GetByVal;
        stubInfo->bytecodeIndex = m_bytecodeIndex;
        if (isOperandConstantInt(property))
            stubInfo->propertyIsInt32 = true;
        gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
        gen.m_unlinkedStubInfo = stubInfo;

        gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
        resetSP(); // We might OSR exit here, so we need to conservatively reset SP

        addSlowCase();
        m_getByVals.append(gen);

        emitValueProfilingSite(bytecode, resultJSR);
        emitPutVirtualRegister(dst, resultJSR);
    }
}

template<typename OpcodeType>
void JIT::generateGetByValSlowCase(const OpcodeType& bytecode, Vector<SlowCaseEntry>::iterator& iter)
{
    if (!hasAnySlowCases(iter))
        return;

    VirtualRegister dst = bytecode.m_dst;

    linkAllSlowCases(iter);

    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationGetByValOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<SlowOperation, 2>();
    constexpr JSValueRegs arg3JSR = preferredArgumentJSR<SlowOperation, 3>();
    constexpr JSValueRegs arg4JSR = preferredArgumentJSR<SlowOperation, 4>();

    static_assert(!BaselineGetByValRegisters::baseJSR.overlaps(arg4JSR));
    moveValueRegs(BaselineGetByValRegisters::propertyJSR, arg4JSR);
    moveValueRegs(BaselineGetByValRegisters::baseJSR, arg3JSR);
    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpcodeType::Metadata::offsetOfArrayProfile(), profileGPR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        dst,
        globalObjectGPR, stubInfoGPR, profileGPR, arg3JSR, arg4JSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR4;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR3; // arg1 arg1 already used.
    constexpr GPRReg profileGPR = argumentGPR2;
    constexpr GPRReg baseGPR = regT0;
    constexpr GPRReg propertyGPR = regT1;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyGPR == argumentGPR1);
    static_assert(BaselineGetByValRegisters::baseJSR.payloadGPR() == regT0);
    static_assert(BaselineGetByValRegisters::propertyJSR.payloadGPR() == regT1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpcodeType::Metadata::offsetOfArrayProfile(), profileGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_val_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(dst, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emitSlow_op_get_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generateGetByValSlowCase(currentInstruction->as<OpGetByVal>(), iter);
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_val_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR4;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR5;
    constexpr GPRReg stubInfoGPR = argumentGPR3;
    constexpr GPRReg profileGPR = argumentGPR2;
    constexpr GPRReg baseGPR = BaselineGetByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineGetByValRegisters::propertyJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyGPR == argumentGPR1);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationGetByValOptimize)>(globalObjectGPR, stubInfoGPR, profileGPR, baseGPR, propertyGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_val_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_get_private_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetPrivateName>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineGetByValRegisters::baseJSR;
    using BaselineGetByValRegisters::propertyJSR;
    using BaselineGetByValRegisters::resultJSR;
    using BaselineGetByValRegisters::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITGetByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetPrivateName,
        RegisterSet::stubUnavailableRegisters(), baseJSR, propertyJSR, resultJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::GetPrivateName;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByVals.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_get_private_name(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    ASSERT(hasAnySlowCases(iter));
    auto bytecode = currentInstruction->as<OpGetPrivateName>();
    VirtualRegister dst = bytecode.m_dst;

    linkAllSlowCases(iter);

    JITGetByValGenerator& gen = m_getByVals[m_getByValIndex++];
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationGetPrivateNameOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_property, propertyJSR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        dst,
        globalObjectGPR, stubInfoGPR, baseJSR, propertyJSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR2; // arg1 already used.
    constexpr GPRReg baseGPR = BaselineGetByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineGetByValRegisters::propertyJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_private_name_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(dst, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_private_name_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR5;
    constexpr GPRReg stubInfoGPR = argumentGPR2;
    constexpr GPRReg baseGPR = regT0;
    constexpr GPRReg propertyGPR = regT1;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyGPR == argumentGPR1);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationGetPrivateNameOptimize)>(globalObjectGPR, stubInfoGPR, baseGPR, propertyGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_xxx_private_name_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_set_private_brand(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpSetPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselinePrivateBrandRegisters::baseJSR;
    using BaselinePrivateBrandRegisters::brandJSR;
    using BaselinePrivateBrandRegisters::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, brandJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITPrivateBrandAccessGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::SetPrivateBrand, RegisterSet::stubUnavailableRegisters(),
        baseJSR, brandJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::SetPrivateBrand;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_privateBrandAccesses.append(gen);

    // We should emit write-barrier at the end of sequence since write-barrier clobbers registers.
    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_set_private_brand(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    UNUSED_PARAM(currentInstruction);

    linkAllSlowCases(iter);

    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    auto bytecode = currentInstruction->as<OpSetPrivateBrand>();

    using SlowOperation = decltype(operationSetPrivateBrandOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs brandJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_brand, brandJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, baseJSR, brandJSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR2; // arg1 already used.
    constexpr GPRReg baseGPR = BaselinePrivateBrandRegisters::baseJSR.payloadGPR();
    constexpr GPRReg brandGPR = BaselinePrivateBrandRegisters::brandJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(brandGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    static_assert(std::is_same<FunctionTraits<decltype(operationSetPrivateBrandOptimize)>::ArgumentTypes, FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_private_name_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
#endif

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_check_private_brand(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpCheckPrivateBrand>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister brand = bytecode.m_brand;

    using BaselinePrivateBrandRegisters::baseJSR;
    using BaselinePrivateBrandRegisters::brandJSR;
    using BaselinePrivateBrandRegisters::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(brand, brandJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITPrivateBrandAccessGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::CheckPrivateBrand, RegisterSet::stubUnavailableRegisters(),
        baseJSR, brandJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::CheckPrivateBrand;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_privateBrandAccesses.append(gen);
}

void JIT::emitSlow_op_check_private_brand(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpCheckPrivateBrand>();
    UNUSED_PARAM(bytecode);

    JITPrivateBrandAccessGenerator& gen = m_privateBrandAccesses[m_privateBrandAccessIndex++];
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationCheckPrivateBrandOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs brandJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_brand, brandJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, baseJSR, brandJSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR2; // arg1 already used.
    constexpr GPRReg baseGPR = BaselinePrivateBrandRegisters::baseJSR.payloadGPR();
    constexpr GPRReg brandGPR = BaselinePrivateBrandRegisters::brandJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(brandGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    static_assert(std::is_same<FunctionTraits<decltype(operationCheckPrivateBrandOptimize)>::ArgumentTypes, FunctionTraits<decltype(operationGetPrivateNameOptimize)>::ArgumentTypes>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_private_name_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

template<typename Op>
void JIT::emit_op_put_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<Op>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    VirtualRegister value = bytecode.m_value;

    using BaselinePutByValRegisters::baseJSR;
    using BaselinePutByValRegisters::propertyJSR;
    using BaselinePutByValRegisters::valueJSR;
    using BaselinePutByValRegisters::stubInfoGPR;
    using BaselinePutByValRegisters::profileGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), profileGPR);
    materializePointerIntoMetadata(bytecode, Op::Metadata::offsetOfArrayProfile(), profileGPR);

    JITPutByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::PutByVal, RegisterSet::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, profileGPR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::PutByVal;
    stubInfo->putKind = std::is_same_v<Op, OpPutByValDirect> ? PutKind::Direct : PutKind::NotDirect;
    stubInfo->ecmaMode = ecmaMode(bytecode);
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    if (isOperandConstantInt(property))
        stubInfo->propertyIsInt32 = true;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

template void JIT::emit_op_put_by_val<OpPutByVal>(const Instruction*);

void JIT::emit_op_put_by_val_direct(const Instruction* currentInstruction)
{
    emit_op_put_by_val<OpPutByValDirect>(currentInstruction);
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
        ecmaMode = bytecode.m_ecmaMode;
    };

    if (isDirect)
        load(currentInstruction->as<OpPutByValDirect>());
    else
        load(currentInstruction->as<OpPutByVal>());

    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];

    linkAllSlowCases(iter);

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationPutByValStrictOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 1>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs valueJSR = preferredArgumentJSR<SlowOperation, 3>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 4>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<SlowOperation, 5>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    if (isDirect)
        materializePointerIntoMetadata(currentInstruction->as<OpPutByValDirect>(), OpPutByValDirect::Metadata::offsetOfArrayProfile(), profileGPR);
    else
        materializePointerIntoMetadata(currentInstruction->as<OpPutByVal>(), OpPutByVal::Metadata::offsetOfArrayProfile(), profileGPR);
    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, baseJSR, propertyJSR, valueJSR, stubInfoGPR, profileGPR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    // They are configured in the fast path and not clobbered.
    // constexpr GPRReg baseGPR = regT0;
    // constexpr GPRReg propertyGPR = regT1;
    // constexpr GPRReg valueGPR = regT2;
    // constexpr GPRReg profileGPR = regT3;
    constexpr GPRReg stubInfoGPR = regT4;
    constexpr GPRReg bytecodeOffsetGPR = regT5;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_put_by_val_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_by_val_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg globalObjectGPR = regT5;
    constexpr GPRReg baseGPR = BaselinePutByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselinePutByValRegisters::propertyJSR.payloadGPR();
    constexpr GPRReg valueGPR = BaselinePutByValRegisters::valueJSR.payloadGPR();
    constexpr GPRReg stubInfoGPR = regT4;
    constexpr GPRReg profileGPR = BaselinePutByValRegisters::profileGPR;
    constexpr GPRReg bytecodeOffsetGPR = regT5;
    {
        RegisterSet used(baseGPR, propertyGPR, valueGPR, profileGPR);
        ASSERT(!used.contains(regT4));
        ASSERT(!used.contains(regT5));
    }


    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));
    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationPutByValStrictOptimize)>(globalObjectGPR, baseGPR, propertyGPR, valueGPR, stubInfoGPR, profileGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR4, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_xxx_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_put_private_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutPrivateName>();
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;
    VirtualRegister value = bytecode.m_value;

    using BaselinePutByValRegisters::baseJSR;
    using BaselinePutByValRegisters::propertyJSR;
    using BaselinePutByValRegisters::valueJSR;
    using BaselinePutByValRegisters::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitGetVirtualRegister(value, valueJSR);

    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITPutByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::PutPrivateName, RegisterSet::stubUnavailableRegisters(),
        baseJSR, propertyJSR, valueJSR, InvalidGPRReg, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::PutPrivateName;
    stubInfo->privateFieldPutKind = bytecode.m_putKind;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_putByVals.append(gen);

    // IC can write new Structure without write-barrier if a base is cell.
    // FIXME: Use UnconditionalWriteBarrier in Baseline effectively to reduce code size.
    // https://bugs.webkit.org/show_bug.cgi?id=209395
    emitWriteBarrier(base, ShouldFilterBase);
}

void JIT::emitSlow_op_put_private_name(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    UNUSED_PARAM(currentInstruction);

    JITPutByValGenerator& gen = m_putByVals[m_putByValIndex++];

    linkAllSlowCases(iter);

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    auto bytecode = currentInstruction->as<OpPutPrivateName>();

    using SlowOperation = decltype(operationPutByValDefinePrivateFieldOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 1>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs valueJSR = preferredArgumentJSR<SlowOperation, 3>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 4>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_property, propertyJSR);
    emitGetVirtualRegister(bytecode.m_value, valueJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, baseJSR, propertyJSR, valueJSR, stubInfoGPR, TrustedImmPtr(nullptr));
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    // constexpr GPRReg baseGPR = regT0;
    // constexpr GPRReg propertyGPR = regT1;
    // constexpr GPRReg valueGPR = regT2;
    constexpr GPRReg stubInfoGPR = regT3;
    constexpr GPRReg bytecodeOffsetGPR = regT4;

    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_put_private_name_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_private_name_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg baseGPR = BaselinePutByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselinePutByValRegisters::propertyJSR.payloadGPR();
    constexpr GPRReg valueGPR = BaselinePutByValRegisters::valueJSR.payloadGPR();
    constexpr GPRReg stubInfoGPR = regT3;
    constexpr GPRReg bytecodeOffsetGPR = regT4;

    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = regT4;

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationPutByValDefinePrivateFieldOptimize)>(globalObjectGPR, baseGPR, propertyGPR, valueGPR, stubInfoGPR, TrustedImmPtr(nullptr));
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR4, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_put_private_name_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

#if USE(JSVALUE64)

void JIT::emit_op_put_getter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT1);
    loadGlobalObject(regT2);
    callOperation(operationPutGetterById, regT2, regT0, m_unlinkedCodeBlock->identifier(bytecode.m_property).impl(), options, regT1);
}

void JIT::emit_op_put_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t options = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT1);
    loadGlobalObject(regT2);
    callOperation(operationPutSetterById, regT2, regT0, m_unlinkedCodeBlock->identifier(bytecode.m_property).impl(), options, regT1);
}

void JIT::emit_op_put_getter_setter_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterSetterById>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    int32_t attribute = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_getter, regT1);
    emitGetVirtualRegister(bytecode.m_setter, regT2);
    loadGlobalObject(regT3);
    callOperation(operationPutGetterSetter, regT3, regT0, m_unlinkedCodeBlock->identifier(bytecode.m_property).impl(), attribute, regT1, regT2);
}

void JIT::emit_op_put_getter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutGetterByVal>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    emitGetVirtualRegister(bytecode.m_property, regT1);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT2);
    loadGlobalObject(regT3);
    callOperation(operationPutGetterByVal, regT3, regT0, regT1, attributes, regT2);
}

void JIT::emit_op_put_setter_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutSetterByVal>();
    emitGetVirtualRegister(bytecode.m_base, regT0);
    emitGetVirtualRegister(bytecode.m_property, regT1);
    int32_t attributes = bytecode.m_attributes;
    emitGetVirtualRegister(bytecode.m_accessor, regT2);
    loadGlobalObject(regT3);
    callOperation(operationPutSetterByVal, regT3, regT0, regT1, attributes, regT2);
}

#endif

void JIT::emit_op_del_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineDelByIdRegisters::baseJSR;
    using BaselineDelByIdRegisters::resultJSR;
    using BaselineDelByIdRegisters::stubInfoGPR;
    using BaselineDelByIdRegisters::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    JITDelByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, resultJSR, stubInfoGPR, scratchGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::DeleteByID;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

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

void JIT::emitSlow_op_del_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpDelById>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITDelByIdGenerator& gen = m_delByIds[m_delByIdIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationDeleteByIdOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(base, baseJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits(), TrustedImm32(bytecode.m_ecmaMode.value()));
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR0;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = argumentGPR2;
    constexpr GPRReg propertyGPR = argumentGPR3;
    constexpr GPRReg ecmaModeGPR = argumentGPR4;

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(base, baseGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    move(TrustedImm32(bytecode.m_ecmaMode.value()), ecmaModeGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_del_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
    static_assert(returnValueGPR == regT0);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    using BaselineDelByIdRegisters::resultJSR;
    static_assert(resultJSR.payloadGPR() == GPRInfo::returnValueGPR);

    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_del_by_id_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR0;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = argumentGPR2;
    constexpr GPRReg propertyGPR = argumentGPR3;
    constexpr GPRReg ecmaModeGPR = argumentGPR4;

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(argumentGPR0, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationDeleteByIdOptimize)>(globalObjectGPR, stubInfoGPR, baseGPR, propertyGPR, ecmaModeGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_del_by_id_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_del_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineDelByValRegisters::baseJSR;
    using BaselineDelByValRegisters::propertyJSR;
    using BaselineDelByValRegisters::resultJSR;
    using BaselineDelByValRegisters::stubInfoGPR;
    using BaselineDelByValRegisters::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitGetVirtualRegister(property, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(propertyJSR, property);

    JITDelByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR, scratchGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::DeleteByVal;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

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

void JIT::emitSlow_op_del_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpDelByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    JITDelByValGenerator& gen = m_delByVals[m_delByValIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationDeleteByValOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, baseJSR, propertyJSR, TrustedImm32(bytecode.m_ecmaMode.value()));
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR0;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = argumentGPR2;
    constexpr GPRReg propertyGPR = argumentGPR3;
    constexpr GPRReg ecmaModeGPR = argumentGPR4;

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(base, baseGPR);
    emitGetVirtualRegister(property, propertyGPR);
    move(TrustedImm32(bytecode.m_ecmaMode.value()), ecmaModeGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_del_by_val_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
    static_assert(returnValueGPR == regT0);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    using BaselineDelByIdRegisters::resultJSR;
    static_assert(resultJSR.payloadGPR() == GPRInfo::returnValueGPR);

    boxBoolean(resultJSR.payloadGPR(), resultJSR);
    emitPutVirtualRegister(dst, resultJSR);
    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_del_by_val_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR0;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = argumentGPR2;
    constexpr GPRReg propertyGPR = argumentGPR3;
    constexpr GPRReg ecmaModeGPR = argumentGPR4;

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(argumentGPR0, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationDeleteByValOptimize)>(globalObjectGPR, stubInfoGPR, baseGPR, propertyGPR, ecmaModeGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_del_by_val_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_try_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineGetByIdRegisters::baseJSR;
    using BaselineGetByIdRegisters::resultJSR;
    using BaselineGetByIdRegisters::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITGetByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::TryGetById);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::TryGetById;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByIds.append(gen);
    
    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_try_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpTryGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationTryGetByIdOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        resultVReg,
        globalObjectGPR, stubInfoGPR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    move(JIT::TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = BaselineGetByIdRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = argumentGPR3;
    static_assert(baseGPR == argumentGPR0 || !isARM64());

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    static_assert(std::is_same<decltype(operationTryGetByIdOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(resultVReg, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_get_by_id_direct(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineGetByIdRegisters::baseJSR;
    using BaselineGetByIdRegisters::resultJSR;
    using BaselineGetByIdRegisters::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITGetByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetByIdDirect);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::GetByIdDirect;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id_direct(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdDirect>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationGetByIdDirectOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        resultVReg,
        globalObjectGPR, stubInfoGPR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = BaselineGetByIdRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = argumentGPR3;
    static_assert(baseGPR == argumentGPR0 || !isARM64());

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    static_assert(std::is_same<decltype(operationGetByIdDirectOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(resultVReg, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_get_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineGetByIdRegisters::baseJSR;
    using BaselineGetByIdRegisters::resultJSR;
    using BaselineGetByIdRegisters::stubInfoGPR;
    using BaselineGetByIdRegisters::scratchGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);
    
    if (*ident == m_vm->propertyNames->length && shouldEmitProfiling()) {
        load8FromMetadata(bytecode, OpGetById::Metadata::offsetOfModeMetadata() + GetByIdModeMetadata::offsetOfMode(), scratchGPR);
        Jump notArrayLengthMode = branch32(NotEqual, TrustedImm32(static_cast<uint8_t>(GetByIdMode::ArrayLength)), scratchGPR);
        emitArrayProfilingSiteWithCell(
            bytecode,
            OpGetById::Metadata::offsetOfModeMetadata() + GetByIdModeMetadataArrayLength::offsetOfArrayProfile(),
            baseJSR.payloadGPR(), scratchGPR);
        notArrayLengthMode.link(this);
    }

    JITGetByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR, AccessType::GetById);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::GetById;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIds.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_get_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITGetByIdGenerator& gen = m_getByIds[m_getByIdIndex++];
    
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationGetByIdOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        resultVReg,
        globalObjectGPR, stubInfoGPR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = BaselineGetByIdRegisters::baseJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    constexpr GPRReg propertyGPR = argumentGPR3;

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(resultVReg, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_get_by_id_with_this(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister thisVReg = bytecode.m_thisValue;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineGetByIdWithThisRegisters::baseJSR;
    using BaselineGetByIdWithThisRegisters::thisJSR;
    using BaselineGetByIdWithThisRegisters::resultJSR;
    using BaselineGetByIdWithThisRegisters::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(thisVReg, thisJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);
    emitJumpSlowCaseIfNotJSCell(thisJSR, thisVReg);

    JITGetByIdWithThisGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), resultJSR, baseJSR, thisJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::GetByIdWithThis;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByIdsWithThis.append(gen);

    emitValueProfilingSite(bytecode, resultJSR);
    emitPutVirtualRegister(resultVReg, resultJSR);
}

#if USE(JSVALUE64)

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_id_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR5;
    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = regT0;
    constexpr GPRReg propertyGPR = argumentGPR3;
    static_assert(baseGPR == argumentGPR0 || !isARM64());

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationGetByIdOptimize)>(globalObjectGPR, stubInfoGPR, baseGPR, propertyGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_id_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

#endif

void JIT::emitSlow_op_get_by_id_with_this(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpGetByIdWithThis>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITGetByIdWithThisGenerator& gen = m_getByIdsWithThis[m_getByIdWithThisIndex++];
    
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationGetByIdWithThisOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs thisJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_thisValue, thisJSR);
    callOperationWithProfile<SlowOperation>(
        bytecode,
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        resultVReg,
        globalObjectGPR, stubInfoGPR, baseJSR, thisJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR2; // arg1 already in use.
    constexpr GPRReg baseGPR = BaselineGetByIdWithThisRegisters::baseJSR.payloadGPR();
    constexpr GPRReg thisGPR = BaselineGetByIdWithThisRegisters::thisJSR.payloadGPR();
    constexpr GPRReg propertyGPR = argumentGPR4;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(thisGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_id_with_this_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitValueProfilingSite(bytecode, returnValueGPR);
    emitPutVirtualRegister(resultVReg, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if USE(JSVALUE64)

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_by_id_with_this_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR5;
    constexpr GPRReg stubInfoGPR = argumentGPR2;
    constexpr GPRReg baseGPR = regT0;
    constexpr GPRReg thisGPR = regT1;
    constexpr GPRReg propertyGPR = argumentGPR4;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(thisGPR == argumentGPR1);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationGetByIdWithThisOptimize)>(globalObjectGPR, stubInfoGPR, baseGPR, thisGPR, propertyGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_by_id_with_this_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

#endif

void JIT::emit_op_put_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpPutById>();
    VirtualRegister baseVReg = bytecode.m_base;
    VirtualRegister valueVReg = bytecode.m_value;
    bool direct = bytecode.m_flags.isDirect();
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    // In order to be able to patch both the Structure, and the object offset, we store one pointer,
    // to just after the arguments have been loaded into registers 'hotPathBegin', and we generate code
    // such that the Structure & offset are always at the same distance from this.

    using BaselinePutByIdRegisters::baseJSR;
    using BaselinePutByIdRegisters::valueJSR;
    using BaselinePutByIdRegisters::stubInfoGPR;
    using BaselinePutByIdRegisters::scratchGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitGetVirtualRegister(valueVReg, valueJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITPutByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident),
        baseJSR, valueJSR, stubInfoGPR, scratchGPR, ecmaMode(bytecode),
        direct ? PutKind::Direct : PutKind::NotDirect);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::PutById;
    stubInfo->putKind = direct ? PutKind::Direct : PutKind::NotDirect;
    stubInfo->ecmaMode = ecmaMode(bytecode);
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
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
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    Label coldPathBegin(this);
    
    JITPutByIdGenerator& gen = m_putByIds[m_putByIdIndex++];

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationPutByIdStrictOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs valueJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_value, valueJSR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        globalObjectGPR, stubInfoGPR, valueJSR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR3; // arg1 already in use.
    constexpr GPRReg valueGPR = BaselinePutByIdRegisters::valueJSR.payloadGPR();
    constexpr GPRReg baseGPR = BaselinePutByIdRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = argumentGPR4;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(valueGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    emitNakedNearCall(vm.getCTIStub(slow_op_put_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_by_id_prepareCallGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    if (!JITCode::useDataIC(JITType::BaselineJIT))
        jit.tagReturnAddress();

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg globalObjectGPR = argumentGPR5;
    constexpr GPRReg stubInfoGPR = argumentGPR3;
    constexpr GPRReg valueGPR = regT1;
    constexpr GPRReg baseGPR = regT0;
    constexpr GPRReg propertyGPR = argumentGPR4;
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(valueGPR == argumentGPR1);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), globalObjectGPR);
    jit.loadPtr(Address(globalObjectGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);

    jit.setupArguments<decltype(operationPutByIdStrictOptimize)>(globalObjectGPR, stubInfoGPR, valueGPR, baseGPR, propertyGPR);
    jit.prepareCallOperation(vm);

    if (JITCode::useDataIC(JITType::BaselineJIT))
        jit.farJump(Address(argumentGPR1, StructureStubInfo::offsetOfSlowOperation()), OperationPtrTag);
    else
        jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_by_id_prepareCall");
}
#endif // ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_in_by_id(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    VirtualRegister baseVReg = bytecode.m_base;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    using BaselineInByIdRegisters::baseJSR;
    using BaselineInByIdRegisters::resultJSR;
    using BaselineInByIdRegisters::stubInfoGPR;

    emitGetVirtualRegister(baseVReg, baseJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, baseVReg);

    JITInByIdGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), RegisterSet::stubUnavailableRegisters(),
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident), baseJSR, resultJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::InById;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByIds.append(gen);

    emitPutVirtualRegister(resultVReg, resultJSR);
}

void JIT::emitSlow_op_in_by_id(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInById>();
    VirtualRegister resultVReg = bytecode.m_dst;
    const Identifier* ident = &(m_unlinkedCodeBlock->identifier(bytecode.m_property));

    JITInByIdGenerator& gen = m_inByIds[m_inByIdIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationInByIdOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        resultVReg,
        globalObjectGPR, stubInfoGPR, baseJSR,
        CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits());
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR1;
    constexpr GPRReg baseGPR = BaselineInByIdRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = argumentGPR3;
    static_assert(baseGPR == argumentGPR0 || !isARM64());

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    move(TrustedImmPtr(CacheableIdentifier::createFromIdentifierOwnedByCodeBlock(m_unlinkedCodeBlock, *ident).rawBits()), propertyGPR);
    // slow_op_get_by_id_prepareCallGenerator will do exactly what we need.
    // So, there's no point in creating a duplicate thunk just to give it a different name.
    static_assert(std::is_same<decltype(operationInByIdOptimize), decltype(operationGetByIdOptimize)>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_id_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(resultVReg, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_in_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpInByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister property = bytecode.m_property;

    using BaselineInByValRegisters::baseJSR;
    using BaselineInByValRegisters::propertyJSR;
    using BaselineInByValRegisters::resultJSR;
    using BaselineInByValRegisters::stubInfoGPR;
    using BaselineInByValRegisters::scratchGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);
    emitArrayProfilingSiteWithCell(bytecode, baseJSR.payloadGPR(), scratchGPR);

    JITInByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::InByVal, RegisterSet::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::InByVal;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByVals.append(gen);

    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitSlow_op_in_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpInByVal>();
    VirtualRegister dst = bytecode.m_dst;

    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];

    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationInByValOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr GPRReg profileGPR = preferredArgumentGPR<SlowOperation, 2>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 3>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 4>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpInByVal::Metadata::offsetOfArrayProfile(), profileGPR);
    emitGetVirtualRegister(bytecode.m_base, baseJSR);
    emitGetVirtualRegister(bytecode.m_property, propertyJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        dst,
        globalObjectGPR, stubInfoGPR, profileGPR, baseJSR, propertyJSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR4;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR3;
    constexpr GPRReg profileGPR = argumentGPR2;
    constexpr GPRReg baseGPR = BaselineInByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineInByValRegisters::propertyJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    materializePointerIntoMetadata(bytecode, OpInByVal::Metadata::offsetOfArrayProfile(), profileGPR);
    // slow_op_get_by_val_prepareCallGenerator will do exactly what we need.
    // So, there's no point in creating a duplicate thunk just to give it a different name.
    static_assert(std::is_same<decltype(operationInByValOptimize), decltype(operationGetByValOptimize)>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_by_val_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(dst, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emitHasPrivate(VirtualRegister dst, VirtualRegister base, VirtualRegister propertyOrBrand, AccessType type)
{
    using BaselineInByValRegisters::baseJSR;
    using BaselineInByValRegisters::propertyJSR;
    using BaselineInByValRegisters::resultJSR;
    using BaselineInByValRegisters::stubInfoGPR;

    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(propertyOrBrand, propertyJSR);
    emitJumpSlowCaseIfNotJSCell(baseJSR, base);

    JITInByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), type, RegisterSet::stubUnavailableRegisters(),
        baseJSR, propertyJSR, resultJSR, stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = type;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    addSlowCase();
    m_inByVals.append(gen);

    emitPutVirtualRegister(dst, resultJSR);
}

void JIT::emitHasPrivateSlow(VirtualRegister dst, VirtualRegister base, VirtualRegister property, AccessType type)
{
    UNUSED_PARAM(base);
    UNUSED_PARAM(property);
    ASSERT_UNUSED(type, type == AccessType::HasPrivateName || type == AccessType::HasPrivateBrand);

    JITInByValGenerator& gen = m_inByVals[m_inByValIndex++];
    Label coldPathBegin = label();

#if !ENABLE(EXTRA_CTI_THUNKS)
    using SlowOperation = decltype(operationHasPrivateNameOptimize);
    constexpr GPRReg globalObjectGPR = preferredArgumentGPR<SlowOperation, 0>();
    constexpr GPRReg stubInfoGPR = preferredArgumentGPR<SlowOperation, 1>();
    constexpr JSValueRegs baseJSR = preferredArgumentJSR<SlowOperation, 2>();
    constexpr JSValueRegs propertyJSR = preferredArgumentJSR<SlowOperation, 3>();

    loadGlobalObject(globalObjectGPR);
    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    emitGetVirtualRegister(base, baseJSR);
    emitGetVirtualRegister(property, propertyJSR);
    callOperation<SlowOperation>(
        Address(stubInfoGPR, StructureStubInfo::offsetOfSlowOperation()),
        dst,
        globalObjectGPR, stubInfoGPR, baseJSR, propertyJSR);
#else
    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR3;
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    constexpr GPRReg stubInfoGPR = argumentGPR2;
    constexpr GPRReg baseGPR = BaselineInByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyOrBrandGPR = BaselineInByValRegisters::propertyJSR.payloadGPR();
    static_assert(baseGPR == argumentGPR0 || !isARM64());
    static_assert(propertyOrBrandGPR == argumentGPR1);

    loadConstant(gen.m_unlinkedStubInfoConstantIndex, stubInfoGPR);
    static_assert(std::is_same<decltype(operationHasPrivateNameOptimize), decltype(operationGetPrivateNameOptimize)>::value);
    static_assert(std::is_same<decltype(operationHasPrivateBrandOptimize), decltype(operationGetPrivateNameOptimize)>::value);
    emitNakedNearCall(vm.getCTIStub(slow_op_get_private_name_prepareCallGenerator).retaggedCode<NoPtrTag>());
    emitNakedNearCall(vm.getCTIStub(checkExceptionGenerator).retaggedCode<NoPtrTag>());

    emitPutVirtualRegister(dst, returnValueGPR);
#endif // ENABLE(EXTRA_CTI_THUNKS)

    gen.reportSlowPathCall(coldPathBegin, Call());
}

void JIT::emit_op_has_private_name(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateName>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_property, AccessType::HasPrivateName);
}

void JIT::emitSlow_op_has_private_name(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpHasPrivateName>();
    emitHasPrivateSlow(bytecode.m_dst, bytecode.m_base, bytecode.m_property, AccessType::HasPrivateName);
}

void JIT::emit_op_has_private_brand(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpHasPrivateBrand>();
    emitHasPrivate(bytecode.m_dst, bytecode.m_base, bytecode.m_brand, AccessType::HasPrivateBrand);
}

void JIT::emitSlow_op_has_private_brand(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpHasPrivateBrand>();
    emitHasPrivateSlow(bytecode.m_dst, bytecode.m_base, bytecode.m_brand, AccessType::HasPrivateBrand);
}

#if USE(JSVALUE64)

void JIT::emitResolveClosure(VirtualRegister dst, VirtualRegister scope, bool needsVarInjectionChecks, unsigned depth)
{
    emitVarInjectionCheck(needsVarInjectionChecks, regT0);
    emitGetVirtualRegister(scope, regT0);
    for (unsigned i = 0; i < depth; ++i)
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
    emitPutVirtualRegister(dst);
}


#if !ENABLE(EXTRA_CTI_THUNKS)
void JIT::emit_op_resolve_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;

    constexpr GPRReg scopeGPR = regT0;

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks, GPRReg globalObjectGPR = InvalidGPRReg) {
        if (!needsVarInjectionChecks)
            return;
        if (globalObjectGPR == InvalidGPRReg) {
            globalObjectGPR = regT4;
            loadGlobalObject(globalObjectGPR);
        }
        loadPtr(Address(globalObjectGPR, OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpoint)), regT3);
        slowCase.append(branch8(Equal, Address(regT3, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitResolveClosure = [&] (bool needsVarInjectionChecks) {
        doVarInjectionCheck(needsVarInjectionChecks);
        load32FromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLocalScopeDepth(), regT1);

        ASSERT(scopeGPR == regT0);
        Label loop = label();
        Jump done = branchTest32(Zero, regT1);
        loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
        sub32(TrustedImm32(1), regT1);
        jump().linkTo(loop, this);
        done.link(this);
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject().
            loadGlobalObject(regT0);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            load32FromMetadata(bytecode, OpResolveScope::Metadata::offsetOfGlobalLexicalBindingEpoch(), regT1);
            slowCase.append(branch32(NotEqual, Address(regT0, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), regT1));
            break;
        }

        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject() for GlobalVar*,
            // and codeBlock->globalObject()->globalLexicalEnvironment() for GlobalLexicalVar*.
            loadGlobalObject(regT0);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                loadPtr(Address(regT0, JSGlobalObject::offsetOfGlobalLexicalEnvironment()), regT0);
            break;
        }
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitResolveClosure(needsVarInjectionChecks(resolveType));
            break;
        case Dynamic:
            slowCase.append(jump());
            break;
        case ResolvedClosureVar:
        case ModuleVar:
        case UnresolvedProperty:
        case UnresolvedPropertyWithVarInjectionChecks:
            RELEASE_ASSERT_NOT_REACHED();
        }
    };

    if (profiledResolveType == ModuleVar) 
        loadPtrFromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLexicalEnvironment(), regT0);
    else {
        emitGetVirtualRegister(scope, scopeGPR);
        if (profiledResolveType == ClosureVar || profiledResolveType == ClosureVarWithVarInjectionChecks)
            emitCode(profiledResolveType);
        else {
            emitGetVirtualRegister(scope, scopeGPR);

            JumpList skipToEnd;
            load32FromMetadata(bytecode, OpResolveScope::Metadata::offsetOfResolveType(), regT1);

            auto emitCase = [&] (ResolveType resolveType) {
                Jump notCase = branch32(NotEqual, regT1, TrustedImm32(resolveType));
                emitCode(resolveType);
                skipToEnd.append(jump());
                notCase.link(this);
            };

            emitCase(GlobalVar);
            emitCase(GlobalProperty);
            emitCase(GlobalLexicalVar);
            emitCase(GlobalVarWithVarInjectionChecks);
            emitCase(GlobalPropertyWithVarInjectionChecks);
            emitCase(GlobalLexicalVarWithVarInjectionChecks);
            slowCase.append(jump());

            skipToEnd.link(this);
        }
    }

    addSlowCase(slowCase);

    emitPutVirtualRegister(dst);
}

#else
void JIT::emit_op_resolve_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpResolveScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_resolveType;
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;

    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    // If we profile certain resolve types, we're gauranteed all linked code will have the same
    // resolve type.

    if (profiledResolveType == ModuleVar) 
        loadPtrFromMetadata(bytecode, OpResolveScope::Metadata::offsetOfLexicalEnvironment(), regT0);
    else {
        uint32_t metadataOffset = m_unlinkedCodeBlock->metadata().offsetInMetadataTable(bytecode);

        constexpr GPRReg metadataGPR = regT2;
        constexpr GPRReg scopeGPR = regT0;
        constexpr GPRReg bytecodeOffsetGPR = regT5;

        emitGetVirtualRegister(scope, scopeGPR);
        addPtr(TrustedImm32(metadataOffset), s_metadataGPR, metadataGPR);

        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

        MacroAssemblerCodeRef<JITThunkPtrTag> code;
        if (profiledResolveType == ClosureVar)
            code = vm.getCTIStub(generateOpResolveScopeThunk<ClosureVar>);
        else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
            code = vm.getCTIStub(generateOpResolveScopeThunk<ClosureVarWithVarInjectionChecks>);
        else if (profiledResolveType == GlobalVar)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalVar>);
        else if (profiledResolveType == GlobalProperty)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalProperty>);
        else if (profiledResolveType == GlobalLexicalVar)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVar>);
        else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalVarWithVarInjectionChecks>);
        else if (profiledResolveType == GlobalPropertyWithVarInjectionChecks)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalPropertyWithVarInjectionChecks>);
        else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
        else
            code = vm.getCTIStub(generateOpResolveScopeThunk<GlobalVar>);

        emitNakedNearCall(code.retaggedCode<NoPtrTag>());
    }

    emitPutVirtualRegister(dst);
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
    constexpr GPRReg metadataGPR = regT2; // incoming
    constexpr GPRReg scopeGPR = regT0; // incoming
    constexpr GPRReg bytecodeOffsetGPR = regT5; // incoming - pass thru to slow path.
    UNUSED_PARAM(bytecodeOffsetGPR);

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks, GPRReg globalObjectGPR = InvalidGPRReg) {
        if (!needsVarInjectionChecks)
            return;
        if (globalObjectGPR == InvalidGPRReg) {
            globalObjectGPR = regT4;
            jit.loadPtr(addressFor(CallFrameSlot::codeBlock), regT3);
            jit.loadPtr(Address(regT3, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
        }
        jit.loadPtr(Address(globalObjectGPR, OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpoint)), regT3);
        slowCase.append(jit.branch8(Equal, Address(regT3, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };

    auto emitResolveClosure = [&] (bool needsVarInjectionChecks) {
        doVarInjectionCheck(needsVarInjectionChecks);
        jit.load32(Address(metadataGPR, Metadata::offsetOfLocalScopeDepth()), regT1);
        RELEASE_ASSERT(scopeGPR == regT0);

        Label loop = jit.label();
        Jump done = jit.branchTest32(Zero, regT1);
        jit.loadPtr(Address(regT0, JSScope::offsetOfNext()), regT0);
        jit.sub32(TrustedImm32(1), regT1);
        jit.jump().linkTo(loop, &jit);
        done.link(&jit);
    };

    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject().
            jit.loadPtr(addressFor(CallFrameSlot::codeBlock), regT3);
            jit.loadPtr(Address(regT3, CodeBlock::offsetOfGlobalObject()), regT0);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            jit.load32(Address(metadataGPR, Metadata::offsetOfGlobalLexicalBindingEpoch()), regT1);
            slowCase.append(jit.branch32(NotEqual, Address(regT0, JSGlobalObject::offsetOfGlobalLexicalBindingEpoch()), regT1));
            break;
        }

        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            // JSScope::constantScopeForCodeBlock() loads codeBlock->globalObject() for GlobalVar*,
            // and codeBlock->globalObject()->globalLexicalEnvironment() for GlobalLexicalVar*.
            jit.loadPtr(addressFor(CallFrameSlot::codeBlock), regT0);
            jit.loadPtr(Address(regT0, CodeBlock::offsetOfGlobalObject()), regT0);
            doVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)
                jit.loadPtr(Address(regT0, JSGlobalObject::offsetOfGlobalLexicalEnvironment()), regT0);
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
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "resolve_scope thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_resolve_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

    // The fast path already pushed the return address.
#if CPU(X86_64)
    jit.push(X86Registers::ebp);
#elif CPU(ARM64)
    jit.pushPair(framePointerRegister, linkRegister);
#endif

    constexpr GPRReg incomingBytecodeOffsetGPR = regT5;
    jit.store32(incomingBytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg codeBlockGPR = argumentGPR3;
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;

    static_assert(incomingBytecodeOffsetGPR != codeBlockGPR);
    static_assert(incomingBytecodeOffsetGPR != globalObjectGPR);
    static_assert(incomingBytecodeOffsetGPR != instructionGPR);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(incomingBytecodeOffsetGPR, instructionGPR);

    jit.setupArguments<decltype(operationResolveScopeForBaseline)>(globalObjectGPR, instructionGPR);
    jit.prepareCallOperation(vm);
    Call operation = jit.call(OperationPtrTag);
    Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

#if CPU(X86_64)
    jit.pop(X86Registers::ebp);
#elif CPU(ARM64)
    jit.popPair(CCallHelpers::framePointerRegister, CCallHelpers::linkRegister);
#endif
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    patchBuffer.link(operation, FunctionPtr<OperationPtrTag>(operationResolveScopeForBaseline));
    auto handler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_resolve_scope");
}
#endif // !ENABLE(EXTRA_CTI_THUNKS)

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
    loadPtr(Address(reg), reg);
}

void JIT::emitGetClosureVar(VirtualRegister scope, uintptr_t operand)
{
    emitGetVirtualRegister(scope, regT0);
    loadPtr(Address(regT0, JSLexicalEnvironment::offsetOfVariables() + operand * sizeof(Register)), regT0);
}

#if !ENABLE(EXTRA_CTI_THUNKS)
void JIT::emit_op_get_from_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    constexpr GPRReg scopeGPR = regT2;
    emitGetVirtualRegister(scope, scopeGPR);

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks) {
        if (!needsVarInjectionChecks)
            return;
        loadGlobalObject(regT3);
        loadPtr(Address(regT3, OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpoint)), regT3);
        slowCase.append(branch8(Equal, Address(regT3, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };
    
    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            loadPtrFromMetadata(bytecode, OpGetFromScope::Metadata::offsetOfStructure(), regT1);
            slowCase.append(branchTestPtr(Zero, regT1));
            load32(Address(regT1, Structure::structureIDOffset()), regT1);
            slowCase.append(branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), regT1));

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadPtr(addressFor(CallFrameSlot::codeBlock), regT3);
                loadPtr(Address(regT3, CodeBlock::offsetOfGlobalObject()), regT3);
                return branchPtr(Equal, scopeGPR, regT3);
            }));

            loadPtrFromMetadata(bytecode, OpGetFromScope::Metadata::offsetOfOperand(), regT1);

            if (ASSERT_ENABLED) {
                Jump isOutOfLine = branch32(GreaterThanOrEqual, regT1, TrustedImm32(firstOutOfLineOffset));
                abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(this);
            }

            loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), regT0);
            neg32(regT1);
            signExtend32ToPtr(regT1, regT1);
            load64(BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), regT0);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            loadPtrFromMetadata(bytecode, OpGetFromScope::Metadata::offsetOfOperand(), regT0);
            loadPtr(Address(regT0), regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                slowCase.append(branchIfEmpty(regT0));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            loadPtrFromMetadata(bytecode, OpGetFromScope::Metadata::offsetOfOperand(), regT3);
            static_assert(1 << 3 == sizeof(Register));
            lshift64(TrustedImm32(3), regT3);
            addPtr(scopeGPR, regT3);
            loadPtr(Address(regT3, JSLexicalEnvironment::offsetOfVariables()), regT0);

            break;
        case Dynamic:
            slowCase.append(jump());
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
        load32FromMetadata(bytecode, OpGetFromScope::Metadata::offsetOfGetPutInfo(), regT0);
        and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        Jump notGlobalVar = branch32(NotEqual, regT0, TrustedImm32(GlobalVar));
        emitCode(GlobalVar);
        skipToEnd.append(jump());
        notGlobalVar.link(this);

        Jump notGlobalVarWithVarInjection = branch32(NotEqual, regT0, TrustedImm32(GlobalVarWithVarInjectionChecks));
        emitCode(GlobalVarWithVarInjectionChecks);
        skipToEnd.append(jump());
        notGlobalVarWithVarInjection.link(this);

        Jump isGlobalProperty = branch32(Equal, regT0, TrustedImm32(GlobalProperty));
        Jump notGlobalPropertyWithVarInjections = branch32(NotEqual, regT0, TrustedImm32(GlobalPropertyWithVarInjectionChecks));
        isGlobalProperty.link(this);
        emitCode(GlobalProperty);
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

        slowCase.append(jump());

        skipToEnd.link(this);
    }

    addSlowCase(slowCase);

    emitValueProfilingSite(bytecode, regT0);
    emitPutVirtualRegister(dst);
}

void JIT::emitSlow_op_get_from_scope(const Instruction* instruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);
    auto bytecode = instruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;

    loadGlobalObject(argumentGPR0);
    callOperationWithProfile(bytecode, operationGetFromScope, dst, argumentGPR0, instruction);
}

#else
void JIT::emit_op_get_from_scope(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromScope>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister scope = bytecode.m_scope;
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();

    VM& vm = this->vm();
    uint32_t bytecodeOffset = m_bytecodeIndex.offset();
    ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

    constexpr GPRReg metadataGPR = regT4;
    constexpr GPRReg scopeGPR = regT2;
    constexpr GPRReg bytecodeOffsetGPR = regT5;

    uint32_t metadataOffset = m_unlinkedCodeBlock->metadata().offsetInMetadataTable(bytecode);

    emitGetVirtualRegister(scope, scopeGPR);
    addPtr(TrustedImm32(metadataOffset), s_metadataGPR, metadataGPR);
    move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

    MacroAssemblerCodeRef<JITThunkPtrTag> code;
    if (profiledResolveType == ClosureVar)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<ClosureVar>);
    else if (profiledResolveType == ClosureVarWithVarInjectionChecks)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<ClosureVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalVar)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);
    else if (profiledResolveType == GlobalVarWithVarInjectionChecks)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalVarWithVarInjectionChecks>);
    else if (profiledResolveType == GlobalProperty)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalProperty>);
    else if (profiledResolveType == GlobalLexicalVar)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVar>);
    else if (profiledResolveType == GlobalLexicalVarWithVarInjectionChecks)
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalLexicalVarWithVarInjectionChecks>);
    else
        code = vm.getCTIStub(generateOpGetFromScopeThunk<GlobalVar>);

    emitNakedNearCall(code.retaggedCode<NoPtrTag>());
    emitPutVirtualRegister(dst);
}

template <ResolveType profiledResolveType>
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::generateOpGetFromScopeThunk(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    using Metadata = OpGetFromScope::Metadata;
    constexpr GPRReg metadataGPR = regT4;
    constexpr GPRReg scopeGPR = regT2;

    CCallHelpers jit;

    jit.tagReturnAddress();

    JumpList slowCase;

    auto doVarInjectionCheck = [&] (bool needsVarInjectionChecks) {
        if (!needsVarInjectionChecks)
            return;
        jit.loadPtr(addressFor(CallFrameSlot::codeBlock), regT3);
        jit.loadPtr(Address(regT3, CodeBlock::offsetOfGlobalObject()), regT3);
        jit.loadPtr(Address(regT3, OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpoint)), regT3);
        slowCase.append(jit.branch8(Equal, Address(regT3, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
    };
    
    auto emitCode = [&] (ResolveType resolveType) {
        switch (resolveType) {
        case GlobalProperty:
        case GlobalPropertyWithVarInjectionChecks: {
            // Structure check covers var injection since we don't cache structures for anything but the GlobalObject. Additionally, resolve_scope handles checking for the var injection.
            jit.loadPtr(Address(metadataGPR, OpGetFromScope::Metadata::offsetOfStructure()), regT1);
            slowCase.append(jit.branchTestPtr(Zero, regT1));
            jit.load32(Address(regT1, Structure::structureIDOffset()), regT1);
            slowCase.append(jit.branch32(NotEqual, Address(scopeGPR, JSCell::structureIDOffset()), regT1));

            jit.jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                jit.loadPtr(addressFor(CallFrameSlot::codeBlock), regT3);
                jit.loadPtr(Address(regT3, CodeBlock::offsetOfGlobalObject()), regT3);
                return jit.branchPtr(Equal, scopeGPR, regT3);
            }));

            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), regT1);

            if (ASSERT_ENABLED) {
                Jump isOutOfLine = jit.branch32(GreaterThanOrEqual, regT1, TrustedImm32(firstOutOfLineOffset));
                jit.abortWithReason(JITOffsetIsNotOutOfLine);
                isOutOfLine.link(&jit);
            }

            jit.loadPtr(Address(scopeGPR, JSObject::butterflyOffset()), regT0);
            jit.neg32(regT1);
            jit.signExtend32ToPtr(regT1, regT1);
            jit.load64(BaseIndex(regT0, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)), regT0);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR, Metadata::offsetOfOperand()), regT0);
            jit.loadPtr(Address(regT0), regT0);
            if (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks) // TDZ check.
                slowCase.append(jit.branchIfEmpty(regT0));
            break;
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            doVarInjectionCheck(needsVarInjectionChecks(resolveType));
            jit.loadPtr(Address(metadataGPR,  Metadata::offsetOfOperand()), regT3);
            static_assert(1 << 3 == sizeof(Register));
            jit.lshift64(TrustedImm32(3), regT3);
            jit.addPtr(scopeGPR, regT3);
            jit.loadPtr(Address(regT3, JSLexicalEnvironment::offsetOfVariables()), regT0);

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
        jit.load32(Address(metadataGPR, Metadata::offsetOfGetPutInfo()), regT0);
        jit.and32(TrustedImm32(GetPutInfo::typeBits), regT0); // Load ResolveType into T0

        auto emitCaseWithoutCheck = [&] (ResolveType resolveType) {
            Jump notCase = jit.branch32(NotEqual, regT0, TrustedImm32(resolveType));
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
    jit.store64(regT0, Address(metadataGPR, Metadata::offsetOfProfile() + ValueProfile::offsetOfFirstBucket()));

    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
    patchBuffer.link(slowCase, CodeLocationLabel(vm.getCTIStub(slow_op_get_from_scopeGenerator).retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "get_from_scope thunk");
}

MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_get_from_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

#if CPU(X86_64)
    jit.push(X86Registers::ebp);
#elif CPU(ARM64)
    jit.pushPair(framePointerRegister, linkRegister);
#endif

    using Metadata = OpGetFromScope::Metadata;
    constexpr GPRReg metadataGPR = regT4;
    constexpr GPRReg incomingBytecodeOffsetGPR = regT5;
    jit.store32(incomingBytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg codeBlockGPR = argumentGPR3;
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;
    static_assert(incomingBytecodeOffsetGPR != codeBlockGPR);
    static_assert(incomingBytecodeOffsetGPR != globalObjectGPR);
    static_assert(incomingBytecodeOffsetGPR != instructionGPR);
    static_assert(metadataGPR != codeBlockGPR);
    static_assert(metadataGPR != globalObjectGPR);
    static_assert(metadataGPR != instructionGPR);

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(incomingBytecodeOffsetGPR, instructionGPR);

    ASSERT(RegisterSet::calleeSaveRegisters().contains(GPRInfo::numberTagRegister));
    jit.move(metadataGPR, GPRInfo::numberTagRegister); // Preserve metadata in a callee saved register.
    jit.setupArguments<decltype(operationGetFromScope)>(globalObjectGPR, instructionGPR);
    jit.prepareCallOperation(vm);
    Call operation = jit.call(OperationPtrTag);
    Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

    jit.store64(regT0, Address(GPRInfo::numberTagRegister, Metadata::offsetOfProfile() + ValueProfile::offsetOfFirstBucket()));
    jit.move(TrustedImm64(JSValue::NumberTag), GPRInfo::numberTagRegister);

#if CPU(X86_64)
    jit.pop(X86Registers::ebp);
#elif CPU(ARM64)
    jit.popPair(framePointerRegister, linkRegister);
#endif
    jit.ret();

    exceptionCheck.link(&jit);
    jit.move(TrustedImm64(JSValue::NumberTag), GPRInfo::numberTagRegister);
    Jump jumpToHandler = jit.jump();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(operation, FunctionPtr<OperationPtrTag>(operationGetFromScope));
    auto handler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);
    patchBuffer.link(jumpToHandler, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_get_from_scope");
}
#endif // !ENABLE(EXTRA_CTI_THUNKS)

void JIT::emit_op_put_to_scope(const Instruction* currentInstruction)
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
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfStructure(), regT1);
            emitGetVirtualRegister(scope, regT0);
            addSlowCase(branchTestPtr(Zero, regT1));
            load32(Address(regT1, Structure::structureIDOffset()), regT1);
            addSlowCase(branch32(NotEqual, Address(regT0, JSCell::structureIDOffset()), regT1));

            emitGetVirtualRegister(value, regT2);

            jitAssert(scopedLambda<Jump(void)>([&] () -> Jump {
                loadGlobalObject(regT3);
                return branchPtr(Equal, regT0, regT3);
            }));

            loadPtr(Address(regT0, JSObject::butterflyOffset()), regT3);
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), regT1);
            negPtr(regT1);
            storePtr(regT2, BaseIndex(regT3, regT1, TimesEight, (firstOutOfLineOffset - 2) * sizeof(EncodedJSValue)));
            emitWriteBarrier(scope, value, ShouldFilterValue);
            break;
        }
        case GlobalVar:
        case GlobalVarWithVarInjectionChecks:
        case GlobalLexicalVar:
        case GlobalLexicalVarWithVarInjectionChecks: {
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);
            emitVarReadOnlyCheck(resolveType, regT0);

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), regT0);

            if (!isInitialization(bytecode.m_getPutInfo.initializationMode()) && (resolveType == GlobalLexicalVar || resolveType == GlobalLexicalVarWithVarInjectionChecks)) {
                // We need to do a TDZ check here because we can't always prove we need to emit TDZ checks statically.
                loadPtr(Address(regT0), regT1);
                addSlowCase(branchIfEmpty(regT1));
            }

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfWatchpointSet(), regT1);
            emitNotifyWriteWatchpoint(regT1);

            emitGetVirtualRegister(value, regT1);
            store64(regT1, Address(regT0));

            emitWriteBarrier(scope, value, ShouldFilterValue);
            break;
        }
        case ResolvedClosureVar:
        case ClosureVar:
        case ClosureVarWithVarInjectionChecks:
            emitVarInjectionCheck(needsVarInjectionChecks(resolveType), regT0);

            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfWatchpointSet(), regT0);
            loadPtrFromMetadata(bytecode, OpPutToScope::Metadata::offsetOfOperand(), regT2);
            emitNotifyWriteWatchpoint(regT0);
            emitGetVirtualRegister(value, regT1);
            emitGetVirtualRegister(scope, regT0);
            store64(regT1, BaseIndex(regT0, regT2, TimesEight, JSLexicalEnvironment::offsetOfVariables()));

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

void JIT::emitSlow_op_put_to_scope(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    linkAllSlowCases(iter);

    auto bytecode = currentInstruction->as<OpPutToScope>();
    ResolveType profiledResolveType = bytecode.metadata(m_profiledCodeBlock).m_getPutInfo.resolveType();
    if (profiledResolveType == ModuleVar) {
        // If any linked CodeBlock saw a ModuleVar, then all linked CodeBlocks are guaranteed
        // to also see ModuleVar.
        JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_throw_strict_mode_readonly_property_write_error);
        slowPathCall.call();
    } else {
#if !ENABLE(EXTRA_CTI_THUNKS)
        loadGlobalObject(argumentGPR0);
        callOperation(operationPutToScope, argumentGPR0, currentInstruction);
#else
        VM& vm = this->vm();
        uint32_t bytecodeOffset = m_bytecodeIndex.offset();
        ASSERT(BytecodeIndex(bytecodeOffset) == m_bytecodeIndex);
        ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex) == currentInstruction);

        constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
        move(TrustedImm32(bytecodeOffset), bytecodeOffsetGPR);

        emitNakedNearCall(vm.getCTIStub(slow_op_put_to_scopeGenerator).retaggedCode<NoPtrTag>());
#endif
    }
}

#if ENABLE(EXTRA_CTI_THUNKS)
MacroAssemblerCodeRef<JITThunkPtrTag> JIT::slow_op_put_to_scopeGenerator(VM& vm)
{
    // The thunk generated by this function can only work with the LLInt / Baseline JIT because
    // it makes assumptions about the right globalObject being available from CallFrame::codeBlock().
    // DFG/FTL may inline functions belonging to other globalObjects, which may not match
    // CallFrame::codeBlock().
    CCallHelpers jit;

#if CPU(X86_64)
    jit.push(X86Registers::ebp);
#elif CPU(ARM64)
    jit.tagReturnAddress();
    jit.pushPair(framePointerRegister, linkRegister);
#endif

    constexpr GPRReg bytecodeOffsetGPR = argumentGPR2;
    jit.store32(bytecodeOffsetGPR, tagFor(CallFrameSlot::argumentCountIncludingThis));

    constexpr GPRReg codeBlockGPR = argumentGPR3;
    constexpr GPRReg globalObjectGPR = argumentGPR0;
    constexpr GPRReg instructionGPR = argumentGPR1;

    jit.loadPtr(addressFor(CallFrameSlot::codeBlock), codeBlockGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfGlobalObject()), globalObjectGPR);
    jit.loadPtr(Address(codeBlockGPR, CodeBlock::offsetOfInstructionsRawPointer()), instructionGPR);
    jit.addPtr(bytecodeOffsetGPR, instructionGPR);

    jit.prepareCallOperation(vm);
    CCallHelpers::Call operation = jit.call(OperationPtrTag);
    CCallHelpers::Jump exceptionCheck = jit.emitNonPatchableExceptionCheck(vm);

#if CPU(X86_64)
    jit.pop(X86Registers::ebp);
#elif CPU(ARM64)
    jit.popPair(framePointerRegister, linkRegister);
#endif
    jit.ret();

    LinkBuffer patchBuffer(jit, GLOBAL_THUNK_ID, LinkBuffer::Profile::ExtraCTIThunk);
    patchBuffer.link(operation, FunctionPtr<OperationPtrTag>(operationPutToScope));
    auto handler = vm.getCTIStub(popThunkStackPreservesAndHandleExceptionGenerator);
    patchBuffer.link(exceptionCheck, CodeLocationLabel(handler.retaggedCode<NoPtrTag>()));
    return FINALIZE_CODE(patchBuffer, JITThunkPtrTag, "Baseline: slow_op_put_to_scope");
}
#endif

void JIT::emit_op_get_from_arguments(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpGetFromArguments>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister arguments = bytecode.m_arguments;
    int index = bytecode.m_index;
    
    emitGetVirtualRegister(arguments, regT0);
    load64(Address(regT0, DirectArguments::storageOffset() + index * sizeof(WriteBarrier<Unknown>)), regT0);
    emitValueProfilingSite(bytecode, regT0);
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
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, &vm(), regT0);
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
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister base = bytecode.m_base;
    unsigned index = bytecode.m_index;

    emitGetVirtualRegister(base, regT1);
    loadPtr(Address(regT1, JSInternalFieldObjectImpl<>::offsetOfInternalField(index)), regT0);

    emitValueProfilingSite(bytecode, regT0);
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

void JIT::emit_op_get_property_enumerator(const Instruction* currentInstruction)
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

    emitLoadStructure(vm(), regT0, regT1, regT2);
    loadPtr(Address(regT1, Structure::previousOrRareDataOffset()), regT1);
    genericCases.append(branchTestPtr(Zero, regT1));
    genericCases.append(branchIfStructure(regT1));
    loadPtr(Address(regT1, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag()), regT1);

    genericCases.append(branchTestPtr(Zero, regT1));
    genericCases.append(branchTestPtr(NonZero, regT1, TrustedImm32(StructureRareData::cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag)));
    emitPutVirtualRegister(dst, regT1);
    doneCases.append(jump());

    genericCases.link(this);
    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_get_property_enumerator);
    slowPathCall.call();

    doneCases.link(this);
}

void JIT::emit_op_enumerator_next(const Instruction* currentInstruction)
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

    JITSlowPathCall slowPathCall(this, currentInstruction, slow_path_enumerator_next);
    slowPathCall.call();

    done.link(this);
}

void JIT::emit_op_enumerator_get_by_val(const Instruction* currentInstruction)
{
    auto bytecode = currentInstruction->as<OpEnumeratorGetByVal>();
    VirtualRegister dst = bytecode.m_dst;
    VirtualRegister mode = bytecode.m_mode;
    VirtualRegister base = bytecode.m_base;
    VirtualRegister index = bytecode.m_index;
    VirtualRegister propertyName = bytecode.m_propertyName;
    VirtualRegister enumerator = bytecode.m_enumerator;

    JumpList doneCases;

    constexpr GPRReg resultGPR = BaselineEnumeratorGetByValRegisters::resultJSR.payloadGPR();
    constexpr GPRReg baseGPR = BaselineEnumeratorGetByValRegisters::baseJSR.payloadGPR();
    constexpr GPRReg propertyGPR = BaselineEnumeratorGetByValRegisters::propertyJSR.payloadGPR();
    constexpr GPRReg stubInfoGPR = BaselineEnumeratorGetByValRegisters::stubInfoGPR;
    constexpr GPRReg scratch1 = BaselineEnumeratorGetByValRegisters::scratch1;
    constexpr GPRReg scratch2 = BaselineEnumeratorGetByValRegisters::scratch2;
    constexpr GPRReg scratch3 = BaselineEnumeratorGetByValRegisters::scratch3;

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

    JITGetByValGenerator gen(
        nullptr, nullptr, JITType::BaselineJIT, CodeOrigin(m_bytecodeIndex), CallSiteIndex(m_bytecodeIndex), AccessType::GetByVal, RegisterSet::stubUnavailableRegisters(),
        JSValueRegs(baseGPR), JSValueRegs(propertyGPR), JSValueRegs(resultGPR), stubInfoGPR);

    auto [ stubInfo, stubInfoIndex ] = addUnlinkedStructureStubInfo();
    stubInfo->accessType = AccessType::GetByVal;
    stubInfo->bytecodeIndex = m_bytecodeIndex;
    gen.m_unlinkedStubInfoConstantIndex = stubInfoIndex;
    gen.m_unlinkedStubInfo = stubInfo;

    gen.generateBaselineDataICFastPath(*this, stubInfoIndex, stubInfoGPR);
    resetSP(); // We might OSR exit here, so we need to conservatively reset SP
    addSlowCase();
    m_getByVals.append(gen);

    doneCases.link(this);

    emitValueProfilingSite(bytecode, JSValueRegs(resultGPR));
    emitPutVirtualRegister(dst);
}

void JIT::emitSlow_op_enumerator_get_by_val(const Instruction* currentInstruction, Vector<SlowCaseEntry>::iterator& iter)
{
    generateGetByValSlowCase(currentInstruction->as<OpEnumeratorGetByVal>(), iter);
}

template <typename Bytecode, typename SlowPathFunctionType>
void JIT::emit_enumerator_has_propertyImpl(const Instruction* currentInstruction, const Bytecode& bytecode, SlowPathFunctionType generalCase)
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
    emitPutVirtualRegister(dst);
    Jump done = jump();

    slowCases.link(this);

    JITSlowPathCall slowPathCall(this, currentInstruction, generalCase);
    slowPathCall.call();

    done.link(this);
}

void JIT::emit_op_enumerator_in_by_val(const Instruction* currentInstruction)
{
    emit_enumerator_has_propertyImpl(currentInstruction, currentInstruction->as<OpEnumeratorInByVal>(), slow_path_enumerator_in_by_val);
}

void JIT::emit_op_enumerator_has_own_property(const Instruction* currentInstruction)
{
    emit_enumerator_has_propertyImpl(currentInstruction, currentInstruction->as<OpEnumeratorHasOwnProperty>(), slow_path_enumerator_has_own_property);
}

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
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, &vm(), regT1);
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

#endif // USE(JSVALUE64)

void JIT::emitWriteBarrier(VirtualRegister owner, WriteBarrierMode mode)
{
    ASSERT(mode == UnconditionalWriteBarrier || mode == ShouldFilterBase);
    emitWriteBarrier(owner, VirtualRegister(), mode);
}

void JIT::emitWriteBarrier(JSCell* owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), owner, regT0);
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, &vm(), owner);
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitWriteBarrier(GPRReg owner)
{
    Jump ownerIsRememberedOrInEden = barrierBranch(vm(), owner, selectScratchGPR(owner));
    callOperationNoExceptionCheck(operationWriteBarrierSlowPath, &vm(), owner);
    ownerIsRememberedOrInEden.link(this);
}

void JIT::emitVarInjectionCheck(bool needsVarInjectionChecks, GPRReg scratchGPR)
{
    if (!needsVarInjectionChecks)
        return;

    loadGlobalObject(scratchGPR);
    loadPtr(Address(scratchGPR, OBJECT_OFFSETOF(JSGlobalObject, m_varInjectionWatchpoint)), scratchGPR);
    addSlowCase(branch8(Equal, Address(scratchGPR, WatchpointSet::offsetOfState()), TrustedImm32(IsInvalidated)));
}

} // namespace JSC

#endif // ENABLE(JIT)
