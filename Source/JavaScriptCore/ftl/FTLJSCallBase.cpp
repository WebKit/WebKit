/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "FTLJSCallBase.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "DFGNode.h"
#include "FTLState.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

JSCallBase::JSCallBase()
    : m_type(CallLinkInfo::None)
    , m_callLinkInfo(nullptr)
    , m_correspondingGenericUnwindOSRExit(nullptr)
{
}

JSCallBase::JSCallBase(CallLinkInfo::CallType type, CodeOrigin semantic, CodeOrigin callSiteDescription)
    : m_type(type)
    , m_semanticeOrigin(semantic)
    , m_callSiteDescriptionOrigin(callSiteDescription)
    , m_callLinkInfo(nullptr)
{
}

void JSCallBase::emit(CCallHelpers& jit, State& /*state*/, int32_t osrExitFromGenericUnwindStackSpillSlot)
{
    RELEASE_ASSERT(!!m_callSiteIndex);

    if (m_correspondingGenericUnwindOSRExit)
        m_correspondingGenericUnwindOSRExit->spillRegistersToSpillSlot(jit, osrExitFromGenericUnwindStackSpillSlot);

    jit.store32(CCallHelpers::TrustedImm32(m_callSiteIndex.bits()), CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));

    m_callLinkInfo = jit.codeBlock()->addCallLinkInfo();
    
    if (CallLinkInfo::callModeFor(m_type) == CallMode::Tail)
        jit.emitRestoreCalleeSaves();

    CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
        CCallHelpers::NotEqual, GPRInfo::regT0, m_targetToCheck,
        CCallHelpers::TrustedImmPtr(0));

    CCallHelpers::Jump done;

    if (CallLinkInfo::callModeFor(m_type) == CallMode::Tail) {
        jit.prepareForTailCallSlow();
        m_fastCall = jit.nearTailCall();
    } else {
        m_fastCall = jit.nearCall();
        done = jit.jump();
    }

    slowPath.link(&jit);

    jit.move(CCallHelpers::TrustedImmPtr(m_callLinkInfo), GPRInfo::regT2);
    m_slowCall = jit.nearCall();

    if (CallLinkInfo::callModeFor(m_type) == CallMode::Tail)
        jit.abortWithReason(JITDidReturnFromTailCall);
    else
        done.link(&jit);

    m_callLinkInfo->setUpCall(m_type, m_semanticeOrigin, GPRInfo::regT0);
}

void JSCallBase::link(VM& vm, LinkBuffer& linkBuffer)
{
    linkBuffer.link(
        m_slowCall, FunctionPtr(vm.getCTIStub(linkCallThunkGenerator).code().executableAddress()));

    m_callLinkInfo->setCallLocations(linkBuffer.locationOfNearCall(m_slowCall),
        linkBuffer.locationOf(m_targetToCheck), linkBuffer.locationOfNearCall(m_fastCall));
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

