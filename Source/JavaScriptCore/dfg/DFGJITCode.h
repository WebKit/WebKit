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

#include "CodeBlock.h"
#include "CompilationResult.h"
#include "DFGCommonData.h"
#include "DFGMinifiedGraph.h"
#include "DFGOSREntry.h"
#include "DFGOSRExit.h"
#include "DFGVariableEventStream.h"
#include "ExecutionCounter.h"
#include "JITCode.h"
#include <wtf/SegmentedVector.h>

namespace JSC {

class TrackedReferences;

namespace DFG {

class JITCompiler;

class JITCode final : public DirectJITCode {
public:
    JITCode();
    ~JITCode() final;
    
    CommonData* dfgCommon() final;
    JITCode* dfg() final;
    
    OSREntryData* osrEntryDataForBytecodeIndex(BytecodeIndex bytecodeIndex)
    {
        return tryBinarySearch<OSREntryData, BytecodeIndex>(
            m_osrEntry, m_osrEntry.size(), bytecodeIndex,
            getOSREntryDataBytecodeIndex);
    }

    void finalizeOSREntrypoints(Vector<DFG::OSREntryData>&&);
    
    void reconstruct(
        CodeBlock*, CodeOrigin, unsigned streamIndex, Operands<ValueRecovery>& result);
    
    // This is only applicable if we're at a point where all values are spilled to the
    // stack. Currently, it also has the restriction that the values must be in their
    // bytecode-designated stack slots.
    void reconstruct(
        CallFrame*, CodeBlock*, CodeOrigin, unsigned streamIndex, Operands<std::optional<JSValue>>& result);

#if ENABLE(FTL_JIT)
    // NB. All of these methods take CodeBlock* because they may want to use
    // CodeBlock's logic about scaling thresholds. It should be a DFG CodeBlock.
    
    bool checkIfOptimizationThresholdReached(CodeBlock*);
    void optimizeNextInvocation(CodeBlock*);
    void dontOptimizeAnytimeSoon(CodeBlock*);
    void optimizeAfterWarmUp(CodeBlock*);
    void optimizeSoon(CodeBlock*);
    void forceOptimizationSlowPathConcurrently(CodeBlock*);
    void setOptimizationThresholdBasedOnCompilationResult(CodeBlock*, CompilationResult);
#endif // ENABLE(FTL_JIT)
    
    void validateReferences(const TrackedReferences&) final;
    
    void shrinkToFit(const ConcurrentJSLocker&) final;

    RegisterSet liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex) final;
#if ENABLE(FTL_JIT)
    CodeBlock* osrEntryBlock() { return m_osrEntryBlock.get(); }
    void setOSREntryBlock(VM&, const JSCell* owner, CodeBlock* osrEntryBlock);
    void clearOSREntryBlockAndResetThresholds(CodeBlock* dfgCodeBlock);
#endif

    static ptrdiff_t commonDataOffset() { return OBJECT_OFFSETOF(JITCode, common); }

    std::optional<CodeOrigin> findPC(CodeBlock*, void* pc) final;

    using DirectJITCode::initializeCodeRefForDFG;
    
private:
    friend class JITCompiler; // Allow JITCompiler to call setCodeRef().

public:
    CommonData common;
    FixedVector<DFG::OSREntryData> m_osrEntry;
    FixedVector<DFG::OSRExit> m_osrExit;
    FixedVector<DFG::SpeculationRecovery> m_speculationRecovery;
    DFG::VariableEventStream variableEventStream;
    DFG::MinifiedGraph minifiedDFG;

#if ENABLE(FTL_JIT)
    uint8_t neverExecutedEntry { 1 };

    UpperTierExecutionCounter tierUpCounter;

    // For osrEntryPoint that are in inner loop, this maps their bytecode to the bytecode
    // of the outerloop entry points in order (from innermost to outermost).
    //
    // The key may not always be a target for OSR Entry but the list in the value is guaranteed
    // to be usable for OSR Entry.
    HashMap<BytecodeIndex, FixedVector<BytecodeIndex>> tierUpInLoopHierarchy;

    // Map each bytecode of CheckTierUpAndOSREnter to its stream index.
    HashMap<BytecodeIndex, unsigned> bytecodeIndexToStreamIndex;

    enum class TriggerReason : uint8_t {
        DontTrigger,
        CompilationDone,
        StartCompilation,
    };

    // Map each bytecode of CheckTierUpAndOSREnter to its trigger forcing OSR Entry.
    // This can never be modified after it has been initialized since the addresses of the triggers
    // are used by the JIT.
    HashMap<BytecodeIndex, TriggerReason> tierUpEntryTriggers;

    WriteBarrier<CodeBlock> m_osrEntryBlock;
    unsigned osrEntryRetry;
    bool abandonOSREntry;
#endif // ENABLE(FTL_JIT)
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
