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
#include "DFGPlan.h"

#if ENABLE(DFG_JIT)

#include "DFGArgumentsEliminationPhase.h"
#include "DFGBackwardsPropagationPhase.h"
#include "DFGByteCodeParser.h"
#include "DFGCFAPhase.h"
#include "DFGCFGSimplificationPhase.h"
#include "DFGCPSRethreadingPhase.h"
#include "DFGCSEPhase.h"
#include "DFGCleanUpPhase.h"
#include "DFGConstantFoldingPhase.h"
#include "DFGConstantHoistingPhase.h"
#include "DFGCopyBarrierOptimizationPhase.h"
#include "DFGCriticalEdgeBreakingPhase.h"
#include "DFGDCEPhase.h"
#include "DFGFailedFinalizer.h"
#include "DFGFixupPhase.h"
#include "DFGGraphSafepoint.h"
#include "DFGIntegerCheckCombiningPhase.h"
#include "DFGIntegerRangeOptimizationPhase.h"
#include "DFGInvalidationPointInjectionPhase.h"
#include "DFGJITCompiler.h"
#include "DFGLICMPhase.h"
#include "DFGLiveCatchVariablePreservationPhase.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGLoopPreHeaderCreationPhase.h"
#include "DFGMaximalFlushInsertionPhase.h"
#include "DFGMovHintRemovalPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGOSREntrypointCreationPhase.h"
#include "DFGObjectAllocationSinkingPhase.h"
#include "DFGPhantomInsertionPhase.h"
#include "DFGPredictionInjectionPhase.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGPutStackSinkingPhase.h"
#include "DFGSSAConversionPhase.h"
#include "DFGSSALoweringPhase.h"
#include "DFGStackLayoutPhase.h"
#include "DFGStaticExecutionCountEstimationPhase.h"
#include "DFGStoreBarrierInsertionPhase.h"
#include "DFGStrengthReductionPhase.h"
#include "DFGStructureRegistrationPhase.h"
#include "DFGTierUpCheckInjectionPhase.h"
#include "DFGTypeCheckHoistingPhase.h"
#include "DFGUnificationPhase.h"
#include "DFGValidate.h"
#include "DFGVarargsForwardingPhase.h"
#include "DFGVirtualRegisterAllocationPhase.h"
#include "DFGWatchpointCollectionPhase.h"
#include "Debugger.h"
#include "JSCInlines.h"
#include "OperandsInlines.h"
#include "ProfilerDatabase.h"
#include "TrackedReferences.h"
#include <wtf/CurrentTime.h>

#if ENABLE(FTL_JIT)
#include "FTLCapabilities.h"
#include "FTLCompile.h"
#include "FTLFail.h"
#include "FTLLink.h"
#include "FTLLowerDFGToB3.h"
#include "FTLState.h"
#endif

