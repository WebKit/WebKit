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

#include "CacheableIdentifierInlines.h"
#include "CodeBlock.h"
#include "InlineAccess.h"
#include "LinkBuffer.h"
#include "StructureStubInfo.h"

namespace JSC {

static StructureStubInfo* garbageStubInfo()
{
    static StructureStubInfo* stubInfo = new StructureStubInfo(AccessType::GetById);
    return stubInfo;
}

JITInlineCacheGenerator::JITInlineCacheGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType,
    const RegisterSet& usedRegisters)
    : m_codeBlock(codeBlock)
{
    m_stubInfo = m_codeBlock ? m_codeBlock->addStubInfo(accessType) : garbageStubInfo();
    m_stubInfo->codeOrigin = codeOrigin;
    m_stubInfo->callSiteIndex = callSite;

    m_stubInfo->usedRegisters = usedRegisters;
}

void JITInlineCacheGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath, CodeLocationLabel<JITStubRoutinePtrTag> start)
{
    m_stubInfo->start = start;

    m_stubInfo->doneLocation = fastPath.locationOf<JSInternalPtrTag>(m_done);

    m_stubInfo->slowPathCallLocation = slowPath.locationOf<JSInternalPtrTag>(m_slowPathCall);
    m_stubInfo->slowPathStartLocation = slowPath.locationOf<JITStubRoutinePtrTag>(m_slowPathBegin);
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType,
    const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs value)
    : JITInlineCacheGenerator(codeBlock, codeOrigin, callSite, accessType, usedRegisters)
    , m_base(base)
    , m_value(value)
{
    m_stubInfo->baseGPR = base.payloadGPR();
    m_stubInfo->valueGPR = value.payloadGPR();
    m_stubInfo->regs.thisGPR = InvalidGPRReg;
#if USE(JSVALUE32_64)
    m_stubInfo->baseTagGPR = base.tagGPR();
    m_stubInfo->valueTagGPR = value.tagGPR();
    m_stubInfo->v.thisTagGPR = InvalidGPRReg;
#endif
}

void JITByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_start.isSet());
    JITInlineCacheGenerator::finalize(
        fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
}

void JITByIdGenerator::generateFastCommon(MacroAssembler& jit, size_t inlineICSize)
{
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
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value, AccessType accessType)
    : JITByIdGenerator(codeBlock, codeOrigin, callSite, accessType, usedRegisters, base, value)
    , m_isLengthAccess(propertyName.uid() == codeBlock->vm().propertyNames->length.impl())
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
}

void JITGetByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastCommon(jit, m_isLengthAccess ? InlineAccess::sizeForLengthAccess() : InlineAccess::sizeForPropertyAccess());
}

JITGetByIdWithThisGenerator::JITGetByIdWithThisGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier, JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs)
    : JITByIdGenerator(codeBlock, codeOrigin, callSite, AccessType::GetByIdWithThis, usedRegisters, base, value)
{
    RELEASE_ASSERT(thisRegs.payloadGPR() != thisRegs.tagGPR());

    m_stubInfo->regs.thisGPR = thisRegs.payloadGPR();
#if USE(JSVALUE32_64)
    m_stubInfo->v.thisTagGPR = thisRegs.tagGPR();
#endif
}

void JITGetByIdWithThisGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters, CacheableIdentifier,
    JSValueRegs base, JSValueRegs value, GPRReg scratch, 
    ECMAMode ecmaMode, PutKind putKind, PrivateFieldAccessKind privateFieldAccessKind)
        : JITByIdGenerator(codeBlock, codeOrigin, callSite, AccessType::Put, usedRegisters, base, value)
        , m_ecmaMode(ecmaMode)
        , m_putKind(putKind)
        , m_privateFieldAccessKind(privateFieldAccessKind)
{
    m_stubInfo->usedRegisters.clear(scratch);
}

void JITPutByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastCommon(jit, InlineAccess::sizeForPropertyReplace());
}

V_JITOperation_GSsiJJC JITPutByIdGenerator::slowPathFunction()
{
    if (m_privateFieldAccessKind != PrivateFieldAccessKind::None) {
        ASSERT(m_putKind == Direct);
        ASSERT(m_ecmaMode.isStrict());
        if (m_privateFieldAccessKind == PrivateFieldAccessKind::Create)
            return operationPutByIdDefinePrivateFieldStrictOptimize;
        return operationPutByIdPutPrivateFieldStrictOptimize;
    }

    if (m_ecmaMode.isStrict()) {
        if (m_putKind == Direct)
            return operationPutByIdDirectStrictOptimize;
        return operationPutByIdStrictOptimize;
    }
    if (m_putKind == Direct)
        return operationPutByIdDirectNonStrictOptimize;
    return operationPutByIdNonStrictOptimize;
}

