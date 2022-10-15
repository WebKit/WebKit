/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ProxyObjectAccessCase.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "LinkBuffer.h"
#include "LinkTimeConstant.h"
#include "PolymorphicAccess.h"
#include "ProxyObject.h"
#include "StructureStubInfo.h"

namespace JSC {

ProxyObjectAccessCase::ProxyObjectAccessCase(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier)
    : Base(vm, owner, type, identifier, invalidOffset, nullptr, ObjectPropertyConditionSet(), nullptr)
{
}

ProxyObjectAccessCase::ProxyObjectAccessCase(const ProxyObjectAccessCase& other)
    : Base(other)
{
    // Keep m_callLinkInfo nullptr.
}

Ref<AccessCase> ProxyObjectAccessCase::create(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier)
{
    return adoptRef(*new ProxyObjectAccessCase(vm, owner, type, identifier));
}

Ref<AccessCase> ProxyObjectAccessCase::cloneImpl() const
{
    auto result = adoptRef(*new ProxyObjectAccessCase(*this));
    result->resetState();
    return result;
}

void ProxyObjectAccessCase::emit(AccessGenerationState& state, MacroAssembler::JumpList& fallThrough)
{
    CCallHelpers& jit = *state.jit;
    CodeBlock* codeBlock = jit.codeBlock();
    VM& vm = state.m_vm;
    StructureStubInfo& stubInfo = *state.stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg scratchGPR = state.scratchGPR;
    GPRReg thisGPR = stubInfo.thisValueIsInExtraGPR() ? stubInfo.thisGPR() : baseGPR;

    JSGlobalObject* globalObject = state.m_globalObject;

    jit.load8(CCallHelpers::Address(baseGPR, JSCell::typeInfoTypeOffset()), scratchGPR);
    fallThrough.append(jit.branch32(CCallHelpers::NotEqual, scratchGPR, CCallHelpers::TrustedImm32(ProxyObjectType)));

    AccessGenerationState::SpillState spillState = state.preserveLiveRegistersToStackForCall();

    jit.store32(
        CCallHelpers::TrustedImm32(state.callSiteIndexForExceptionHandlingOrOriginal().bits()),
        CCallHelpers::tagFor(CallFrameSlot::argumentCountIncludingThis));

    state.setSpillStateForJSCall(spillState);

    ASSERT(!callLinkInfo());
    auto* callLinkInfo = state.m_callLinkInfos.add(stubInfo.codeOrigin, codeBlock->useDataIC() ? CallLinkInfo::UseDataIC::Yes : CallLinkInfo::UseDataIC::No);
    m_callLinkInfo = callLinkInfo;

    callLinkInfo->disallowStubs();

    callLinkInfo->setUpCall(CallLinkInfo::Call, scratchGPR);

    unsigned numberOfParameters = 5;

    unsigned numberOfRegsForCall = CallFrame::headerSizeInRegisters + roundArgumentCountToAlignFrame(numberOfParameters);
    ASSERT(!(numberOfRegsForCall % stackAlignmentRegisters()));
    unsigned numberOfBytesForCall = numberOfRegsForCall * sizeof(Register) - sizeof(CallerFrameAndPC);

    unsigned alignedNumberOfBytesForCall = WTF::roundUpToMultipleOf(stackAlignmentBytes(), numberOfBytesForCall);

    jit.subPtr(
        CCallHelpers::TrustedImm32(alignedNumberOfBytesForCall),
        CCallHelpers::stackPointerRegister);

    CCallHelpers::Address calleeFrame = CCallHelpers::Address(CCallHelpers::stackPointerRegister, -static_cast<ptrdiff_t>(sizeof(CallerFrameAndPC)));

    jit.store32(CCallHelpers::TrustedImm32(numberOfParameters), calleeFrame.withOffset(CallFrameSlot::argumentCountIncludingThis * sizeof(Register) + PayloadOffset));

    jit.storeTrustedValue(jsUndefined(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(0).offset() * sizeof(Register)));

    jit.loadPtr(CCallHelpers::Address(baseGPR, ProxyObject::offsetOfTarget()), scratchGPR);
    jit.storeCell(scratchGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(1).offset() * sizeof(Register)));

