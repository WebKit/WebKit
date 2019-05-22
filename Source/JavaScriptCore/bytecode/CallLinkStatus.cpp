/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
#include "CallLinkInfo.h"
#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "InlineCallFrame.h"
#include "InterpreterInlines.h"
#include "LLIntCallLinkInfo.h"
#include "JSCInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>

namespace JSC {

namespace CallLinkStatusInternal {
static const bool verbose = false;
}

CallLinkStatus::CallLinkStatus(JSValue value)
    : m_couldTakeSlowPath(false)
    , m_isProved(false)
{
    if (!value || !value.isCell()) {
        m_couldTakeSlowPath = true;
        return;
    }
    
    m_variants.append(CallVariant(value.asCell()));
}

CallLinkStatus CallLinkStatus::computeFromLLInt(const ConcurrentJSLocker&, CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#if ENABLE(DFG_JIT)
    if (profiledBlock->unlinkedCodeBlock()->hasExitSite(DFG::FrequentExitSite(bytecodeIndex, BadCell))) {
        // We could force this to be a closure call, but instead we'll just assume that it
        // takes slow path.
        return takesSlowPath();
    }
#endif

    auto instruction = profiledBlock->instructions().at(bytecodeIndex);
    OpcodeID op = instruction->opcodeID();

    LLIntCallLinkInfo* callLinkInfo;
    switch (op) {
    case op_call:
        callLinkInfo = &instruction->as<OpCall>().metadata(profiledBlock).m_callLinkInfo;
        break;
    case op_construct:
        callLinkInfo = &instruction->as<OpConstruct>().metadata(profiledBlock).m_callLinkInfo;
        break;
    case op_tail_call:
        callLinkInfo = &instruction->as<OpTailCall>().metadata(profiledBlock).m_callLinkInfo;
        break;
    default:
        return CallLinkStatus();
    }
    
    
    return CallLinkStatus(callLinkInfo->lastSeenCallee());
}

CallLinkStatus CallLinkStatus::computeFor(
    CodeBlock* profiledBlock, unsigned bytecodeIndex, const ICStatusMap& map,
    ExitSiteData exitSiteData)
{
    ConcurrentJSLocker locker(profiledBlock->m_lock);
    
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
    UNUSED_PARAM(map);
    UNUSED_PARAM(exitSiteData);
#if ENABLE(DFG_JIT)
    CallLinkInfo* callLinkInfo = map.get(CodeOrigin(bytecodeIndex)).callLinkInfo;
    if (!callLinkInfo) {
        if (exitSiteData.takesSlowPath)
            return takesSlowPath();
        return computeFromLLInt(locker, profiledBlock, bytecodeIndex);
    }
    
    return computeFor(locker, profiledBlock, *callLinkInfo, exitSiteData);
#else
    return CallLinkStatus();
#endif
}

CallLinkStatus CallLinkStatus::computeFor(
    CodeBlock* profiledBlock, unsigned bytecodeIndex, const ICStatusMap& map)
{
    return computeFor(profiledBlock, bytecodeIndex, map, computeExitSiteData(profiledBlock, bytecodeIndex));
}

CallLinkStatus::ExitSiteData CallLinkStatus::computeExitSiteData(CodeBlock* profiledBlock, unsigned bytecodeIndex)
{
    ExitSiteData exitSiteData;
#if ENABLE(DFG_JIT)
    UnlinkedCodeBlock* codeBlock = profiledBlock->unlinkedCodeBlock();
    ConcurrentJSLocker locker(codeBlock->m_lock);

    auto takesSlowPath = [&] (ExitingInlineKind inlineKind) -> ExitFlag {
        return ExitFlag(
            codeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadType, ExitFromAnything, inlineKind))
            || codeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadExecutable, ExitFromAnything, inlineKind)),
            inlineKind);
    };
    
    auto badFunction = [&] (ExitingInlineKind inlineKind) -> ExitFlag {
        return ExitFlag(codeBlock->hasExitSite(locker, DFG::FrequentExitSite(bytecodeIndex, BadCell, ExitFromAnything, inlineKind)), inlineKind);
    };
    
    exitSiteData.takesSlowPath |= takesSlowPath(ExitFromNotInlined);
    exitSiteData.takesSlowPath |= takesSlowPath(ExitFromInlined);
    exitSiteData.badFunction |= badFunction(ExitFromNotInlined);
    exitSiteData.badFunction |= badFunction(ExitFromInlined);
