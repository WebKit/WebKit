/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGCommonData.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGNode.h"
#include "DFGPlan.h"
#include "InlineCallFrame.h"
#include "JSCInlines.h"
#include "TrackedReferences.h"
#include "VM.h"

#include <wtf/NeverDestroyed.h>

namespace JSC { namespace DFG {

void CommonData::notifyCompilingStructureTransition(Plan& plan, CodeBlock* codeBlock, Node* node)
{
    plan.transitions().addLazily(
        codeBlock,
        node->origin.semantic.codeOriginOwner(),
        node->transition()->previous.get(),
        node->transition()->next.get());
}

CallSiteIndex CommonData::addCodeOrigin(CodeOrigin codeOrigin)
{
    if (codeOrigins.isEmpty()
        || codeOrigins.last() != codeOrigin)
        codeOrigins.append(codeOrigin);
    unsigned index = codeOrigins.size() - 1;
    ASSERT(codeOrigins[index] == codeOrigin);
    return CallSiteIndex(index);
}

CallSiteIndex CommonData::addUniqueCallSiteIndex(CodeOrigin codeOrigin)
{
    codeOrigins.append(codeOrigin);
    unsigned index = codeOrigins.size() - 1;
    ASSERT(codeOrigins[index] == codeOrigin);
    return CallSiteIndex(index);
}

CallSiteIndex CommonData::lastCallSite() const
{
    RELEASE_ASSERT(codeOrigins.size());
    return CallSiteIndex(codeOrigins.size() - 1);
}

DisposableCallSiteIndex CommonData::addDisposableCallSiteIndex(CodeOrigin codeOrigin)
{
    if (callSiteIndexFreeList.size()) {
        unsigned index = callSiteIndexFreeList.takeAny();
        codeOrigins[index] = codeOrigin;
        return DisposableCallSiteIndex(index);
    }

    codeOrigins.append(codeOrigin);
    unsigned index = codeOrigins.size() - 1;
    ASSERT(codeOrigins[index] == codeOrigin);
    return DisposableCallSiteIndex(index);
}


void CommonData::removeDisposableCallSiteIndex(DisposableCallSiteIndex callSite)
{
    RELEASE_ASSERT(callSite.bits() < codeOrigins.size());
    callSiteIndexFreeList.add(callSite.bits());
    codeOrigins[callSite.bits()] = CodeOrigin();
}

void CommonData::shrinkToFit()
{
    codeOrigins.shrinkToFit();
    dfgIdentifiers.shrinkToFit();
    weakReferences.shrinkToFit();
    weakStructureReferences.shrinkToFit();
    transitions.shrinkToFit();
    catchEntrypoints.shrinkToFit();
    jumpReplacements.shrinkToFit();
}

static Lock pcCodeBlockMapLock;
inline HashMap<void*, CodeBlock*>& pcCodeBlockMap(AbstractLocker&)
{
    static NeverDestroyed<HashMap<void*, CodeBlock*>> pcCodeBlockMap;
    return pcCodeBlockMap;
}

bool CommonData::invalidate()
{
    if (!isStillValid)
        return false;

    if (UNLIKELY(hasVMTrapsBreakpointsInstalled)) {
        LockHolder locker(pcCodeBlockMapLock);
        auto& map = pcCodeBlockMap(locker);
        for (auto& jumpReplacement : jumpReplacements)
            map.remove(jumpReplacement.dataLocation());
        hasVMTrapsBreakpointsInstalled = false;
    }

    for (unsigned i = jumpReplacements.size(); i--;)
        jumpReplacements[i].fire();
    isStillValid = false;
    return true;
}

CommonData::~CommonData()
{
    if (UNLIKELY(hasVMTrapsBreakpointsInstalled)) {
        LockHolder locker(pcCodeBlockMapLock);
        auto& map = pcCodeBlockMap(locker);
        for (auto& jumpReplacement : jumpReplacements)
            map.remove(jumpReplacement.dataLocation());
    }
}

void CommonData::installVMTrapBreakpoints(CodeBlock* owner)
{
    LockHolder locker(pcCodeBlockMapLock);
    if (!isStillValid || hasVMTrapsBreakpointsInstalled)
        return;
    hasVMTrapsBreakpointsInstalled = true;

    auto& map = pcCodeBlockMap(locker);
#if !defined(NDEBUG)
    // We need to be able to handle more than one invalidation point at the same pc
    // but we want to make sure we don't forget to remove a pc from the map.
    HashSet<void*> newReplacements;
#endif
    for (auto& jumpReplacement : jumpReplacements) {
        jumpReplacement.installVMTrapBreakpoint();
        void* source = jumpReplacement.dataLocation();
        auto result = map.add(source, owner);
        UNUSED_PARAM(result);
#if !defined(NDEBUG)
        ASSERT(result.isNewEntry || newReplacements.contains(source));
        newReplacements.add(source);
#endif
    }
}

CodeBlock* codeBlockForVMTrapPC(void* pc)
{
    ASSERT(isJITPC(pc));
    LockHolder locker(pcCodeBlockMapLock);
    auto& map = pcCodeBlockMap(locker);
    auto result = map.find(pc);
    if (result == map.end())
        return nullptr;
    return result->value;
}

bool CommonData::isVMTrapBreakpoint(void* address)
{
    if (!isStillValid)
        return false;
    for (unsigned i = jumpReplacements.size(); i--;) {
        if (address == jumpReplacements[i].dataLocation())
            return true;
    }
    return false;
}

void CommonData::validateReferences(const TrackedReferences& trackedReferences)
{
    if (InlineCallFrameSet* set = inlineCallFrames.get()) {
        for (InlineCallFrame* inlineCallFrame : *set) {
            for (ValueRecovery& recovery : inlineCallFrame->argumentsWithFixup) {
                if (recovery.isConstant())
                    trackedReferences.check(recovery.constant());
            }
            
            if (CodeBlock* baselineCodeBlock = inlineCallFrame->baselineCodeBlock.get())
                trackedReferences.check(baselineCodeBlock);
            
            if (inlineCallFrame->calleeRecovery.isConstant())
                trackedReferences.check(inlineCallFrame->calleeRecovery.constant());
        }
    }
    
    for (AdaptiveStructureWatchpoint* watchpoint : adaptiveStructureWatchpoints)
        watchpoint->key().validateReferences(trackedReferences);
}

void CommonData::finalizeCatchEntrypoints()
{
    std::sort(catchEntrypoints.begin(), catchEntrypoints.end(),
        [] (const CatchEntrypointData& a, const CatchEntrypointData& b) { return a.bytecodeIndex < b.bytecodeIndex; });

#if ASSERT_ENABLED
    for (unsigned i = 0; i + 1 < catchEntrypoints.size(); ++i)
        ASSERT(catchEntrypoints[i].bytecodeIndex <= catchEntrypoints[i + 1].bytecodeIndex);
#endif
}

void CommonData::clearWatchpoints()
{
    watchpoints.clear();
    adaptiveStructureWatchpoints.clear();
    adaptiveInferredPropertyValueWatchpoints.clear();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

