/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "GCLogging.h"

using WTF::PrintStream;

namespace JSC {

#if PLATFORM(IOS_FAMILY)
#define MAXIMUM_NUMBER_OF_FTL_COMPILER_THREADS 2
#else
#define MAXIMUM_NUMBER_OF_FTL_COMPILER_THREADS 8
#endif

#if ENABLE(WEBASSEMBLY_STREAMING_API)
constexpr bool enableWebAssemblyStreamingApi = true;
#else
constexpr bool enableWebAssemblyStreamingApi = false;
#endif

// How do JSC VM options work?
// ===========================
// The FOR_EACH_JSC_OPTION() macro below defines a list of all JSC options in use,
// along with their types and default values. The options values are actually
// realized as fields in OptionsStorage embedded in JSC::Config.
//
//     Options::initialize() will initialize the option values with the defaults
// specified in FOR_EACH_JSC_OPTION() below. After that, the values can be
// programmatically read and written to using an accessor method with the same
// name as the option. For example, the option "useJIT" can be read and set like
// so:
//
//     bool jitIsOn = Options::useJIT();  // Get the option value.
//     Options::useJIT() = false;         // Sets the option value.
//
//     If you want to tweak any of these values programmatically for testing
// purposes, you can do so in Options::initialize() after the default values
// are set.
//
//     Alternatively, you can override the default values by specifying
// environment variables of the form: JSC_<name of JSC option>.
//
// Note: Options::initialize() tries to ensure some sanity on the option values
// which are set by doing some range checks, and value corrections. These
// checks are done after the option values are set. If you alter the option
// values after the sanity checks (for your own testing), then you're liable to
// ensure that the new values set are sane and reasonable for your own run.
//
// Any modifications to options must be done before the first VM is instantiated.
// On instantiation of the first VM instance, the Options will be write protected
// and cannot be modified thereafter.

#define FOR_EACH_JSC_OPTION(v)                                          \
    v(Bool, useKernTCSM, true, Normal, "Note: this needs to go before other options since they depend on this value.") \
    v(Bool, validateOptions, false, Normal, "crashes if mis-typed JSC options were passed to the VM") \
    v(Unsigned, dumpOptions, 0, Normal, "dumps JSC options (0 = None, 1 = Overridden only, 2 = All, 3 = Verbose)") \
    v(OptionString, configFile, nullptr, Normal, "file to configure JSC options and logging location") \
    \
    v(Bool, useLLInt,  true, Normal, "allows the LLINT to be used if true") \
    v(Bool, useJIT, jitEnabledByDefault(), Normal, "allows the executable pages to be allocated for JIT and thunks if true") \
    v(Bool, useBaselineJIT, true, Normal, "allows the baseline JIT to be used if true") \
    v(Bool, useDFGJIT, true, Normal, "allows the DFG JIT to be used if true") \
    v(Bool, useRegExpJIT, jitEnabledByDefault(), Normal, "allows the RegExp JIT to be used if true") \
    v(Bool, useDOMJIT, is64Bit(), Normal, "allows the DOMJIT to be used if true") \
    \
    v(Bool, reportMustSucceedExecutableAllocations, false, Normal, nullptr) \
    \
    v(Unsigned, maxPerThreadStackUsage, 5 * MB, Normal, "Max allowed stack usage by the VM") \
    v(Unsigned, softReservedZoneSize, 128 * KB, Normal, "A buffer greater than reservedZoneSize that reserves space for stringifying exceptions.") \
    v(Unsigned, reservedZoneSize, 64 * KB, Normal, "The amount of stack space we guarantee to our clients (and to interal VM code that does not call out to clients).") \
    \
    v(Bool, crashIfCantAllocateJITMemory, false, Normal, nullptr) \
    v(Unsigned, jitMemoryReservationSize, 0, Normal, "Set this number to change the executable allocation size in ExecutableAllocatorFixedVMPool. (In bytes.)") \
    v(Bool, useSeparatedWXHeap, false, Normal, nullptr) \
    \
    v(Bool, forceCodeBlockLiveness, false, Normal, nullptr) \
    v(Bool, forceICFailure, false, Normal, nullptr) \
    \
    v(Unsigned, repatchCountForCoolDown, 8, Normal, nullptr) \
    v(Unsigned, initialCoolDownCount, 20, Normal, nullptr) \
    v(Unsigned, repatchBufferingCountdown, 8, Normal, nullptr) \
    \
    v(Bool, dumpGeneratedBytecodes, false, Normal, nullptr) \
    v(Bool, dumpGeneratedWasmBytecodes, false, Normal, nullptr) \
    v(Bool, dumpBytecodeLivenessResults, false, Normal, nullptr) \
    v(Bool, validateBytecode, false, Normal, nullptr) \
    v(Bool, forceDebuggerBytecodeGeneration, false, Normal, nullptr) \
    v(Bool, dumpBytecodesBeforeGeneratorification, false, Normal, nullptr) \
    \
    v(Bool, useFunctionDotArguments, true, Normal, nullptr) \
    v(Bool, useTailCalls, true, Normal, nullptr) \
    v(Bool, optimizeRecursiveTailCalls, true, Normal, nullptr) \
    v(Bool, alwaysUseShadowChicken, false, Normal, nullptr) \
    v(Unsigned, shadowChickenLogSize, 1000, Normal, nullptr) \
    v(Unsigned, shadowChickenMaxTailDeletedFramesSize, 128, Normal, nullptr) \
    \
    v(Bool, useIterationIntrinsics, true, Normal, nullptr) \
    \
    v(Bool, useOSLog, false, Normal, "Log dataLog()s to os_log instead of stderr") \
    /* dumpDisassembly implies dumpDFGDisassembly. */ \
    v(Bool, dumpDisassembly, false, Normal, "dumps disassembly of all JIT compiled code upon compilation") \
    v(Bool, asyncDisassembly, false, Normal, nullptr) \
    v(Bool, dumpDFGDisassembly, false, Normal, "dumps disassembly of DFG function upon compilation") \
    v(Bool, dumpFTLDisassembly, false, Normal, "dumps disassembly of FTL function upon compilation") \
    v(Bool, dumpRegExpDisassembly, false, Normal, "dumps disassembly of RegExp upon compilation") \
    v(Bool, dumpWasmDisassembly, false, Normal, "dumps disassembly of all Wasm code upon compilation") \
    v(Bool, dumpBBQDisassembly, false, Normal, "dumps disassembly of BBQ Wasm code upon compilation") \
    v(Bool, dumpOMGDisassembly, false, Normal, "dumps disassembly of OMG Wasm code upon compilation") \
    v(Bool, logJITCodeForPerf, false, Configurable, nullptr) \
    v(OptionRange, bytecodeRangeToJITCompile, 0, Normal, "bytecode size range to allow compilation on, e.g. 1:100") \
    v(OptionRange, bytecodeRangeToDFGCompile, 0, Normal, "bytecode size range to allow DFG compilation on, e.g. 1:100") \
    v(OptionRange, bytecodeRangeToFTLCompile, 0, Normal, "bytecode size range to allow FTL compilation on, e.g. 1:100") \
    v(OptionString, jitWhitelist, nullptr, Normal, "file with list of function signatures to allow compilation on") \
    v(OptionString, dfgWhitelist, nullptr, Normal, "file with list of function signatures to allow DFG compilation on") \
    v(OptionString, ftlWhitelist, nullptr, Normal, "file with list of function signatures to allow FTL compilation on") \
    v(Bool, dumpSourceAtDFGTime, false, Normal, "dumps source code of JS function being DFG compiled") \
    v(Bool, dumpBytecodeAtDFGTime, false, Normal, "dumps bytecode of JS function being DFG compiled") \
    v(Bool, dumpGraphAfterParsing, false, Normal, nullptr) \
    v(Bool, dumpGraphAtEachPhase, false, Normal, nullptr) \
    v(Bool, dumpDFGGraphAtEachPhase, false, Normal, "dumps the DFG graph at each phase of DFG compilation (note this excludes DFG graphs during FTL compilation)") \
    v(Bool, dumpDFGFTLGraphAtEachPhase, false, Normal, "dumps the DFG graph at each phase of DFG compilation when compiling FTL code") \
    v(Bool, dumpB3GraphAtEachPhase, false, Normal, "dumps the B3 graph at each phase of compilation") \
    v(Bool, dumpAirGraphAtEachPhase, false, Normal, "dumps the Air graph at each phase of compilation") \
    v(Bool, verboseDFGBytecodeParsing, false, Normal, nullptr) \
    v(Bool, safepointBeforeEachPhase, true, Normal, nullptr) \
    v(Bool, verboseCompilation, false, Normal, nullptr) \
    v(Bool, verboseFTLCompilation, false, Normal, nullptr) \
    v(Bool, logCompilationChanges, false, Normal, nullptr) \
    v(Bool, useProbeOSRExit, false, Normal, nullptr) \
    v(Bool, printEachOSRExit, false, Normal, nullptr) \
    v(Bool, validateGraph, false, Normal, nullptr) \
    v(Bool, validateGraphAtEachPhase, false, Normal, nullptr) \
    v(Bool, verboseValidationFailure, false, Normal, nullptr) \
    v(Bool, verboseOSR, false, Normal, nullptr) \
    v(Bool, verboseDFGOSRExit, false, Normal, nullptr) \
    v(Bool, verboseFTLOSRExit, false, Normal, nullptr) \
    v(Bool, verboseCallLink, false, Normal, nullptr) \
    v(Bool, verboseCompilationQueue, false, Normal, nullptr) \
    v(Bool, reportCompileTimes, false, Normal, "dumps JS function signature and the time it took to compile in all tiers") \
    v(Bool, reportBaselineCompileTimes, false, Normal, "dumps JS function signature and the time it took to BaselineJIT compile") \
    v(Bool, reportDFGCompileTimes, false, Normal, "dumps JS function signature and the time it took to DFG and FTL compile") \
    v(Bool, reportFTLCompileTimes, false, Normal, "dumps JS function signature and the time it took to FTL compile") \
    v(Bool, reportTotalCompileTimes, false, Normal, nullptr) \
    v(Bool, reportTotalPhaseTimes, false, Normal, "This prints phase times at the end of running script inside jsc.cpp") \
    v(Bool, reportParseTimes, false, Normal, "dumps JS function signature and the time it took to parse") \
    v(Bool, reportBytecodeCompileTimes, false, Normal, "dumps JS function signature and the time it took to bytecode compile") \
    v(Bool, countParseTimes, false, Normal, "counts parse times") \
    v(Bool, verboseExitProfile, false, Normal, nullptr) \
    v(Bool, verboseCFA, false, Normal, nullptr) \
    v(Bool, verboseDFGFailure, false, Normal, nullptr) \
    v(Bool, verboseFTLToJSThunk, false, Normal, nullptr) \
    v(Bool, verboseFTLFailure, false, Normal, nullptr) \
    v(Bool, alwaysComputeHash, false, Normal, nullptr) \
    v(Bool, testTheFTL, false, Normal, nullptr) \
    v(Bool, verboseSanitizeStack, false, Normal, nullptr) \
    v(Bool, useGenerationalGC, true, Normal, nullptr) \
    v(Bool, useConcurrentGC, true, Normal, nullptr) \
    v(Bool, collectContinuously, false, Normal, nullptr) \
    v(Double, collectContinuouslyPeriodMS, 1, Normal, nullptr) \
    v(Bool, forceFencedBarrier, false, Normal, nullptr) \
    v(Bool, verboseVisitRace, false, Normal, nullptr) \
    v(Bool, optimizeParallelSlotVisitorsForStoppedMutator, false, Normal, nullptr) \
    v(Unsigned, largeHeapSize, 32 * 1024 * 1024, Normal, nullptr) \
    v(Unsigned, smallHeapSize, 1 * 1024 * 1024, Normal, nullptr) \
    v(Double, smallHeapRAMFraction, 0.25, Normal, nullptr) \
    v(Double, smallHeapGrowthFactor, 2, Normal, nullptr) \
    v(Double, mediumHeapRAMFraction, 0.5, Normal, nullptr) \
    v(Double, mediumHeapGrowthFactor, 1.5, Normal, nullptr) \
    v(Double, largeHeapGrowthFactor, 1.24, Normal, nullptr) \
    v(Double, miniVMHeapGrowthFactor, 1.27, Normal, nullptr) \
    v(Double, criticalGCMemoryThreshold, 0.80, Normal, "percent memory in use the GC considers critical.  The collector is much more aggressive above this threshold") \
    v(Double, minimumMutatorUtilization, 0, Normal, nullptr) \
    v(Double, maximumMutatorUtilization, 0.7, Normal, nullptr) \
    v(Double, epsilonMutatorUtilization, 0.01, Normal, nullptr) \
    v(Double, concurrentGCMaxHeadroom, 1.5, Normal, nullptr) \
    v(Double, concurrentGCPeriodMS, 2, Normal, nullptr) \
    v(Bool, useStochasticMutatorScheduler, true, Normal, nullptr) \
    v(Double, minimumGCPauseMS, 0.3, Normal, nullptr) \
    v(Double, gcPauseScale, 0.3, Normal, nullptr) \
    v(Double, gcIncrementBytes, 10000, Normal, nullptr) \
    v(Double, gcIncrementMaxBytes, 100000, Normal, nullptr) \
    v(Double, gcIncrementScale, 0, Normal, nullptr) \
    v(Bool, scribbleFreeCells, false, Normal, nullptr) \
    v(Double, sizeClassProgression, 1.4, Normal, nullptr) \
    v(Unsigned, preciseAllocationCutoff, 100000, Normal, nullptr) \
    v(Bool, dumpSizeClasses, false, Normal, nullptr) \
    v(Bool, useBumpAllocator, true, Normal, nullptr) \
    v(Bool, stealEmptyBlocksFromOtherAllocators, true, Normal, nullptr) \
    v(Bool, eagerlyUpdateTopCallFrame, false, Normal, nullptr) \
    v(Bool, dumpZappedCellCrashData, false, Normal, nullptr) \
    \
    v(Bool, useOSREntryToDFG, true, Normal, nullptr) \
    v(Bool, useOSREntryToFTL, true, Normal, nullptr) \
    \
    v(Bool, useFTLJIT, true, Normal, "allows the FTL JIT to be used if true") \
    v(Bool, validateFTLOSRExitLiveness, false, Normal, nullptr) \
    v(Unsigned, defaultB3OptLevel, 2, Normal, nullptr) \
    v(Bool, b3AlwaysFailsBeforeCompile, false, Normal, nullptr) \
    v(Bool, b3AlwaysFailsBeforeLink, false, Normal, nullptr) \
    v(Bool, ftlCrashes, false, Normal, nullptr) /* fool-proof way of checking that you ended up in the FTL. ;-) */\
    v(Bool, clobberAllRegsInFTLICSlowPath, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, enableJITDebugAssertions, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, useAccessInlining, true, Normal, nullptr) \
    v(Unsigned, maxAccessVariantListSize, 8, Normal, nullptr) \
    v(Bool, usePolyvariantDevirtualization, true, Normal, nullptr) \
    v(Bool, usePolymorphicAccessInlining, true, Normal, nullptr) \
    v(Unsigned, maxPolymorphicAccessInliningListSize, 8, Normal, nullptr) \
    v(Bool, usePolymorphicCallInlining, true, Normal, nullptr) \
    v(Bool, usePolymorphicCallInliningForNonStubStatus, false, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSize, 15, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSizeForTopTier, 5, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSizeForWebAssemblyToJS, 5, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantsForInlining, 5, Normal, nullptr) \
    v(Unsigned, frequentCallThreshold, 2, Normal, nullptr) \
    v(Double, minimumCallToKnownRate, 0.51, Normal, nullptr) \
    v(Bool, createPreHeaders, true, Normal, nullptr) \
    v(Bool, useMovHintRemoval, true, Normal, nullptr) \
    v(Bool, usePutStackSinking, true, Normal, nullptr) \
    v(Bool, useObjectAllocationSinking, true, Normal, nullptr) \
    v(Bool, useValueRepElimination, true, Normal, nullptr) \
    v(Bool, useArityFixupInlining, true, Normal, nullptr) \
    v(Bool, logExecutableAllocation, false, Normal, nullptr) \
    v(Unsigned, maxDFGNodesInBasicBlockForPreciseAnalysis, 20000, Normal, "Disable precise but costly analysis and give conservative results if the number of DFG nodes in a block exceeds this threshold") \
    \
    v(Bool, useConcurrentJIT, true, Normal, "allows the DFG / FTL compilation in threads other than the executing JS thread") \
    v(Unsigned, numberOfDFGCompilerThreads, computeNumberOfWorkerThreads(3, 2) - 1, Normal, nullptr) \
    v(Unsigned, numberOfFTLCompilerThreads, computeNumberOfWorkerThreads(MAXIMUM_NUMBER_OF_FTL_COMPILER_THREADS, 2) - 1, Normal, nullptr) \
    v(Int32, priorityDeltaOfDFGCompilerThreads, computePriorityDeltaOfWorkerThreads(-1, 0), Normal, nullptr) \
    v(Int32, priorityDeltaOfFTLCompilerThreads, computePriorityDeltaOfWorkerThreads(-2, 0), Normal, nullptr) \
    v(Int32, priorityDeltaOfWasmCompilerThreads, computePriorityDeltaOfWorkerThreads(-1, 0), Normal, nullptr) \
    \
    v(Bool, useProfiler, false, Normal, nullptr) \
    v(Bool, disassembleBaselineForProfiler, true, Normal, nullptr) \
    \
    v(Bool, useArchitectureSpecificOptimizations, true, Normal, nullptr) \
    \
    v(Bool, breakOnThrow, false, Normal, nullptr) \
    \
    v(Unsigned, maximumOptimizationCandidateBytecodeCost, 100000, Normal, nullptr) \
    \
    v(Unsigned, maximumFunctionForCallInlineCandidateBytecodeCost, 120, Normal, nullptr) \
    v(Unsigned, maximumFunctionForClosureCallInlineCandidateBytecodeCost, 100, Normal, nullptr) \
    v(Unsigned, maximumFunctionForConstructInlineCandidateBytecoodeCost, 100, Normal, nullptr) \
    \
    v(Unsigned, maximumFTLCandidateBytecodeCost, 20000, Normal, nullptr) \
    \
    /* Depth of inline stack, so 1 = no inlining, 2 = one level, etc. */ \
    v(Unsigned, maximumInliningDepth, 5, Normal, "maximum allowed inlining depth.  Depth of 1 means no inlining") \
    v(Unsigned, maximumInliningRecursion, 2, Normal, nullptr) \
    \
    /* Maximum size of a caller for enabling inlining. This is purely to protect us */\
    /* from super long compiles that take a lot of memory. */\
    v(Unsigned, maximumInliningCallerBytecodeCost, 10000, Normal, nullptr) \
    \
    v(Unsigned, maximumVarargsForInlining, 100, Normal, nullptr) \
    \
    v(Unsigned, maximumBinaryStringSwitchCaseLength, 50, Normal, nullptr) \
    v(Unsigned, maximumBinaryStringSwitchTotalLength, 2000, Normal, nullptr) \
    \
    v(Double, jitPolicyScale, 1.0, Normal, "scale JIT thresholds to this specified ratio between 0.0 (compile ASAP) and 1.0 (compile like normal).") \
    v(Bool, forceEagerCompilation, false, Normal, nullptr) \
    v(Int32, thresholdForJITAfterWarmUp, 500, Normal, nullptr) \
    v(Int32, thresholdForJITSoon, 100, Normal, nullptr) \
    \
    v(Int32, thresholdForOptimizeAfterWarmUp, 1000, Normal, nullptr) \
    v(Int32, thresholdForOptimizeAfterLongWarmUp, 1000, Normal, nullptr) \
    v(Int32, thresholdForOptimizeSoon, 1000, Normal, nullptr) \
    v(Int32, executionCounterIncrementForLoop, 1, Normal, nullptr) \
    v(Int32, executionCounterIncrementForEntry, 15, Normal, nullptr) \
    \
    v(Int32, thresholdForFTLOptimizeAfterWarmUp, 100000, Normal, nullptr) \
    v(Int32, thresholdForFTLOptimizeSoon, 1000, Normal, nullptr) \
    v(Int32, ftlTierUpCounterIncrementForLoop, 1, Normal, nullptr) \
    v(Int32, ftlTierUpCounterIncrementForReturn, 15, Normal, nullptr) \
    v(Unsigned, ftlOSREntryFailureCountForReoptimization, 15, Normal, nullptr) \
    v(Unsigned, ftlOSREntryRetryThreshold, 100, Normal, nullptr) \
    \
    v(Int32, evalThresholdMultiplier, 10, Normal, nullptr) \
    v(Unsigned, maximumEvalCacheableSourceLength, 256, Normal, nullptr) \
    \
    v(Bool, randomizeExecutionCountsBetweenCheckpoints, false, Normal, nullptr) \
    v(Int32, maximumExecutionCountsBetweenCheckpointsForBaseline, 1000, Normal, nullptr) \
    v(Int32, maximumExecutionCountsBetweenCheckpointsForUpperTiers, 50000, Normal, nullptr) \
    \
    v(Unsigned, likelyToTakeSlowCaseMinimumCount, 20, Normal, nullptr) \
    v(Unsigned, couldTakeSlowCaseMinimumCount, 10, Normal, nullptr) \
    \
    v(Unsigned, osrExitCountForReoptimization, 100, Normal, nullptr) \
    v(Unsigned, osrExitCountForReoptimizationFromLoop, 5, Normal, nullptr) \
    \
    v(Unsigned, reoptimizationRetryCounterMax, 0, Normal, nullptr)  \
    \
    v(Unsigned, minimumOptimizationDelay, 1, Normal, nullptr) \
    v(Unsigned, maximumOptimizationDelay, 5, Normal, nullptr) \
    v(Double, desiredProfileLivenessRate, 0.75, Normal, nullptr) \
    v(Double, desiredProfileFullnessRate, 0.35, Normal, nullptr) \
    \
    v(Double, doubleVoteRatioForDoubleFormat, 2, Normal, nullptr) \
    v(Double, structureCheckVoteRatioForHoisting, 1, Normal, nullptr) \
    v(Double, checkArrayVoteRatioForHoisting, 1, Normal, nullptr) \
    \
    v(Unsigned, maximumDirectCallStackSize, 200, Normal, nullptr) \
    \
    v(Unsigned, minimumNumberOfScansBetweenRebalance, 100, Normal, nullptr) \
    v(Unsigned, numberOfGCMarkers, computeNumberOfGCMarkers(8), Normal, nullptr) \
    v(Bool, useParallelMarkingConstraintSolver, true, Normal, nullptr) \
    v(Unsigned, opaqueRootMergeThreshold, 1000, Normal, nullptr) \
    v(Double, minHeapUtilization, 0.8, Normal, nullptr) \
    v(Double, minMarkedBlockUtilization, 0.9, Normal, nullptr) \
    v(Unsigned, slowPathAllocsBetweenGCs, 0, Normal, "force a GC on every Nth slow path alloc, where N is specified by this option") \
    \
    v(Double, percentCPUPerMBForFullTimer, 0.0003125, Normal, nullptr) \
    v(Double, percentCPUPerMBForEdenTimer, 0.0025, Normal, nullptr) \
    v(Double, collectionTimerMaxPercentCPU, 0.05, Normal, nullptr) \
    \
    v(Bool, forceWeakRandomSeed, false, Normal, nullptr) \
    v(Unsigned, forcedWeakRandomSeed, 0, Normal, nullptr) \
    \
    v(Bool, useZombieMode, false, Normal, "debugging option to scribble over dead objects with 0xbadbeef0") \
    v(Bool, useImmortalObjects, false, Normal, "debugging option to keep all objects alive forever") \
    v(Bool, sweepSynchronously, false, Normal, "debugging option to sweep all dead objects synchronously at GC end before resuming mutator") \
    v(Unsigned, maxSingleAllocationSize, 0, Configurable, "debugging option to limit individual allocations to a max size (0 = limit not set, N = limit size in bytes)") \
    \
    v(GCLogLevel, logGC, GCLogging::None, Normal, "debugging option to log GC activity (0 = None, 1 = Basic, 2 = Verbose)") \
    v(Bool, useGC, true, Normal, nullptr) \
    v(Bool, gcAtEnd, false, Normal, "If true, the jsc CLI will do a GC before exiting") \
    v(Bool, forceGCSlowPaths, false, Normal, "If true, we will force all JIT fast allocations down their slow paths.") \
    v(Bool, forceDidDeferGCWork, false, Normal, "If true, we will force all DeferGC destructions to perform a GC.") \
    v(Unsigned, gcMaxHeapSize, 0, Normal, nullptr) \
    v(Unsigned, forceRAMSize, 0, Normal, nullptr) \
    v(Bool, recordGCPauseTimes, false, Normal, nullptr) \
    v(Bool, dumpHeapStatisticsAtVMDestruction, false, Normal, nullptr) \
    v(Bool, forceCodeBlockToJettisonDueToOldAge, false, Normal, "If true, this means that anytime we can jettison a CodeBlock due to old age, we do.") \
    v(Bool, useEagerCodeBlockJettisonTiming, false, Normal, "If true, the time slices for jettisoning a CodeBlock due to old age are shrunk significantly.") \
    \
    v(Bool, useTypeProfiler, false, Normal, nullptr) \
    v(Bool, useControlFlowProfiler, false, Normal, nullptr) \
    \
    v(Bool, useSamplingProfiler, false, Normal, nullptr) \
    v(Unsigned, sampleInterval, 1000, Normal, "Time between stack traces in microseconds.") \
    v(Bool, collectSamplingProfilerDataForJSCShell, false, Normal, "This corresponds to the JSC shell's --sample option.") \
    v(Unsigned, samplingProfilerTopFunctionsCount, 12, Normal, "Number of top functions to report when using the command line interface.") \
    v(Unsigned, samplingProfilerTopBytecodesCount, 40, Normal, "Number of top bytecodes to report when using the command line interface.") \
    v(OptionString, samplingProfilerPath, nullptr, Normal, "The path to the directory to write sampiling profiler output to. This probably will not work with WK2 unless the path is in the whitelist.") \
    v(Bool, sampleCCode, false, Normal, "Causes the sampling profiler to record profiling data for C frames.") \
    \
    v(Bool, alwaysGeneratePCToCodeOriginMap, false, Normal, "This will make sure we always generate a PCToCodeOriginMap for JITed code.") \
    \
    v(Double, randomIntegrityAuditRate, 0.05, Normal, "Probability of random integrity audits [0.0 - 1.0]") \
    v(Bool, verifyHeap, false, Normal, nullptr) \
    v(Unsigned, numberOfGCCyclesToRecordForVerification, 3, Normal, nullptr) \
    \
    v(Unsigned, exceptionStackTraceLimit, 100, Normal, "Stack trace limit for internal Exception object") \
    v(Unsigned, defaultErrorStackTraceLimit, 100, Normal, "The default value for Error.stackTraceLimit") \
    v(Bool, useExceptionFuzz, false, Normal, nullptr) \
    v(Unsigned, fireExceptionFuzzAt, 0, Normal, nullptr) \
    v(Bool, validateDFGExceptionHandling, false, Normal, "Causes the DFG to emit code validating exception handling for each node that can exit") /* This is true by default on Debug builds */\
    v(Bool, dumpSimulatedThrows, false, Normal, "Dumps the call stack of the last simulated throw if exception scope verification fails") \
    v(Bool, validateExceptionChecks, false, Normal, "Verifies that needed exception checks are performed.") \
    v(Unsigned, unexpectedExceptionStackTraceLimit, 100, Normal, "Stack trace limit for debugging unexpected exceptions observed in the VM") \
    \
    v(Bool, validateDFGClobberize, false, Normal, "Emits extra validation code in the DFG/FTL for the Clobberize phase")\
    \
    v(Bool, useExecutableAllocationFuzz, false, Normal, nullptr) \
    v(Unsigned, fireExecutableAllocationFuzzAt, 0, Normal, nullptr) \
    v(Unsigned, fireExecutableAllocationFuzzAtOrAfter, 0, Normal, nullptr) \
    v(Bool, verboseExecutableAllocationFuzz, false, Normal, nullptr) \
    \
    v(Bool, useOSRExitFuzz, false, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAtStatic, 0, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAt, 0, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAtOrAfter, 0, Normal, nullptr) \
    \
    v(Unsigned, seedOfVMRandomForFuzzer, 0, Normal, "0 means not fuzzing this; use a cryptographically random seed") \
    v(Bool, useRandomizingFuzzerAgent, false, Normal, nullptr) \
    v(Unsigned, seedOfRandomizingFuzzerAgent, 1, Normal, nullptr) \
    v(Bool, dumpFuzzerAgentPredictions, false, Normal, nullptr) \
    v(Bool, useDoublePredictionFuzzerAgent, false, Normal, nullptr) \
    v(Bool, useFileBasedFuzzerAgent, false, Normal, nullptr) \
    v(Bool, usePredictionFileCreatingFuzzerAgent, false, Normal, nullptr) \
    v(Bool, requirePredictionForFileBasedFuzzerAgent, false, Normal, nullptr) \
    v(OptionString, fuzzerPredictionsFile, nullptr, Normal, "file with list of predictions for FileBasedFuzzerAgent") \
    v(Bool, useNarrowingNumberPredictionFuzzerAgent, false, Normal, nullptr) \
    v(Bool, useWideningNumberPredictionFuzzerAgent, false, Normal, nullptr) \
    \
    v(Bool, logPhaseTimes, false, Normal, nullptr) \
    v(Double, rareBlockPenalty, 0.001, Normal, nullptr) \
    v(Bool, airLinearScanVerbose, false, Normal, nullptr) \
    v(Bool, airLinearScanSpillsEverything, false, Normal, nullptr) \
    v(Bool, airForceBriggsAllocator, false, Normal, nullptr) \
    v(Bool, airForceIRCAllocator, false, Normal, nullptr) \
    v(Bool, airRandomizeRegs, false, Normal, nullptr) \
    v(Unsigned, airRandomizeRegsSeed, 0, Normal, nullptr) \
    v(Bool, coalesceSpillSlots, true, Normal, nullptr) \
    v(Bool, logAirRegisterPressure, false, Normal, nullptr) \
    v(Bool, useB3TailDup, true, Normal, nullptr) \
    v(Unsigned, maxB3TailDupBlockSize, 3, Normal, nullptr) \
    v(Unsigned, maxB3TailDupBlockSuccessors, 3, Normal, nullptr) \
    \
    v(Bool, useDollarVM, false, Restricted, "installs the $vm debugging tool in global objects") \
    v(OptionString, functionOverrides, nullptr, Restricted, "file with debugging overrides for function bodies") \
    v(Bool, useSigillCrashAnalyzer, false, Configurable, "logs data about SIGILL crashes") \
    \
    v(Unsigned, watchdog, 0, Normal, "watchdog timeout (0 = Disabled, N = a timeout period of N milliseconds)") \
    v(Bool, usePollingTraps, false, Normal, "use polling (instead of signalling) VM traps") \
    \
    v(Bool, useMachForExceptions, true, Normal, "Use mach exceptions rather than signals to handle faults and pass thread messages. (This does nothing on platforms without mach)") \
    \
    v(Bool, useICStats, false, Normal, nullptr) \
    \
    v(Unsigned, prototypeHitCountForLLIntCaching, 2, Normal, "Number of prototype property hits before caching a prototype in the LLInt. A count of 0 means never cache.") \
    \
    v(Bool, dumpCompiledRegExpPatterns, false, Normal, nullptr) \
    \
    v(Bool, dumpModuleRecord, false, Normal, nullptr) \
    v(Bool, dumpModuleLoadingState, false, Normal, nullptr) \
    v(Bool, exposeInternalModuleLoader, false, Normal, "expose the internal module loader object to the global space for debugging") \
    \
    v(Bool, useSuperSampler, false, Normal, nullptr) \
    \
    v(Bool, useSourceProviderCache, true, Normal, "If false, the parser will not use the source provider cache. It's good to verify everything works when this is false. Because the cache is so successful, it can mask bugs.") \
    v(Bool, useCodeCache, true, Normal, "If false, the unlinked byte code cache will not be used.") \
    \
    v(Bool, useWebAssembly, true, Normal, "Expose the WebAssembly global object.") \
    \
    v(Bool, failToCompileWebAssemblyCode, false, Normal, "If true, no Wasm::Plan will sucessfully compile a function.") \
    v(Size, webAssemblyPartialCompileLimit, 5000, Normal, "Limit on the number of bytes a Wasm::Plan::compile should attempt before checking for other work.") \
    v(Unsigned, webAssemblyBBQAirOptimizationLevel, 0, Normal, "Air Optimization level for BBQ Web Assembly module compilations.") \
    v(Unsigned, webAssemblyBBQB3OptimizationLevel, 1, Normal, "B3 Optimization level for BBQ Web Assembly module compilations.") \
    v(Unsigned, webAssemblyOMGOptimizationLevel, Options::defaultB3OptLevel(), Normal, "B3 Optimization level for OMG Web Assembly module compilations.") \
    v(Unsigned, webAssemblyBBQFallbackSize, 50000, Normal, "Limit of Wasm function size above which we fallback to BBQ compilation mode.") \
    \
    v(Bool, useBBQTierUpChecks, true, Normal, "Enables tier up checks for our BBQ code.") \
    v(Bool, useWebAssemblyOSR, true, Normal, nullptr) \
    v(Int32, thresholdForBBQOptimizeAfterWarmUp, 150, Normal, "The count before we tier up a function to BBQ.") \
    v(Int32, thresholdForBBQOptimizeSoon, 50, Normal, nullptr) \
    v(Int32, thresholdForOMGOptimizeAfterWarmUp, 50000, Normal, "The count before we tier up a function to OMG.") \
    v(Int32, thresholdForOMGOptimizeSoon, 500, Normal, nullptr) \
    v(Int32, omgTierUpCounterIncrementForLoop, 1, Normal, "The amount the tier up counter is incremented on each loop backedge.") \
    v(Int32, omgTierUpCounterIncrementForEntry, 15, Normal, "The amount the tier up counter is incremented on each function entry.") \
    /* FIXME: enable fast memories on iOS and pre-allocate them. https://bugs.webkit.org/show_bug.cgi?id=170774 */ \
    v(Bool, useWebAssemblyFastMemory, OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH) >= 48, Normal, "If true, we will try to use a 32-bit address space with a signal handler to bounds check wasm memory.") \
    v(Bool, logWebAssemblyMemory, false, Normal, nullptr) \
    v(Unsigned, webAssemblyFastMemoryRedzonePages, 128, Normal, "WebAssembly fast memories use 4GiB virtual allocations, plus a redzone (counted as multiple of 64KiB WebAssembly pages) at the end to catch reg+imm accesses which exceed 32-bit, anything beyond the redzone is explicitly bounds-checked") \
    v(Bool, crashIfWebAssemblyCantFastMemory, false, Normal, "If true, we will crash if we can't obtain fast memory for wasm.") \
    v(Bool, crashOnFailedWebAssemblyValidate, false, Normal, "If true, we will crash if we can't validate a wasm module instead of throwing an exception.") \
    v(Unsigned, maxNumWebAssemblyFastMemories, 4, Normal, nullptr) \
    v(Bool, useFastTLSForWasmContext, true, Normal, "If true, we will store context in fast TLS. If false, we will pin it to a register.") \
    v(Bool, wasmBBQUsesAir, true, Normal, nullptr) \
    v(Bool, useWasmLLInt, true, Normal, nullptr) \
    v(Bool, useBBQJIT, true, Normal, "allows the BBQ JIT to be used if true") \
    v(Bool, useOMGJIT, true, Normal, "allows the OMG JIT to be used if true") \
    v(Bool, useWasmLLIntPrologueOSR, true, Normal, "allows prologue OSR from Wasm LLInt if true") \
    v(Bool, useWasmLLIntLoopOSR, true, Normal, "allows loop OSR from Wasm LLInt if true") \
    v(Bool, useWasmLLIntEpilogueOSR, true, Normal, "allows epilogue OSR from Wasm LLInt if true") \
    v(OptionRange, wasmFunctionIndexRangeToCompile, 0, Normal, "wasm function index range to allow compilation on, e.g. 1:100") \
    v(Bool, wasmLLIntTiersUpToBBQ, true, Normal, nullptr) \
    v(Size, webAssemblyBBQAirModeThreshold, isIOS() ? (10 * MB) : 0, Normal, "If 0, we always use BBQ Air. If Wasm module code size hits this threshold, we compile Wasm module with B3 BBQ mode.") \
    v(Bool, useWebAssemblyStreamingApi, enableWebAssemblyStreamingApi, Normal, "Allow to run WebAssembly's Streaming API") \
    v(Bool, useEagerWebAssemblyModuleHashing, false, Normal, "Unnamed WebAssembly modules are identified in backtraces through their hash, if available.") \
    v(Bool, useWebAssemblyReferences, false, Normal, "Allow types from the wasm references spec.") \
    v(Bool, useWebAssemblyMultiValues, true, Normal, "Allow types from the wasm mulit-values spec.") \
    v(Bool, useWeakRefs, false, Normal, "Expose the WeakRef constructor.") \
    v(Bool, useBigInt, true, Normal, "If true, we will enable BigInt support.") \
    v(Bool, useIntlLocale, false, Normal, "Expose the Intl.Locale constructor.") \
    v(Bool, useIntlRelativeTimeFormat, false, Normal, "Expose the Intl.RelativeTimeFormat constructor.") \
    v(Bool, useArrayAllocationProfiling, true, Normal, "If true, we will use our normal array allocation profiling. If false, the allocation profile will always claim to be undecided.") \
    v(Bool, forcePolyProto, false, Normal, "If true, create_this will always create an object with a poly proto structure.") \
    v(Bool, forceMiniVMMode, false, Normal, "If true, it will force mini VM mode on.") \
    v(Bool, useTracePoints, false, Normal, nullptr) \
    v(Bool, traceLLIntExecution, false, Configurable, nullptr) \
    v(Bool, traceLLIntSlowPath, false, Configurable, nullptr) \
    v(Bool, traceBaselineJITExecution, false, Normal, nullptr) \
    v(Unsigned, thresholdForGlobalLexicalBindingEpoch, UINT_MAX, Normal, "Threshold for global lexical binding epoch. If the epoch reaches to this value, CodeBlock metadata for scope operations will be revised globally. It needs to be greater than 1.") \
    v(OptionString, diskCachePath, nullptr, Restricted, nullptr) \
    v(Bool, forceDiskCache, false, Restricted, nullptr) \
    v(Bool, validateAbstractInterpreterState, false, Restricted, nullptr) \
    v(Double, validateAbstractInterpreterStateProbability, 0.5, Normal, nullptr) \
    v(OptionString, dumpJITMemoryPath, nullptr, Restricted, nullptr) \
    v(Double, dumpJITMemoryFlushInterval, 10, Restricted, "Maximum time in between flushes of the JIT memory dump in seconds.") \
    v(Bool, useUnlinkedCodeBlockJettisoning, false, Normal, "If true, UnlinkedCodeBlock can be jettisoned.") \
    v(Bool, forceOSRExitToLLInt, false, Normal, "If true, we always exit to the LLInt. If false, we exit to whatever is most convenient.") \
    v(Unsigned, getByValICMaxNumberOfIdentifiers, 4, Normal, "Number of identifiers we see in the LLInt that could cause us to bail on generating an IC for get_by_val.") \
    v(Bool, usePublicClassFields, true, Normal, "If true, the parser will understand public data fields inside classes.") \
    v(Bool, useRandomizingExecutableIslandAllocation, false, Normal, "For the arm64 ExecutableAllocator, if true, select which region to use randomly. This is useful for testing that jump islands work.") \

