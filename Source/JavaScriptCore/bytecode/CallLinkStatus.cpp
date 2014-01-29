/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "CallLinkInfo.h"
#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "LLIntCallLinkInfo.h"
#include "Operations.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

static const bool verbose = false;

CallLinkStatus::CallLinkStatus(JSValue value)
    : m_callTarget(value)
    , m_executable(0)
    , m_structure(0)
    , m_couldTakeSlowPath(false)
    , m_isProved(false)
{
    if (!value || !value.isCell())
        return;
    
    m_structure = value.asCell()->structure();
    
    if (!value.asCell()->inherits(JSFunction::info()))
        return;
    
    m_executable = jsCast<JSFunction*>(value.asCell())->executable();
}

JSFunction* CallLinkStatus::function() const
{
    if (!m_callTarget || !m_callTarget.isCell())
        return 0;
    
    if (!m_callTarget.asCell()->inherits(JSFunction::info()))
        return 0;
    
    return jsCast<JSFunction*>(m_callTarget.asCell());
}

InternalFunction* CallLinkStatus::internalFunction() const
{
    if (!m_callTarget || !m_callTarget.isCell())
        return 0;
    
    if (!m_callTarget.asCell()->inherits(InternalFunction::info()))
        return 0;
    
    return jsCast<InternalFunction*>(m_callTarget.asCell());
}

Intrinsic CallLinkStatus::intrinsicFor(CodeSpecializationKind kind) const
{
    if (!m_executable)
        return NoIntrinsic;
    
    return m_executable->intrinsicFor(kind);
}

CallLinkStatus CallLinkStatus::computeFromLLInt(const ConcurrentJITLocker& locker, CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(LLINT)
#if ENABLE(DFG_JIT)
    if (profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadFunction))) {
        // We could force this to be a closure call, but instead we'll just assume that it
        // takes slow path.
        return takesSlowPath();
    }
#else
    UNUSED_PARAM(locker);
#endif

    VM& vm = *profiledBlock->vm();
    
    Instruction* instruction = profiledBlock->instructions().begin() + bytecodeIndex;
    OpcodeID op = vm.interpreter->getOpcodeID(instruction[0].u.opcode);
    if (op != op_call && op != op_construct)
        return CallLinkStatus();
    
    LLIntCallLinkInfo* callLinkInfo = instruction[5].u.callLinkInfo;
    
    return CallLinkStatus(callLinkInfo->lastSeenCallee.get());
#else
    return CallLinkStatus();
#endif
}

CallLinkStatus CallLinkStatus::computeFor(CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    ConcurrentJITLocker locker(profiledBlock->m_lock);
    
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(JIT)
    if (profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCache))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCacheWatchpoint))
        || profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadExecutable)))
        return takesSlowPath();
    
    if (!profiledBlock->hasBaselineJITProfiling())
        return computeFromLLInt(locker, profiledBlock, bytecodeIndex);
    
    if (profiledBlock->couldTakeSlowCase(bytecodeIndex))
        return takesSlowPath();
    
    CallLinkInfo& callLinkInfo = profiledBlock->getCallLinkInfo(bytecodeIndex);
    
    CallLinkStatus result = computeFor(locker, callLinkInfo);
    if (!result)
        return computeFromLLInt(locker, profiledBlock, bytecodeIndex);
    
    if (profiledBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadFunction)))
        result.makeClosureCall();
    
    return result;
#else
    return CallLinkStatus();
#endif
}

#if ENABLE(JIT)
CallLinkStatus CallLinkStatus::computeFor(const ConcurrentJITLocker&, CallLinkInfo& callLinkInfo)
{
    if (callLinkInfo.stub)
        return CallLinkStatus(callLinkInfo.stub->executable(), callLinkInfo.stub->structure());
    
    JSFunction* target = callLinkInfo.lastSeenCallee.get();
    if (!target)
        return CallLinkStatus();
    
    if (callLinkInfo.hasSeenClosure)
        return CallLinkStatus(target->executable(), target->structure());

    return CallLinkStatus(target);
}
#endif

