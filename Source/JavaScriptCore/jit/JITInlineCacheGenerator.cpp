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

#include "BaselineJITRegisters.h"
#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "DFGJITCompiler.h"
#include "InlineAccess.h"
#include "JITInlines.h"
#include "LinkBuffer.h"
#include "StructureStubInfo.h"

namespace JSC {

JITInlineCacheGenerator::JITInlineCacheGenerator(CodeBlock*, CompileTimeStructureStubInfo stubInfo, JITType, CodeOrigin, AccessType accessType)
    : m_accessType(accessType)
{
    std::visit(WTF::makeVisitor(
        [&](StructureStubInfo* stubInfo) {
            m_stubInfo = stubInfo;
        },
        [&](BaselineUnlinkedStructureStubInfo* stubInfo) {
            m_unlinkedStubInfo = stubInfo;
        }
#if ENABLE(DFG_JIT)
        ,
        [&](DFG::UnlinkedStructureStubInfo* stubInfo) {
            m_unlinkedStubInfo = stubInfo;
        }
#endif
        ), stubInfo);
}

void JITInlineCacheGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath, CodeLocationLabel<JITStubRoutinePtrTag> start)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_stubInfo->startLocation = start;
    m_stubInfo->doneLocation = fastPath.locationOf<JSInternalPtrTag>(m_done);
    m_stubInfo->m_slowPathCallLocation = slowPath.locationOf<JSInternalPtrTag>(m_slowPathCall);
    m_stubInfo->slowPathStartLocation = slowPath.locationOf<JITStubRoutinePtrTag>(m_slowPathBegin);
}

void JITInlineCacheGenerator::generateDataICFastPath(CCallHelpers& jit, GPRReg stubInfoGPR)
{
    m_start = jit.label();
    if (Options::useHandlerIC()) {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), GPRInfo::handlerGPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    } else {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), jit.scratchRegister());
        jit.farJump(CCallHelpers::Address(jit.scratchRegister(), InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    }
    m_done = jit.label();
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, AccessType accessType,
    JSValueRegs base, JSValueRegs value)
    : JITInlineCacheGenerator(codeBlock, stubInfo, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_value(value)
{
}

void JITByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    JITInlineCacheGenerator::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

void JITByIdGenerator::generateFastCommon(CCallHelpers& jit, size_t inlineICSize)
{
    ASSERT(!m_stubInfo->useDataIC);
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
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSetBuilder& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, AccessType accessType, CacheType cacheType)
    : JITByIdGenerator(codeBlock, stubInfo, jitType, codeOrigin, accessType, base, value)
    , m_isLengthAccess(codeBlock && propertyName.uid() == codeBlock->vm().propertyNames->length.impl())
    , m_cacheType(cacheType)
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, cacheType, codeOrigin, callSite, usedRegisters, propertyName, base, value, stubInfoGPR);
    }, stubInfo);
}

static void generateGetByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg stubInfoGPR, JSValueRegs baseJSR, GPRReg scratch1GPR, JSValueRegs resultJSR, CacheType cacheType)
{
    CCallHelpers::JumpList slowCases;
    CCallHelpers::JumpList doneCases;

    switch (cacheType) {
    case CacheType::GetByIdSelf: {
        jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
        slowCases.append(jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID())));
        jit.load32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfByIdSelfOffset()), scratch1GPR);
        jit.loadProperty(baseJSR.payloadGPR(), scratch1GPR, resultJSR);
        doneCases.append(jit.jump());
        break;
    }
    case CacheType::GetByIdPrototype: {
        jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
        slowCases.append(jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID())));
        jit.load32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfByIdSelfOffset()), scratch1GPR);
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineHolder()), resultJSR.payloadGPR());
        jit.loadProperty(resultJSR.payloadGPR(), scratch1GPR, resultJSR);
        doneCases.append(jit.jump());
        break;
    }
    case CacheType::ArrayLength: {
        jit.load8(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::indexingTypeAndMiscOffset()), scratch1GPR);
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, scratch1GPR, CCallHelpers::TrustedImm32(IsArray)));
        slowCases.append(jit.branchTest32(CCallHelpers::Zero, scratch1GPR, CCallHelpers::TrustedImm32(IndexingShapeMask)));
        jit.loadPtr(CCallHelpers::Address(baseJSR.payloadGPR(), JSObject::butterflyOffset()), scratch1GPR);
        jit.load32(CCallHelpers::Address(scratch1GPR, ArrayStorage::lengthOffset()), scratch1GPR);
        slowCases.append(jit.branch32(CCallHelpers::LessThan, scratch1GPR, CCallHelpers::TrustedImm32(0)));
        jit.boxInt32(scratch1GPR, resultJSR);
        doneCases.append(jit.jump());
        break;
    }
    default:
        break;
    }

    slowCases.link(&jit);
    if (Options::useHandlerIC()) {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), GPRInfo::handlerGPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    } else {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), jit.scratchRegister());
        jit.farJump(CCallHelpers::Address(jit.scratchRegister(), InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    }
    doneCases.link(&jit);
}

void JITGetByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    generateFastCommon(jit, m_isLengthAccess ? InlineAccess::sizeForLengthAccess() : InlineAccess::sizeForPropertyAccess());
}

void JITGetByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    m_start = jit.label();

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::stubInfoGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;

    generateGetByIdInlineAccessBaselineDataIC(jit, stubInfoGPR, baseJSR, scratch1GPR, resultJSR, m_cacheType);

    m_done = jit.label();
}

JITGetByIdWithThisGenerator::JITGetByIdWithThisGenerator(
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSetBuilder& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg stubInfoGPR)
    : JITByIdGenerator(codeBlock, stubInfo, jitType, codeOrigin, AccessType::GetByIdWithThis, base, value)
{
    RELEASE_ASSERT(thisRegs.payloadGPR() != thisRegs.tagGPR());
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, AccessType::GetByIdWithThis, CacheType::GetByIdSelf, codeOrigin, callSite, usedRegisters, propertyName, value, base, thisRegs, stubInfoGPR);
    }, stubInfo);
}

void JITGetByIdWithThisGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITGetByIdWithThisGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    m_start = jit.label();

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::resultJSR;
    using BaselineJITRegisters::GetByIdWithThis::stubInfoGPR;
    using BaselineJITRegisters::GetByIdWithThis::scratch1GPR;

    generateGetByIdInlineAccessBaselineDataIC(jit, stubInfoGPR, baseJSR, scratch1GPR, resultJSR, CacheType::GetByIdSelf);

    m_done = jit.label();
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSetBuilder& usedRegisters, CacheableIdentifier propertyName,
    JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, GPRReg scratch,
    AccessType accessType)
        : JITByIdGenerator(codeBlock, stubInfo, jitType, codeOrigin, accessType, base, value)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::PutByIdReplace, codeOrigin, callSite, usedRegisters, propertyName, base, value, stubInfoGPR, scratch);
    }, stubInfo);
}

static void generatePutByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg stubInfoGPR, JSValueRegs baseJSR, JSValueRegs valueJSR, GPRReg scratch1GPR, GPRReg scratch2GPR)
{
    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
    auto doNotInlineAccess = jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID()));
    jit.load32(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfByIdSelfOffset()), scratch1GPR);
    // The second scratch can be the same to baseJSR.
    jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    auto done = jit.jump();
    doNotInlineAccess.link(&jit);
    if (Options::useHandlerIC()) {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), GPRInfo::handlerGPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    } else {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), jit.scratchRegister());
        jit.farJump(CCallHelpers::Address(jit.scratchRegister(), InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    }
    done.link(&jit);
}

void JITPutByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::stubInfoGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;

    m_start = jit.label();
    // The second scratch can be the same to baseJSR. In Baseline JIT, we clobber the baseJSR to save registers.
    generatePutByIdInlineAccessBaselineDataIC(jit, stubInfoGPR, baseJSR, valueJSR, scratch1GPR, baseJSR.payloadGPR());
    m_done = jit.label();
}

void JITPutByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    generateFastCommon(jit, InlineAccess::sizeForPropertyReplace());
}

JITDelByValGenerator::JITDelByValGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, stubInfoGPR);
    }, stubInfo);
}

void JITDelByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::DelByVal::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITDelByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITDelByIdGenerator::JITDelByIdGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, propertyName, base, result, stubInfoGPR);
    }, stubInfo);
}

void JITDelByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::DelById::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITDelByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITInByValGenerator::JITInByValGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, arrayProfileGPR, stubInfoGPR);
    }, stubInfo);
}

void JITInByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::InByVal::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITInByValGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_start.isSet());
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITInByIdGenerator::JITInByIdGenerator(
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSetBuilder& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR)
    : JITByIdGenerator(codeBlock, stubInfo, jitType, codeOrigin, AccessType::InById, base, value)
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, AccessType::InById, CacheType::InByIdSelf, codeOrigin, callSite, usedRegisters, propertyName, base, value, stubInfoGPR);
    }, stubInfo);
}

static void generateInByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg stubInfoGPR, JSValueRegs baseJSR, GPRReg scratch1GPR, JSValueRegs resultJSR)
{
    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
    auto skipInlineAccess = jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfInlineAccessBaseStructureID()));
    jit.boxBoolean(true, resultJSR);
    auto finished = jit.jump();
    skipInlineAccess.link(&jit);
    if (Options::useHandlerIC()) {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), GPRInfo::handlerGPR);
        jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    } else {
        jit.loadPtr(CCallHelpers::Address(stubInfoGPR, StructureStubInfo::offsetOfHandler()), jit.scratchRegister());
        jit.farJump(CCallHelpers::Address(jit.scratchRegister(), InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    }
    finished.link(&jit);
}

void JITInByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITInByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::InById::baseJSR;
    using BaselineJITRegisters::InById::resultJSR;
    using BaselineJITRegisters::InById::stubInfoGPR;
    using BaselineJITRegisters::InById::scratch1GPR;

    m_start = jit.label();
    generateInByIdInlineAccessBaselineDataIC(jit, stubInfoGPR, baseJSR, scratch1GPR, resultJSR);
    m_done = jit.label();
}

JITInstanceOfGenerator::JITInstanceOfGenerator(
    CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex,
    const RegisterSetBuilder& usedRegisters, GPRReg result, GPRReg value, GPRReg prototype, GPRReg stubInfoGPR,
    bool prototypeIsKnownObject)
    : JITInlineCacheGenerator(codeBlock, stubInfo, jitType, codeOrigin, AccessType::InstanceOf)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, AccessType::InstanceOf, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, result, value, prototype, stubInfoGPR, prototypeIsKnownObject);
    }, stubInfo);
}

void JITInstanceOfGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInstanceOfGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::Instanceof::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITInstanceOfGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITGetByValGenerator::JITGetByValGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_result(result)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, arrayProfileGPR, stubInfoGPR);
    }, stubInfo);
}

void JITGetByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITGetByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::GetByVal::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITGetByValGenerator::generateEmptyPath(CCallHelpers& jit)
{
    m_start = jit.label();
    m_done = jit.label();
}

void JITGetByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITGetByValWithThisGenerator::JITGetByValWithThisGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs thisRegs, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_result(result)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, thisRegs, result, arrayProfileGPR, stubInfoGPR);
    }, stubInfo);
}

void JITGetByValWithThisGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

#if USE(JSVALUE64)
void JITGetByValWithThisGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::GetByValWithThis::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}
#endif

void JITGetByValWithThisGenerator::generateEmptyPath(CCallHelpers& jit)
{
    m_start = jit.label();
    m_done = jit.label();
}

void JITGetByValWithThisGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITPutByValGenerator::JITPutByValGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs value, GPRReg arrayProfileGPR, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_value(value)
{
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, value, arrayProfileGPR, stubInfoGPR);
    }, stubInfo);
}

void JITPutByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPutByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PutByVal::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITPutByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

JITPrivateBrandAccessGenerator::JITPrivateBrandAccessGenerator(CodeBlock* codeBlock, CompileTimeStructureStubInfo stubInfo, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSetBuilder& usedRegisters, JSValueRegs base, JSValueRegs brand, GPRReg stubInfoGPR)
    : Base(codeBlock, stubInfo, jitType, codeOrigin, accessType)
{
    ASSERT(accessType == AccessType::CheckPrivateBrand || accessType == AccessType::SetPrivateBrand);
    std::visit([&](auto* stubInfo) {
        setUpStubInfo(*stubInfo, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, brand, stubInfoGPR);
    }, stubInfo);
}

void JITPrivateBrandAccessGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_stubInfo);
    ASSERT(!m_stubInfo->useDataIC);
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPrivateBrandAccessGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PrivateBrand::stubInfoGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, stubInfoGPR);
}

void JITPrivateBrandAccessGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_stubInfo);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(!m_stubInfo->useDataIC);
}

} // namespace JSC

#endif // ENABLE(JIT)