enum OptionEquivalence {
    SameOption,
    InvertedOption,
};

#define FOR_EACH_JSC_ALIASED_OPTION(v) \
    v(enableFunctionDotArguments, useFunctionDotArguments, SameOption) \
    v(enableTailCalls, useTailCalls, SameOption) \
    v(showDisassembly, dumpDisassembly, SameOption) \
    v(showDFGDisassembly, dumpDFGDisassembly, SameOption) \
    v(showFTLDisassembly, dumpFTLDisassembly, SameOption) \
    v(alwaysDoFullCollection, useGenerationalGC, InvertedOption) \
    v(enableOSREntryToDFG, useOSREntryToDFG, SameOption) \
    v(enableOSREntryToFTL, useOSREntryToFTL, SameOption) \
    v(enableAccessInlining, useAccessInlining, SameOption) \
    v(enablePolyvariantDevirtualization, usePolyvariantDevirtualization, SameOption) \
    v(enablePolymorphicAccessInlining, usePolymorphicAccessInlining, SameOption) \
    v(enablePolymorphicCallInlining, usePolymorphicCallInlining, SameOption) \
    v(enableMovHintRemoval, useMovHintRemoval, SameOption) \
    v(enableObjectAllocationSinking, useObjectAllocationSinking, SameOption) \
    v(enableConcurrentJIT, useConcurrentJIT, SameOption) \
    v(enableProfiler, useProfiler, SameOption) \
    v(enableArchitectureSpecificOptimizations, useArchitectureSpecificOptimizations, SameOption) \
    v(enablePolyvariantCallInlining, usePolyvariantCallInlining, SameOption) \
    v(enablePolyvariantByIdInlining, usePolyvariantByIdInlining, SameOption) \
    v(objectsAreImmortal, useImmortalObjects, SameOption) \
    v(showObjectStatistics, dumpObjectStatistics, SameOption) \
    v(disableGC, useGC, InvertedOption) \
    v(enableTypeProfiler, useTypeProfiler, SameOption) \
    v(enableControlFlowProfiler, useControlFlowProfiler, SameOption) \
    v(enableExceptionFuzz, useExceptionFuzz, SameOption) \
    v(enableExecutableAllocationFuzz, useExecutableAllocationFuzz, SameOption) \
    v(enableOSRExitFuzz, useOSRExitFuzz, SameOption) \
    v(enableDollarVM, useDollarVM, SameOption) \
    v(enableWebAssembly, useWebAssembly, SameOption) \
    v(maximumOptimizationCandidateInstructionCount, maximumOptimizationCandidateBytecodeCost, SameOption) \
    v(maximumFunctionForCallInlineCandidateInstructionCount, maximumFunctionForCallInlineCandidateBytecodeCost, SameOption) \
    v(maximumFunctionForClosureCallInlineCandidateInstructionCount, maximumFunctionForClosureCallInlineCandidateBytecodeCost, SameOption) \
    v(maximumFunctionForConstructInlineCandidateInstructionCount, maximumFunctionForConstructInlineCandidateBytecoodeCost, SameOption) \
    v(maximumFTLCandidateInstructionCount, maximumFTLCandidateBytecodeCost, SameOption) \
    v(maximumInliningCallerSize, maximumInliningCallerBytecodeCost, SameOption) \