void CallLinkStatus::computeDFGStatuses(
    CodeBlock* dfgCodeBlock, CallLinkStatus::ContextMap& map)
{
#if ENABLE(DFG_JIT)
    RELEASE_ASSERT(dfgCodeBlock->jitType() == JITCode::DFGJIT);
    CodeBlock* baselineCodeBlock = dfgCodeBlock->alternative();
    DFG::JITCode* jitCode = dfgCodeBlock->jitCode()->dfg();
    RELEASE_ASSERT(dfgCodeBlock->numberOfCallLinkInfos() <= jitCode->slowPathCalls.size());
    
    for (size_t i = dfgCodeBlock->numberOfCallLinkInfos(); i--;) {
        CallLinkInfo& info = dfgCodeBlock->callLinkInfo(i);
        CodeOrigin codeOrigin = info.codeOrigin;
        
        bool takeSlowPath;
        bool badFunction;
        
        // Check if we had already previously made a terrible mistake in the FTL for this
        // code origin. Note that this is approximate because we could have a monovariant
        // inline in the FTL that ended up failing. We should fix that at some point by
        // having data structures to track the context of frequent exits. This is currently
        // challenging because it would require creating a CodeOrigin-based database in
        // baseline CodeBlocks, but those CodeBlocks don't really have a place to put the
        // InlineCallFrames.
        CodeBlock* currentBaseline =
            baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, baselineCodeBlock);
        {
            ConcurrentJITLocker locker(currentBaseline->m_lock);
            takeSlowPath =
                currentBaseline->hasExitSite(locker, DFG::FrequentExitSite(codeOrigin.bytecodeIndex, BadCache, ExitFromFTL))
                || currentBaseline->hasExitSite(locker, DFG::FrequentExitSite(codeOrigin.bytecodeIndex, BadCacheWatchpoint, ExitFromFTL))
                || currentBaseline->hasExitSite(locker, DFG::FrequentExitSite(codeOrigin.bytecodeIndex, BadExecutable, ExitFromFTL));
            badFunction =
                currentBaseline->hasExitSite(locker, DFG::FrequentExitSite(codeOrigin.bytecodeIndex, BadFunction, ExitFromFTL));
        }
        
        {
            ConcurrentJITLocker locker(dfgCodeBlock->m_lock);
            if (takeSlowPath || jitCode->slowPathCalls[i] >= Options::couldTakeSlowCaseMinimumCount())
                map.add(info.codeOrigin, takesSlowPath());
            else {
                CallLinkStatus status = computeFor(locker, info);
                if (status.isSet()) {
                    if (badFunction)
                        status.makeClosureCall();
                    map.add(info.codeOrigin, status);
                }
            }
        }
    }
#else
    UNUSED_PARAM(dfgCodeBlock);
#endif // ENABLE(DFG_JIT)
    
    if (verbose) {
        dataLog("Context map:\n");
        ContextMap::iterator iter = map.begin();
        ContextMap::iterator end = map.end();
        for (; iter != end; ++iter) {
            dataLog("    ", iter->key, ":\n");
            dataLog("        ", iter->value, "\n");
        }
    }
}

CallLinkStatus CallLinkStatus::computeFor(
    CodeBlock* profiledBlock, CodeOrigin codeOrigin, const CallLinkStatus::ContextMap& map)
{
    ContextMap::const_iterator iter = map.find(codeOrigin);
    if (iter != map.end())
        return iter->value;
    
    return computeFor(profiledBlock, codeOrigin.bytecodeIndex);
}

void CallLinkStatus::dump(PrintStream& out) const
{
    if (!isSet()) {
        out.print("Not Set");
        return;
    }
    
    CommaPrinter comma;
    
    if (m_isProved)
        out.print(comma, "Statically Proved");
    
    if (m_couldTakeSlowPath)
        out.print(comma, "Could Take Slow Path");
    
    if (m_callTarget)
        out.print(comma, "Known target: ", m_callTarget);
    
    if (m_executable) {
        out.print(comma, "Executable/CallHash: ", RawPointer(m_executable));
        if (!isCompilationThread())
            out.print("/", m_executable->hashFor(CodeForCall));
    }
    
    if (m_structure)
        out.print(comma, "Structure: ", RawPointer(m_structure));
}

} // namespace JSC

