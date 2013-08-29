/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "DFGPlan.h"

#if ENABLE(DFG_JIT)

#include "DFGArgumentsSimplificationPhase.h"
#include "DFGBackwardsPropagationPhase.h"
#include "DFGByteCodeParser.h"
#include "DFGCFAPhase.h"
#include "DFGCFGSimplificationPhase.h"
#include "DFGCPSRethreadingPhase.h"
#include "DFGCSEPhase.h"
#include "DFGConstantFoldingPhase.h"
#include "DFGCriticalEdgeBreakingPhase.h"
#include "DFGDCEPhase.h"
#include "DFGFailedFinalizer.h"
#include "DFGFlushLivenessAnalysisPhase.h"
#include "DFGFixupPhase.h"
#include "DFGJITCompiler.h"
#include "DFGLICMPhase.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGLoopPreHeaderCreationPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGPredictionInjectionPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGSSAConversionPhase.h"
#include "DFGTypeCheckHoistingPhase.h"
#include "DFGUnificationPhase.h"
#include "DFGValidate.h"
#include "DFGVirtualRegisterAllocationPhase.h"
#include "Operations.h"
#include <wtf/CurrentTime.h>

#if ENABLE(FTL_JIT)
#include "FTLCapabilities.h"
#include "FTLCompile.h"
#include "FTLFail.h"
#include "FTLLink.h"
#include "FTLLowerDFGToLLVM.h"
#include "FTLState.h"
#endif