constexpr size_t countNumberOfJSCOptions()
{
#define COUNT_OPTION(type_, name_, defaultValue_, availability_, description_) count++;
    size_t count = 0;
    FOR_EACH_JSC_OPTION(COUNT_OPTION);
    return count;
#undef COUNT_OPTION
}

constexpr size_t NumberOfOptions = countNumberOfJSCOptions();

class OptionRange {
private:
    enum RangeState { Uninitialized, InitError, Normal, Inverted };
public:
    OptionRange& operator=(int value)
    {
        // Only used for initialization to state Uninitialized.
        // OptionsList specifies OptionRange options with default value 0.
        RELEASE_ASSERT(!value);

        m_state = Uninitialized;
        m_rangeString = nullptr;
        m_lowLimit = 0;
        m_highLimit = 0;
        return *this;
    }

    bool init(const char*);
    bool isInRange(unsigned);
    const char* rangeString() const { return (m_state > InitError) ? m_rangeString : s_nullRangeStr; }
    
    void dump(PrintStream& out) const;

private:
    static const char* const s_nullRangeStr;

    RangeState m_state;
    const char* m_rangeString;
    unsigned m_lowLimit;
    unsigned m_highLimit;
};

struct OptionsStorage {
    using Bool = bool;
    using Unsigned = unsigned;
    using Double = double;
    using Int32 = int32_t;
    using Size = size_t;
    using OptionRange = JSC::OptionRange;
    using OptionString = const char*;
    using GCLogLevel = GCLogging::Level;

#define DECLARE_OPTION(type_, name_, defaultValue_, availability_, description_) \
    type_ name_; \
    type_ name_##Default;
FOR_EACH_JSC_OPTION(DECLARE_OPTION)
#undef DECLARE_OPTION
};

// Options::Metadata's offsetOfOption and offsetOfOptionDefault relies on this.
static_assert(sizeof(OptionsStorage) <= 16 * KB);

} // namespace JSC