#else
    UNUSED_PARAM(profiledBlock);
    UNUSED_PARAM(bytecodeIndex);
#endif
    
    return exitSiteData;
}

#if ENABLE(JIT)
CallLinkStatus CallLinkStatus::computeFor(
    const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, CallLinkInfo& callLinkInfo)
{
    // We don't really need this, but anytime we have to debug this code, it becomes indispensable.
    UNUSED_PARAM(profiledBlock);
    
    CallLinkStatus result = computeFromCallLinkInfo(locker, callLinkInfo);
    result.m_maxNumArguments = callLinkInfo.maxNumArguments();
    return result;
}

CallLinkStatus CallLinkStatus::computeFromCallLinkInfo(
    const ConcurrentJSLocker&, CallLinkInfo& callLinkInfo)
{
    // We cannot tell you anything about direct calls.
    if (callLinkInfo.isDirect())
        return CallLinkStatus();
    
    if (callLinkInfo.clearedByGC() || callLinkInfo.clearedByVirtual())
        return takesSlowPath();
    
    // Note that despite requiring that the locker is held, this code is racy with respect
    // to the CallLinkInfo: it may get cleared while this code runs! This is because
    // CallLinkInfo::unlink() may be called from a different CodeBlock than the one that owns
    // the CallLinkInfo and currently we save space by not having CallLinkInfos know who owns
    // them. So, there is no way for either the caller of CallLinkInfo::unlock() or unlock()
    // itself to figure out which lock to lock.
    //
    // Fortunately, that doesn't matter. The only things we ask of CallLinkInfo - the slow
    // path count, the stub, and the target - can all be asked racily. Stubs and targets can
    // only be deleted at next GC, so if we load a non-null one, then it must contain data
    // that is still marginally valid (i.e. the pointers ain't stale). This kind of raciness
    // is probably OK for now.
    
    // PolymorphicCallStubRoutine is a GCAwareJITStubRoutine, so if non-null, it will stay alive
    // until next GC even if the CallLinkInfo is concurrently cleared. Also, the variants list is
    // never mutated after the PolymorphicCallStubRoutine is instantiated. We have some conservative
    // fencing in place to make sure that we see the variants list after construction.
    if (PolymorphicCallStubRoutine* stub = callLinkInfo.stub()) {
        WTF::loadLoadFence();
        
        if (!stub->hasEdges()) {
            // This means we have an FTL profile, which has incomplete information.
            //
            // It's not clear if takesSlowPath() or CallLinkStatus() are appropriate here, but I
            // think that takesSlowPath() is a narrow winner.
            //
            // Either way, this is telling us that the FTL had been led to believe that it's
            // profitable not to inline anything.
            return takesSlowPath();
        }
        
        CallEdgeList edges = stub->edges();
        
        // Now that we've loaded the edges list, there are no further concurrency concerns. We will
        // just manipulate and prune this list to our liking - mostly removing entries that are too
        // infrequent and ensuring that it's sorted in descending order of frequency.
        
        RELEASE_ASSERT(edges.size());
        
        std::sort(
            edges.begin(), edges.end(),
            [] (CallEdge a, CallEdge b) {
                return a.count() > b.count();
            });
        RELEASE_ASSERT(edges.first().count() >= edges.last().count());
        
        double totalCallsToKnown = 0;
        double totalCallsToUnknown = callLinkInfo.slowPathCount();
        CallVariantList variants;
        for (size_t i = 0; i < edges.size(); ++i) {
            CallEdge edge = edges[i];
            // If the call is at the tail of the distribution, then we don't optimize it and we
            // treat it as if it was a call to something unknown. We define the tail as being either
            // a call that doesn't belong to the N most frequent callees (N =
            // maxPolymorphicCallVariantsForInlining) or that has a total call count that is too
            // small.
            if (i >= Options::maxPolymorphicCallVariantsForInlining()
                || edge.count() < Options::frequentCallThreshold())
                totalCallsToUnknown += edge.count();
            else {
                totalCallsToKnown += edge.count();
                variants.append(edge.callee());
            }
        }
        
        // Bail if we didn't find any calls that qualified.
        RELEASE_ASSERT(!!totalCallsToKnown == !!variants.size());
        if (variants.isEmpty())
            return takesSlowPath();
        
        // We require that the distribution of callees is skewed towards a handful of common ones.
        if (totalCallsToKnown / totalCallsToUnknown < Options::minimumCallToKnownRate())
            return takesSlowPath();
        
        RELEASE_ASSERT(totalCallsToKnown);
        RELEASE_ASSERT(variants.size());
        
        CallLinkStatus result;
        result.m_variants = variants;
        result.m_couldTakeSlowPath = !!totalCallsToUnknown;
        result.m_isBasedOnStub = true;
        return result;
    }
    
    CallLinkStatus result;
    
    if (JSObject* target = callLinkInfo.lastSeenCallee()) {
        CallVariant variant(target);
        if (callLinkInfo.hasSeenClosure())
            variant = variant.despecifiedClosure();
        result.m_variants.append(variant);
    }
    
    result.m_couldTakeSlowPath = !!callLinkInfo.slowPathCount();

    return result;
}

