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
#include "PropertyInlineCache.h"

namespace JSC {

JITInlineCacheGenerator::JITInlineCacheGenerator(CodeBlock*, CompileTimePropertyInlineCache propertyCache, JITType, CodeOrigin, AccessType accessType)
    : m_accessType(accessType)
{
    WTF::visit(WTF::makeVisitor(
        [&](PropertyInlineCache* propertyCache) {
            m_propertyCache = propertyCache;
        },
        [&](BaselineUnlinkedPropertyInlineCache* propertyCache) {
            m_unlinkedPropertyCache = propertyCache;
        }
#if ENABLE(DFG_JIT)
        ,
        [&](DFG::UnlinkedPropertyInlineCache* propertyCache) {
            m_unlinkedPropertyCache = propertyCache;
        }
#endif
        ), propertyCache);
}

void JITInlineCacheGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath, CodeLocationLabel<JITStubRoutinePtrTag> start)
{
    ASSERT(m_propertyCache);
    auto& repatchingIC = downcast<RepatchingPropertyInlineCache>(*m_propertyCache);
    repatchingIC.startLocation = start;
    m_propertyCache->doneLocation = fastPath.locationOf<JSInternalPtrTag>(m_done);
    repatchingIC.m_slowPathCallLocation = slowPath.locationOf<JSInternalPtrTag>(m_slowPathCall);
    m_propertyCache->slowPathStartLocation = slowPath.locationOf<JITStubRoutinePtrTag>(m_slowPathBegin);
}

void JITInlineCacheGenerator::generateDataICFastPath(CCallHelpers& jit, GPRReg propertyCacheGPR)
{
    m_start = jit.label();
    jit.loadPtr(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfHandler()), GPRInfo::handlerGPR);
    jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    m_done = jit.label();
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, AccessType accessType,
    JSValueRegs base, JSValueRegs value)
    : JITInlineCacheGenerator(codeBlock, propertyCache, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_value(value)
{
}

void JITByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    JITInlineCacheGenerator::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

void JITByIdGenerator::generateFastCommon(CCallHelpers& jit, size_t inlineICSize)
{
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    jit.padBeforePatch(); // On ARMv7, this ensures that the patchable jump does not make the inline code too large.
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
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR, AccessType accessType, CacheType cacheType)
    : JITByIdGenerator(codeBlock, propertyCache, jitType, codeOrigin, accessType, base, value)
    , m_isLengthAccess(codeBlock && propertyName.uid() == codeBlock->vm().propertyNames->length.impl())
    , m_cacheType(cacheType)
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSite, usedRegisters, propertyName, base, value, propertyCacheGPR);
    }, propertyCache);
}

static void generateGetByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg propertyCacheGPR, JSValueRegs baseJSR, GPRReg scratch1GPR, JSValueRegs resultJSR, CacheType cacheType)
{
    CCallHelpers::JumpList slowCases;
    CCallHelpers::JumpList doneCases;

    switch (cacheType) {
    case CacheType::GetByIdSelf: {
        jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
        slowCases.append(jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfInlineAccessBaseStructureID())));
        jit.load32(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfByIdSelfOffset()), scratch1GPR);
        jit.loadProperty(baseJSR.payloadGPR(), scratch1GPR, resultJSR);
        doneCases.append(jit.jump());
        break;
    }
    case CacheType::GetByIdPrototype: {
        jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
        slowCases.append(jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfInlineAccessBaseStructureID())));
        jit.load32(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfByIdSelfOffset()), scratch1GPR);
        jit.loadPtr(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfInlineHolder()), resultJSR.payloadGPR());
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
    jit.loadPtr(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfHandler()), GPRInfo::handlerGPR);
    jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    doneCases.link(&jit);
}

void JITGetByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    generateFastCommon(jit, m_isLengthAccess ? InlineAccess::sizeForLengthAccess() : InlineAccess::sizeForPropertyAccess());
}

void JITGetByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    m_start = jit.label();

    using BaselineJITRegisters::GetById::baseJSR;
    using BaselineJITRegisters::GetById::resultJSR;
    using BaselineJITRegisters::GetById::propertyCacheGPR;
    using BaselineJITRegisters::GetById::scratch1GPR;

    generateGetByIdInlineAccessBaselineDataIC(jit, propertyCacheGPR, baseJSR, scratch1GPR, resultJSR, m_cacheType);

    m_done = jit.label();
}

