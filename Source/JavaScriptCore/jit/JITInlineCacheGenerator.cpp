/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
    static StructureStubInfo* stubInfo = new StructureStubInfo();
    return stubInfo;
}

JITInlineCacheGenerator::JITInlineCacheGenerator(CodeBlock* codeBlock, CodeOrigin codeOrigin)
    : m_codeBlock(codeBlock)
{
    m_stubInfo = m_codeBlock ? m_codeBlock->addStubInfo() : garbageStubInfo();
    m_stubInfo->codeOrigin = codeOrigin;
}

JITByIdGenerator::JITByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, const RegisterSet& usedRegisters,
    JSValueRegs base, JSValueRegs value, bool registersFlushed)
    : JITInlineCacheGenerator(codeBlock, codeOrigin)
    , m_base(base)
    , m_value(value)
{
    m_stubInfo->patch.registersFlushed = registersFlushed;
    m_stubInfo->patch.usedRegisters = usedRegisters;
    
    // This is a convenience - in cases where the only registers you're using are base/value,
    // it allows you to pass RegisterSet() as the usedRegisters argument.
    m_stubInfo->patch.usedRegisters.set(base);
    m_stubInfo->patch.usedRegisters.set(value);
    
    m_stubInfo->patch.baseGPR = static_cast<int8_t>(base.payloadGPR());
    m_stubInfo->patch.valueGPR = static_cast<int8_t>(value.payloadGPR());
#if USE(JSVALUE32_64)
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
    m_stubInfo->patch.deltaCallToStorageLoad = MacroAssembler::differenceBetweenCodePtr(
        callReturnLocation, fastPath.locationOf(m_propertyStorageLoad));
}

void JITByIdGenerator::finalize(LinkBuffer& linkBuffer)
{
    finalize(linkBuffer, linkBuffer);
}

void JITByIdGenerator::generateFastPathChecks(MacroAssembler& jit, GPRReg butterfly)
{
    m_structureCheck = jit.patchableBranchPtrWithPatch(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(m_base.payloadGPR(), JSCell::structureOffset()),
        m_structureImm, MacroAssembler::TrustedImmPtr(reinterpret_cast<void*>(unusedPointer)));
    
    m_propertyStorageLoad = jit.convertibleLoadPtr(
        MacroAssembler::Address(m_base.payloadGPR(), JSObject::butterflyOffset()), butterfly);
}

void JITGetByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastPathChecks(jit, m_value.payloadGPR());
    
#if USE(JSVALUE64)
    m_loadOrStore = jit.load64WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_value.payloadGPR(), 0), m_value.payloadGPR()).label();
#else
    m_tagLoadOrStore = jit.load32WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_value.payloadGPR(), 0), m_value.tagGPR()).label();
    m_loadOrStore = jit.load32WithCompactAddressOffsetPatch(
        MacroAssembler::Address(m_value.payloadGPR(), 0), m_value.payloadGPR()).label();
#endif
    
    m_done = jit.label();
}

JITPutByIdGenerator::JITPutByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, const RegisterSet& usedRegisters,
    JSValueRegs base, JSValueRegs value, GPRReg scratch, bool registersFlushed,
    ECMAMode ecmaMode, PutKind putKind)
    : JITByIdGenerator(codeBlock, codeOrigin, usedRegisters, base, value, registersFlushed)
    , m_scratch(scratch)
    , m_ecmaMode(ecmaMode)
    , m_putKind(putKind)
{
    m_stubInfo->patch.usedRegisters.clear(scratch);
}

void JITPutByIdGenerator::generateFastPath(MacroAssembler& jit)
{
    generateFastPathChecks(jit, m_scratch);
    
#if USE(JSVALUE64)
    m_loadOrStore = jit.store64WithAddressOffsetPatch(
        m_value.payloadGPR(), MacroAssembler::Address(m_scratch, 0)).label();
#else
    m_tagLoadOrStore = jit.store32WithAddressOffsetPatch(
        m_value.tagGPR(), MacroAssembler::Address(m_scratch, 0)).label();
    m_loadOrStore = jit.store32WithAddressOffsetPatch(
        m_value.payloadGPR(), MacroAssembler::Address(m_scratch, 0)).label();
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

