/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "CodeBlock.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"

namespace JSC {

static StructureStubInfo* garbageStubInfo()
{
    static StructureStubInfo* stubInfo = new StructureStubInfo(AccessType::Get);
    return stubInfo;
}

JITInlineCacheGenerator::JITInlineCacheGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType)
    : m_codeBlock(codeBlock)
{
    m_stubInfo = m_codeBlock ? m_codeBlock->addStubInfo(accessType) : garbageStubInfo();
    m_stubInfo->codeOrigin = codeOrigin;
    m_stubInfo->callSiteIndex = callSite;
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, AccessType accessType,
    const RegisterSet& usedRegisters, JSValueRegs base, JSValueRegs value)
    : JITInlineCacheGenerator(codeBlock, codeOrigin, callSite, accessType)
    , m_base(base)
    , m_value(value)
{
    m_stubInfo->patch.usedRegisters = usedRegisters;
    
    m_stubInfo->patch.baseGPR = static_cast<int8_t>(base.payloadGPR());
    m_stubInfo->patch.valueGPR = static_cast<int8_t>(value.payloadGPR());
#if USE(JSVALUE32_64)
    m_stubInfo->patch.baseTagGPR = static_cast<int8_t>(base.tagGPR());
    m_stubInfo->patch.valueTagGPR = static_cast<int8_t>(value.tagGPR());
#endif
}

void JITByIdGenerator::finalize(LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    CodeLocationCall callReturnLocation = slowPath.locationOf(m_call);
    m_stubInfo->callReturnLocation = callReturnLocation;
    m_stubInfo->patch.deltaCheckImmToCall = MacroAssembler::differenceBetweenCodePtr(
        fastPath.locationOf(m_structureImm), callReturnLocation);
    m_stubInfo->patch.deltaCallToJump = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_structureCheck));
#if USE(JSVALUE64)
    m_stubInfo->patch.deltaCallToLoadOrStore = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_loadOrStore));
#else
    m_stubInfo->patch.deltaCallToTagLoadOrStore = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_tagLoadOrStore));
    m_stubInfo->patch.deltaCallToPayloadLoadOrStore = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_loadOrStore));
#endif
    m_stubInfo->patch.deltaCallToSlowCase = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, slowPath.locationOf(m_slowPathBegin));
    m_stubInfo->patch.deltaCallToDone = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_done));
}

void JITByIdGenerator::finalize(LinkBuffer& linkBuffer)
{
    finalize(linkBuffer, linkBuffer);
}

void JITByIdGenerator::generateFastPathChecks(MacroAssembler& jit)
{
    m_structureCheck = jit.patchableBranch32WithPatch(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(m_base.payloadGPR(), JSCell::structureIDOffset()),
        m_structureImm, MacroAssembler::TrustedImm32(0));
}

JITGetByIdGenerator::JITGetByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    JSValueRegs base, JSValueRegs value)
    : JITByIdGenerator(
        codeBlock, codeOrigin, callSite, AccessType::Get, usedRegisters, base, value)
{
    RELEASE_ASSERT(base.payloadGPR() != value.tagGPR());
}

void JITGetByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastPathChecks(jit);
    
#if USE(JSVALUE64)
    m_loadOrStore = jit.load64WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_base.payloadGPR(), 0), m_value.payloadGPR()).label();
#else
    m_tagLoadOrStore = jit.load32WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_base.payloadGPR(), 0), m_value.tagGPR()).label();
    m_loadOrStore = jit.load32WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_base.payloadGPR(), 0), m_value.payloadGPR()).label();
#endif
    
    m_done = jit.label();
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, CallSiteIndex callSite, const RegisterSet& usedRegisters,
    JSValueRegs base, JSValueRegs value, GPRReg scratch, 
    ECMAMode ecmaMode, PutKind putKind)
    : JITByIdGenerator(
        codeBlock, codeOrigin, callSite, AccessType::Put, usedRegisters, base, value)
    , m_ecmaMode(ecmaMode)
    , m_putKind(putKind)
{
    m_stubInfo->patch.usedRegisters.clear(scratch);
}

void JITPutByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastPathChecks(jit);
    
#if USE(JSVALUE64)
    m_loadOrStore = jit.store64WithAddressOffsetPatch(
        m_value.payloadGPR(), MacroAssembler::Address(m_base.payloadGPR(), 0)).label();
#else
    m_tagLoadOrStore = jit.store32WithAddressOffsetPatch(
        m_value.tagGPR(), MacroAssembler::Address(m_base.payloadGPR(), 0)).label();
    m_loadOrStore = jit.store32WithAddressOffsetPatch(
        m_value.payloadGPR(), MacroAssembler::Address(m_base.payloadGPR(), 0)).label();
#endif
    
    m_done = jit.label();
}

V_JITOperation_ESsiJJI JITPutByIdGenerator::slowPathFunction()
{
    if (m_ecmaMode == StrictMode) {
        if (m_putKind == Direct)
            return operationPutByIdDirectStrictOptimize;
        return operationPutByIdStrictOptimize;
    }
    if (m_putKind == Direct)
        return operationPutByIdDirectNonStrictOptimize;
    return operationPutByIdNonStrictOptimize;
}

} // namespace JSC

#endif // ENABLE(JIT)