namespace JSC { namespace DFG {

namespace {

double totalDFGCompileTime;
double totalFTLCompileTime;
double totalFTLDFGCompileTime;
double totalFTLB3CompileTime;

void dumpAndVerifyGraph(Graph& graph, const char* text, bool forceDump = false)
{
    GraphDumpMode modeForFinalValidate = DumpGraph;
    if (verboseCompilationEnabled(graph.m_plan.mode) || forceDump) {
        dataLog(text, "\n");
        graph.dump();
        modeForFinalValidate = DontDumpGraph;
    }
    if (validationEnabled())
        validate(graph, modeForFinalValidate);
}

Profiler::CompilationKind profilerCompilationKindForMode(CompilationMode mode)
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

} // anonymous namespace

Plan::Plan(CodeBlock* passedCodeBlock, CodeBlock* profiledDFGCodeBlock,
    CompilationMode mode, unsigned osrEntryBytecodeIndex,
    const Operands<JSValue>& mustHandleValues)
    : vm(*passedCodeBlock->vm())
    , codeBlock(passedCodeBlock)
    , profiledDFGCodeBlock(profiledDFGCodeBlock)
    , mode(mode)
    , osrEntryBytecodeIndex(osrEntryBytecodeIndex)
    , mustHandleValues(mustHandleValues)
    , compilation(codeBlock->vm()->m_perBytecodeProfiler ? adoptRef(new Profiler::Compilation(codeBlock->vm()->m_perBytecodeProfiler->ensureBytecodesFor(codeBlock), profilerCompilationKindForMode(mode))) : 0)
    , inlineCallFrames(adoptRef(new InlineCallFrameSet()))
    , identifiers(codeBlock)
    , weakReferences(codeBlock)
    , willTryToTierUp(false)
    , stage(Preparing)
{
}

Plan::~Plan()
{
}

bool Plan::computeCompileTimes() const
{
    return reportCompileTimes()
        || Options::reportTotalCompileTimes();
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
    if (computeCompileTimes())
        before = monotonicallyIncreasingTimeMS();
    if (reportCompileTimes())
        codeBlockName = toCString(*codeBlock);
    
    SamplingRegion samplingRegion("DFG Compilation (Plan)");
    CompilationScope compilationScope;

    if (logCompilationChanges(mode))
        dataLog("DFG(Plan) compiling ", *codeBlock, " with ", mode, ", number of instructions = ", codeBlock->instructionCount(), "\n");

    CompilationPath path = compileInThreadImpl(longLivedState);

    RELEASE_ASSERT(path == CancelPath || finalizer);
    RELEASE_ASSERT((path == CancelPath) == (stage == Cancelled));
    
    double after = 0;
    if (computeCompileTimes())
        after = monotonicallyIncreasingTimeMS();
    
    if (Options::reportTotalCompileTimes()) {
        if (isFTL(mode)) {
            totalFTLCompileTime += after - before;
            totalFTLDFGCompileTime += m_timeBeforeFTL - before;
            totalFTLB3CompileTime += after - m_timeBeforeFTL;
        } else
            totalDFGCompileTime += after - before;
    }
    
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
#if COMPILER_QUIRK(CONSIDERS_UNREACHABLE_CODE)
            pathName = "";
#endif
            break;
        }
        dataLog("Optimized ", codeBlockName, " using ", mode, " with ", pathName, " into ", finalizer ? finalizer->codeSize() : 0, " bytes in ", after - before, " ms");
        if (path == FTLPath)
            dataLog(" (DFG: ", m_timeBeforeFTL - before, ", B3: ", after - m_timeBeforeFTL, ")");
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
        finalizer = std::make_unique<FailedFinalizer>(*this);
        return FailPath;
    }

    codeBlock->setCalleeSaveRegisters(RegisterSet::dfgCalleeSaveRegisters());
    
    // By this point the DFG bytecode parser will have potentially mutated various tables
    // in the CodeBlock. This is a good time to perform an early shrink, which is more
    // powerful than a late one. It's safe to do so because we haven't generated any code
    // that references any of the tables directly, yet.
    codeBlock->shrinkToFit(CodeBlock::EarlyShrink);

    if (validationEnabled())
        validate(dfg);
    
    if (Options::dumpGraphAfterParsing()) {
        dataLog("Graph after parsing:\n");
        dfg.dump();
    }

    performLiveCatchVariablePreservationPhase(dfg);

    if (Options::useMaximalFlushInsertionPhase())
        performMaximalFlushInsertion(dfg);
    
    performCPSRethreading(dfg);
    performUnification(dfg);
    performPredictionInjection(dfg);
    
    performStaticExecutionCountEstimation(dfg);
    
    if (mode == FTLForOSREntryMode) {
        bool result = performOSREntrypointCreation(dfg);
        if (!result) {
            finalizer = std::make_unique<FailedFinalizer>(*this);
            return FailPath;
        }
        performCPSRethreading(dfg);
    }
    
    if (validationEnabled())
        validate(dfg);
    
    performBackwardsPropagation(dfg);
    performPredictionPropagation(dfg);
    performFixup(dfg);
    performStructureRegistration(dfg);
    performInvalidationPointInjection(dfg);
    performTypeCheckHoisting(dfg);
    
    dfg.m_fixpointState = FixpointNotConverged;
    