JITGetByIdWithThisGenerator::JITGetByIdWithThisGenerator(
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg propertyCacheGPR)
    : JITByIdGenerator(codeBlock, propertyCache, jitType, codeOrigin, AccessType::GetByIdWithThis, base, value)
{
    RELEASE_ASSERT(thisRegs.payloadGPR() != thisRegs.tagGPR());
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, AccessType::GetByIdWithThis, CacheType::GetByIdSelf, codeOrigin, callSite, usedRegisters, propertyName, value, base, thisRegs, propertyCacheGPR);
    }, propertyCache);
}

void JITGetByIdWithThisGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITGetByIdWithThisGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    m_start = jit.label();

    using BaselineJITRegisters::GetByIdWithThis::baseJSR;
    using BaselineJITRegisters::GetByIdWithThis::resultJSR;
    using BaselineJITRegisters::GetByIdWithThis::propertyCacheGPR;
    using BaselineJITRegisters::GetByIdWithThis::scratch1GPR;

    generateGetByIdInlineAccessBaselineDataIC(jit, propertyCacheGPR, baseJSR, scratch1GPR, resultJSR, CacheType::GetByIdSelf);

    m_done = jit.label();
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
    JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR, GPRReg scratch,
    AccessType accessType)
        : JITByIdGenerator(codeBlock, propertyCache, jitType, codeOrigin, accessType, base, value)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::PutByIdReplace, codeOrigin, callSite, usedRegisters, propertyName, base, value, propertyCacheGPR, scratch);
    }, propertyCache);
}

static void generatePutByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg propertyCacheGPR, JSValueRegs baseJSR, JSValueRegs valueJSR, GPRReg scratch1GPR, GPRReg scratch2GPR)
{
    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
    auto doNotInlineAccess = jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfInlineAccessBaseStructureID()));
    jit.load32(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfByIdSelfOffset()), scratch1GPR);
    // The second scratch can be the same to baseJSR.
    jit.storeProperty(valueJSR, baseJSR.payloadGPR(), scratch1GPR, scratch2GPR);
    auto done = jit.jump();
    doNotInlineAccess.link(&jit);
    jit.loadPtr(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfHandler()), GPRInfo::handlerGPR);
    jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    done.link(&jit);
}

void JITPutByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PutById::baseJSR;
    using BaselineJITRegisters::PutById::valueJSR;
    using BaselineJITRegisters::PutById::propertyCacheGPR;
    using BaselineJITRegisters::PutById::scratch1GPR;

    m_start = jit.label();
    // The second scratch can be the same to baseJSR. In Baseline JIT, we clobber the baseJSR to save registers.
    generatePutByIdInlineAccessBaselineDataIC(jit, propertyCacheGPR, baseJSR, valueJSR, scratch1GPR, baseJSR.payloadGPR());
    m_done = jit.label();
}

void JITPutByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    generateFastCommon(jit, InlineAccess::sizeForPropertyReplace());
}

JITDelByValGenerator::JITDelByValGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, propertyCacheGPR);
    }, propertyCache);
}

void JITDelByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::DelByVal::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITDelByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITDelByIdGenerator::JITDelByIdGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs result, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, propertyName, base, result, propertyCacheGPR);
    }, propertyCache);
}

void JITDelByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::DelById::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITDelByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITInByValGenerator::JITInByValGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, arrayProfileGPR, propertyCacheGPR);
    }, propertyCache);
}

void JITInByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::InByVal::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITInByValGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_start.isSet());
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITInByIdGenerator::JITInByIdGenerator(
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR)
    : JITByIdGenerator(codeBlock, propertyCache, jitType, codeOrigin, AccessType::InById, base, value)
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, AccessType::InById, CacheType::InByIdSelf, codeOrigin, callSite, usedRegisters, propertyName, base, value, propertyCacheGPR);
    }, propertyCache);
}