CallLinkStatus CallLinkStatus::computeFor(
    const ConcurrentJSLocker& locker, CodeBlock* profiledBlock, CallLinkInfo& callLinkInfo,
    ExitSiteData exitSiteData, ExitingInlineKind inlineKind)
{
    CallLinkStatus result = computeFor(locker, profiledBlock, callLinkInfo);
    result.accountForExits(exitSiteData, inlineKind);
    return result;
}

void CallLinkStatus::accountForExits(ExitSiteData exitSiteData, ExitingInlineKind inlineKind)
{
    if (exitSiteData.badFunction.isSet(inlineKind)) {
        if (isBasedOnStub()) {
            // If we have a polymorphic stub, then having an exit site is not quite so useful. In
            // most cases, the information in the stub has higher fidelity.
            makeClosureCall();
        } else {
            // We might not have a polymorphic stub for any number of reasons. When this happens, we
            // are in less certain territory, so exit sites mean a lot.
            m_couldTakeSlowPath = true;
        }
    }
    
    if (exitSiteData.takesSlowPath.isSet(inlineKind))
        m_couldTakeSlowPath = true;
}

CallLinkStatus CallLinkStatus::computeFor(
    CodeBlock* profiledBlock, CodeOrigin codeOrigin,
    const ICStatusMap& baselineMap, const ICStatusContextStack& optimizedStack)
{
    if (CallLinkStatusInternal::verbose)
        dataLog("Figuring out call profiling for ", codeOrigin, "\n");
    ExitSiteData exitSiteData = computeExitSiteData(profiledBlock, codeOrigin.bytecodeIndex());
    if (CallLinkStatusInternal::verbose) {
        dataLog("takesSlowPath = ", exitSiteData.takesSlowPath, "\n");
        dataLog("badFunction = ", exitSiteData.badFunction, "\n");
    }
    
    for (const ICStatusContext* context : optimizedStack) {
        if (CallLinkStatusInternal::verbose)
            dataLog("Examining status in ", pointerDump(context->optimizedCodeBlock), "\n");
        ICStatus status = context->get(codeOrigin);
        
        // If we have both a CallLinkStatus and a callLinkInfo for the same origin, then it means
        // one of two things:
        //
        // 1) We initially thought that we'd be able to inline this call so we recorded a status
        //    but then we realized that it was pointless and gave up and emitted a normal call
        //    anyway.
        //
        // 2) We did a polymorphic call inline that had a slow path case.
        //
        // In the first case, it's essential that we use the callLinkInfo, since the status may
        // be polymorphic but the link info benefits from polyvariant profiling.
        //
        // In the second case, it's essential that we use the status, since the callLinkInfo
        // corresponds to the slow case.
        //
        // It would be annoying to distinguish those two cases. However, we know that:
        //
        // If the first case happens in the FTL, then it means that even with polyvariant
        // profiling, we failed to do anything useful. That suggests that in the FTL, it's OK to
        // prioritize the status. That status will again tell us to not do anything useful.
        //
        // The second case only happens in the FTL.
        //
        // Therefore, we prefer the status in the FTL and the info in the DFG.
        //
        // Luckily, this case doesn't matter for the other ICStatuses, since they never do the
        // fast-path-slow-path control-flow-diamond style of IC inlining. It's either all fast
        // path or it's a full IC. So, for them, if there is an IC status then it means case (1).
        
        bool checkStatusFirst = context->optimizedCodeBlock->jitType() == JITType::FTLJIT;
        
        auto bless = [&] (CallLinkStatus& result) {
            if (!context->isInlined(codeOrigin))
                result.merge(computeFor(profiledBlock, codeOrigin.bytecodeIndex(), baselineMap, exitSiteData));
        };
        
        auto checkInfo = [&] () -> CallLinkStatus {
            if (!status.callLinkInfo)
                return CallLinkStatus();
            
            if (CallLinkStatusInternal::verbose)
                dataLog("Have CallLinkInfo with CodeOrigin = ", status.callLinkInfo->codeOrigin(), "\n");
            CallLinkStatus result;
            {
                ConcurrentJSLocker locker(context->optimizedCodeBlock->m_lock);
                result = computeFor(
                    locker, context->optimizedCodeBlock, *status.callLinkInfo, exitSiteData,
                    context->inlineKind(codeOrigin));
                if (CallLinkStatusInternal::verbose)
                    dataLog("Got result: ", result, "\n");
            }
            bless(result);
            return result;
        };
        
        auto checkStatus = [&] () -> CallLinkStatus {
            if (!status.callStatus)
                return CallLinkStatus();
            CallLinkStatus result = *status.callStatus;
            if (CallLinkStatusInternal::verbose)
                dataLog("Have callStatus: ", result, "\n");
            result.accountForExits(exitSiteData, context->inlineKind(codeOrigin));
            bless(result);
            return result;
        };
        
        if (checkStatusFirst) {
            if (CallLinkStatus result = checkStatus())
                return result;
            if (CallLinkStatus result = checkInfo())
                return result;
            continue;
        }
        
        if (CallLinkStatus result = checkInfo())
            return result;
        if (CallLinkStatus result = checkStatus())
            return result;
    }
    
    return computeFor(profiledBlock, codeOrigin.bytecodeIndex(), baselineMap, exitSiteData);
}
#endif