namespace JSC { namespace DFG {

static void dumpAndVerifyGraph(Graph& graph, const char* text)
{
    GraphDumpMode modeForFinalValidate = DumpGraph;
    if (verboseCompilationEnabled()) {
        dataLog(text, "\n");
        graph.dump();
        modeForFinalValidate = DontDumpGraph;
    }
    if (validationEnabled())
        validate(graph, modeForFinalValidate);
}

Plan::Plan(
    PassRefPtr<CodeBlock> passedCodeBlock, CompilationMode mode,
    unsigned osrEntryBytecodeIndex, unsigned numVarsWithValues)
    : vm(*passedCodeBlock->vm())
    , codeBlock(passedCodeBlock)
    , mode(mode)
    , osrEntryBytecodeIndex(osrEntryBytecodeIndex)
    , numVarsWithValues(numVarsWithValues)
    , mustHandleValues(codeBlock->numParameters(), numVarsWithValues)
    , compilation(codeBlock->vm()->m_perBytecodeProfiler ? adoptRef(new Profiler::Compilation(codeBlock->vm()->m_perBytecodeProfiler->ensureBytecodesFor(codeBlock.get()), Profiler::DFG)) : 0)
    , identifiers(codeBlock.get())
    , weakReferences(codeBlock.get())
    , isCompiled(false)
{
}

Plan::~Plan()
{
}

void Plan::compileInThread(LongLivedState& longLivedState)
{
    double before = 0;
    if (Options::reportCompileTimes())
        before = currentTimeMS();
    
    SamplingRegion samplingRegion("DFG Compilation (Plan)");
    CompilationScope compilationScope;

    if (logCompilationChanges())
        dataLog("DFG(Plan) compiling ", *codeBlock, ", number of instructions = ", codeBlock->instructionCount(), "\n");

    CompilationPath path = compileInThreadImpl(longLivedState);

    RELEASE_ASSERT(finalizer);
    
    if (Options::reportCompileTimes()) {
        const char* pathName;
        switch (path) {
        case FailPath:
            pathName = "N/A (fail)";
            break;
        case DFGPath:
            pathName = "DFG";
            break;
        case FTLPath:
            pathName = "FTL";
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            pathName = "";
            break;
        }
        double now = currentTimeMS();
        dataLog("Optimized ", *codeBlock->alternative(), " with ", pathName, " in ", now - before, " ms");
        if (path == FTLPath)
            dataLog(" (DFG: ", beforeFTL - before, ", LLVM: ", now - beforeFTL, ")");
        dataLog(".\n");
    }
}

Plan::CompilationPath Plan::compileInThreadImpl(LongLivedState& longLivedState)
{
    Graph dfg(vm, *this, longLivedState);
    
    if (!parse(dfg)) {
        finalizer = adoptPtr(new FailedFinalizer(*this));
        return FailPath;
    }
    
    // By this point the DFG bytecode parser will have potentially mutated various tables
    // in the CodeBlock. This is a good time to perform an early shrink, which is more
    // powerful than a late one. It's safe to do so because we haven't generated any code
    // that references any of the tables directly, yet.
    codeBlock->shrinkToFit(CodeBlock::EarlyShrink);

    if (validationEnabled())
        validate(dfg);
    
    performCPSRethreading(dfg);
    performUnification(dfg);
    performPredictionInjection(dfg);
    
    if (validationEnabled())
        validate(dfg);
    
    performBackwardsPropagation(dfg);
    performPredictionPropagation(dfg);
    performFixup(dfg);
    performTypeCheckHoisting(dfg);
    
    unsigned count = 1;
    dfg.m_fixpointState = FixpointNotConverged;
    for (;; ++count) {
        if (logCompilationChanges())
            dataLogF("DFG beginning optimization fixpoint iteration #%u.\n", count);
        bool changed = false;
        
        if (validationEnabled())
            validate(dfg);
        
        performCFA(dfg);
        changed |= performConstantFolding(dfg);
        changed |= performArgumentsSimplification(dfg);
        changed |= performCFGSimplification(dfg);
        changed |= performCSE(dfg);
        
        if (!changed)
            break;
        
        performCPSRethreading(dfg);
    }
    
    if (logCompilationChanges())
        dataLogF("DFG optimization fixpoint converged in %u iterations.\n", count);

    dfg.m_fixpointState = FixpointConverged;

    performStoreElimination(dfg);
    
    // If we're doing validation, then run some analyses, to give them an opportunity
    // to self-validate. Now is as good a time as any to do this.
    if (validationEnabled()) {
        dfg.m_dominators.computeIfNecessary(dfg);
        dfg.m_naturalLoops.computeIfNecessary(dfg);
    }

#if ENABLE(FTL_JIT)
    if (Options::useExperimentalFTL()
        && codeBlock->codeType() == FunctionCode
        && FTL::canCompile(dfg)) {
        
        performCriticalEdgeBreaking(dfg);
        performLoopPreHeaderCreation(dfg);
        performCPSRethreading(dfg);
        performSSAConversion(dfg);
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        performLICM(dfg);
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        performDCE(dfg); // We rely on this to convert dead SetLocals into the appropriate hint, and to kill dead code that won't be recognized as dead by LLVM.
        performLivenessAnalysis(dfg);
        performFlushLivenessAnalysis(dfg);
        performOSRAvailabilityAnalysis(dfg);
        
        dumpAndVerifyGraph(dfg, "Graph just before FTL lowering:");
        
        // FIXME: Support OSR entry.
        // https://bugs.webkit.org/show_bug.cgi?id=113625
        
        FTL::State state(dfg);
        FTL::lowerDFGToLLVM(state);
        
        if (Options::reportCompileTimes())
            beforeFTL = currentTimeMS();
        
        if (Options::llvmAlwaysFails()) {
            FTL::fail(state);
            return FTLPath;
        }
        
        FTL::compile(state);
        FTL::link(state);
        return FTLPath;
    }
#else
    RELEASE_ASSERT(!Options::useExperimentalFTL());
#endif // ENABLE(FTL_JIT)
    
    performCPSRethreading(dfg);
    performDCE(dfg);
    performVirtualRegisterAllocation(dfg);
    dumpAndVerifyGraph(dfg, "Graph after optimization:");

    JITCompiler dataFlowJIT(dfg);
    if (codeBlock->codeType() == FunctionCode) {
        dataFlowJIT.compileFunction();
        dataFlowJIT.linkFunction();
    } else {
        dataFlowJIT.compile();
        dataFlowJIT.link();
    }
    
    return DFGPath;
}

bool Plan::isStillValid()
{
    return watchpoints.areStillValid()
        && chains.areStillValid();
}

void Plan::reallyAdd(CommonData* commonData)
{
    watchpoints.reallyAdd();
    identifiers.reallyAdd(vm, commonData);
    weakReferences.reallyAdd(vm, commonData);
    transitions.reallyAdd(vm, commonData);
    writeBarriers.trigger(vm);
}

void Plan::notifyReady()
{
    callback->compilationDidBecomeReadyAsynchronously(codeBlock.get());
    isCompiled = true;
}

CompilationResult Plan::finalizeWithoutNotifyingCallback()
{
    if (!isStillValid())
        return CompilationInvalidated;
    
    bool result;
    if (codeBlock->codeType() == FunctionCode)
        result = finalizer->finalizeFunction();
    else
        result = finalizer->finalize();
    
    if (!result)
        return CompilationFailed;
    
    reallyAdd(codeBlock->jitCode()->dfgCommon());
    
    return CompilationSuccessful;
}

void Plan::finalizeAndNotifyCallback()
{
    callback->compilationDidComplete(codeBlock.get(), finalizeWithoutNotifyingCallback());
}

CompilationKey Plan::key()
{
    return CompilationKey(codeBlock->alternative(), mode);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

