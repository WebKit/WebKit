/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "JITInlineCacheGenerator.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "InlineAccess.h"
#include "JITInlines.h"
#include "LinkBuffer.h"
#include "StructureStubInfo.h"

namespace JSC {

JITInlineCacheGenerator::JITInlineCacheGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType,
    const RegisterSet& usedRegisters)
    : m_jitType(jitType)
{
    if (JITCode::isOptimizingJIT(m_jitType)) {
        ASSERT_UNUSED(codeBlock, codeBlock);
        ASSERT(stubInfos);
        m_stubInfo = stubInfos->add(accessType, codeOrigin);
        m_stubInfo->callSiteIndex = callSite;
        m_stubInfo->usedRegisters = usedRegisters;
    } else {
        ASSERT(!codeBlock);
        ASSERT(!stubInfos);
    }
}

void JITInlineCacheGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath, CodeLocationLabel<JITStubRoutinePtrTag> start)
{
    ASSERT(m_stubInfo);
    m_stubInfo->start = start;
    m_stubInfo->doneLocation = fastPath.locationOf<JSInternalPtrTag>(m_done);

    if (!JITCode::useDataIC(m_jitType))
        m_stubInfo->m_slowPathCallLocation = slowPath.locationOf<JSInternalPtrTag>(m_slowPathCall);
    m_stubInfo->slowPathStartLocation = slowPath.locationOf<JITStubRoutinePtrTag>(m_slowPathBegin);
}

void JITInlineCacheGenerator::generateBaselineDataICFastPath(JIT& jit, unsigned stubInfo, GPRReg stubInfoGPR)
{
    m_start = jit.label();
    RELEASE_ASSERT(JITCode::useDataIC(m_jitType));
    jit.loadConstant(stubInfo, stubInfoGPR);
    jit.farJump(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    m_done = jit.label();
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType,
    const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR)
    : JITInlineCacheGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSite, accessType, usedRegisters)
    , m_base(base)
    , m_value(value)
{
    if (m_stubInfo) {
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->valueGPR = value.payloadGPR();
        m_stubInfo->regs.thisGPR = InvalidGPRReg;
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = value.tagGPR();
        m_stubInfo->v.thisTagGPR = InvalidGPRReg;
#endif
    }
}

void JITByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    JITInlineCacheGenerator::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

void JITByIdGenerator::generateFastCommon(MacroAssembler& jit, size_t inlineICSize)
{
    // We generate the same code regardless of whether SharedIC is enabled because we still need to use InlineAccess
    // for the performance reason.
    m_start = jit.label();
    size_t startSize = jit.m_assembler.buffer().codeSize();
    m_slowPathJump = jit.jump();
    size_t jumpSize = jit.m_assembler.buffer().codeSize() - startSize;
    size_t nopsToEmitInBytes = inlineICSize - jumpSize;
    jit.emitNops(nopsToEmitInBytes);
    ASSERT(jit.m_assembler.buffer().codeSize() - startSize == inlineICSize);
    m_done = jit.label();
}

JITGetByIdGenerator::JITGetByIdGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, AccessType accessType)
    : JITByIdGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSite, accessType, usedRegisters, base, value, stubInfoGPR)
    , m_isLengthAccess(codeBlock && propertyName.uid() == codeBlock->vm().propertyNames->length.impl())
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
}

void JITGetByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    generateFastCommon(jit, m_isLengthAccess ? InlineAccess::sizeForLengthAccess() : InlineAccess::sizeForPropertyAccess());
}

static void generateGetByIdInlineAccess(JIT& jit, GPRReg stubInfoGPR, JSValueRegs baseJSR, GPRReg scratchGPR, JSValueRegs resultJSR)
{
    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratchGPR);
    auto doInlineAccess = jit.branch32(CCallHelpers::Equal, scratchGPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID()));
    jit.farJump(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    doInlineAccess.link(&jit);
    jit.load32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfByIdSelfOffset()), scratchGPR);
    jit.loadProperty(baseJSR.payloadGPR(), scratchGPR, resultJSR);
}

void JITGetByIdGenerator::generateBaselineDataICFastPath(JIT& jit, unsigned stubInfo, GPRReg stubInfoGPR)
{
    RELEASE_ASSERT(JITCode::useDataIC(m_jitType));

    m_start = jit.label();

    using BaselineGetByIdRegisters::baseJSR;
    using BaselineGetByIdRegisters::resultJSR;
    using BaselineGetByIdRegisters::scratchGPR;

    jit.loadConstant(stubInfo, stubInfoGPR);
    generateGetByIdInlineAccess(jit, stubInfoGPR, baseJSR, scratchGPR, resultJSR);

    m_done = jit.label();
}

