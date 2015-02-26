/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "DFGFixupPhase.h"
#include "DFGGraphSafepoint.h"
#include "DFGIntegerCheckCombiningPhase.h"
#include "DFGInvalidationPointInjectionPhase.h"
#include "DFGJITCompiler.h"
#include "DFGLICMPhase.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGLoopPreHeaderCreationPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGOSREntrypointCreationPhase.h"
#include "DFGPredictionInjectionPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGResurrectionForValidationPhase.h"
#include "DFGSSAConversionPhase.h"
#include "DFGSSALoweringPhase.h"
#include "DFGStackLayoutPhase.h"
#include "DFGStaticExecutionCountEstimationPhase.h"
#include "DFGStoreBarrierElisionPhase.h"
#include "DFGStrengthReductionPhase.h"
#include "DFGTierUpCheckInjectionPhase.h"
#include "DFGTypeCheckHoistingPhase.h"
#include "DFGUnificationPhase.h"
#include "DFGValidate.h"
#include "DFGVirtualRegisterAllocationPhase.h"
#include "DFGWatchpointCollectionPhase.h"
#include "Debugger.h"
#include "JSCInlines.h"
#include "OperandsInlines.h"
#include "ProfilerDatabase.h"
#include <wtf/CurrentTime.h>

#if ENABLE(FTL_JIT)
#include "FTLCapabilities.h"
#include "FTLCompile.h"
#include "FTLFail.h"
#include "FTLLink.h"
#include "FTLLowerDFGToLLVM.h"
#include "FTLState.h"
#include "InitializeLLVM.h"
#endif

