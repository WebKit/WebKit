/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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
#include "FTLJSCall.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "DFGNode.h"
#include "FTLState.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

JSCall::JSCall()
    : m_stackmapID(UINT_MAX)
    , m_instructionOffset(UINT_MAX)
{
}

JSCall::JSCall(unsigned stackmapID, Node* node, CodeOrigin callSiteDescriptionOrigin)
    : JSCallBase(node->op() == Construct ? CallLinkInfo::Construct : CallLinkInfo::Call, node->origin.semantic, callSiteDescriptionOrigin)
    , m_stackmapID(stackmapID)
    , m_instructionOffset(0)
{
    ASSERT(node->op() == Call || node->op() == Construct || node->op() == TailCallInlinedCaller);
}

void JSCall::emit(CCallHelpers& jit, State& state, int32_t osrExitFromGenericUnwindSpillSlots)
{
    JSCallBase::emit(jit, state, osrExitFromGenericUnwindSpillSlots);

#if FTL_USES_B3
    jit.addPtr(CCallHelpers::TrustedImm32(- static_cast<int64_t>(state.jitCode->common.frameRegisterCount * sizeof(EncodedJSValue))), CCallHelpers::framePointerRegister, CCallHelpers::stackPointerRegister);
#else // FTL_USES_B3
    jit.addPtr(CCallHelpers::TrustedImm32(- static_cast<int64_t>(state.jitCode->stackmaps.stackSizeForLocals())), CCallHelpers::framePointerRegister, CCallHelpers::stackPointerRegister);
#endif // FTL_USES_B3
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

