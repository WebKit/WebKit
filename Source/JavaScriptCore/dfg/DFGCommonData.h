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

#pragma once

#if ENABLE(DFG_JIT)

#include "CodeBlockJettisoningWatchpoint.h"
#include "DFGAdaptiveInferredPropertyValueWatchpoint.h"
#include "DFGAdaptiveStructureWatchpoint.h"
#include "DFGJumpReplacement.h"
#include "DFGOSREntry.h"
#include "InlineCallFrameSet.h"
#include "JSCast.h"
#include "ProfilerCompilation.h"
#include "RecordedStatuses.h"
#include <wtf/Bag.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class CodeBlock;
class Identifier;
class TrackedReferences;

namespace DFG {

struct Node;
class Plan;

// CommonData holds the set of data that both DFG and FTL code blocks need to know
// about themselves.

struct WeakReferenceTransition {
    WeakReferenceTransition() { }
    
    WeakReferenceTransition(VM& vm, JSCell* owner, JSCell* codeOrigin, JSCell* from, JSCell* to)
        : m_from(vm, owner, from)
        , m_to(vm, owner, to)
    {
        if (!!codeOrigin)
            m_codeOrigin.set(vm, owner, codeOrigin);
    }
    
    WriteBarrier<JSCell> m_codeOrigin;
    WriteBarrier<JSCell> m_from;
    WriteBarrier<JSCell> m_to;
};
        
class CommonData {
    WTF_MAKE_NONCOPYABLE(CommonData);
public:
    CommonData()
        : isStillValid(true)
        , frameRegisterCount(std::numeric_limits<unsigned>::max())
        , requiredRegisterCountForExit(std::numeric_limits<unsigned>::max())
    { }
    ~CommonData();
    
    void notifyCompilingStructureTransition(Plan&, CodeBlock*, Node*);
    CallSiteIndex addCodeOrigin(CodeOrigin);
    CallSiteIndex addUniqueCallSiteIndex(CodeOrigin);
    CallSiteIndex lastCallSite() const;
    void removeCallSiteIndex(CallSiteIndex);
    
    void shrinkToFit();
    
    bool invalidate(); // Returns true if we did invalidate, or false if the code block was already invalidated.
    bool hasInstalledVMTrapsBreakpoints() const { return isStillValid && hasVMTrapsBreakpointsInstalled; }
    void installVMTrapBreakpoints(CodeBlock* owner);
    bool isVMTrapBreakpoint(void* address);

    CatchEntrypointData* catchOSREntryDataForBytecodeIndex(unsigned bytecodeIndex)
    {
        return tryBinarySearch<CatchEntrypointData, unsigned>(
            catchEntrypoints, catchEntrypoints.size(), bytecodeIndex,
            [] (const CatchEntrypointData* item) { return item->bytecodeIndex; });
    }

    void appendCatchEntrypoint(unsigned bytecodeIndex, MacroAssemblerCodePtr<ExceptionHandlerPtrTag> machineCode, Vector<FlushFormat>&& argumentFormats)
    {
        catchEntrypoints.append(CatchEntrypointData { machineCode,  WTFMove(argumentFormats), bytecodeIndex });
    }

    void finalizeCatchEntrypoints();

    unsigned requiredRegisterCountForExecutionAndExit() const
    {
        return std::max(frameRegisterCount, requiredRegisterCountForExit);
    }
    
    void validateReferences(const TrackedReferences&);

    static ptrdiff_t frameRegisterCountOffset() { return OBJECT_OFFSETOF(CommonData, frameRegisterCount); }
    
    void clearWatchpoints();

    RefPtr<InlineCallFrameSet> inlineCallFrames;
    Vector<CodeOrigin, 0, UnsafeVectorOverflow> codeOrigins;
    
    Vector<Identifier> dfgIdentifiers;
    Vector<WeakReferenceTransition> transitions;
    Vector<WriteBarrier<JSCell>> weakReferences;
    Vector<WriteBarrier<Structure>> weakStructureReferences;
    Vector<CatchEntrypointData> catchEntrypoints;
    Bag<CodeBlockJettisoningWatchpoint> watchpoints;
    Bag<AdaptiveStructureWatchpoint> adaptiveStructureWatchpoints;
    Bag<AdaptiveInferredPropertyValueWatchpoint> adaptiveInferredPropertyValueWatchpoints;
    RecordedStatuses recordedStatuses;
    Vector<JumpReplacement> jumpReplacements;
    
    ScratchBuffer* catchOSREntryBuffer;
    RefPtr<Profiler::Compilation> compilation;
    bool livenessHasBeenProved; // Initialized and used on every GC.
    bool allTransitionsHaveBeenMarked; // Initialized and used on every GC.
    bool isStillValid;
    bool hasVMTrapsBreakpointsInstalled { false };
    
#if USE(JSVALUE32_64)
    std::unique_ptr<Bag<double>> doubleConstants;
#endif
    
    unsigned frameRegisterCount;
    unsigned requiredRegisterCountForExit;

private:
    HashSet<unsigned, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> callSiteIndexFreeList;

};

CodeBlock* codeBlockForVMTrapPC(void* pc);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