JITGetByIdWithThisGenerator::JITGetByIdWithThisGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier, JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg stubInfoGPR)
    : JITByIdGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSite, AccessType::GetByIdWithThis, usedRegisters, base, value, stubInfoGPR)
{
    RELEASE_ASSERT(thisRegs.payloadGPR() != thisRegs.tagGPR());
    if (m_stubInfo) {
        m_stubInfo->regs.thisGPR = thisRegs.payloadGPR();
#if USE(JSVALUE32_64)
        m_stubInfo->v.thisTagGPR = thisRegs.tagGPR();
#endif
    }
}

void JITGetByIdWithThisGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITGetByIdWithThisGenerator::generateBaselineDataICFastPath(JIT& jit, unsigned stubInfo, GPRReg stubInfoGPR)
{
    RELEASE_ASSERT(JITCode::useDataIC(m_jitType));

    m_start = jit.label();

    using BaselineGetByIdWithThisRegisters::baseJSR;
    using BaselineGetByIdWithThisRegisters::resultJSR;
    using BaselineGetByIdWithThisRegisters::scratchGPR;

    jit.loadConstant(stubInfo, stubInfoGPR);
    generateGetByIdInlineAccess(jit, stubInfoGPR, baseJSR, scratchGPR, resultJSR);

    m_done = jit.label();
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters, CacheableIdentifier,
    JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, GPRReg scratch, 
    ECMAMode ecmaMode, PutKind putKind)
        : JITByIdGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSite, AccessType::PutById, usedRegisters, base, value, stubInfoGPR)
        , m_ecmaMode(ecmaMode)
        , m_putKind(putKind)
{
    if (m_stubInfo)
        m_stubInfo->usedRegisters.clear(scratch);
}

void JITPutByIdGenerator::generateBaselineDataICFastPath(JIT& jit, unsigned stubInfo, GPRReg stubInfoGPR)
{
    RELEASE_ASSERT(JITCode::useDataIC(m_jitType));

    m_start = jit.label();

    jit.loadConstant(stubInfo, stubInfoGPR);

    using BaselinePutByIdRegisters::baseJSR;
    using BaselinePutByIdRegisters::valueJSR;
    using BaselinePutByIdRegisters::scratchGPR;
    using BaselinePutByIdRegisters::scratch2GPR;

    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratchGPR);
    auto doInlineAccess = jit.branch32(CCallHelpers::Equal, scratchGPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID()));
    jit.farJump(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    doInlineAccess.link(&jit);
    jit.load32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfByIdSelfOffset()), scratchGPR);
    jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratchGPR, scratch2GPR);
    m_done = jit.label();
}

void JITPutByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    generateFastCommon(jit, InlineAccess::sizeForPropertyReplace());
}

V_JITOperation_GSsiJJC JITPutByIdGenerator::slowPathFunction()
{
    switch (m_putKind) {
    case PutKind::NotDirect:
        if (m_ecmaMode.isStrict())
            return operationPutByIdStrictOptimize;
        return operationPutByIdNonStrictOptimize;
    case PutKind::Direct:
        if (m_ecmaMode.isStrict())
            return operationPutByIdDirectStrictOptimize;
        return operationPutByIdDirectNonStrictOptimize;
    case PutKind::DirectPrivateFieldDefine:
        ASSERT(m_ecmaMode.isStrict());
        return operationPutByIdDefinePrivateFieldStrictOptimize;
    case PutKind::DirectPrivateFieldSet:
        ASSERT(m_ecmaMode.isStrict());
        return operationPutByIdSetPrivateFieldStrictOptimize;
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

JITDelByValGenerator::JITDelByValGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratch)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, AccessType::DeleteByVal, usedRegisters)
{
    ASSERT(base.payloadGPR() != result.payloadGPR());
#if USE(JSVALUE32_64)
    ASSERT(base.tagGPR() != result.tagGPR());
#endif
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = false;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.propertyGPR = property.payloadGPR();
        m_stubInfo->valueGPR = result.payloadGPR();
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = result.tagGPR();
        m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
        m_stubInfo->usedRegisters.clear(scratch);
    }
}

void JITDelByValGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITDelByIdGenerator::JITDelByIdGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier, JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratch)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, AccessType::DeleteByID, usedRegisters)
{
    ASSERT(base.payloadGPR() != result.payloadGPR());
#if USE(JSVALUE32_64)
    ASSERT(base.tagGPR() != result.tagGPR());
#endif
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = true;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.propertyGPR = InvalidGPRReg;
        m_stubInfo->valueGPR = result.payloadGPR();
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = result.tagGPR();
        m_stubInfo->v.propertyTagGPR = InvalidGPRReg;
#endif
        m_stubInfo->usedRegisters.clear(scratch);
    }
}

void JITDelByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITInByValGenerator::JITInByValGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, accessType, usedRegisters)
{
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = false;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.propertyGPR = property.payloadGPR();
        m_stubInfo->valueGPR = result.payloadGPR();
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = result.tagGPR();
        m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
    }
}

void JITInByValGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInByValGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_start.isSet());
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITInByIdGenerator::JITInByIdGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR)
    : JITByIdGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSite, AccessType::InById, usedRegisters, base, value, stubInfoGPR)
{
    // FIXME: We are not supporting fast path for "length" property.
    UNUSED_PARAM(propertyName);
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
}

void JITInByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITInByIdGenerator::generateBaselineDataICFastPath(JIT& jit, unsigned stubInfo, GPRReg stubInfoGPR)
{
    RELEASE_ASSERT(JITCode::useDataIC(m_jitType));

    m_start = jit.label();

    jit.loadConstant(stubInfo, stubInfoGPR);

    using BaselineInByIdRegisters::baseJSR;
    using BaselineInByIdRegisters::resultJSR;
    using BaselineInByIdRegisters::scratchGPR;

    CCallHelpers::JumpList done;

    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratchGPR);
    auto skipInlineAccess = jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID()));
    jit.boxBoolean(true, resultJSR);
    auto finished = jit.jump();

    skipInlineAccess.link(&jit);
    jit.farJump(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);

    finished.link(&jit);
    m_done = jit.label();
}

JITInstanceOfGenerator::JITInstanceOfGenerator(
    CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex,
    const RegisterSet& usedRegisters, GPRReg result, GPRReg value, GPRReg prototype, GPRReg stubInfoGPR,
    GPRReg scratch1, GPRReg scratch2, bool prototypeIsKnownObject)
    : JITInlineCacheGenerator(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, AccessType::InstanceOf, usedRegisters)
{
    if (m_stubInfo) {
        m_stubInfo->baseGPR = value;
        m_stubInfo->valueGPR = result;
        m_stubInfo->regs.prototypeGPR = prototype;
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = InvalidGPRReg;
        m_stubInfo->valueTagGPR = InvalidGPRReg;
        m_stubInfo->v.thisTagGPR = InvalidGPRReg;
#endif

        m_stubInfo->usedRegisters.clear(result);
        if (scratch1 != InvalidGPRReg)
            m_stubInfo->usedRegisters.clear(scratch1);
        if (scratch2 != InvalidGPRReg)
            m_stubInfo->usedRegisters.clear(scratch2);

        m_stubInfo->prototypeIsKnownObject = prototypeIsKnownObject;
        m_stubInfo->hasConstantIdentifier = false;
    }
}

void JITInstanceOfGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInstanceOfGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITGetByValGenerator::JITGetByValGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, accessType, usedRegisters)
    , m_base(base)
    , m_result(result)
{
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = false;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.propertyGPR = property.payloadGPR();
        m_stubInfo->valueGPR = result.payloadGPR();
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = result.tagGPR();
        m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
    }
}

void JITGetByValGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITGetByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITPutByValGenerator::JITPutByValGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs value, GPRReg arrayProfileGPR, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, accessType, usedRegisters)
    , m_base(base)
    , m_value(value)
{
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = false;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.propertyGPR = property.payloadGPR();
        m_stubInfo->valueGPR = value.payloadGPR();
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
        m_stubInfo->m_arrayProfileGPR = arrayProfileGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->valueTagGPR = value.tagGPR();
        m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
    }
}

void JITPutByValGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPutByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

JITPrivateBrandAccessGenerator::JITPrivateBrandAccessGenerator(CodeBlock* codeBlock, Bag<StructureStubInfo>* stubInfos, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs brand, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfos, jitType, codeOrigin, callSiteIndex, accessType, usedRegisters)
{
    ASSERT(accessType == AccessType::CheckPrivateBrand || accessType == AccessType::SetPrivateBrand);
    if (m_stubInfo) {
        m_stubInfo->hasConstantIdentifier = false;
        m_stubInfo->baseGPR = base.payloadGPR();
        m_stubInfo->regs.brandGPR = brand.payloadGPR();
        m_stubInfo->valueGPR = InvalidGPRReg;
        m_stubInfo->m_stubInfoGPR = stubInfoGPR;
#if USE(JSVALUE32_64)
        m_stubInfo->baseTagGPR = base.tagGPR();
        m_stubInfo->v.brandTagGPR = brand.tagGPR();
        m_stubInfo->valueTagGPR = InvalidGPRReg;
#endif
    }
}

void JITPrivateBrandAccessGenerator::generateFastPath(MacroAssembler& jit)
{
    ASSERT(m_stubInfo);
    m_start = jit.label();
    if (JITCode::useDataIC(m_jitType)) {
        jit.move(CCallHelpers::TrustedImmPtr(m_stubInfo), m_stubInfo->m_stubInfoGPR);
        jit.farJump(CCallHelpers::Address(m_stubInfo->m_stubInfoGPR, StructureStubInfo::offsetOfCodePtr()), JITStubRoutinePtrTag);
    } else
        m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPrivateBrandAccessGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    if (JITCode::useDataIC(m_jitType))
        m_stubInfo->m_codePtr = m_stubInfo->slowPathStartLocation;
}

} // namespace JSC

#endif // ENABLE(JIT)