static void generateInByIdInlineAccessBaselineDataIC(CCallHelpers& jit, GPRReg propertyCacheGPR, JSValueRegs baseJSR, GPRReg scratch1GPR, JSValueRegs resultJSR)
{
    jit.load32(CCallHelpers::Address(baseJSR.payloadGPR(), JSCell::structureIDOffset()), scratch1GPR);
    auto skipInlineAccess = jit.branch32(CCallHelpers::NotEqual, scratch1GPR, CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfInlineAccessBaseStructureID()));
    jit.boxBoolean(true, resultJSR);
    auto finished = jit.jump();
    skipInlineAccess.link(&jit);
    jit.loadPtr(CCallHelpers::Address(propertyCacheGPR, PropertyInlineCache::offsetOfHandler()), GPRInfo::handlerGPR);
    jit.call(CCallHelpers::Address(GPRInfo::handlerGPR, InlineCacheHandler::offsetOfCallTarget()), JITStubRoutinePtrTag);
    finished.link(&jit);
}

void JITInByIdGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

void JITInByIdGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::InById::baseJSR;
    using BaselineJITRegisters::InById::resultJSR;
    using BaselineJITRegisters::InById::propertyCacheGPR;
    using BaselineJITRegisters::InById::scratch1GPR;

    m_start = jit.label();
    generateInByIdInlineAccessBaselineDataIC(jit, propertyCacheGPR, baseJSR, scratch1GPR, resultJSR);
    m_done = jit.label();
}

JITInstanceOfGenerator::JITInstanceOfGenerator(
    CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex,
    const RegisterSet& usedRegisters, GPRReg result, GPRReg value, GPRReg prototype, GPRReg propertyCacheGPR,
    bool prototypeIsKnownObject)
    : JITInlineCacheGenerator(codeBlock, propertyCache, jitType, codeOrigin, AccessType::InstanceOf)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, AccessType::InstanceOf, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, result, value, prototype, propertyCacheGPR, prototypeIsKnownObject);
    }, propertyCache);
}

void JITInstanceOfGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITInstanceOfGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::Instanceof::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITInstanceOfGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITGetByValGenerator::JITGetByValGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_result(result)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, result, arrayProfileGPR, propertyCacheGPR);
    }, propertyCache);
}

void JITGetByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITGetByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::GetByVal::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITGetByValGenerator::generateEmptyPath(CCallHelpers& jit)
{
    m_start = jit.label();
    m_done = jit.label();
}

void JITGetByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITGetByValWithThisGenerator::JITGetByValWithThisGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs thisRegs, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_result(result)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, thisRegs, result, arrayProfileGPR, propertyCacheGPR);
    }, propertyCache);
}

void JITGetByValWithThisGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

#if USE(JSVALUE64)
void JITGetByValWithThisGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::GetByValWithThis::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}
#endif

void JITGetByValWithThisGenerator::generateEmptyPath(CCallHelpers& jit)
{
    m_start = jit.label();
    m_done = jit.label();
}

void JITGetByValWithThisGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITPutByValGenerator::JITPutByValGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs value, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
    , m_base(base)
    , m_value(value)
{
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, property, value, arrayProfileGPR, propertyCacheGPR);
    }, propertyCache);
}

void JITPutByValGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPutByValGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PutByVal::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITPutByValGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

JITPrivateBrandAccessGenerator::JITPrivateBrandAccessGenerator(CodeBlock* codeBlock, CompileTimePropertyInlineCache propertyCache, JITType jitType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs brand, GPRReg propertyCacheGPR)
    : Base(codeBlock, propertyCache, jitType, codeOrigin, accessType)
{
    ASSERT(accessType == AccessType::CheckPrivateBrand || accessType == AccessType::SetPrivateBrand);
    WTF::visit([&](auto* propertyCache) {
        setUpPropertyInlineCache(*propertyCache, codeBlock, accessType, CacheType::Unset, codeOrigin, callSiteIndex, usedRegisters, base, brand, propertyCacheGPR);
    }, propertyCache);
}

void JITPrivateBrandAccessGenerator::generateFastPath(CCallHelpers& jit)
{
    ASSERT(m_propertyCache);
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITPrivateBrandAccessGenerator::generateDataICFastPath(CCallHelpers& jit)
{
    using BaselineJITRegisters::PrivateBrand::propertyCacheGPR;
    JITInlineCacheGenerator::generateDataICFastPath(jit, propertyCacheGPR);
}

void JITPrivateBrandAccessGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_propertyCache);
    Base::finalize(fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
    ASSERT(is<RepatchingPropertyInlineCache>(*m_propertyCache));
}

} // namespace JSC

#endif // ENABLE(JIT)