    // For now we're back to avoiding a fixpoint. Note that we've ping-ponged on this decision
    // many times. For maximum throughput, it's best to fixpoint. But the throughput benefit is
    // small and not likely to show up in FTL anyway. On the other hand, not fixpointing means
    // that the compiler compiles more quickly. We want the third tier to compile quickly, which
    // not fixpointing accomplishes; and the fourth tier shouldn't need a fixpoint.
    if (validationEnabled())
        validate(dfg);
        
    performStrengthReduction(dfg);
    performLocalCSE(dfg);
    performCPSRethreading(dfg);
    performCFA(dfg);
    performConstantFolding(dfg);
    bool changed = false;
    changed |= performCFGSimplification(dfg);
    changed |= performLocalCSE(dfg);
    
    if (validationEnabled())
        validate(dfg);
    
    performCPSRethreading(dfg);
    if (!isFTL(mode)) {
        // Only run this if we're not FTLing, because currently for a LoadVarargs that is forwardable and
        // in a non-varargs inlined call frame, this will generate ForwardVarargs while the FTL
        // ArgumentsEliminationPhase will create a sequence of GetStack+PutStacks. The GetStack+PutStack
        // sequence then gets sunk, eliminating anything that looks like an escape for subsequent phases,
        // while the ForwardVarargs doesn't get simplified until later (or not at all) and looks like an
        // escape for all of the arguments. This then disables object allocation sinking.
        //
        // So, for now, we just disable this phase for the FTL.
        //
        // If we wanted to enable it, we'd have to do any of the following:
        // - Enable ForwardVarargs->GetStack+PutStack strength reduction, and have that run before
        //   PutStack sinking and object allocation sinking.
        // - Make VarargsForwarding emit a GetLocal+SetLocal sequence, that we can later turn into
        //   GetStack+PutStack.
        //
        // But, it's not super valuable to enable those optimizations, since the FTL
        // ArgumentsEliminationPhase does everything that this phase does, and it doesn't introduce this
        // pathology.
        
        changed |= performVarargsForwarding(dfg); // Do this after CFG simplification and CPS rethreading.
    }
    if (changed) {
        performCFA(dfg);
        performConstantFolding(dfg);
    }
    
    // If we're doing validation, then run some analyses, to give them an opportunity
    // to self-validate. Now is as good a time as any to do this.
    if (validationEnabled()) {
        dfg.ensureDominators();
        dfg.ensureNaturalLoops();
        dfg.ensurePrePostNumbering();
    }