JITDelByValGenerator::JITDelByValGenerator(CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg scratch)
    : Base(codeBlock, codeOrigin, callSiteIndex, AccessType::DeleteByVal, usedRegisters)
{
    m_stubInfo->hasConstantIdentifier = false;
    ASSERT(base.payloadGPR() != result.payloadGPR());
    m_stubInfo->baseGPR = base.payloadGPR();
    m_stubInfo->regs.propertyGPR = property.payloadGPR();
    m_stubInfo->valueGPR = result.payloadGPR();
#if USE(JSVALUE32_64)
    ASSERT(base.tagGPR() != result.tagGPR());
    m_stubInfo->baseTagGPR = base.tagGPR();
    m_stubInfo->valueTagGPR = result.tagGPR();
    m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
    m_stubInfo->usedRegisters.clear(scratch);
}

void JITDelByValGenerator::generateFastPath(MacroAssembler& jit)
{
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByValGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_slowPathJump.m_jump.isSet());
    Base::finalize(
        fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
}

JITDelByIdGenerator::JITDelByIdGenerator(CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier, JSValueRegs base, JSValueRegs result, GPRReg scratch)
    : Base(codeBlock, codeOrigin, callSiteIndex, AccessType::DeleteByID, usedRegisters)
{
    m_stubInfo->hasConstantIdentifier = true;
    ASSERT(base.payloadGPR() != result.payloadGPR());
    m_stubInfo->baseGPR = base.payloadGPR();
    m_stubInfo->regs.propertyGPR = InvalidGPRReg;
    m_stubInfo->valueGPR = result.payloadGPR();
#if USE(JSVALUE32_64)
    ASSERT(base.tagGPR() != result.tagGPR());
    m_stubInfo->baseTagGPR = base.tagGPR();
    m_stubInfo->valueTagGPR = result.tagGPR();
    m_stubInfo->v.propertyTagGPR = InvalidGPRReg;
#endif
    m_stubInfo->usedRegisters.clear(scratch);
}

void JITDelByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITDelByIdGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_slowPathJump.m_jump.isSet());
    Base::finalize(
        fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
}

JITInByIdGenerator::JITInByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    CacheableIdentifier propertyName, JSValueRegs base, JSValueRegs value)
    : JITByIdGenerator(codeBlock, codeOrigin, callSite, AccessType::In, usedRegisters, base, value)
{
    // FIXME: We are not supporting fast path for "length" property.
    UNUSED_PARAM(propertyName);
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
}

void JITInByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastCommon(jit, InlineAccess::sizeForPropertyAccess());
}

JITInstanceOfGenerator::JITInstanceOfGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex,
    const RegisterSet& usedRegisters, GPRReg result, GPRReg value, GPRReg prototype,
    GPRReg scratch1, GPRReg scratch2, bool prototypeIsKnownObject)
    : JITInlineCacheGenerator(
        codeBlock, codeOrigin, callSiteIndex, AccessType::InstanceOf, usedRegisters)
{
    m_stubInfo->baseGPR = value;
    m_stubInfo->valueGPR = result;
    m_stubInfo->regs.prototypeGPR = prototype;
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

void JITInstanceOfGenerator::generateFastPath(MacroAssembler& jit)
{
    m_jump = jit.patchableJump();
    m_done = jit.label();
}

void JITInstanceOfGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    JITInlineCacheGenerator::finalize(
        fastPath, slowPath,
        fastPath.locationOf<JITStubRoutinePtrTag>(m_jump));
    
    fastPath.link(m_jump.m_jump, slowPath.locationOf<NoPtrTag>(m_slowPathBegin));
}

JITGetByValGenerator::JITGetByValGenerator(CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, AccessType accessType, const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs property, JSValueRegs result)
    : Base(codeBlock, codeOrigin, callSiteIndex, accessType, usedRegisters)
    , m_base(base)
    , m_result(result)
{
    m_stubInfo->hasConstantIdentifier = false;

    m_stubInfo->baseGPR = base.payloadGPR();
    m_stubInfo->regs.propertyGPR = property.payloadGPR();
    m_stubInfo->valueGPR = result.payloadGPR();
#if USE(JSVALUE32_64)
    m_stubInfo->baseTagGPR = base.tagGPR();
    m_stubInfo->valueTagGPR = result.tagGPR();
    m_stubInfo->v.propertyTagGPR = property.tagGPR();
#endif
}

void JITGetByValGenerator::generateFastPath(MacroAssembler& jit)
{
    m_start = jit.label();
    m_slowPathJump = jit.patchableJump();
    m_done = jit.label();
}

void JITGetByValGenerator::finalize(
    LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    ASSERT(m_start.isSet());
    Base::finalize(
        fastPath, slowPath, fastPath.locationOf<JITStubRoutinePtrTag>(m_start));
}

} // namespace JSC

#endif // ENABLE(JIT)

