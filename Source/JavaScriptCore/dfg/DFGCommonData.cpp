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
#include "JSCJSValueInlines.h"
#include "TrackedReferences.h"
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace JSC { namespace DFG {

void CommonData::shrinkToFit()
{
    codeOrigins->shrinkToFit();
    m_jumpReplacements.shrinkToFit();
}

static Lock pcCodeBlockMapLock;
inline HashMap<void*, CodeBlock*>& pcCodeBlockMap() WTF_REQUIRES_LOCK(pcCodeBlockMapLock)
{
    static LazyNeverDestroyed<HashMap<void*, CodeBlock*>> pcCodeBlockMap;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        pcCodeBlockMap.construct();
    });
    return pcCodeBlockMap;
}

bool CommonData::invalidate()
{
    if (!isStillValid)
        return false;

    if (UNLIKELY(hasVMTrapsBreakpointsInstalled)) {
        Locker locker { pcCodeBlockMapLock };
        auto& map = pcCodeBlockMap();
        for (auto& jumpReplacement : m_jumpReplacements)
            map.remove(jumpReplacement.dataLocation());
        hasVMTrapsBreakpointsInstalled = false;
    }

    for (unsigned i = m_jumpReplacements.size(); i--;)
        m_jumpReplacements[i].fire();
    isStillValid = false;
    return true;
}

CommonData::~CommonData()
{
    if (UNLIKELY(hasVMTrapsBreakpointsInstalled)) {
        Locker locker { pcCodeBlockMapLock };
        auto& map = pcCodeBlockMap();
        for (auto& jumpReplacement : m_jumpReplacements)
            map.remove(jumpReplacement.dataLocation());
    }
}

void CommonData::installVMTrapBreakpoints(CodeBlock* owner)
{
    Locker locker { pcCodeBlockMapLock };
    if (!isStillValid || hasVMTrapsBreakpointsInstalled)
        return;
    hasVMTrapsBreakpointsInstalled = true;

    auto& map = pcCodeBlockMap();
#if !defined(NDEBUG)
    // We need to be able to handle more than one invalidation point at the same pc
    // but we want to make sure we don't forget to remove a pc from the map.
    HashSet<void*> newReplacements;
#endif
    for (auto& jumpReplacement : m_jumpReplacements) {
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
    Locker locker { pcCodeBlockMapLock };
    auto& map = pcCodeBlockMap();
    auto result = map.find(pc);
    if (result == map.end())
        return nullptr;
    return result->value;
}

bool CommonData::isVMTrapBreakpoint(void* address)
{
    if (!isStillValid)
        return false;
    for (unsigned i = m_jumpReplacements.size(); i--;) {
        if (address == m_jumpReplacements[i].dataLocation())
            return true;
    }
    return false;
}

void CommonData::validateReferences(const TrackedReferences& trackedReferences)
{
    if (InlineCallFrameSet* set = inlineCallFrames.get()) {
        for (InlineCallFrame* inlineCallFrame : *set) {
            for (ValueRecovery& recovery : inlineCallFrame->m_argumentsWithFixup) {
                if (recovery.isConstant())
                    trackedReferences.check(recovery.constant());
            }
            
            if (CodeBlock* baselineCodeBlock = inlineCallFrame->baselineCodeBlock.get())
                trackedReferences.check(baselineCodeBlock);
            
            if (inlineCallFrame->calleeRecovery.isConstant())
                trackedReferences.check(inlineCallFrame->calleeRecovery.constant());
        }
    }
    
    for (auto& watchpoint : m_adaptiveStructureWatchpoints)
        watchpoint.key().validateReferences(trackedReferences);
}

void CommonData::finalizeCatchEntrypoints(Vector<CatchEntrypointData>&& catchEntrypoints)
{
    std::sort(catchEntrypoints.begin(), catchEntrypoints.end(),
        [] (const CatchEntrypointData& a, const CatchEntrypointData& b) { return a.bytecodeIndex < b.bytecodeIndex; });
    ASSERT(m_catchEntrypoints.isEmpty());
    m_catchEntrypoints = WTFMove(catchEntrypoints);

#if ASSERT_ENABLED
    for (unsigned i = 0; i + 1 < m_catchEntrypoints.size(); ++i)
        ASSERT(m_catchEntrypoints[i].bytecodeIndex <= m_catchEntrypoints[i + 1].bytecodeIndex);
#endif
}

void CommonData::clearWatchpoints()
{
    m_watchpoints = FixedVector<CodeBlockJettisoningWatchpoint>();
    m_adaptiveStructureWatchpoints = FixedVector<AdaptiveStructureWatchpoint>();
    m_adaptiveInferredPropertyValueWatchpoints = FixedVector<AdaptiveInferredPropertyValueWatchpoint>();
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