    jit.storeCell(thisGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(2).offset() * sizeof(Register)));

#if USE(JSVALUE32_64)
    jit.load32(CCallHelpers::Address(baseGPR, ProxyObject::offsetOfHandler() + TagOffset), scratchGPR);
    jit.store32(scratchGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(3).offset() * sizeof(Register) + TagOffset));
    jit.load32(CCallHelpers::Address(baseGPR, ProxyObject::offsetOfHandler() + PayloadOffset), scratchGPR);
    jit.store32(scratchGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(3).offset() * sizeof(Register) + PayloadOffset));
#else
    jit.loadValue(CCallHelpers::Address(baseGPR, ProxyObject::offsetOfHandler()), JSValueRegs { scratchGPR });
    jit.storeValue(JSValueRegs { scratchGPR }, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(3).offset() * sizeof(Register)));
#endif

    if (!stubInfo.hasConstantIdentifier) {
        RELEASE_ASSERT(identifier());
        GPRReg propertyGPR = stubInfo.propertyGPR();
        jit.storeCell(propertyGPR, calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(4).offset() * sizeof(Register)));
    } else
        jit.storeTrustedValue(identifier().cell(), calleeFrame.withOffset(virtualRegisterForArgumentIncludingThis(4).offset() * sizeof(Register)));

    jit.move(CCallHelpers::TrustedImmPtr(globalObject->linkTimeConstant(LinkTimeConstant::performProxyObjectGet)), scratchGPR);
    jit.storeCell(scratchGPR, calleeFrame.withOffset(CallFrameSlot::callee * sizeof(Register)));

    auto slowCase = CallLinkInfo::emitFastPath(jit, callLinkInfo, scratchGPR, scratchGPR == GPRInfo::regT2 ? GPRInfo::regT0 : GPRInfo::regT2);
    auto doneLocation = jit.label();

    jit.setupResults(valueRegs);
    auto done = jit.jump();

    slowCase.link(&jit);
    auto slowPathStart = jit.label();
    jit.move(scratchGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
    // We *always* know that the proxy function, if non-null, is a cell.
    jit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
    jit.move(CCallHelpers::TrustedImmPtr(globalObject), GPRInfo::regT3);
    callLinkInfo->emitSlowPath(vm, jit);

    jit.setupResults(valueRegs);

    done.link(&jit);

    int stackPointerOffset = (codeBlock->stackPointerOffset() * sizeof(Register)) - state.preservedReusedRegisterState.numberOfBytesPreserved - spillState.numberOfStackBytesUsedForRegisterPreservation;
    jit.addPtr(CCallHelpers::TrustedImm32(stackPointerOffset), GPRInfo::callFrameRegister, CCallHelpers::stackPointerRegister);

    RegisterSet dontRestore;
    // This is the result value. We don't want to overwrite the result with what we stored to the stack.
    // We sometimes have to store it to the stack just in case we throw an exception and need the original value.
    dontRestore.set(valueRegs);
    state.restoreLiveRegistersFromStackForCall(spillState, dontRestore);

    jit.addLinkTask([=, this] (LinkBuffer& linkBuffer) {
        this->callLinkInfo()->setCodeLocations(
            linkBuffer.locationOf<JSInternalPtrTag>(slowPathStart),
            linkBuffer.locationOf<JSInternalPtrTag>(doneLocation));
    });
    state.succeed();
}

void ProxyObjectAccessCase::dumpImpl(PrintStream& out, CommaPrinter& comma, Indenter& indent) const
{
    Base::dumpImpl(out, comma, indent);
    if (callLinkInfo())
        out.print(comma, "callLinkInfo = ", RawPointer(callLinkInfo()));
}

} // namespace JSC

#endif // ENABLE(JIT)