namespace JSC { namespace DFG {

static void dumpAndVerifyGraph(Graph& graph, const char* text)
{
    GraphDumpMode modeForFinalValidate = DumpGraph;
    if (verboseCompilationEnabled(graph.m_plan.mode)) {
        dataLog(text, "\n");
        graph.dump();
        modeForFinalValidate = DontDumpGraph;
    }
    if (validationEnabled())
        validate(graph, modeForFinalValidate);
}

static Profiler::CompilationKind profilerCompilationKindForMode(CompilationMode mode)
{
    switch (mode) {
    case InvalidCompilationMode:
        RELEASE_ASSERT_NOT_REACHED();
        return Profiler::DFG;
    case DFGMode:
        return Profiler::DFG;
    case FTLMode:
        return Profiler::FTL;
    case FTLForOSREntryMode:
        return Profiler::FTLForOSREntry;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return Profiler::DFG;
}

Plan::Plan(PassRefPtr<CodeBlock> passedCodeBlock, CodeBlock* profiledDFGCodeBlock,
    CompilationMode mode, unsigned osrEntryBytecodeIndex,
    const Operands<JSValue>& mustHandleValues)
    : vm(*passedCodeBlock->vm())
    , codeBlock(passedCodeBlock)
    , profiledDFGCodeBlock(profiledDFGCodeBlock)
    , mode(mode)
    , osrEntryBytecodeIndex(osrEntryBytecodeIndex)
    , mustHandleValues(mustHandleValues)
    , compilation(codeBlock->vm()->m_perBytecodeProfiler ? adoptRef(new Profiler::Compilation(codeBlock->vm()->m_perBytecodeProfiler->ensureBytecodesFor(codeBlock.get()), profilerCompilationKindForMode(mode))) : 0)
    , inlineCallFrames(adoptRef(new InlineCallFrameSet()))
    , identifiers(codeBlock.get())
    , weakReferences(codeBlock.get())
    , willTryToTierUp(false)
    , stage(Preparing)
{
}

Plan::~Plan()
{
}

bool Plan::reportCompileTimes() const
{
    return Options::reportCompileTimes()
        || (Options::reportFTLCompileTimes() && isFTL(mode));
}

void Plan::compileInThread(LongLivedState& longLivedState, ThreadData* threadData)
{
    this->threadData = threadData;
    
    double before = 0;
    CString codeBlockName;
    if (reportCompileTimes()) {
        before = currentTimeMS();
        codeBlockName = toCString(*codeBlock);
    }
    
    SamplingRegion samplingRegion("DFG Compilation (Plan)");
    CompilationScope compilationScope;

    if (logCompilationChanges(mode))
        dataLog("DFG(Plan) compiling ", *codeBlock, " with ", mode, ", number of instructions = ", codeBlock->instructionCount(), "\n");

    CompilationPath path = compileInThreadImpl(longLivedState);

    RELEASE_ASSERT(path == CancelPath || finalizer);
    RELEASE_ASSERT((path == CancelPath) == (stage == Cancelled));
    
    if (reportCompileTimes()) {
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
        case CancelPath:
            pathName = "Cancelled";
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            pathName = "";
            break;
        }
        double now = currentTimeMS();
        dataLog("Optimized ", codeBlockName, " using ", mode, " with ", pathName, " into ", finalizer ? finalizer->codeSize() : 0, " bytes in ", now - before, " ms");
        if (path == FTLPath)
            dataLog(" (DFG: ", beforeFTL - before, ", LLVM: ", now - beforeFTL, ")");
        dataLog(".\n");
    }
}

Plan::CompilationPath Plan::compileInThreadImpl(LongLivedState& longLivedState)
{
    if (verboseCompilationEnabled(mode) && osrEntryBytecodeIndex != UINT_MAX) {
        dataLog("\n");
        dataLog("Compiler must handle OSR entry from bc#", osrEntryBytecodeIndex, " with values: ", mustHandleValues, "\n");
        dataLog("\n");
    }
    
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
    
    performStaticExecutionCountEstimation(dfg);
    
    if (mode == FTLForOSREntryMode) {
        bool result = performOSREntrypointCreation(dfg);
        if (!result) {
            finalizer = adoptPtr(new FailedFinalizer(*this));
            return FailPath;
        }
        performCPSRethreading(dfg);
    }
    
    if (validationEnabled())
        validate(dfg);
    
    performBackwardsPropagation(dfg);
    performPredictionPropagation(dfg);
    performFixup(dfg);
    performInvalidationPointInjection(dfg);
    performTypeCheckHoisting(dfg);
    
    unsigned count = 1;
    dfg.m_fixpointState = FixpointNotConverged;
    for (;; ++count) {
        if (logCompilationChanges(mode))
            dataLogF("DFG beginning optimization fixpoint iteration #%u.\n", count);
        bool changed = false;
        
        if (validationEnabled())
            validate(dfg);
        
        changed |= performStrengthReduction(dfg);
        performCFA(dfg);
        changed |= performConstantFolding(dfg);
        changed |= performArgumentsSimplification(dfg);
        changed |= performCFGSimplification(dfg);
        changed |= performCSE(dfg);
        
        if (!changed)
            break;
        
        performCPSRethreading(dfg);
    }
    
    if (logCompilationChanges(mode))
        dataLogF("DFG optimization fixpoint converged in %u iterations.\n", count);

    dfg.m_fixpointState = FixpointConverged;

    // If we're doing validation, then run some analyses, to give them an opportunity
    // to self-validate. Now is as good a time as any to do this.
    if (validationEnabled()) {
        dfg.m_dominators.computeIfNecessary(dfg);
        dfg.m_naturalLoops.computeIfNecessary(dfg);
    }

    switch (mode) {
    case DFGMode: {
        performTierUpCheckInjection(dfg);

        performStoreBarrierElision(dfg);
        performStoreElimination(dfg);
        performCPSRethreading(dfg);
        performDCE(dfg);
        performStackLayout(dfg);
        performVirtualRegisterAllocation(dfg);
        performWatchpointCollection(dfg);
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
    
    case FTLMode:
    case FTLForOSREntryMode: {
#if ENABLE(FTL_JIT)
        if (FTL::canCompile(dfg) == FTL::CannotCompile) {
            finalizer = adoptPtr(new FailedFinalizer(*this));
            return FailPath;
        }
        
        performCriticalEdgeBreaking(dfg);
        performLoopPreHeaderCreation(dfg);
        performCPSRethreading(dfg);
        performSSAConversion(dfg);
        performSSALowering(dfg);
        performCSE(dfg);
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        performLICM(dfg);
        performIntegerCheckCombining(dfg);
        performCSE(dfg);
        
        // At this point we're not allowed to do any further code motion because our reasoning
        // about code motion assumes that it's OK to insert GC points in random places.
        
        performStoreBarrierElision(dfg);
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        if (Options::validateFTLOSRExitLiveness())
            performResurrectionForValidation(dfg);
        performDCE(dfg); // We rely on this to convert dead SetLocals into the appropriate hint, and to kill dead code that won't be recognized as dead by LLVM.
        performStackLayout(dfg);
        performLivenessAnalysis(dfg);
        performOSRAvailabilityAnalysis(dfg);
        performWatchpointCollection(dfg);
        
        dumpAndVerifyGraph(dfg, "Graph just before FTL lowering:");
        
        bool haveLLVM;
        Safepoint::Result safepointResult;
        {
            GraphSafepoint safepoint(dfg, safepointResult);
            haveLLVM = initializeLLVM();
        }
        if (safepointResult.didGetCancelled())
            return CancelPath;
        
        if (!haveLLVM) {
            finalizer = adoptPtr(new FailedFinalizer(*this));
            return FailPath;
        }
            
        FTL::State state(dfg);
        if (!FTL::lowerDFGToLLVM(state)) {
            FTL::fail(state);
            return FTLPath;
        }
        
        if (reportCompileTimes())
            beforeFTL = currentTimeMS();
        
        if (Options::llvmAlwaysFailsBeforeCompile()) {
            FTL::fail(state);
            return FTLPath;
        }
        
        FTL::compile(state, safepointResult);
        if (safepointResult.didGetCancelled())
            return CancelPath;
        
        if (Options::llvmAlwaysFailsBeforeLink()) {
            FTL::fail(state);
            return FTLPath;
        }

        if (state.jitCode->stackmaps.stackSize() > Options::llvmMaxStackSize()) {
            FTL::fail(state);
            return FTLPath;
        }

        FTL::link(state);
        return FTLPath;
#else
        RELEASE_ASSERT_NOT_REACHED();
        return FailPath;
#endif // ENABLE(FTL_JIT)
    }
        
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return FailPath;
    }
}

bool Plan::isStillValid()
{
    CodeBlock* replacement = codeBlock->replacement();
    if (!replacement)
        return false;
    // FIXME: This is almost certainly not necessary. There's no way for the baseline
    // code to be replaced during a compilation, except if we delete the plan, in which
    // case we wouldn't be here.
    // https://bugs.webkit.org/show_bug.cgi?id=132707
    if (codeBlock->alternative() != replacement->baselineVersion())
        return false;
    if (!watchpoints.areStillValid())
        return false;
    if (!chains.areStillValid())
        return false;
    return true;
}

void Plan::reallyAdd(CommonData* commonData)
{
    watchpoints.reallyAdd(codeBlock.get(), *commonData);
    identifiers.reallyAdd(vm, commonData);
    weakReferences.reallyAdd(vm, commonData);
    transitions.reallyAdd(vm, commonData);
    writeBarriers.trigger(vm);
}

void Plan::notifyCompiling()
{
    stage = Compiling;
}

void Plan::notifyCompiled()
{
    stage = Compiled;
}

void Plan::notifyReady()
{
    callback->compilationDidBecomeReadyAsynchronously(codeBlock.get());
    stage = Ready;
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

void Plan::checkLivenessAndVisitChildren(SlotVisitor& visitor, CodeBlockSet& codeBlocks)
{
    if (!isKnownToBeLiveDuringGC())
        return;
    
    for (unsigned i = mustHandleValues.size(); i--;)
        visitor.appendUnbarrieredValue(&mustHandleValues[i]);
    
    codeBlocks.mark(codeBlock->alternative());
    codeBlocks.mark(codeBlock.get());
    codeBlocks.mark(profiledDFGCodeBlock.get());
    
    chains.visitChildren(visitor);
    weakReferences.visitChildren(visitor);
    writeBarriers.visitChildren(visitor);
    transitions.visitChildren(visitor);
}

bool Plan::isKnownToBeLiveDuringGC()
{
    if (stage == Cancelled)
        return false;
    if (!Heap::isMarked(codeBlock->ownerExecutable()))
        return false;
    if (!codeBlock->alternative()->isKnownToBeLiveDuringGC())
        return false;
    if (!!profiledDFGCodeBlock && !profiledDFGCodeBlock->isKnownToBeLiveDuringGC())
        return false;
    return true;
}

void Plan::cancel()
{
    codeBlock = nullptr;
    profiledDFGCodeBlock = nullptr;
    mustHandleValues.clear();
    compilation = nullptr;
    finalizer.clear();
    inlineCallFrames = nullptr;
    watchpoints = DesiredWatchpoints();
    identifiers = DesiredIdentifiers();
    chains = DesiredStructureChains();
    weakReferences = DesiredWeakReferences();
    writeBarriers = DesiredWriteBarriers();
    transitions = DesiredTransitions();
    callback = nullptr;
    stage = Cancelled;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