void CallLinkStatus::setProvenConstantCallee(CallVariant variant)
{
    m_variants = CallVariantList{ variant };
    m_couldTakeSlowPath = false;
    m_isProved = true;
}

bool CallLinkStatus::isClosureCall() const
{
    for (unsigned i = m_variants.size(); i--;) {
        if (m_variants[i].isClosureCall())
            return true;
    }
    return false;
}

void CallLinkStatus::makeClosureCall()
{
    m_variants = despecifiedVariantList(m_variants);
}

bool CallLinkStatus::finalize(VM& vm)
{
    for (CallVariant& variant : m_variants) {
        if (!variant.finalize(vm))
            return false;
    }
    return true;
}

void CallLinkStatus::merge(const CallLinkStatus& other)
{
    m_couldTakeSlowPath |= other.m_couldTakeSlowPath;
    
    for (const CallVariant& otherVariant : other.m_variants) {
        bool found = false;
        for (CallVariant& thisVariant : m_variants) {
            if (thisVariant.merge(otherVariant)) {
                found = true;
                break;
            }
        }
        if (!found)
            m_variants.append(otherVariant);
    }
}

void CallLinkStatus::filter(VM& vm, JSValue value)
{
    m_variants.removeAllMatching(
        [&] (CallVariant& variant) -> bool {
            variant.filter(vm, value);
            return !variant;
        });
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
    
    if (m_isBasedOnStub)
        out.print(comma, "Based On Stub");
    
    if (!m_variants.isEmpty())
        out.print(comma, listDump(m_variants));
    
    if (m_maxNumArguments)
        out.print(comma, "maxNumArguments = ", m_maxNumArguments);
}

} // namespace JSC

