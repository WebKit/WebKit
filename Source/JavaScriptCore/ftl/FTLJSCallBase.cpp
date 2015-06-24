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

#if ENABLE(FTL_JIT)

#include "DFGNode.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

JSCallBase::JSCallBase()
    : m_type(CallLinkInfo::None)
    , m_callLinkInfo(nullptr)
{
}

JSCallBase::JSCallBase(CallLinkInfo::CallType type, CodeOrigin origin)
    : m_type(type)
    , m_origin(origin)
    , m_callLinkInfo(nullptr)
{
}

void JSCallBase::emit(CCallHelpers& jit)
{
    m_callLinkInfo = jit.codeBlock()->addCallLinkInfo();
    
    CCallHelpers::Jump slowPath = jit.branchPtrWithPatch(
        CCallHelpers::NotEqual, GPRInfo::regT0, m_targetToCheck,
        CCallHelpers::TrustedImmPtr(0));
    
    m_fastCall = jit.nearCall();
    CCallHelpers::Jump done = jit.jump();
    
    slowPath.link(&jit);
    
    jit.move(CCallHelpers::TrustedImmPtr(m_callLinkInfo), GPRInfo::regT2);
    m_slowCall = jit.nearCall();
    
    done.link(&jit);
}

void JSCallBase::link(VM& vm, LinkBuffer& linkBuffer)
{
    ThunkGenerator generator = linkThunkGeneratorFor(
        CallLinkInfo::specializationKindFor(m_type), MustPreserveRegisters);
    
    linkBuffer.link(
        m_slowCall, FunctionPtr(vm.getCTIStub(generator).code().executableAddress()));

    m_callLinkInfo->setUpCallFromFTL(m_type, m_origin, linkBuffer.locationOfNearCall(m_slowCall),
        linkBuffer.locationOf(m_targetToCheck), linkBuffer.locationOfNearCall(m_fastCall),
        GPRInfo::regT0);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