    switch (mode) {
    case DFGMode: {
        dfg.m_fixpointState = FixpointConverged;
    
        performTierUpCheckInjection(dfg);

        performFastStoreBarrierInsertion(dfg);
        performCleanUp(dfg);
        performCPSRethreading(dfg);
        performDCE(dfg);
        if (Options::useCopyBarrierOptimization())
            performCopyBarrierOptimization(dfg);
        performPhantomInsertion(dfg);
        performStackLayout(dfg);
        performVirtualRegisterAllocation(dfg);
        performWatchpointCollection(dfg);
        dumpAndVerifyGraph(dfg, "Graph after optimization:");
        
        JITCompiler dataFlowJIT(dfg);
        if (codeBlock->codeType() == FunctionCode)
            dataFlowJIT.compileFunction();
        else
            dataFlowJIT.compile();
        
        return DFGPath;
    }
    
    case FTLMode:
    case FTLForOSREntryMode: {
#if ENABLE(FTL_JIT)
        if (FTL::canCompile(dfg) == FTL::CannotCompile) {
            finalizer = std::make_unique<FailedFinalizer>(*this);
            return FailPath;
        }
        
        performCleanUp(dfg); // Reduce the graph size a bit.
        performCriticalEdgeBreaking(dfg);
        if (Options::createPreHeaders())
            performLoopPreHeaderCreation(dfg);
        performCPSRethreading(dfg);
        performSSAConversion(dfg);
        performSSALowering(dfg);
        
        // Ideally, these would be run to fixpoint with the object allocation sinking phase.
        performArgumentsElimination(dfg);
        if (Options::usePutStackSinking())
            performPutStackSinking(dfg);
        
        performConstantHoisting(dfg);
        performGlobalCSE(dfg);
        performLivenessAnalysis(dfg);
        performIntegerRangeOptimization(dfg);
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        performConstantFolding(dfg);
        performCleanUp(dfg); // Reduce the graph size a lot.
        changed = false;
        changed |= performStrengthReduction(dfg);
        if (Options::useObjectAllocationSinking()) {
            changed |= performCriticalEdgeBreaking(dfg);
            changed |= performObjectAllocationSinking(dfg);
        }
        if (changed) {
            // State-at-tail and state-at-head will be invalid if we did strength reduction since
            // it might increase live ranges.
            performLivenessAnalysis(dfg);
            performCFA(dfg);
            performConstantFolding(dfg);
        }
        
        // Currently, this relies on pre-headers still being valid. That precludes running CFG
        // simplification before it, unless we re-created the pre-headers. There wouldn't be anything
        // wrong with running LICM earlier, if we wanted to put other CFG transforms above this point.
        // Alternatively, we could run loop pre-header creation after SSA conversion - but if we did that
        // then we'd need to do some simple SSA fix-up.
        performLICM(dfg);
        
        performCleanUp(dfg);
        performIntegerCheckCombining(dfg);
        performGlobalCSE(dfg);
        
        // At this point we're not allowed to do any further code motion because our reasoning
        // about code motion assumes that it's OK to insert GC points in random places.
        dfg.m_fixpointState = FixpointConverged;
        
        performLivenessAnalysis(dfg);
        performCFA(dfg);
        performGlobalStoreBarrierInsertion(dfg);
        if (Options::useMovHintRemoval())
            performMovHintRemoval(dfg);
        performCleanUp(dfg);
        performDCE(dfg); // We rely on this to kill dead code that won't be recognized as dead by B3.
        if (Options::useCopyBarrierOptimization())
            performCopyBarrierOptimization(dfg);
        performStackLayout(dfg);
        performLivenessAnalysis(dfg);
        performOSRAvailabilityAnalysis(dfg);
        performWatchpointCollection(dfg);
        
        if (FTL::canCompile(dfg) == FTL::CannotCompile) {
            finalizer = std::make_unique<FailedFinalizer>(*this);
            return FailPath;
        }

        dumpAndVerifyGraph(dfg, "Graph just before FTL lowering:", shouldDumpDisassembly(mode));

        // Flash a safepoint in case the GC wants some action.
        Safepoint::Result safepointResult;
        {
            GraphSafepoint safepoint(dfg, safepointResult);
        }
        if (safepointResult.didGetCancelled())
            return CancelPath;

        FTL::State state(dfg);
        FTL::lowerDFGToB3(state);
        
        if (computeCompileTimes())
            m_timeBeforeFTL = monotonicallyIncreasingTimeMS();
        
        if (Options::b3AlwaysFailsBeforeCompile()) {
            FTL::fail(state);
            return FTLPath;
        }
        
        FTL::compile(state, safepointResult);
        if (safepointResult.didGetCancelled())
            return CancelPath;
        
        if (Options::b3AlwaysFailsBeforeLink()) {
            FTL::fail(state);
            return FTLPath;
        }
        
        if (state.allocationFailed) {
            FTL::fail(state);
            return FTLPath;
        }

        FTL::link(state);
        
        if (state.allocationFailed) {
            FTL::fail(state);
            return FTLPath;
        }
        
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
    return true;
}

void Plan::reallyAdd(CommonData* commonData)
{
    watchpoints.reallyAdd(codeBlock, *commonData);
    identifiers.reallyAdd(vm, commonData);
    weakReferences.reallyAdd(vm, commonData);
    transitions.reallyAdd(vm, commonData);
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
    callback->compilationDidBecomeReadyAsynchronously(codeBlock, profiledDFGCodeBlock);
    stage = Ready;
}

CompilationResult Plan::finalizeWithoutNotifyingCallback()
{
    // We will establish new references from the code block to things. So, we need a barrier.
    vm.heap.writeBarrier(codeBlock);
    
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
    
    if (validationEnabled()) {
        TrackedReferences trackedReferences;
        
        for (WriteBarrier<JSCell>& reference : codeBlock->jitCode()->dfgCommon()->weakReferences)
            trackedReferences.add(reference.get());
        for (WriteBarrier<Structure>& reference : codeBlock->jitCode()->dfgCommon()->weakStructureReferences)
            trackedReferences.add(reference.get());
        for (WriteBarrier<Unknown>& constant : codeBlock->constants())
            trackedReferences.add(constant.get());
        
        // Check that any other references that we have anywhere in the JITCode are also
        // tracked either strongly or weakly.
        codeBlock->jitCode()->validateReferences(trackedReferences);
    }
    
    return CompilationSuccessful;
}

void Plan::finalizeAndNotifyCallback()
{
    callback->compilationDidComplete(codeBlock, profiledDFGCodeBlock, finalizeWithoutNotifyingCallback());
}

CompilationKey Plan::key()
{
    return CompilationKey(codeBlock->alternative(), mode);
}

void Plan::rememberCodeBlocks()
{
    // Compilation writes lots of values to a CodeBlock without performing
    // an explicit barrier. So, we need to be pessimistic and assume that
    // all our CodeBlocks must be visited during GC.

    Heap::heap(codeBlock)->writeBarrier(codeBlock);
    Heap::heap(codeBlock)->writeBarrier(codeBlock->alternative());
    if (profiledDFGCodeBlock)
        Heap::heap(profiledDFGCodeBlock)->writeBarrier(profiledDFGCodeBlock);
}

void Plan::checkLivenessAndVisitChildren(SlotVisitor& visitor)
{
    if (!isKnownToBeLiveDuringGC())
        return;
    
    for (unsigned i = mustHandleValues.size(); i--;)
        visitor.appendUnbarrieredValue(&mustHandleValues[i]);

    visitor.appendUnbarrieredReadOnlyPointer(codeBlock);
    visitor.appendUnbarrieredReadOnlyPointer(codeBlock->alternative());
    visitor.appendUnbarrieredReadOnlyPointer(profiledDFGCodeBlock);

    if (inlineCallFrames) {
        for (auto* inlineCallFrame : *inlineCallFrames) {
            ASSERT(inlineCallFrame->baselineCodeBlock.get());
            visitor.appendUnbarrieredReadOnlyPointer(inlineCallFrame->baselineCodeBlock.get());
        }
    }

    weakReferences.visitChildren(visitor);
    transitions.visitChildren(visitor);
}

bool Plan::isKnownToBeLiveDuringGC()
{
    if (stage == Cancelled)
        return false;
    if (!Heap::isMarked(codeBlock->ownerExecutable()))
        return false;
    if (!Heap::isMarked(codeBlock->alternative()))
        return false;
    if (!!profiledDFGCodeBlock && !Heap::isMarked(profiledDFGCodeBlock))
        return false;
    return true;
}

void Plan::cancel()
{
    codeBlock = nullptr;
    profiledDFGCodeBlock = nullptr;
    mustHandleValues.clear();
    compilation = nullptr;
    finalizer = nullptr;
    inlineCallFrames = nullptr;
    watchpoints = DesiredWatchpoints();
    identifiers = DesiredIdentifiers();
    weakReferences = DesiredWeakReferences();
    transitions = DesiredTransitions();
    callback = nullptr;
    stage = Cancelled;
}

HashMap<CString, double> Plan::compileTimeStats()
{
    HashMap<CString, double> result;
    if (Options::reportTotalCompileTimes()) {
        result.add("Compile Time", totalDFGCompileTime + totalFTLCompileTime);
        result.add("DFG Compile Time", totalDFGCompileTime);
        result.add("FTL Compile Time", totalFTLCompileTime);
        result.add("FTL (DFG) Compile Time", totalFTLDFGCompileTime);
        result.add("FTL (B3) Compile Time", totalFTLB3CompileTime);
    }
    return result;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

