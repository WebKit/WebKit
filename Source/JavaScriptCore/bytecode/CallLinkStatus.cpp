/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "CallLinkStatus.h"

#include "CodeBlock.h"
#include "LLIntCallLinkInfo.h"

namespace JSC {

CallLinkStatus CallLinkStatus::computeFromLLInt(CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(LLINT)
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;
    LLIntCallLinkInfo* callLinkInfo = instruction[4].u.callLinkInfo;
    
    return CallLinkStatus(callLinkInfo->lastSeenCallee.get(), false, false);
#else
    return CallLinkStatus(0, false, false);
#endif
}

CallLinkStatus CallLinkStatus::computeFor(CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(JIT) && ENABLE(VALUE_PROFILER)
    if (!profiledBlock->numberOfCallLinkInfos())
        return computeFromLLInt(profiledBlock, bytecodeIndex);
    
    if (profiledBlock->couldTakeSlowCase(bytecodeIndex))
        return CallLinkStatus(0, true);
    
    CallLinkInfo& callLinkInfo = profiledBlock->getCallLinkInfo(bytecodeIndex);
    JSFunction* target = callLinkInfo.lastSeenCallee.get();
    if (!target)
        return computeFromLLInt(profiledBlock, bytecodeIndex);
    
    return CallLinkStatus(target, false, !!callLinkInfo.stub);
#else
    return CallLinkStatus(0, false, false);
#endif
}

} // namespace JSC

