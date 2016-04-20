/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
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

#ifndef Options_h
#define Options_h

#include "GCLogging.h"
#include "JSExportMacros.h"
#include <stdint.h>
#include <stdio.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>

namespace WTF {
class StringBuilder;
}
using WTF::StringBuilder;

namespace JSC {

// How do JSC VM options work?
// ===========================
// The JSC_OPTIONS() macro below defines a list of all JSC options in use,
// along with their types and default values. The options values are actually
// realized as an array of Options::Entry elements.
//
//     Options::initialize() will initialize the array of options values with
// the defaults specified in JSC_OPTIONS() below. After that, the values can
// be programmatically read and written to using an accessor method with the
// same name as the option. For example, the option "useJIT" can be read and
// set like so:
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

class OptionRange {
private:
    enum RangeState { Uninitialized, InitError, Normal, Inverted };
public:
    OptionRange& operator= (const int& rhs)
    { // Only needed for initialization
        if (!rhs) {
            m_state = Uninitialized;
            m_rangeString = 0;
            m_lowLimit = 0;
            m_highLimit = 0;
        }
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

typedef OptionRange optionRange;
typedef const char* optionString;

#define JSC_OPTIONS(v) \
    v(bool, validateOptions, false, "crashes if mis-typed JSC options were passed to the VM") \
    v(unsigned, dumpOptions, 0, "dumps JSC options (0 = None, 1 = Overridden only, 2 = All, 3 = Verbose)") \
    \
    v(bool, useLLInt,  true, "allows the LLINT to be used if true") \
    v(bool, useJIT,    true, "allows the baseline JIT to be used if true") \
    v(bool, useDFGJIT, true, "allows the DFG JIT to be used if true") \
    v(bool, useRegExpJIT, true, "allows the RegExp JIT to be used if true") \
    \
    v(bool, reportMustSucceedExecutableAllocations, false, nullptr) \
    \
    v(unsigned, maxPerThreadStackUsage, 4 * MB, nullptr) \
    v(unsigned, reservedZoneSize, 128 * KB, nullptr) \
    v(unsigned, errorModeReservedZoneSize, 64 * KB, nullptr) \
    \
    v(bool, crashIfCantAllocateJITMemory, false, nullptr) \
    v(unsigned, jitMemoryReservationSize, 0, "Set this number to change the executable allocation size in ExecutableAllocatorFixedVMPool. (In bytes.)") \
    v(bool, useSeparatedWXHeap, false, nullptr) \
    \
    v(bool, forceCodeBlockLiveness, false, nullptr) \
    v(bool, forceICFailure, false, nullptr) \
    \
    v(unsigned, repatchCountForCoolDown, 10, nullptr) \
    v(unsigned, initialCoolDownCount, 20, nullptr) \
    v(unsigned, repatchBufferingCountdown, 10, nullptr) \
    \
    v(bool, dumpGeneratedBytecodes, false, nullptr) \
    v(bool, dumpBytecodeLivenessResults, false, nullptr) \
    v(bool, validateBytecode, false, nullptr) \
    v(bool, forceDebuggerBytecodeGeneration, false, nullptr) \
    v(bool, forceProfilerBytecodeGeneration, false, nullptr) \
    \
    v(bool, useFunctionDotArguments, true, nullptr) \
    v(bool, useTailCalls, true, nullptr) \
    v(bool, alwaysUseShadowChicken, false, nullptr) \
    v(unsigned, shadowChickenLogSize, 1000, nullptr) \
    v(unsigned, shadowChickenStackSizeLimit, 100000, nullptr) \
    \
    /* dumpDisassembly implies dumpDFGDisassembly. */ \
    v(bool, dumpDisassembly, false, "dumps disassembly of all JIT compiled code upon compilation") \
    v(bool, asyncDisassembly, false, nullptr) \
    v(bool, dumpDFGDisassembly, false, "dumps disassembly of DFG function upon compilation") \
    v(bool, dumpFTLDisassembly, false, "dumps disassembly of FTL function upon compilation") \
    v(bool, dumpAllDFGNodes, false, nullptr) \
    v(optionRange, bytecodeRangeToDFGCompile, 0, "bytecode size range to allow DFG compilation on, e.g. 1:100") \
    v(optionRange, bytecodeRangeToFTLCompile, 0, "bytecode size range to allow FTL compilation on, e.g. 1:100") \
    v(optionString, dfgWhitelist, nullptr, "file with list of function signatures to allow DFG compilation on") \
    v(bool, dumpSourceAtDFGTime, false, "dumps source code of JS function being DFG compiled") \
    v(bool, dumpBytecodeAtDFGTime, false, "dumps bytecode of JS function being DFG compiled") \
    v(bool, dumpGraphAfterParsing, false, nullptr) \
    v(bool, dumpGraphAtEachPhase, false, nullptr) \
    v(bool, dumpDFGGraphAtEachPhase, false, "dumps the DFG graph at each phase DFG of complitaion (note this excludes DFG graphs during FTL compilation)") \
    v(bool, dumpDFGFTLGraphAtEachPhase, false, "dumps the DFG graph at each phase DFG of complitaion when compiling FTL code") \
    v(bool, dumpB3GraphAtEachPhase, false, "dumps the B3 graph at each phase of compilation") \
    v(bool, dumpAirGraphAtEachPhase, false, "dumps the Air graph at each phase of compilation") \
    v(bool, verboseDFGByteCodeParsing, false, nullptr) \
    v(bool, verboseCompilation, false, nullptr) \
    v(bool, verboseFTLCompilation, false, nullptr) \
    v(bool, logCompilationChanges, false, nullptr) \
    v(bool, printEachOSRExit, false, nullptr) \
    v(bool, validateGraph, false, nullptr) \
    v(bool, validateGraphAtEachPhase, false, nullptr) \
    v(bool, verboseValidationFailure, false, nullptr) \
    v(bool, verboseOSR, false, nullptr) \
    v(bool, verboseFTLOSRExit, false, nullptr) \
    v(bool, verboseCallLink, false, nullptr) \
    v(bool, verboseCompilationQueue, false, nullptr) \
    v(bool, reportCompileTimes, false, "dumps JS function signature and the time it took to compile") \
    v(bool, reportFTLCompileTimes, false, "dumps JS function signature and the time it took to FTL compile") \
    v(bool, reportTotalCompileTimes, false, nullptr) \
    v(bool, verboseCFA, false, nullptr) \
    v(bool, verboseFTLToJSThunk, false, nullptr) \
    v(bool, verboseFTLFailure, false, nullptr) \
    v(bool, alwaysComputeHash, false, nullptr) \
    v(bool, testTheFTL, false, nullptr) \
    v(bool, verboseSanitizeStack, false, nullptr) \
    v(bool, useGenerationalGC, true, nullptr) \
    v(bool, eagerlyUpdateTopCallFrame, false, nullptr) \
    \
    v(bool, useOSREntryToDFG, true, nullptr) \
    v(bool, useOSREntryToFTL, true, nullptr) \
    \
    v(bool, useFTLJIT, true, "allows the FTL JIT to be used if true") \
    v(bool, useFTLTBAA, true, nullptr) \
    v(bool, validateFTLOSRExitLiveness, false, nullptr) \
    v(bool, b3AlwaysFailsBeforeCompile, false, nullptr) \
    v(bool, b3AlwaysFailsBeforeLink, false, nullptr) \
    v(bool, ftlCrashes, false, nullptr) /* fool-proof way of checking that you ended up in the FTL. ;-) */\
    v(bool, clobberAllRegsInFTLICSlowPath, !ASSERT_DISABLED, nullptr) \
    v(bool, useAccessInlining, true, nullptr) \
    v(unsigned, maxAccessVariantListSize, 13, nullptr) \
    v(unsigned, megamorphicLoadCost, 999, nullptr) /* This used to be 10, but we're temporarily testing what happens when the feature is disabled. */\
    v(bool, usePolyvariantDevirtualization, true, nullptr) \
    v(bool, usePolymorphicAccessInlining, true, nullptr) \
    v(bool, usePolymorphicCallInlining, true, nullptr) \
    v(bool, usePolymorphicCallInliningForNonStubStatus, false, nullptr) \
    v(unsigned, maxPolymorphicCallVariantListSize, 15, nullptr) \
    v(unsigned, maxPolymorphicCallVariantListSizeForTopTier, 5, nullptr) \
    v(unsigned, maxPolymorphicCallVariantsForInlining, 5, nullptr) \
    v(unsigned, frequentCallThreshold, 2, nullptr) \
    v(double, minimumCallToKnownRate, 0.51, nullptr) \
    v(bool, createPreHeaders, true, nullptr) \
    v(bool, useMovHintRemoval, true, nullptr) \
    v(bool, usePutStackSinking, true, nullptr) \
    v(bool, useObjectAllocationSinking, true, nullptr) \
    \
    v(bool, useConcurrentJIT, true, "allows the DFG / FTL compilation in threads other than the executing JS thread") \
    v(unsigned, numberOfDFGCompilerThreads, computeNumberOfWorkerThreads(2, 2) - 1, nullptr) \
    v(unsigned, numberOfFTLCompilerThreads, computeNumberOfWorkerThreads(8, 2) - 1, nullptr) \
    v(int32, priorityDeltaOfDFGCompilerThreads, computePriorityDeltaOfWorkerThreads(-1, 0), nullptr) \
    v(int32, priorityDeltaOfFTLCompilerThreads, computePriorityDeltaOfWorkerThreads(-2, 0), nullptr) \
    \
    v(bool, useProfiler, false, nullptr) \
    v(bool, disassembleBaselineForProfiler, true, nullptr) \
    \
    v(bool, useArchitectureSpecificOptimizations, true, nullptr) \
    \
    v(bool, breakOnThrow, false, nullptr) \
    \
    v(unsigned, maximumOptimizationCandidateInstructionCount, 100000, nullptr) \
    \
    v(unsigned, maximumFunctionForCallInlineCandidateInstructionCount, 180, nullptr) \
    v(unsigned, maximumFunctionForClosureCallInlineCandidateInstructionCount, 100, nullptr) \
    v(unsigned, maximumFunctionForConstructInlineCandidateInstructionCount, 100, nullptr) \
    \
    v(unsigned, maximumFTLCandidateInstructionCount, 20000, nullptr) \
    \
    /* Depth of inline stack, so 1 = no inlining, 2 = one level, etc. */ \
    v(unsigned, maximumInliningDepth, 5, "maximum allowed inlining depth.  Depth of 1 means no inlining") \
    v(unsigned, maximumInliningRecursion, 2, nullptr) \
    \
    /* Maximum size of a caller for enabling inlining. This is purely to protect us */\
    /* from super long compiles that take a lot of memory. */\
    v(unsigned, maximumInliningCallerSize, 10000, nullptr) \
    \
    v(unsigned, maximumVarargsForInlining, 100, nullptr) \
    \
    v(bool, usePolyvariantCallInlining, true, nullptr) \
    v(bool, usePolyvariantByIdInlining, true, nullptr) \
    \
    v(bool, useMaximalFlushInsertionPhase, false, "Setting to true allows the DFG's MaximalFlushInsertionPhase to run.") \
    \
    v(unsigned, maximumBinaryStringSwitchCaseLength, 50, nullptr) \
    v(unsigned, maximumBinaryStringSwitchTotalLength, 2000, nullptr) \
    \
    v(double, jitPolicyScale, 1.0, "scale JIT thresholds to this specified ratio between 0.0 (compile ASAP) and 1.0 (compile like normal).") \
    v(bool, forceEagerCompilation, false, nullptr) \
    v(int32, thresholdForJITAfterWarmUp, 500, nullptr) \
    v(int32, thresholdForJITSoon, 100, nullptr) \
    \
    v(int32, thresholdForOptimizeAfterWarmUp, 1000, nullptr) \
    v(int32, thresholdForOptimizeAfterLongWarmUp, 1000, nullptr) \
    v(int32, thresholdForOptimizeSoon, 1000, nullptr) \
    v(int32, executionCounterIncrementForLoop, 1, nullptr) \
    v(int32, executionCounterIncrementForEntry, 15, nullptr) \
    \
    v(int32, thresholdForFTLOptimizeAfterWarmUp, 100000, nullptr) \
    v(int32, thresholdForFTLOptimizeSoon, 1000, nullptr) \
    v(int32, ftlTierUpCounterIncrementForLoop, 1, nullptr) \
    v(int32, ftlTierUpCounterIncrementForReturn, 15, nullptr) \
    v(unsigned, ftlOSREntryFailureCountForReoptimization, 15, nullptr) \
    v(unsigned, ftlOSREntryRetryThreshold, 100, nullptr) \
    \
    v(int32, evalThresholdMultiplier, 10, nullptr) \
    v(unsigned, maximumEvalCacheableSourceLength, 256, nullptr) \
    \
    v(bool, randomizeExecutionCountsBetweenCheckpoints, false, nullptr) \
    v(int32, maximumExecutionCountsBetweenCheckpointsForBaseline, 1000, nullptr) \
    v(int32, maximumExecutionCountsBetweenCheckpointsForUpperTiers, 50000, nullptr) \
    \
    v(unsigned, likelyToTakeSlowCaseMinimumCount, 20, nullptr) \
    v(unsigned, couldTakeSlowCaseMinimumCount, 10, nullptr) \
    \
    v(unsigned, osrExitCountForReoptimization, 100, nullptr) \
    v(unsigned, osrExitCountForReoptimizationFromLoop, 5, nullptr) \
    \
    v(unsigned, reoptimizationRetryCounterMax, 0, nullptr)  \
    \
    v(unsigned, minimumOptimizationDelay, 1, nullptr) \
    v(unsigned, maximumOptimizationDelay, 5, nullptr) \
    v(double, desiredProfileLivenessRate, 0.75, nullptr) \
    v(double, desiredProfileFullnessRate, 0.35, nullptr) \
    \
    v(double, doubleVoteRatioForDoubleFormat, 2, nullptr) \
    v(double, structureCheckVoteRatioForHoisting, 1, nullptr) \
    v(double, checkArrayVoteRatioForHoisting, 1, nullptr) \
    \
    v(unsigned, minimumNumberOfScansBetweenRebalance, 100, nullptr) \
    v(unsigned, numberOfGCMarkers, computeNumberOfGCMarkers(7), nullptr) \
    v(unsigned, opaqueRootMergeThreshold, 1000, nullptr) \
    v(double, minHeapUtilization, 0.8, nullptr) \
    v(double, minCopiedBlockUtilization, 0.9, nullptr) \
    v(double, minMarkedBlockUtilization, 0.9, nullptr) \
    v(unsigned, slowPathAllocsBetweenGCs, 0, "force a GC on every Nth slow path alloc, where N is specified by this option") \
    \
    v(double, percentCPUPerMBForFullTimer, 0.0003125, nullptr) \
    v(double, percentCPUPerMBForEdenTimer, 0.0025, nullptr) \
    v(double, collectionTimerMaxPercentCPU, 0.05, nullptr) \
    \
    v(bool, forceWeakRandomSeed, false, nullptr) \
    v(unsigned, forcedWeakRandomSeed, 0, nullptr) \
    \
    v(bool, useZombieMode, false, "debugging option to scribble over dead objects with 0xdeadbeef") \
    v(bool, useImmortalObjects, false, "debugging option to keep all objects alive forever") \
    v(bool, dumpObjectStatistics, false, nullptr) \
    \
    v(gcLogLevel, logGC, GCLogging::None, "debugging option to log GC activity (0 = None, 1 = Basic, 2 = Verbose)") \
    v(bool, useGC, true, nullptr) \
    v(bool, gcAtEnd, false, "If true, the jsc CLI will do a GC before exiting") \
    v(bool, forceGCSlowPaths, false, "If true, we will force all JIT fast allocations down their slow paths.")\
    v(unsigned, gcMaxHeapSize, 0, nullptr) \
    v(unsigned, forceRAMSize, 0, nullptr) \
    v(bool, recordGCPauseTimes, false, nullptr) \
    v(bool, logHeapStatisticsAtExit, false, nullptr) \
    v(bool, useTypeProfiler, false, nullptr) \
    v(bool, useControlFlowProfiler, false, nullptr) \
    v(bool, useSamplingProfiler, false, nullptr) \
    v(bool, collectSamplingProfilerDataForJSCShell, false, "This corresponds to the JSC shell's --reportSamplingProfilerData option.") \
    v(bool, alwaysGeneratePCToCodeOriginMap, false, "This will make sure we always generate a PCToCodeOriginMap for JITed code.") \
    \
    v(bool, verifyHeap, false, nullptr) \
    v(unsigned, numberOfGCCyclesToRecordForVerification, 3, nullptr) \
    \
    v(bool, useExceptionFuzz, false, nullptr) \
    v(unsigned, fireExceptionFuzzAt, 0, nullptr) \
    v(bool, validateDFGExceptionHandling, false, "Causes the DFG to emit code validating exception handling for each node that can exit") /* This is true by default on Debug builds */\
    \
    v(bool, useExecutableAllocationFuzz, false, nullptr) \
    v(unsigned, fireExecutableAllocationFuzzAt, 0, nullptr) \
    v(unsigned, fireExecutableAllocationFuzzAtOrAfter, 0, nullptr) \
    v(bool, verboseExecutableAllocationFuzz, false, nullptr) \
    \
    v(bool, useOSRExitFuzz, false, nullptr) \
    v(unsigned, fireOSRExitFuzzAtStatic, 0, nullptr) \
    v(unsigned, fireOSRExitFuzzAt, 0, nullptr) \
    v(unsigned, fireOSRExitFuzzAtOrAfter, 0, nullptr) \
    \
    v(bool, logB3PhaseTimes, false, nullptr) \
    v(double, rareBlockPenalty, 0.001, nullptr) \
    v(bool, airSpillsEverything, false, nullptr) \
    v(bool, logAirRegisterPressure, false, nullptr) \
    v(unsigned, maxB3TailDupBlockSize, 3, nullptr) \
    v(unsigned, maxB3TailDupBlockSuccessors, 3, nullptr) \
    \
    v(bool, useDollarVM, false, "installs the $vm debugging tool in global objects") \
    v(optionString, functionOverrides, nullptr, "file with debugging overrides for function bodies") \
    \
    v(unsigned, watchdog, 0, "watchdog timeout (0 = Disabled, N = a timeout period of N milliseconds)") \
    \
    v(bool, useICStats, false, nullptr) \
    \
    v(bool, dumpModuleRecord, false, nullptr) \
    v(bool, dumpModuleLoadingState, false, nullptr) \
    v(bool, exposeInternalModuleLoader, false, "expose the internal module loader object to the global space for debugging") \
    \
    v(bool, useSuperSampler, false, nullptr)

enum OptionEquivalence {
    SameOption,
    InvertedOption,
};

#define JSC_ALIASED_OPTIONS(v) \
    v(enableFunctionDotArguments, useFunctionDotArguments, SameOption) \
    v(enableTailCalls, useTailCalls, SameOption) \
    v(showDisassembly, dumpDisassembly, SameOption) \
    v(showDFGDisassembly, dumpDFGDisassembly, SameOption) \
    v(showFTLDisassembly, dumpFTLDisassembly, SameOption) \
    v(showAllDFGNodes, dumpAllDFGNodes, SameOption) \
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
    v(enableMaximalFlushInsertionPhase, useMaximalFlushInsertionPhase, SameOption) \
    v(objectsAreImmortal, useImmortalObjects, SameOption) \
    v(showObjectStatistics, dumpObjectStatistics, SameOption) \
    v(disableGC, useGC, InvertedOption) \
    v(enableTypeProfiler, useTypeProfiler, SameOption) \
    v(enableControlFlowProfiler, useControlFlowProfiler, SameOption) \
    v(enableExceptionFuzz, useExceptionFuzz, SameOption) \
    v(enableExecutableAllocationFuzz, useExecutableAllocationFuzz, SameOption) \
    v(enableOSRExitFuzz, useOSRExitFuzz, SameOption) \
    v(enableDollarVM, useDollarVM, SameOption) \

class Options {
public:
    enum class DumpLevel {
        None = 0,
        Overridden,
        All,
        Verbose
    };
    
    // This typedef is to allow us to eliminate the '_' in the field name in
    // union inside Entry. This is needed to keep the style checker happy.
    typedef int32_t int32;

    // Declare the option IDs:
    enum OptionID {
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_) \
        name_##ID,
        JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION
        numberOfOptions
    };

    enum class Type {
        boolType,
        unsignedType,
        doubleType,
        int32Type,
        optionRangeType,
        optionStringType,
        gcLogLevelType,
    };

    JS_EXPORT_PRIVATE static void initialize();

    // Parses a string of options where each option is of the format "--<optionName>=<value>"
    // and are separated by a space. The leading "--" is optional and will be ignored.
    JS_EXPORT_PRIVATE static bool setOptions(const char* optionsList);

    // Parses a single command line option in the format "<optionName>=<value>"
    // (no spaces allowed) and set the specified option if appropriate.
    JS_EXPORT_PRIVATE static bool setOption(const char* arg);

    JS_EXPORT_PRIVATE static void dumpAllOptions(FILE*, DumpLevel, const char* title = nullptr);
    JS_EXPORT_PRIVATE static void dumpAllOptionsInALine(StringBuilder&);

    JS_EXPORT_PRIVATE static void ensureOptionsAreCoherent();

    // Declare accessors for each option:
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_) \
    ALWAYS_INLINE static type_& name_() { return s_options[name_##ID].type_##Val; } \
    ALWAYS_INLINE static type_& name_##Default() { return s_defaultOptions[name_##ID].type_##Val; }

    JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

private:
    // For storing for an option value:
    union Entry {
        bool boolVal;
        unsigned unsignedVal;
        double doubleVal;
        int32 int32Val;
        OptionRange optionRangeVal;
        const char* optionStringVal;
        GCLogging::Level gcLogLevelVal;
    };

    // For storing constant meta data about each option:
    struct EntryInfo {
        const char* name;
        const char* description;
        Type type;
    };

    Options();

    enum DumpDefaultsOption {
        DontDumpDefaults,
        DumpDefaults
    };
    static void dumpOptionsIfNeeded();
    static void dumpAllOptions(StringBuilder&, DumpLevel, const char* title,
        const char* separator, const char* optionHeader, const char* optionFooter, DumpDefaultsOption);
    static void dumpOption(StringBuilder&, DumpLevel, OptionID,
        const char* optionHeader, const char* optionFooter, DumpDefaultsOption);

    static bool setOptionWithoutAlias(const char* arg);
    static bool setAliasedOption(const char* arg);
    static bool overrideAliasedOptionWithHeuristic(const char* name);

    // Declare the singleton instance of the options store:
    JS_EXPORTDATA static Entry s_options[numberOfOptions];
    static Entry s_defaultOptions[numberOfOptions];
    static const EntryInfo s_optionsInfo[numberOfOptions];

    friend class Option;
};

class Option {
public:
    Option(Options::OptionID id)
        : m_id(id)
        , m_entry(Options::s_options[m_id])
    {
    }
    
    void dump(StringBuilder&) const;

    bool operator==(const Option& other) const;
    bool operator!=(const Option& other) const { return !(*this == other); }
    
    const char* name() const;
    const char* description() const;
    Options::Type type() const;
    bool isOverridden() const;
    const Option defaultOption() const;
    
    bool& boolVal();
    unsigned& unsignedVal();
    double& doubleVal();
    int32_t& int32Val();
    OptionRange optionRangeVal();
    const char* optionStringVal();
    GCLogging::Level& gcLogLevelVal();
    
private:
    // Only used for constructing default Options.
    Option(Options::OptionID id, Options::Entry& entry)
        : m_id(id)
        , m_entry(entry)
    {
    }
    
    Options::OptionID m_id;
    Options::Entry& m_entry;
};

inline const char* Option::name() const
{
    return Options::s_optionsInfo[m_id].name;
}

inline const char* Option::description() const
{
    return Options::s_optionsInfo[m_id].description;
}

inline Options::Type Option::type() const
{
    return Options::s_optionsInfo[m_id].type;
}

inline bool Option::isOverridden() const
{
    return *this != defaultOption();
}

inline const Option Option::defaultOption() const
{
    return Option(m_id, Options::s_defaultOptions[m_id]);
}

inline bool& Option::boolVal()
{
    return m_entry.boolVal;
}

inline unsigned& Option::unsignedVal()
{
    return m_entry.unsignedVal;
}

inline double& Option::doubleVal()
{
    return m_entry.doubleVal;
}

inline int32_t& Option::int32Val()
{
    return m_entry.int32Val;
}

inline OptionRange Option::optionRangeVal()
{
    return m_entry.optionRangeVal;
}

inline const char* Option::optionStringVal()
{
    return m_entry.optionStringVal;
}

inline GCLogging::Level& Option::gcLogLevelVal()
{
    return m_entry.gcLogLevelVal;
}

} // namespace JSC

#endif // Options_h
