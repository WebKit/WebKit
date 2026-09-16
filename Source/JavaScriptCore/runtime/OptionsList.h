/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/GCLogging.h>
#include <JavaScriptCore/JSCWebPreferenceOptions.h>
#include <JavaScriptCore/JSExportMacros.h>

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

using WTF::PrintStream;

namespace JSC {

#define MAXIMUM_NUMBER_OF_FTL_COMPILER_THREADS 8

JS_EXPORT_PRIVATE bool canUseJITCage();
bool canUseWasm();
bool hasCapacityToUseLargeGigacage();

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

/* Field types (DESIGN-field-type-watchpoints.md). THREE options, and deliberately only three:

     useFieldTypeAssumptions  the feature switch -- this is the +8.38% on delta-blue, and what a rollout flips
     validateFieldTypes       the heap verifier. Dumps AND fails on a claim left false. Defaults off: it aborts,
                              and the sampling that makes it affordable is --collectContinuously-only
     logFieldTypes            every informational report. This is what located the ByVal IC holes

   Thirteen further options once gated the individual legs of the mechanism -- recording, narrowing, IC declines,
   weak clearing, and so on. They were bisect scaffolding, and they were deleted rather than landed. The
   justification offered for keeping them was "each is a mutation switch, so disabling it must fail a test", and
   that did not hold up: mutation testing works perfectly well by editing the guard back in and rebuilding, which
   is how every test in JSTests/stress/field-type-*.js that actually has teeth was proven. Meanwhile 13 booleans
   is 8192 configurations of which exactly one is ever run, each of the deleted options had type confusion as its
   failure mode, and an audit found 4 of the 13 killed no test at all. */

#define FOR_EACH_JSC_OPTION(v)                                          \
    v(Bool, useKernTCSM, defaultTCSMValue(), Normal, "Note: this needs to go before other options since they depend on this value."_s) \
    v(Bool, validateOptions, false, Normal, "crashes if mis-typed JSC options were passed to the VM"_s) \
    v(Unsigned, dumpOptions, 0, Normal, "dumps JSC options (0 = None, 1 = Overridden only, 2 = All, 3 = Verbose)"_s) \
    v(OptionString, configFile, nullptr, Normal, "file to configure JSC options and logging location"_s) \
    \
    v(Bool, useLLInt,  true, Normal, "allows the LLINT to be used if true"_s) \
    v(Bool, useJIT, jitEnabledByDefault(), Normal, "allows the executable pages to be allocated for JIT and thunks if true"_s) \
    v(Bool, useBaselineJIT, true, Normal, "allows the baseline JIT to be used if true"_s) \
    v(Bool, useDFGJIT, jitEnabledByDefault(), Normal, "allows the DFG JIT to be used if true"_s) \
    v(Bool, useRegExpJIT, jitEnabledByDefault(), Normal, "allows the RegExp JIT to be used if true"_s) \
    v(Bool, useDOMJIT, jitEnabledByDefault(), Normal, "allows the DOMJIT to be used if true"_s) \
    \
    v(Bool, reportMustSucceedExecutableAllocations, false, Normal, nullptr) \
    \
    v(Unsigned, maxPerThreadStackUsage, 5 * MB, Normal, "Max allowed stack usage by the VM"_s) \
    v(Unsigned, softReservedZoneSize, 128 * KB, Normal, "A buffer greater than reservedZoneSize that reserves space for stringifying exceptions."_s) \
    v(Unsigned, reservedZoneSize, 64 * KB, Normal, "The amount of stack space we guarantee to our clients (and to interal VM code that does not call out to clients)."_s) \
    \
    v(Bool, crashOnDisallowedVMEntry, ASSERT_ENABLED, Normal, "Forces a crash if we attempt to enter the VM when disallowed"_s) \
    v(Bool, crashIfCantAllocateJITMemory, false, Normal, nullptr) \
    v(Unsigned, structureHeapSizeInKB, 0, Normal, "Override for Structure Heap size (in KBs) if non-zero"_s) \
    v(Unsigned, jitMemoryReservationSize, 0, Normal, "Set this number to change the executable allocation size in ExecutableAllocatorFixedVMPool. (In bytes.)"_s) \
    v(Size, jitMemoryReservationAddress, 0, Restricted, "If non-zero, we will attempt to allocate JIT memory at the address provided and crash if we cannot.") \
    \
    v(Bool, forceCodeBlockLiveness, false, Normal, nullptr) \
    v(Bool, forceICFailure, false, Normal, nullptr) \
    v(Bool, forceUnlinkedDFG, false, Normal, nullptr) \
    \
    v(Unsigned, repatchCountForCoolDown, 8, Normal, nullptr) \
    v(Unsigned, initialCoolDownCount, 20, Normal, nullptr) \
    v(Unsigned, repatchBufferingCountdown, 6, Normal, nullptr) \
    v(Unsigned, initialRepatchBufferingCountdown, 6, Normal, nullptr) \
    \
    v(Bool, dumpGeneratedBytecodes, false, Normal, nullptr) \
    v(Bool, dumpBytecodeLivenessResults, false, Normal, nullptr) \
    v(Bool, validateBytecode, false, Normal, nullptr) \
    v(Bool, forceDebuggerBytecodeGeneration, false, Normal, nullptr) \
    v(Bool, debuggerTriggersBreakpointException, false, Normal, "Using the debugger statement will trigger an breakpoint exception (Useful when lldbing)"_s) \
    v(Bool, verboseWasmDebugger, false, Normal, nullptr) \
    v(Bool, enableWasmDebugger, false, Normal, nullptr) \
    v(Bool, verboseWasmTypeCleanup, false, Normal, "Log per-invocation counts from Wasm::TypeInformation::tryCleanup (scanned / live / reclaimed)."_s) \
    v(Bool, dumpBytecodesBeforeGeneratorification, false, Normal, nullptr) \
    v(Unsigned, switchJumpTableAmountThreshold, 15, Normal, nullptr) \
    \
    v(Bool, useFunctionDotArguments, true, Normal, nullptr) \
    v(Bool, useTailCalls, true, Normal, nullptr) \
    v(Bool, optimizeRecursiveTailCalls, true, Normal, nullptr) \
    v(Bool, alwaysUseShadowChicken, false, Normal, nullptr) \
    v(Unsigned, shadowChickenLogSize, 1000, Normal, nullptr) \
    v(Unsigned, shadowChickenMaxTailDeletedFramesSize, 128, Normal, nullptr) \
    \
    v(OSLogType, useOSLog, OSLogType::None, Normal, "Log dataLog()s to os_log instead of stderr"_s) \
    /* dumpDisassembly implies dumpDFGDisassembly. */ \
    v(Bool, needDisassemblySupport, false, Normal, nullptr) \
    v(Bool, dumpDisassembly, false, Normal, "dumps disassembly of all JIT compiled code upon compilation"_s) \
    v(Bool, logJIT, false, Normal, nullptr) \
    v(Bool, dumpBaselineDisassembly, false, Normal, "dumps disassembly of Baseline function upon compilation"_s) \
    v(Bool, dumpDFGDisassembly, false, Normal, "dumps disassembly of DFG function upon compilation"_s) \
    v(Bool, dumpFTLDisassembly, false, Normal, "dumps disassembly of FTL function upon compilation"_s) \
    v(Bool, dumpCSSJITDisassembly, false, Normal, "dumps disassembly of CSS Selector JIT upon compilation"_s) \
    v(Bool, dumpRegExpDisassembly, false, Normal, "dumps disassembly of RegExp upon compilation"_s) \
    v(Bool, traceRegExpJITExecution, false, Normal, "traces RegExp JIT execution at reentry points"_s) \
    v(Bool, dumpWasmDisassembly, false, Normal, "dumps disassembly of all wasm code upon compilation"_s) \
    v(OptionString, dumpWasmSourceFileName, nullptr, Normal, "log every wasm module validation, and dump source bytes to <filename>.0.wasm, <filename>.1.wasm, etc..."_s) \
    v(OptionString, wasmOMGFunctionsToDump, nullptr, Normal, "file with newline separated list of function indices to dump IR/disassembly for, if no such file exists, the function index itself"_s) \
    v(Bool, dumpBBQDisassembly, false, Normal, "dumps disassembly of BBQ wasm code upon compilation"_s) \
    v(Bool, dumpOMGDisassembly, false, Normal, "dumps disassembly of OMG wasm code upon compilation"_s) \
    v(Bool, useJITDump, false, Normal, "generates JITDump side-data") \
    v(Bool, useGdbJITInfo, false, Normal, "generates GDB JIT API side-data; to use with lldb on macos, add `settings set plugin.jit-loader.gdb.enable on` to .lldbinit") \
    v(Bool, useTextMarkers, false, Normal, "generates text markers side-data") \
    v(OptionString, jitDumpDirectory, nullptr, Normal, "Directory to place JITDump"_s) \
    v(Bool, useIRDump, false, Normal, "generates IR dump files and JIT_CODE_DEBUG_INFO in JITDump"_s) \
    v(OptionString, irDumpDirectory, nullptr, Normal, "Directory to place IR dump files"_s) \
    v(Bool, useSourceCodeDump, false, Normal, "generates source code debug info in JITDump"_s) \
    v(OptionString, sourceCodeDumpDirectory, nullptr, Normal, "Directory to place dumped source files"_s) \
    v(OptionString, textMarkersDirectory, nullptr, Normal, "Directory to place MarkerTxt") \
    v(OptionRange, bytecodeRangeToJITCompile, nullptr, Normal, "bytecode size range to allow compilation on, e.g. 1:100"_s) \
    v(OptionRange, bytecodeRangeToDFGCompile, nullptr, Normal, "bytecode size range to allow DFG compilation on, e.g. 1:100"_s) \
    v(OptionRange, bytecodeRangeToFTLCompile, nullptr, Normal, "bytecode size range to allow FTL compilation on, e.g. 1:100"_s) \
    v(OptionString, jitAllowlist, nullptr, Normal, "file with newline separated list of function signatures to allow compilation on or, if no such file exists, the function signature to allow"_s) \
    v(OptionString, dfgAllowlist, nullptr, Normal, "file with newline separated list of function signatures to allow DFG compilation on or, if no such file exists, the function signature to allow"_s) \
    v(OptionString, ftlAllowlist, nullptr, Normal, "file with newline separated list of function signatures to allow FTL compilation on or, if no such file exists, the function signature to allow"_s) \
    v(OptionString, bbqAllowlist, nullptr, Normal, "file with newline separated list of function indices to allow BBQ compilation on or, if no such file exists, the function index to allow"_s) \
    v(OptionString, omgAllowlist, nullptr, Normal, "file with newline separated list of function indices to allow OMG compilation on or, if no such file exists, the function index to allow"_s) \
    v(OptionString, loopUnrollingAllowlist, nullptr, Normal, "file with newline separated list of function signatures to allow loop unrolling on or, if no such file exists, the function signature to allow"_s) \
    v(OptionString, dumpGraphAllowlist, nullptr, Normal, "file with newline separated list of function signatures to filter graph dumps without restricting JIT compilation, or if no such file exists, the function signature to allow (affects dumpGraphAtEachPhase, dumpDFGGraphAtEachPhase, and dumpDFGFTLGraphAtEachPhase)"_s) \
    v(Bool, dumpSourceAtDFGTime, false, Normal, "dumps source code of JS function being DFG compiled"_s) \
    v(Bool, dumpBytecodeAtDFGTime, false, Normal, "dumps bytecode of JS function being DFG compiled"_s) \
    v(Bool, dumpGraphAfterParsing, false, Normal, nullptr) \
    v(Bool, dumpGraphAtEachPhase, false, Normal, nullptr) \
    v(Bool, dumpDFGGraphAtEachPhase, false, Normal, "dumps the DFG graph at each phase of DFG compilation (note this excludes DFG graphs during FTL compilation)"_s) \
    v(Bool, dumpDFGFTLGraphAtEachPhase, false, Normal, "dumps the DFG graph at each phase of DFG compilation when compiling FTL code"_s) \
    v(Bool, dumpB3GraphAtEachPhase, false, Normal, "dumps the B3 graph at each phase of compilation"_s) \
    v(Bool, dumpAirGraphAtEachPhase, false, Normal, "dumps the Air graph at each phase of compilation"_s) \
    v(Bool, verboseDFGBytecodeParsing, false, Normal, nullptr) \
    v(Bool, safepointBeforeEachPhase, true, Normal, nullptr) \
    v(Bool, verboseCompilation, false, Normal, nullptr) \
    v(Bool, verboseFTLCompilation, false, Normal, nullptr) \
    v(Bool, logCompilationChanges, false, Normal, nullptr) \
    v(Bool, printEachOSRExit, false, Normal, nullptr) \
    v(Bool, printEachDFGFTLInlineCall, false, Normal, nullptr) \
    v(Bool, useJITAsserts, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, validateDoesGC, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, validateGraph, false, Normal, nullptr) \
    v(Bool, validateGraphAtEachPhase, false, Normal, nullptr) \
    v(Bool, verboseValidationFailure, false, Normal, nullptr) \
    v(Bool, verboseOSR, false, Normal, nullptr) \
    v(Bool, verboseDFGOSRExit, false, Normal, nullptr) \
    v(Bool, verboseFTLOSRExit, false, Normal, nullptr) \
    v(Bool, verboseCallLink, false, Normal, nullptr) \
    v(Bool, verboseCompilationQueue, false, Normal, nullptr) \
    v(Bool, reportCompileTimes, false, Normal, "dumps JS function signature and the time it took to compile in all tiers"_s) \
    v(Bool, reportBaselineCompileTimes, false, Normal, "dumps JS function signature and the time it took to BaselineJIT compile"_s) \
    v(Bool, reportDFGCompileTimes, false, Normal, "dumps JS function signature and the time it took to DFG and FTL compile"_s) \
    v(Bool, reportFTLCompileTimes, false, Normal, "dumps JS function signature and the time it took to FTL compile"_s) \
    v(Bool, reportTotalCompileTimes, false, Normal, nullptr) \
    v(Bool, reportTotalPhaseTimes, false, Normal, "This prints phase times at the end of running script inside jsc.cpp"_s) \
    v(Bool, reportParseTimes, false, Normal, "dumps JS function signature and the time it took to parse"_s) \
    v(Bool, reportBytecodeCompileTimes, false, Normal, "dumps JS function signature and the time it took to bytecode compile"_s) \
    v(Bool, reportBytecodeCacheDecodeTimes, false, Normal, "dumps the time it took to decode bytecode from the disk cache"_s) \
    v(Bool, countParseTimes, false, Normal, "counts parse times"_s) \
    v(Bool, verboseExitProfile, false, Normal, nullptr) \
    v(Bool, verboseCFA, false, Normal, nullptr) \
    v(Bool, verboseDFGFailure, false, Normal, nullptr) \
    v(Bool, verboseFTLToJSThunk, false, Normal, nullptr) \
    v(Bool, verboseFTLFailure, false, Normal, nullptr) \
    v(Bool, testTheFTL, false, Normal, nullptr) \
    v(Bool, verboseSanitizeStack, false, Normal, nullptr) \
    v(Bool, useGenerationalGC, true, Normal, nullptr) \
    v(Bool, useConcurrentGC, true, Normal, nullptr) \
    v(Bool, collectContinuously, false, Normal, nullptr) \
    v(Double, collectContinuouslyPeriodMS, 1, Normal, nullptr) \
    v(Bool, forceFencedBarrier, false, Normal, nullptr) \
    v(Bool, verboseVisitRace, false, Normal, nullptr) \
    v(Bool, optimizeParallelSlotVisitorsForStoppedMutator, false, Normal, nullptr) \
    v(Bool, verboseHeapSnapshotLogging, true, Normal, nullptr) \
    v(Unsigned, largeHeapSize, 32 * 1024 * 1024, Normal, nullptr) \
    v(Unsigned, mediumHeapSize, 4 * 1024 * 1024, Normal, nullptr) \
    v(Unsigned, smallHeapSize, 1 * 1024 * 1024, Normal, nullptr) \
    v(Double, smallHeapRAMFraction, 0.25, Normal, nullptr) \
    v(Double, smallHeapGrowthFactor, 2, Normal, nullptr) \
    v(Double, mediumHeapRAMFraction, 0.5, Normal, nullptr) \
    v(Double, mediumHeapGrowthFactor, 1.5, Normal, nullptr) \
    v(Double, largeHeapGrowthFactor, 1.24, Normal, nullptr) \
    v(Double, miniVMHeapGrowthFactor, 1.20, Normal, nullptr) \
    v(Double, heapGrowthSteepnessFactor, 2.00, Normal, nullptr) \
    v(Double, heapGrowthMaxIncrease, 3.00, Normal, nullptr) \
    v(Unsigned, aggressiveHeapThresholdInMB, 16 * 1024, Normal, nullptr) \
    v(Double, maxEdenSizeForRateLimitingMultiplier, 8.0, Normal, nullptr) \
    v(Double, gcRateLimitingHalfLifeInMS, 1000.00, Normal, nullptr) \
    v(Bool, validateFieldTypes, false, Normal, "walk the heap after every collection and verify that every recorded field type actually holds. The JSC counterpart of V8's field-type heap check (src/diagnostics/objects-debug.cc:638-649). Reports the structure, offset, expected and observed structure of any violation, which localises an unsound store path to the exact field instead of to a downstream crash"_s) \
    v(Bool, logFieldTypes, false, Normal, "trace the field-type record lifecycle: CREATE / GENERALIZE / STORE-CHECK on the runtime side, NARROW / DECLINE on the compiler side. For diagnosing unsound narrowings"_s) \
    v(Bool, useFieldTypeAssumptions, true, Normal, "infer a structure for values loaded out of object fields (V8-style Map field types) so that CheckStructure on a loaded field folds away. Requested by DFGConstantFoldingPhase, consumed by Graph::inferredValueForProperty, and depends on the loaded structure's transition watchpoint"_s) \
    v(Bool, useFieldTypeClaimSurrenderOnReplace, true, Normal, "when a claim would leave an LLInt REPLACE store site permanently uncached, surrender the claim and let the site cache instead. Transition sites still keep the claim: delta-blue's claims live on transitions (13 of its 14 value-carrying records are never replace-stored) while babel-wtb has 200x more replace traffic. Set false for the decline-always behaviour"_s) \
    v(Unsigned, fieldTypeClaimSurrenderThreshold, 0, Normal, "how many LLInt replace sites a claim may leave uncached before useFieldTypeClaimSurrenderOnReplace gives it up. 0 surrenders at the first refusal; 200 surrenders only for fields babel-wtb has and delta-blue provably does not (its hottest claim re-enters 129 times)"_s) \
    v(Bool, useFieldTypeNarrowing, true, Normal, "let the compiler CONSUME claims: Graph::fieldTypeAssumptionValue narrows a loaded field to its claimed structure so the CheckStructure folds. This is the entire benefit side of the mechanism; turning it off leaves recording, the store-side hoist and invalidation running, which is the bisection that separates recorder cost from consumer cost"_s) \
    v(Bool, useFieldTypeStoreHoist, true, Normal, "emit the store-side field-type check when a JIT-compiled transition store writes a claimed field. Turning it off is sound but gives up the claim, because a non-hoisted compiled store falls into fieldTypeRecordForStore and calls addFieldTypeToGeneralize"_s) \
    v(Bool, useFieldTypeReplaceMaintenance, true, Normal, "run maintainFieldTypeRecord on C++ replace stores. MEASUREMENT SWITCH, unsound when false. Prices the Structure::findOffsetOwner CHAIN WALK that runs on every replace store as soon as the table is non-empty -- the claim-word short-circuit is tested only AFTER that walk, so it is paid per store even when no claim exists anywhere. Sound to disable when combined with fieldTypeClaimableKinds=4, since then there is no claim to maintain"_s) \
    v(Bool, useFieldTypeCreationRecording, true, Normal, "run recordFieldTypeAtCreation on property creation. MEASUREMENT SWITCH, unsound when false; sound in combination with fieldTypeClaimableKinds=4. Prices the creation-side hook independently of whether any claim is live"_s) \
    v(Bool, useFieldTypeHoistedCheckEmission, true, Normal, "emit the parse-time CheckStructure that hoistFieldTypeCheckForTransition generates for a claimed field. MEASUREMENT SWITCH, UNSOUND when false: the store then writes unchecked while the claim stays live. Unlike useFieldTypeStoreHoist=0, this keeps fieldTypeCheckHoisted set and the watchpoint dependency registered, so the DFG/FTL backend fail-safe does NOT run and claims are NOT destroyed -- which is what makes it an isolation of the check's own cost rather than a third measurement of claim liveness"_s) \
    v(Bool, useFieldTypeCreationDeferGC, true, Normal, "take the DeferGCForAWhile in putDirectInternal around field-type recording. MEASUREMENT SWITCH: it guards the nuked-transition window that caused the babylon-wtb heap corruption, so false is unsafe for anything but measuring the two m_deferralDepth RMWs per property creation"_s) \
    v(Bool, useFieldTypeCreationArmsTransitionWatchpoint, true, Normal, "call startWatching() on the transition watchpoint set of a structure observed as a cell-valued field, in recordFieldTypeAtCreation. MEASUREMENT SWITCH, UNSOUND when false: an unarmed set makes fireAll a no-op, so Graph::tryWatch can adopt an already-false claim. It exists because this is the ONLY effect the creation recorder has outside the field-type system, and creation recording is what carries json-parse-inspector's entire +1.64% even though that test narrows nothing -- arming the set is also what lets the pre-existing DFG structure-transition machinery fold, so the win may belong to JSC's own watchpoints rather than to claims"_s) \
    v(Unsigned, fieldTypeCreationDummyWork, 0, Normal, "substitute a strict subset of recordFieldTypeAtCreation's work when useFieldTypeCreationRecording is false: 1 = its reads only, 2 = reads plus the claim-word write, 3 = reads plus a table lookup, 4 = reads plus the LOCKED HASH INSERT with no record allocated, which is the leg that separates the table's cost from the per-entry FieldTypeRecord allocation. MEASUREMENT SWITCH, records nothing and is inert unless recording is already off. It exists to decide whether creation recording is a real mechanism on json-parse-inspector or whether ANY added work in the property-creation path would produce the same win -- the accident hypothesis has to be excluded by measurement, not argument"_s) \
    v(Unsigned, fieldTypeTimeRecorderStopAfter, 0, Normal, "with fieldTypeTimeRecorder=1, return from recordFieldTypeAtCreation after stage N so the timer measures a cumulative PREFIX of it: 1 = options+isDictionary, 2 = +the ancestor computation, 3 = +the claim-word fast path, 4 = +the ancestor withdrawal loop, 5 = +arming the value's transition watchpoint, 0 = the whole function. Differences between consecutive N give each stage's cost. MEASUREMENT SWITCH, unsound for N>0. Two timer reads regardless of N keeps the perturbation constant, which per-section timers would not; and the timer resolves a few nanoseconds where ablation cannot -- each stage is worth ~0.1% of chai-wtb, below the paired instrument's 0.3% floor, which is why every ablation of these stages read zero"_s) \
    v(Unsigned, fieldTypeTimeStoreSite, 0, Normal, "time FieldTypeWatchpointTable::recordForStoreSite directly: 1 = the real function, 2 = an empty section so the two MonotonicTime reads own overhead is measured. MEASUREMENT SWITCH; 2 also returns null, so it is unsound. It exists because every sub-gate of the store-site machinery measures inert on raytrace while switching the machinery off recovers 5.75 points -- the same diffuse-cost situation as chai-wtb, where only direct timing resolved it"_s) \
    v(Unsigned, fieldTypeTimeRecorder, 0, Normal, "time recordFieldTypeAtCreation directly and report total nanoseconds and call count: 1 = time the real recorder, 2 = time an EMPTY section so the two MonotonicTime reads' own overhead is measured rather than assumed. MEASUREMENT SWITCH; 2 also disables recording, so it is unsound. It exists because neither sampling nor ablation could locate the recorder's cost on chai-wtb -- every operation inside it measures ~0 in a layout-matched pair while the whole measures 2.16 points, and the samply profiles failed their reproduction check"_s) \
    v(Bool, useFieldTypeCreationAncestorCheck, true, Normal, "derive, on every property creation, whether an ancestor structure could own a record at this offset. MEASUREMENT SWITCH, UNSOUND when false: two objects of one shape can reach it by different transition paths, so a claim established via the other path is then never withdrawn. It exists to bound what a cache of the negative answer could recover -- the check chases owner->previousID() into a SECOND Structure on every creation whose offset is the shape's maximum, which is the common fresh-offset case, and creation recording is what carries chai-wtb's -3.06 of -3.80 points"_s) \
    v(Bool, useFieldTypePruneWalk, true, Normal, "walk the field-type table at end of marking to clear claims naming dead structures. MEASUREMENT SWITCH: false leaks stale claims (unsound), and exists to price the per-entry isMarked walk that runs on every collection including Eden"_s) \
    v(Bool, useFieldTypeICChecks, true, Normal, "emit the inline field-type check in ById/ByVal inline-cache store handlers for a claimed field. MEASUREMENT SWITCH: setting this false is UNSOUND -- a cached IC store would then write a violating value with nothing withdrawing the claim, while compiled code that deleted a CheckStructure reads the field. It exists because the IC checks are the only claim-liveness-dependent mechanism not covered by useFieldTypeNarrowing or useFieldTypeStoreHoist, and the two largest regressions are attributed to claim liveness"_s) \
    v(Unsigned, fieldTypeStoreSiteClaimHitLimit, 0, Normal, "surrender a field-type claim once store sites have consulted it more than this many times (0 disables). DEFAULT CHANGED 1 -> 0 on 2026-08-22: this tier discriminator was the campaign's biggest step when it was worth ~8 points on FlightPlanner, but it was MASKING the megamorphic put-cache decline. With that root cause fixed (useFieldTypeMegamorphicPutSurrender) the heuristic is pure cost -- n=20: FlightPlanner -0.60% p=0.48, esprima-next-wtb +0.14% p=0.74, delta-blue -0.24% p=0.70, and raytrace GAINS +1.95% p=0.0002 when it is removed. A root-cause fix retiring a heuristic is the outcome to want. A claim that store sites keep re-consulting costs more than it earns: measured per (owner, offset), delta-blue's hottest claim is consulted 105 times while FlightPlanner has three at 84,010 and babel-wtb one at 407, so any limit above 105 is provably inert on delta-blue. Sound: generalising only removes information"_s) \
    v(Bool, useFieldTypeICHandlerDowngrade, true, Normal, "let a live claim route a put inline cache away from its cheap handler -- the bare inline self-replace stub in Repatch and the four pre-compiled shared ById/ByVal Replace and Transition thunks -- and on to a per-site GENERATED handler that can bake the expected StructureID. MEASUREMENT SWITCH, unsound alone (the shared thunk then stores unchecked under a live claim), sound combined with useFieldTypeNarrowing=0. This prices handler SELECTION, which useFieldTypeICChecks does not: that option removes the compare inside the generated handler while still forcing the handler to be generated, so the two isolate different halves of the same site"_s) \
    v(Bool, useFieldTypeLLIntCheckEmission, true, Normal, "bake the expected StructureID into OpPutById metadata so the LLInt enforces a claim from asm. MEASUREMENT SWITCH, unsound alone, sound combined with useFieldTypeNarrowing=0. Keeps every bookkeeping side effect -- poisoning, the consultation count, the hot-claim surrender -- and bakes nothing, so it prices the asm compare PLUS the stores that a stale-SET baked ID diverts to the C++ slow path for the rest of the run. That divert is invisible to STORE-SITE-CLAIM-HIT, because a generalized claim leaves the owner word at entryWithoutClaim and the consultation returns before it is counted; LLINT-CHECK-MISS is the counter for it"_s) \
    v(Bool, useFieldTypeMegamorphicPutSurrender, true, Normal, "surrender a field-type claim when a megamorphic put site would otherwise be denied its cache, instead of declining the cache. THE FIX for the largest cost this mechanism imposes: a declined site never installs a cache, so every store there pays a full C++ put for the rest of the run -- 1,403,740 such stores on esprima-next-wtb, 160,521 on FlightPlanner, and 0 on delta-blue, so surrendering here is free for the mechanism's beneficiary. Sound: generalising only removes information, and it happens before the unchecked cache is installed"_s) \
    v(Bool, useFieldTypeCompilerOwnerMemo, true, Normal, "memoise Structure::findOffsetOwner per compilation in DFG::Graph, for the consumer narrowing path and the store-owner resolution. Behaviour-preserving -- both legs compute the same owner -- so this prices the walk rather than gating a mechanism. Thread-safe by construction: a Graph is one compilation on one thread, so unlike the table-side memo it needs no lock and no end-of-marking clear. Targets the 15,292 per-load-site walks that cost gbemu 1.53% of First-Score"_s) \
    v(Bool, useFieldTypeOwnerMemo, true, Normal, "memoise Structure::findOffsetOwner for the C++ replace-store path in a direct-mapped table cleared at end of marking. THE FIX for verified/03: that chain walk runs on every replace store as soon as the table is non-empty (299,630 calls on typescript-lib, 151,540 on jsdom-d3-startup, 129,520 on babylonjs-scene-es6) because the claim-word short-circuit is only reachable once the owner is known. Behaviour-preserving -- both legs compute the same owner -- so this option prices the memo rather than gating a mechanism"_s) \
    v(Bool, countReoptimizationOnWatchpointJettison, true, Normal, "count the reoptimization when a DFG watchpoint jettisons a CodeBlock. MEASUREMENT SWITCH and an UPPER BOUND, not a shippable fix: it relaxes the penalty for every watchpoint kind, not just field-type withdrawals. Exists because a field-type withdrawal is an external event and claims are monotone, so the recompile cannot reuse the claim and the reoptimization penalty has nothing to teach -- but confirming that requires knowing the global relaxation helps at all"_s) \
    v(Bool, useFieldTypeStoreSiteRecordCreation, true, Normal, "create the unclaimed FieldTypeRecord that a compiled store site needs in order to load a claim established later (FieldTypeWatchpointTable::ensureRecordForStoreSite). MEASUREMENT SWITCH, unsound alone, sound combined with fieldTypeClaimableKinds=4. Unlike recordForStoreSite this ALLOCATES a record plus WatchpointSet, and both parse-time hoists call it per put site per compilation -- unconditional work concentrated in the first compiles, which is where the systematic First-Score penalty lives"_s) \
    v(Bool, useFieldTypeMaterializationRecording, true, Normal, "when the DFG/FTL materializes a sunk object allocation, RUN THE CREATION RECORDER over its fields instead of poisoning them. Allocation sinking is meant to be transparent: the object was created, the compiler merely deferred it, so running the hook at materialization restores the invariant that no object predates a record. Poisoning instead destroys live claims -- the entire raytrace-family regression (color 106, direction 107, position 69 dependents) is this one site, and delta-blue has ZERO of them. Reuses recordFieldTypesForCopiedObject, the same primitive Object.assign's fast path uses for exactly this problem"_s) \
    v(Bool, useFieldTypeAncestryProof, true, Normal, "in Graph::poisonFieldTypeAcrossAncestry, do not generalize an ancestor claim the stored value is PROVEN to satisfy. Same argument as the materialization proof: the store cannot violate a claim it matches, and claims are monotone so no dependency is needed. This is the second of the two poison walks -- it fires 150 times on jsdom-d3-startup and 17 on babylonjs-scene-es6, where the first one fires not at all"_s) \
    v(Bool, useFieldTypeUncheckedWritePoisoning, true, Normal, "let Graph::poisonFieldTypeForUncheckedWrite and the offset-wide/ancestry poison walks GENERALIZE a live claim when the DFG emits a store it cannot check. This is the path that destroys raytrace color/direction/position (106/107/69 dependents) -- no value ever contradicts them, the compiler discards them pre-emptively. UNSOUND when false: an unchecked store then runs under a live claim. The sound replacement is a runtime-loaded check at the store site, which is what V8 does (its handler loads the field type instead of baking it, so a site compiled before a claim existed still honours one that appears later)"_s) \
    v(Bool, useFieldTypeDFGStoreSitePoisoning, true, Normal, "let the DFG store-hoist decision poison a field as a side effect of asking whether it carries a claim (Graph::fieldTypeRecordForStore calls recordForStoreSite, which inserts the permanently-generalised entry on a new key and returns null). False uses the non-poisoning recordFor instead. UNSOUND when false until the establishment hole is closed: the emitted check loads addressOfExpected() and exits on ZERO, which covers a claim being WITHDRAWN but not one being ESTABLISHED after the site was compiled without a check. It exists because the poisoning is measured to be pure cost on both sides -- STORE-CHECK is 0 across raytrace, delta-blue, chai-wtb and babel-wtb while STORE-DECLINE(no-record) is 565/173/79/1747, and removing the store-site machinery recovers raytrace +6.01 points while IMPROVING delta-blue from +9.47% to +9.88%"_s) \
    v(Unsigned, fieldTypeMaxDependentsPerClaim, 0, Normal, "stop letting new compilations depend on a field-type claim once this many already do (0 = unlimited). SOUND at any value: declining to narrow forgoes an optimisation and registers no dependency. It bounds the blast radius of a withdrawal, which is the only lever left on the raytrace family -- three claims there (direction 107 dependents, color 106, position 69) discard 282 CodeBlocks and carry the entire 5.2-point regression, and they are contradicted only AFTER those dependents accrue, so admission, provenance and seven count axes are all closed. JSC has no cheap invalidation currency to move onto: every DesiredWatchpoints path installs a CodeBlockJettisoningWatchpoint. WIRED UP 2026-08-26 in Graph::fieldTypeAssumptionValue. Measured maxima per claim: delta-blue 556, acorn-wtb 2544 (its Token.type claim holds 738 across 29 distinct functions and takes 22 CodeBlocks down at one instant), babel-wtb 32 -- so a threshold in (556, 738) bounds acorn with delta-blue untouched, and CANNOT help babel-wtb, whose regression is therefore not fan-out driven"_s) \
    v(Bool, useFieldTypeStoreSiteRecording, true, Normal, "poison a field when a store site is compiled or cached without an inline field-type check (FieldTypeWatchpointTable::recordForStoreSite). MEASUREMENT SWITCH, unsound alone, sound combined with fieldTypeClaimableKinds=4. Prices the lock + hash add that runs on EVERY store-site consultation -- 256k on FlightPlanner, 244k on jsdom-d3-startup -- and which is reached on first execution of every store site, making it the leading candidate for the systematic First-Score penalty"_s) \
    v(Bool, useFieldTypeStoreSideLoadedCheck, false, Normal, "HALF 1 of the check-based field-type dependency: the store-side hoists and the FTL variant check LOAD the claim from record->addressOfExpected() at runtime instead of baking it, and register no watchpoint. Sound by construction -- a withdrawal zeroes the slot, the compare fails, the site OSR-exits and recompiles without a check. Costs CSE/LICM visibility through the check. Useless alone (16% measured) and must be combined with useFieldTypeConsumerLoadedCheck: the hot functions are registered through BOTH paths, so killing one leaves the jettisons intact"_s) \
    v(Bool, fieldTypeMeasureLoadedCheckCostOnly, false, Normal, "RISK PROBE, not a design. Keep the BAKED narrowing (so CSE/LICM still fold it and the benefit is intact) AND additionally insert the CheckFieldType that the loaded design would rely on. Measures ONE thing: what a runtime-loaded check costs at a narrowed load. If delta-blue -- which narrows 3,909 times -- tolerates it, the loaded design is worth the AI/ConstantFolding restructuring; if delta-blue collapses, a baked immediate really is what folds and the design is shut for real, without building it"_s) \
    v(Bool, useFieldTypeConsumerLoadedCheck, false, Normal, "HALF 2: a compilation that narrows a load does not depend on the claim; DFGConstantFoldingPhase inserts a CheckFieldType after the load instead, so a withdrawal costs one OSR exit rather than discarding every dependent. Combined with useFieldTypeStoreSideLoadedCheck this removes all 6 UnprofiledWatchpoint jettisons on raytrace, whose measured ceiling is +5.59 points (107%) with delta-blue preserved"_s) \
    v(Bool, useFieldTypeFoldGatedNarrowing, false, Normal, "PAY FOR WHAT YOU USE: stop narrowing on a claim whose narrowings have never folded a structure check. Measured benefit density differs 50x -- delta-blue folds ~20 checks per 1000 narrowings (CheckStructure 139 -> 61 on its five hottest narrowed functions), acorn-wtb folds ~0.4 (169 -> 167) while paying 36 CodeBlock jettisons for it. This is the first gate in the campaign to ask about the OUTCOME of a narrowing rather than a property of the claim, which is why the six admission refutations (README 0.6) do not apply: nothing observable at claim time predicts harm (verified/16 section 8), but whether a fold happened is already known to the compiler. SOUND UNCONDITIONALLY -- the only action is to decline to narrow, which forgoes an optimisation and registers no dependency, so a mis-attributed fold count can only mis-tune"_s) \
    v(Unsigned, fieldTypeFoldlessNarrowingBudget, 64, Normal, "how many narrowings a claim may spend before it must have folded at least one structure check, or narrowing on it stops. Must exceed the per-compilation narrowing count of a claim that IS valuable, or a beneficiary is cut off mid-warmup: delta-blue's top claim narrows 670 times across the run but folds on its first compilation, so any budget above a few compilations' worth is safe for it"_s) \
    v(Bool, useFieldTypeConsumerWatchpointRegistration, true, Normal, "register the CONSUMER side of a field-type claim: a compilation that narrows a load depends on the record and is jettisoned when the claim is withdrawn (DFGGraph.cpp, fieldTypeAssumptionValue). MEASUREMENT SWITCH, UNSOUND when false -- narrowed code is then never invalidated. It exists to bound the only untried lever on the raytrace family: the 282 dependents behind its 5.2-point regression are all consumer-side, and the sound fix (emit CheckFieldType at the narrowed load, which reads the claim at runtime and needs no dependency) is a restructuring of AI/ConstantFoldingPhase cooperation that should only be attempted if this ceiling justifies it"_s) \
    v(Bool, useFieldTypeWatchpointRegistration, true, Normal, "register the claim's WatchpointSet on the compilation (DesiredWatchpoints) and its owner as a DFG weak reference when a claimed field's store is compiled. MEASUREMENT SWITCH, UNSOUND alone -- baked code would then survive the claim's withdrawal -- and sound only when combined with useFieldTypeHoistedCheckEmission=0 so nothing depends on the claim. Exists because claim liveness costs raytrace 44% MORE COMPILE TIME (65 -> 94 ms, and 76 -> 88 plans) which neither useFieldTypeNarrowing=0 nor useFieldTypeHoistedCheckEmission=0 recovers, and this registration is the one liveness-dependent compile-time action both of those gates leave intact"_s) \
    v(Bool, useFieldTypeMegamorphicPutDecline, true, Normal, "let a live claim decline the megamorphic put cache, keeping the C++ slow path that maintains the record. Only consulted when useFieldTypeMegamorphicPutSurrender is false. MEASUREMENT SWITCH, unsound alone, sound combined with useFieldTypeNarrowing=0; it is what priced this mechanism. Note this is NOT what fix #3 measured on 2026-08-20: that withdrew claims globally, so it changed claim liveness and measured a different thing"_s) \
    v(Bool, useFieldTypeJSReceiverOnlyClaims, true, Normal, "V8 PARITY: claim a field type only when the value is a JSReceiver, which is JSC's analogue of the IsJSReceiverMap half of Object::OptimalType (objects.cc). V8 refuses non-receiver values outright and therefore NEVER claims a string-valued field. JSC has no such gate, and the reason it matters is that string REPRESENTATION is not stable -- an atomic string, a rope and an 8- vs 16-bit string are different Structures, so a claim naming one is contradicted by the next store of a differently represented string. Measured: all four string-typed harmful claims on acorn-wtb and babel-wtb name the SAME structure (16777808), and they account for 176 of acorn's 870 destroyed dependents and 10 of babel's 61. Also drops Symbol and BigInt, which the OtherCellClaim bucket currently admits"_s) \
    v(Unsigned, fieldTypeClaimableTypeMask, 23, Normal, "BITMASK of cell buckets a field-type claim may name: 1 final objects, 2 callables, 4 strings, 8 arrays, 16 other cells. DEFAULT 19 = final objects + callables + other cells, i.e. EXCLUDING BOTH ARRAYS AND STRINGS -- behaviourally the old kinds=2. It was briefly 23 (strings claimable) on the strength of isolated per-test measurements, cdjs +1.85% and delta-blue +0.72%, and that was WRONG IN THE DEPLOYED CONFIGURATION: making strings claimable creates more claims across all 75 tests, so more claims are contradicted, so the unstable-name filter accumulates more strikes (9,650 -> 10,153 names) and issues 8,893 MORE declines, and delta-blue -- which runs late and shares that global strike set -- drops from 2084.5 to 1945.2 in-suite, -6.7%. Two changes each sound alone interact through VM-global state. Raise this to 23 only once the filter no longer accumulates across unrelated code. This replaced an ordered enum whose levels forced independent decisions onto one axis, and that conflation cost real points: the measured reason to exclude anything is arrays -- a structure-IDENTITY claim cannot cover an array's lifecycle, and 71% of babel-wtb's contradictions are Array -> Array with identical class, indexing type and maxOffset -- but the only level that excluded arrays also excluded STRINGS, because 2 > 1. Strings were never implicated (kinds=1 leaves sha256's narrowing count unchanged, so it is arrays specifically), and the collateral was not free: isolating the string exclusion (kinds 0 -> 1) costs cdjs -1.83% (p<0.0001) and is n.s. on sha256, chai-wtb and delta-blue. cdjs -1.35% was on record as the ONLY loss from landing the array exclusion -- that loss was the string exclusion all along. A skipped kind takes the non-cell path: a claimless, permanently-generalised entry, which only ever removes information"_s) \
    v(Unsigned, fieldTypeClaimableKinds, 5, Normal, "LEGACY ordered enum, retained so that every recorded measurement leg keeps working -- most of this campaign's ablations pass 4 for NeverClaim. 0 all cells, 1 skip strings, 2 skip strings and arrays, 3 final objects only, 4 never, and 5 (the default) means NOT SET, use fieldTypeClaimableTypeMask instead. Prefer the mask for anything new: this enum cannot express skip-arrays-but-keep-strings, which is the configuration the evidence actually supports"_s) \
    v(Bool, dumpParsePatternStats, false, Normal, "count field-chain and shape-creation patterns during bytecode generation and report them at exit. Diagnostic for the parse-time admission question: whether a decision taken BEFORE any object is created -- which the \"no object may predate a claim\" invariant requires -- can predict whether a program benefits from field types"_s) \
    v(Bool, useFieldTypeLazyEntries, false, Normal, "THE LAZY TABLE ENTRY. A claim needs no m_records entry to be ENFORCED: enforcement reads the owner shape's claim word plus the interned m_claimedStructureIDBits, and that is all. The entry exists only to hold a WatchpointSet and a FieldTypeRecord, which field-types/verified/12 measures as needed by 1,794 of 273,947 entries -- 99.35% are built and never consulted by a compiler. So the claim is established in the word alone and the entry is created on the first consultation that could lead to a dependency or a store check. This is what V8 gets structurally: its field type lives in the Map's DescriptorArray and its dependents hang off Map::dependent_code_, which a fresh map initialises to the shared read-only empty_dependent_code (dependent-code.h:102, factory.cc:2765), so a map nobody optimises against costs one pointer to a shared constant. SOUNDNESS CONTRACT: every path that can create a dependency or emit a store check must materialise first, so a word-only claim has ZERO dependents by construction and withdrawing it needs no watchpoint fire"_s) \
    v(Bool, useFieldTypeLazyRecords, false, Normal, "DEFAULT FALSE: BUILT, SOUND, AND MEASURED SCORE-NEUTRAL. Kept behind the option so the result can be reproduced in one command. Measured with it on: chai-wtb recovers 0.63 of 3.21 points, mobx-startup 0.28 of 2.13, babylonjs-startup-es6 0.00, proxy-mobx -0.31, and peak RSS moves 188->188 MB on chai and 521->513 MB on babylonjs-startup. The ceiling that justified building it (2.33 / 2.13 / ~2.5) was WRONG because the leg that produced it, fieldTypeCreationDummyWork=4, requires fieldTypeClaimableKinds=4 to be sound -- so it removed claim LIVENESS as well as the record allocation, and over-stated the recoverable amount by exactly the liveness component (see todo/11 F17). What it does: carry a newly created claim's StructureID in the table entry instead of allocating a FieldTypeRecord and its WatchpointSet for it, and materialise the record only when a compiler first consults the field. THE FIX for the recorder class. A claim does not need a record to be ENFORCED -- enforcement reads the owner shape's claim word plus the interned claimed-StructureID vector -- so a claim no compiler has looked at pays two allocations for nothing. Populations: chai-wtb creates 28,936 claims to serve 147 narrowings (195:1), babylonjs-startup-es6 93,146 to serve 277 (336:1), against delta-blue's 861 serving 4,039 (1:4.7). Measured ceiling via fieldTypeCreationDummyWork=4, which prices exactly this design: chai-wtb 2.33 of its 3.21 points, mobx-startup all of 2.13, proxy-mobx all of 1.20, babylonjs-startup-es6 ~2.5 of 2.76. Free for delta-blue by construction, since its records materialise immediately. SOUNDNESS: recordFor() and recordsAtOffset() materialise rather than return null, because a null return means \"no claim, nothing to enforce\" to every store-side caller"_s) \
    v(Unsigned, fieldTypeOwnerMemoSize, 512, Normal, "how many entries of the direct-mapped findOffsetOwner memo a run uses (rounded down to a power of two, 0 or oversize means the 65536 maximum). The original 512 was sized against the distinct (shape, offset) pairs a SINGLE benchmark touches; in-suite the working set is far larger -- 1,082,267 replace-store maintenance calls, of which only 2.69% get past the claim-word short-circuit, against a field-type table reaching 169,778 entries -- so conflict misses are the expected failure and every one of them is a transition-chain walk. Behaviour-preserving at every value: both legs compute the same owner, so this prices a cache rather than gating a mechanism. Report the hit rate with --useDollarVM=1 (OWNER-MEMO)"_s) \
    v(Double, criticalGCMemoryThreshold, 0.80, Normal, "percent memory in use the GC considers critical.  The collector is much more aggressive above this threshold"_s) \
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
    v(Bool, useWarmUpMarkedBlocks, true, Normal, "hand MarkedBlock allocation pages that a helper thread already made resident"_s) \
    v(Unsigned, warmUpMarkedBlockCount, 32, Normal, "how many MarkedBlocks the helper thread keeps ready with their pages already resident; 0 turns it off"_s) \
    v(Double, warmUpMarkedBlockIdleTimeout, 10, Normal, "seconds without a MarkedBlock request before the helper thread releases what it is holding and shuts down"_s) \
    v(Bool, scribbleFreeCells, false, Normal, nullptr) \
    v(Double, sizeClassProgression, 1.4, Normal, nullptr) \
    v(Unsigned, preciseAllocationCutoff, 100000, Normal, nullptr) \
    v(Bool, dumpSizeClasses, false, Normal, nullptr) \
    v(Bool, stealEmptyBlocksFromOtherAllocators, true, Normal, nullptr) \
    v(Bool, eagerlyUpdateTopCallFrame, false, Normal, nullptr) \
    v(Bool, dumpZappedCellCrashData, false, Normal, nullptr) \
    \
    v(Bool, useOSREntryToDFG, true, Normal, nullptr) \
    v(Bool, useOSREntryToFTL, true, Normal, nullptr) \
    \
    v(Bool, useFTLJIT, true, Normal, "allows the FTL JIT to be used if true"_s) \
    v(Bool, validateFTLOSRExitLiveness, false, Normal, nullptr) \
    v(Bool, poisonDeadOSRExitVariables, ASSERT_ENABLED, Normal, "Put an unmapped cell-like pointer (poisonedDeadOSRExitValue) into dead OSR exit values rather than jsUndefined, so accidental reads of dead variables crash at the access site"_s) \
    v(Unsigned, defaultB3OptLevel, 2, Normal, nullptr) \
    v(Bool, b3AlwaysFailsBeforeCompile, false, Normal, nullptr) \
    v(Bool, b3AlwaysFailsBeforeLink, false, Normal, nullptr) \
    v(Bool, validateSerializedValue, false, Normal, nullptr) /* tests CloneSerializer/Deserializer */ \
    v(Bool, ftlCrashes, false, Normal, nullptr) /* fool-proof way of checking that you ended up in the FTL. ;-) */\
    v(Bool, clobberAllRegsInFTLICSlowPath, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, useJITDebugAssertions, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, useAccessInlining, true, Normal, nullptr) \
    v(Unsigned, maxAccessVariantListSize, 8, Normal, nullptr) \
    v(Double, thresholdForUndesiredMegamorphicAccessVariantListSize, 0.5, Normal, nullptr) \
    v(Bool, usePolyvariantDevirtualization, true, Normal, nullptr) \
    v(Bool, usePolymorphicAccessInlining, true, Normal, nullptr) \
    v(Unsigned, maxPolymorphicAccessInliningListSize, 8, Normal, nullptr) \
    v(Bool, usePolymorphicCallInlining, true, Normal, nullptr) \
    v(Bool, usePolymorphicCallInliningForNonStubStatus, false, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSize, 8, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSizeForTopTier, 5, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantListSizeForWasmToJS, 5, Normal, nullptr) \
    v(Unsigned, maxPolymorphicCallVariantsForInlining, 5, Normal, nullptr) \
    v(Unsigned, frequentCallThreshold, 2, Normal, nullptr) \
    v(Double, minimumCallToKnownRate, 0.51, Normal, nullptr) \
    v(Bool, createPreHeaders, true, Normal, nullptr) \
    v(Bool, useMovHintRemoval, true, Normal, nullptr) \
    v(Bool, usePutStackSinking, true, Normal, nullptr) \
    v(Bool, useObjectAllocationSinking, true, Normal, nullptr) \
    v(Bool, verboseObjectAllocationSinking, false, Normal, nullptr) \
    v(Bool, useValueRepElimination, true, Normal, nullptr) \
    v(Bool, useArityFixupInlining, true, Normal, nullptr) \
    v(Bool, logExecutableAllocation, false, Normal, nullptr) \
    v(Unsigned, maxDFGNodesInBasicBlockForPreciseAnalysis, 20000, Normal, "Disable precise but costly analysis and give conservative results if the number of DFG nodes in a block exceeds this threshold"_s) \
    \
    v(Bool, useConcurrentJIT, true, Normal, "allows the DFG / FTL compilation in threads other than the executing JS thread"_s) \
    v(Unsigned, minNumberOfWorklistThreads, computeNumberOfWorkerThreads(3, 2), Normal, nullptr) \
    v(Unsigned, maxNumberOfWorklistThreads, computeNumberOfWorkerThreads(3, 2), Normal, nullptr) \
    v(Unsigned, numberOfBaselineCompilerThreads, computeNumberOfWorkerThreads(3, 2), Normal, nullptr) \
    v(Unsigned, numberOfDFGCompilerThreads, computeNumberOfWorkerThreads(3, 2) - 1, Normal, nullptr) \
    v(Unsigned, numberOfFTLCompilerThreads, computeNumberOfWorkerThreads(MAXIMUM_NUMBER_OF_FTL_COMPILER_THREADS, 2) - 1, Normal, nullptr) \
    v(Unsigned, numberOfWasmCompilerThreads, computeNumberOfWorkerThreads(INT32_MAX, 2) - 1, Normal, nullptr) \
    v(Unsigned, worklistLoadFactor, 1, Normal, nullptr) \
    v(Unsigned, worklistBaselineLoadWeight, 1, Normal, nullptr) \
    v(Unsigned, worklistDFGLoadWeight, 1, Normal, nullptr) \
    v(Unsigned, worklistFTLLoadWeight, 1, Normal, nullptr) \
    v(Int32, priorityDeltaOfDFGCompilerThreads, computePriorityDeltaOfWorkerThreads(-1, 0), Normal, nullptr) \
    v(Int32, priorityDeltaOfFTLCompilerThreads, computePriorityDeltaOfWorkerThreads(-2, 0), Normal, nullptr) \
    v(Int32, priorityDeltaOfWasmCompilerThreads, computePriorityDeltaOfWorkerThreads(-1, 0), Normal, nullptr) \
    \
    v(Bool, useProfiler, false, Normal, nullptr) \
    v(Bool, dumpProfilerDataAtExit, false, Normal, nullptr) \
    v(Bool, disassembleBaselineForProfiler, true, Normal, nullptr) \
    v(Unsigned, abbreviateSourceCodeForProfiler, 0, Normal, nullptr) \
    \
    v(Bool, useArchitectureSpecificOptimizations, true, Normal, nullptr) \
    \
    v(Bool, breakOnThrow, false, Normal, nullptr) \
    \
    v(Unsigned, maximumOptimizationCandidateBytecodeCost, 100000, Normal, nullptr) \
    \
    v(Unsigned, maximumFunctionForCallInlineCandidateBytecodeCostForDFG, 80, Normal, nullptr) \
    v(Unsigned, maximumFunctionForClosureCallInlineCandidateBytecodeCostForDFG, 80, Normal, nullptr) \
    v(Unsigned, maximumFunctionForConstructInlineCandidateBytecodeCostForDFG, 80, Normal, nullptr) \
    v(Unsigned, maximumFunctionForCallInlineCandidateBytecodeCostForFTL, 170, Normal, nullptr) \
    v(Unsigned, maximumFunctionForClosureCallInlineCandidateBytecodeCostForFTL, 100, Normal, nullptr) \
    v(Unsigned, maximumFunctionForConstructInlineCandidateBytecodeCostForFTL, 100, Normal, nullptr) \
    \
    v(Unsigned, maximumFTLCandidateBytecodeCost, 60000, Normal, nullptr) \
    \
    v(Double, ratioFTLNodesToBytecodeCost, 1.9, Normal, "Ratio converting FTL # of DFG nodes to approx bytecode cost") \
    \
    /* Depth of inline stack, so 1 = no inlining, 2 = one level, etc. */ \
    v(Unsigned, maximumInliningDepth, 5, Normal, "maximum allowed inlining depth.  Depth of 1 means no inlining"_s) \
    v(Unsigned, maximumInliningRecursion, 2, Normal, nullptr) \
    \
    /* Maximum size of a caller for enabling inlining. This is purely to protect us */\
    /* from super long compiles that take a lot of memory. */\
    v(Unsigned, maximumInliningCallerBytecodeCost, 10000, Normal, nullptr) \
    \
    v(Bool, useGlobalInliningPlanner, true, Normal, "Survey and rank every inlining candidate before parsing and spend one compilation-wide budget on the best of them, instead of deciding each call site in bytecode order"_s) \
    v(Unsigned, globalInliningPlanBudgetForDFG, 2500, Normal, "Total callee bytecode cost the DFG may plan to inline in one compilation"_s) \
    v(Unsigned, globalInliningPlanBudgetForFTL, 12000, Normal, "Total callee bytecode cost the FTL may plan to inline in one compilation"_s) \
    v(Unsigned, maximumGlobalInliningPlanSites, 20000, Normal, "Cap on how many call sites one inlining plan will survey"_s) \
    v(Double, inliningPlanTierBonusBase, 2.0, Normal, "Multiplicative benefit per tier the callee has reached (LLInt, Baseline, DFG, FTL) when ranking inlining candidates"_s) \
    v(Double, inliningPlanTierBonusPowerForFTL, 3.0, Normal, "Base for the bonus multiplier for FTL callees"_s) \
    v(Double, inliningPlanTierBonusPowerForDFG, 2.0, Normal, "Base for the bonus multiplier for DFG callees"_s) \
    v(Double, inliningPlanTierBonusPowerForBaseline, 1.0, Normal, "Base for the bonus multiplier for Baseline callees"_s) \
    v(Double, inliningPlanDepthPenalty, 1.5, Normal, "Divisive benefit penalty per level of inline-stack nesting when ranking inlining candidates"_s) \
    \
    v(Unsigned, maximumVarargsForInlining, 100, Normal, nullptr) \
    \
    v(Unsigned, maximumBinaryStringSwitchCaseLength, 50, Normal, nullptr) \
    v(Unsigned, maximumBinaryStringSwitchTotalLength, 2000, Normal, nullptr) \
    v(Unsigned, maximumInlineStringSwitchCaseCount, 64, Normal, "Maximum number of cases for which the baseline JIT dispatches op_switch_string inline instead of calling out."_s) \
    v(Unsigned, maximumRegExpTestInlineCodesize, 500, Normal, "Maximum code size in bytes for inlined RegExp.test JIT code."_s) \
    v(Unsigned, maximumRegExpJITCodeSize, 16 * MB, Normal, "Maximum generated code size in bytes for RegExp JIT compilation before falling back to the interpreter."_s) \
    \
    v(Unsigned, wasmInliningMaximumDepth, 7, Normal, "Maximum inlining depth to consider inlining a wasm function."_s) \
    v(Unsigned, wasmInliningMaximumWasmCalleeSize, 500, Normal, "Maximum wasm size in bytes to consider inlining a wasm function."_s) \
    v(Unsigned, wasmInliningMaximumCount, 60, Normal, "Maximum inlining count to consider inlining a wasm function."_s) \
    v(Unsigned, wasmInliningMinimumBudget, 50, Normal, "Minimum budget for which the wasmInliningFactor does not apply"_s) \
    v(Unsigned, wasmInliningFactor, 5, Normal, "Maximum multiple budget in comparison to initial wasm size"_s) \
    v(Unsigned, wasmInliningBudget, 6000, Normal, "Maximum budget that allows inlining more"_s) \
    v(Double, wasmInliningLargeFunctionGrowthFactor, 1.4, Normal, "Minimum growth factor (multiplied by initial wasm size) that bounds the large-function inlining budget."_s) \
    v(Unsigned, wasmInliningTinyFunctionThreshold, 12, Normal, "Wasm size threshold for tiny wasm functions"_s) \
    v(Unsigned, wasmInliningSmallFunctionThreshold, 50, Normal, "Wasm size threshold for small wasm functions"_s) \
    \
    v(Double, jitPolicyScale, 1.0, Normal, "scale JIT thresholds to this specified ratio between 0.0 (compile ASAP) and 1.0 (compile like normal)."_s) \
    v(Int32, numberOfSuperAndPerformanceCoresOverride, 0, Normal, "If non-zero, overrides the number of Super and Performance (i.e. non-Efficiency) cores reported by the hardware; 0 means use the value reported by the hardware."_s) \
    v(Double, dfgThresholdScaleForFewPerformanceCores, 2.0, Normal, "On Apple silicon Macs with few Super and Performance cores, scale the DFG tier-up thresholds (thresholdForOptimize*) by this factor."_s) \
    v(Double, ftlThresholdScaleForFewPerformanceCores, 1.5, Normal, "On Apple silicon Macs with few Super and Performance cores, scale the FTL tier-up thresholds (thresholdForFTLOptimize*) by this factor."_s) \
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
    v(Int32, thresholdForFTLOptimizeAfterWarmUp, 64000, Normal, nullptr) \
    v(Int32, thresholdForFTLOptimizeSoon, 1000, Normal, nullptr) \
    v(Int32, ftlTierUpCounterIncrementForLoop, 1, Normal, nullptr) \
    v(Int32, ftlTierUpCounterIncrementForReturn, 15, Normal, nullptr) \
    v(Unsigned, ftlOSREntryFailureCountForReoptimization, 15, Normal, nullptr) \
    v(Unsigned, ftlOSREntryRetryThreshold, 100, Normal, nullptr) \
    \
    v(Int32, evalThresholdMultiplier, 10, Normal, nullptr) \
    v(Unsigned, maximumEvalCacheableSourceLength, 256, Normal, nullptr) \
    \
    v(Int32, maximumExecutionCountsBetweenCheckpointsForBaseline, 1000, Normal, nullptr) \
    v(Int32, maximumExecutionCountsBetweenCheckpointsForUpperTiers, 30000, Normal, nullptr) \
    v(Int32, highCostBaselineProfilingFunctionBytecodeCost, 10000, Normal, nullptr) \
    v(Int32, valueProfileFillingRateMonitoringBytecodeCost, 5000, Normal, nullptr) \
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
    v(Double, quickDFGTierUpThresholdFactor, defaultQuickDFGTierUpThresholdFactor(), Normal, "Threshold factor for quick DFG tier-up"_s) \
    v(Double, relaxedProfileCoverageFactorForQuickDFGTierUp, defaultRelaxedProfileCoverageFactorForQuickDFGTierUp(), Normal, "Profile coverage scaling factor for quick DFG tier-up"_s) \
    v(Double, quickFTLTierUpThresholdFactor, defaultQuickFTLTierUpThresholdFactor(), Normal, "Threshold factor for quick FTL tier-up"_s) \
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
    v(Unsigned, maxHeapSizeAsRAMSizeMultiple, 0, Normal, nullptr) \
    v(Double, minHeapUtilization, 0.8, Normal, nullptr) \
    v(Double, minMarkedBlockUtilization, 0.9, Normal, nullptr) \
    v(Unsigned, slowPathAllocsBetweenGCs, 0, Normal, "force a GC on every Nth slow path alloc, where N is specified by this option"_s) \
    /* WARNING: this option is important for compatibility be *VERY* careful when lowering it. See: rdar://145585141 and https://bugs.webkit.org/show_bug.cgi?id=289330 */ \
    v(Unsigned, maxRegExpStackSize, 128 * MB, Normal, nullptr) \
    \
    v(Double, percentCPUPerMBForFullTimer, 0.0003125, Normal, nullptr) \
    v(Double, percentCPUPerMBForEdenTimer, 0.0025, Normal, nullptr) \
    v(Double, collectionTimerMaxPercentCPU, 0.10, Normal, nullptr) \
    \
    v(Bool, forceWeakRandomSeed, false, Normal, nullptr) \
    v(Unsigned, forcedWeakRandomSeed, 0, Normal, nullptr) \
    \
    v(Bool, alwaysHaveABadTime, false, Normal, "debugging option to test HaveABadTime mode"_s) \
    v(Bool, allowDoubleShape, true, Normal, "debugging option to test disabling use of DoubleShape"_s) \
    v(Bool, useZombieMode, false, Normal, "debugging option to scribble over dead objects with 0xbadbeef0"_s) \
    v(Bool, useImmortalObjects, false, Normal, "debugging option to keep all objects alive forever"_s) \
    v(Bool, sweepSynchronously, false, Normal, "debugging option to sweep all dead objects synchronously at GC end before resuming mutator"_s) \
    v(Unsigned, maxSingleAllocationSize, 0, Configurable, "debugging option to limit individual allocations to a max size (0 = limit not set, N = limit size in bytes)"_s) \
    \
    v(GCLogLevel, logGC, GCLogging::None, Normal, "debugging option to log GC activity (0 = None, 1 = Basic, 2 = Verbose)"_s) \
    v(Bool, useGC, true, Normal, nullptr) \
    v(Bool, useGlobalGC, false, Normal, nullptr) \
    v(Bool, gcAtEnd, false, Normal, "If true, the jsc CLI will do a GC before exiting"_s) \
    v(Bool, forceGCSlowPaths, false, Normal, "If true, we will force all JIT fast allocations down their slow paths."_s) \
    v(Bool, forceDidDeferGCWork, false, Normal, "If true, we will force all DeferGC destructions to perform a GC."_s) \
    v(Unsigned, gcMaxHeapSize, 0, Normal, nullptr) \
    v(Size, forceRAMSize, 0, Normal, nullptr) \
    v(Bool, recordGCPauseTimes, false, Normal, nullptr) \
    v(Bool, dumpHeapStatisticsAtVMDestruction, false, Normal, nullptr) \
    v(Bool, enableStrongRefTracker, false, Normal, "Enable logging of live Strong<*> values. Use alongside $vm.triggerMemoryPressure() and dumpHeapOnLowMemory."_s) \
    v(Bool, dumpHeapOnLowMemory, false, Normal, "Dump a heap dump when the memory handler is triggered. Use alongside $vm.triggerMemoryPressure() and enableStrongRefTracker."_s) \
    v(Bool, forceCodeBlockToJettisonDueToOldAge, false, Normal, "If true, this means that anytime we can jettison a CodeBlock due to old age, we do."_s) \
    v(Bool, useEagerCodeBlockJettisonTiming, false, Normal, "If true, the time slices for jettisoning a CodeBlock due to old age are shrunk significantly."_s) \
    \
    v(Bool, useTypeProfiler, false, Normal, nullptr) \
    v(Bool, useControlFlowProfiler, false, Normal, nullptr) \
    \
    v(Bool, useSamplingProfiler, false, Normal, nullptr) \
    v(Unsigned, sampleInterval, 1000, Normal, "Time between stack traces in microseconds."_s) \
    v(Bool, collectExtraSamplingProfilerData, false, Normal, "This corresponds to the JSC shell's --sample option, or if we're wanting to use the sampling profiler via the Debug menu in the browser."_s) \
    v(Unsigned, samplingProfilerTopFunctionsCount, 12, Normal, "Number of top functions to report when using the command line interface."_s) \
    v(Unsigned, samplingProfilerTopBytecodesCount, 40, Normal, "Number of top bytecodes to report when using the command line interface."_s) \
    v(Bool, samplingProfilerIgnoreExternalSourceID, false, Normal, "Ignore external source ID when aggregating results from sampling profiler"_s) \
    v(OptionString, samplingProfilerPath, nullptr, Normal, "The path to the directory to write sampiling profiler output to. This probably will not work with WK2 unless the path is in the sandbox."_s) \
    v(Bool, sampleCCode, false, Normal, "Causes the sampling profiler to record profiling data for C frames."_s) \
    \
    v(Bool, alwaysGeneratePCToCodeOriginMap, false, Normal, "This will make sure we always generate a PCToCodeOriginMap for JITed code."_s) \
    \
    v(Double, randomIntegrityAuditRate, 0.05, Normal, "Probability of random integrity audits [0.0 - 1.0]"_s) \
    v(Bool, verifyGC, false, Normal, nullptr) \
    v(Bool, verboseVerifyGC, false, Normal, nullptr) \
    v(Bool, verifyHeap, false, Normal, nullptr) \
    v(Unsigned, numberOfGCCyclesToRecordForVerification, 3, Normal, nullptr) \
    \
    v(Unsigned, exceptionStackTraceLimit, 100, Normal, "Stack trace limit for internal Exception object"_s) \
    v(Unsigned, defaultErrorStackTraceLimit, 100, Normal, "The default value for Error.stackTraceLimit"_s) \
    v(Bool, exitOnResourceExhaustion, false, Normal, nullptr) \
    v(Bool, useExceptionFuzz, false, Normal, nullptr) \
    v(Unsigned, fireExceptionFuzzAt, 0, Normal, nullptr) \
    v(Bool, fuzzAtomicJITMemcpy, false, Normal, nullptr) \
    v(Bool, validateDFGExceptionHandling, ASSERT_ENABLED, Normal, "Causes the DFG to emit code validating exception handling for each node that can exit"_s) \
    v(Bool, dumpSimulatedThrows, false, Normal, "Dumps the call stack of the last simulated throw if exception scope verification fails"_s) \
    v(Bool, validateExceptionChecks, false, Normal, "Verifies that needed exception checks are performed."_s) \
    v(Unsigned, unexpectedExceptionStackTraceLimit, 100, Normal, "Stack trace limit for debugging unexpected exceptions observed in the VM"_s) \
    \
    v(Bool, validateDFGClobberize, false, Normal, "Emits code in the DFG/FTL to validate the Clobberize phase"_s) \
    v(Bool, validateBoundsCheckElimination, false, Normal, "Emits code in the DFG/FTL to validate bounds check elimination"_s) \
    v(Bool, validateDFGMayExit, ASSERT_ENABLED, Normal, "Emits code in the DFG/FTL to validate the MayExit phase"_s) \
    \
    v(Bool, validateVMEntryCalleeSaves, false, Configurable, "Causes vmEntryToJavaScript to validate VMEntry callee saves are properly restored"_s) \
    \
    v(Bool, useExecutableAllocationFuzz, false, Normal, nullptr) \
    v(Unsigned, fireExecutableAllocationFuzzAt, 0, Normal, nullptr) \
    v(Unsigned, fireExecutableAllocationFuzzAtOrAfter, 0, Normal, nullptr) \
    v(Bool, fireExecutableAllocationFuzzRandomly, false, Normal, nullptr) \
    v(Double, fireExecutableAllocationFuzzRandomlyProbability, 0.1, Normal, nullptr) \
    v(Bool, verboseExecutableAllocationFuzz, false, Normal, nullptr) \
    v(Bool, zeroExecutableMemoryOnFree, false, Normal, "0 out instructions when freeing JIT memory."_s) \
    \
    v(Bool, useOSRExitFuzz, false, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAtStatic, 0, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAt, 0, Normal, nullptr) \
    v(Unsigned, fireOSRExitFuzzAtOrAfter, 0, Normal, nullptr) \
    v(Bool, verboseOSRExitFuzz, true, Normal, nullptr) \
    \
    /* LOL options */ \
    v(Bool, useLOLJIT, false, Normal, "Use LOL instead of Baseline"_s) \
    v(Bool, verboseLOLAllocation, false, Normal, "Log info about LOL's register allocation state"_s) \
    \
    v(Unsigned, seedOfVMRandomForFuzzer, 0, Normal, "0 means not fuzzing this; use a cryptographically random seed"_s) \
    v(Bool, useRandomizingFuzzerAgent, false, Normal, nullptr) \
    v(Unsigned, seedOfRandomizingFuzzerAgent, 1, Normal, nullptr) \
    v(Bool, dumpFuzzerAgentPredictions, false, Normal, nullptr) \
    v(Bool, useDoublePredictionFuzzerAgent, false, Normal, nullptr) \
    v(Bool, useFileBasedFuzzerAgent, false, Normal, nullptr) \
    v(Bool, usePredictionFileCreatingFuzzerAgent, false, Normal, nullptr) \
    v(Bool, requirePredictionForFileBasedFuzzerAgent, false, Normal, nullptr) \
    v(OptionString, fuzzerPredictionsFile, nullptr, Normal, "file with list of predictions for FileBasedFuzzerAgent"_s) \
    v(Bool, useNarrowingNumberPredictionFuzzerAgent, false, Normal, nullptr) \
    v(Bool, useWideningNumberPredictionFuzzerAgent, false, Normal, nullptr) \
    \
    v(Bool, logPhaseTimes, false, Normal, nullptr) \
    v(Double, rareBlockPenalty, 0.001, Normal, nullptr) \
    v(Bool, airGreedyRegAllocVerbose, false, Normal, nullptr) \
    v(OptionString, airGreedyRegAllocDumpFunction, nullptr, Normal, "dump greedy register allocator state and IR for functions matching this substring"_s) \
    v(Double, airGreedyRegAllocSplitMultiplier, 2.0, Normal, nullptr) \
    v(Bool, airGreedyRegAllocSplitAroundLoops, false, Normal, nullptr) \
    v(Double, airGreedyRegAllocLoopSplitMaxLoopFraction, 0.75, Normal, nullptr) \
    v(Bool, airGreedyRegAllocSpillsEverything, false, Normal, nullptr) \
    v(Bool, airDumpPhaseStats, false, Normal, nullptr) \
    v(Bool, airValidateGreedRegAlloc, ASSERT_ENABLED, Normal, nullptr) \
    v(Bool, airRandomizeRegs, false, Normal, nullptr) \
    v(Unsigned, airRandomizeRegsSeed, 0, Normal, nullptr) \
    v(Bool, coalesceSpillSlots, true, Normal, nullptr) \
    v(Bool, logAirRegisterPressure, false, Normal, nullptr) \
    v(Bool, useB3TailDup, true, Normal, nullptr) \
    v(Unsigned, maxB3TailDupBlockSize, 3, Normal, nullptr) \
    v(Unsigned, maxB3TailDupBlockSuccessors, 3, Normal, nullptr) \
    v(Bool, useB3HoistLoopInvariantValues, true, Normal, nullptr) \
    v(Bool, useB3CanonicalizePrePostIncrements, false, Normal, nullptr) \
    v(Bool, useB3EliminateWasmGCAllocations, true, Normal, "eliminate non-escaping wasm-GC struct allocations in B3"_s) \
    v(Bool, useB3ReduceStrengthFixpoint, false, Normal, "iterate B3 reduceStrength to a fixpoint instead of a single pass (for debugging)"_s) \
    v(Bool, useAirOptimizePairedLoadStore, true, Normal, nullptr) \
    \
    v(Bool, useDollarVM, false, Restricted, "installs the $vm debugging tool in global objects"_s) \
    v(OptionString, functionOverrides, nullptr, Restricted, "file with debugging overrides for function bodies"_s) \
    \
    v(Unsigned, watchdog, 0, Normal, "watchdog timeout (0 = Disabled, N = a timeout period of N milliseconds)"_s) \
    v(Bool, usePollingTraps, false, Normal, "use polling (instead of signalling) VM traps"_s) \
    v(Bool, forceTrapAwareStackChecks, false, Normal, "force trap aware stack checks to be taken for testing"_s) \
    \
    v(Bool, useMachForExceptions, true, Normal, "Use mach exceptions rather than signals to handle faults and pass thread messages. (This does nothing on platforms without mach)"_s) \
    v(Bool, allowNonSPTagging, true, Normal, "allow use of the pacib instruction instead of just pacibsp (This can break lldb/posix signals as it puts live data below SP)"_s) \
    \
    v(Bool, useICStats, false, Normal, nullptr) \
    \
    v(Bool, useFuzzerMode, false, Normal, nullptr) \
    \
    v(Unsigned, prototypeHitCountForLLIntCaching, 2, Normal, "Number of prototype property hits before caching a prototype in the LLInt. A count of 0 means never cache."_s) \
    \
    v(Bool, dumpCompiledRegExpPatterns, false, Normal, nullptr) \
    v(Bool, verboseRegExpCompilation, false, Normal, nullptr) \
    \
    v(Bool, dumpModuleRecord, false, Normal, nullptr) \
    v(Bool, dumpModuleLoadingState, false, Normal, nullptr) \
    v(Bool, exposeInternalModuleLoader, false, Normal, "expose the internal module loader object to the global space for debugging"_s) \
    \
    v(Bool, exposePrivateIdentifiers, false, Normal, "Allow non-builtin scripts to use private identifiers. Mostly useful to expose @superSamplerBegin/End intrinsics for profiling"_s) \
    \
    v(Bool, useSuperSampler, false, Normal, nullptr) \
    \
    v(Bool, useSourceProviderCache, true, Normal, "If false, the parser will not use the source provider cache. It's good to verify everything works when this is false. Because the cache is so successful, it can mask bugs."_s) \
    v(Bool, useCodeCache, true, Normal, "If false, the unlinked byte code cache will not be used."_s) \
    \
    v(Bool, useWasm, canUseWasm(), Normal, "Expose the Wasm global object."_s) \
    \
    v(Bool, failToCompileWasmCode, false, Normal, "If true, no Wasm::Plan will sucessfully compile a function."_s) \
    v(Size, wasmSmallPartialCompileLimit, 5000, Normal, "Limit on the number of bytes a Wasm::Plan::compile should attempt for small wasm binary before checking for other work."_s) \
    v(Size, wasmLargePartialCompileLimit, 20000, Normal, "Limit on the number of bytes a Wasm::Plan::compile should attempt for large wasm binary before checking for other work."_s) \
    v(Unsigned, wasmOMGOptimizationLevel, Options::defaultB3OptLevel(), Normal, "B3 Optimization level for OMG Web Assembly module compilations."_s) \
    v(Bool, useWasmByteLoopReplacement, true, Normal, "If true, OMG replaces a loop that copies or fills linear memory one byte per iteration with the equivalent bulk memory operation."_s) \
    \
    v(Bool, useBBQTierUpChecks, true, Normal, "Enables tier up checks for our BBQ code."_s) \
    v(Bool, useWasmOSR, true, Normal, nullptr) \
    v(Int32, thresholdForBBQOptimizeAfterWarmUp, 150, Normal, "The count before we tier up a function to BBQ."_s) \
    v(Int32, thresholdForBBQOptimizeSoon, 50, Normal, nullptr) \
    v(Int32, thresholdForOMGOptimizeAfterWarmUp, 50000, Normal, "The count before we tier up a function to OMG."_s) \
    v(Int32, thresholdForOMGOptimizeSoon, 500, Normal, nullptr) \
    v(Unsigned, maximumOMGCandidateCost, 100000, Normal, nullptr) \
    v(Int32, omgTierUpCounterIncrementForLoop, 1, Normal, "The amount the tier up counter is incremented on each loop backedge."_s) \
    v(Int32, omgTierUpCounterIncrementForEntry, 15, Normal, "The amount the tier up counter is incremented on each function entry."_s) \
    v(Int32, wasmOMGEntryIncrementSizeReference, 128, Normal, "If non-zero, the BBQ->OMG function-entry tier-up increment is scaled down for functions whose bytecode size is below this reference (work-proportional tier-up): increment = clamp(entryIncrement * size / reference, 1, entryIncrement). 0 disables (flat increment)."_s) \
    v(Bool, useWasmFastMemory, true, Normal, "If true, we will try to use a 32-bit address space with a signal handler to bounds check wasm memory."_s) \
    v(Bool, logWasmMemory, false, Normal, nullptr) \
    v(Unsigned, wasmFastMemoryRedzonePages, 128, Normal, "Wasm fast memories use 4GiB virtual allocations, plus a redzone (counted as multiple of 64KiB Wasm pages) at the end to catch reg+imm accesses which exceed 32-bit, anything beyond the redzone is explicitly bounds-checked"_s) \
    v(Bool, crashIfWasmCantFastMemory, false, Normal, "If true, we will crash if we can't obtain fast memory for wasm."_s) \
    v(Bool, crashOnFailedWasmValidate, false, Normal, "If true, we will crash if we can't validate a wasm module instead of throwing an exception."_s) \
    v(Unsigned, maxNumWasmFastMemories, hasCapacityToUseLargeGigacage() ? 8 : 3, Normal, nullptr) \
    v(Bool, verboseBBQJITAllocation, false, Normal, "Logs extra information about register allocation during BBQ JIT"_s) \
    v(Bool, verboseBBQJITInstructions, false, Normal, "Logs instruction information during BBQ JIT"_s) \
    v(Bool, disableBBQConsts, false, Normal, "Wasm <type>.const instructions in BBQ JIT won't lower to a const BBQ::Value"_s) \
    v(Bool, useBBQJIT, true, Normal, "allows the BBQ JIT to be used if true"_s) \
    v(Bool, useOMGJIT, true, Normal, "allows the OMG JIT to be used if true"_s) \
    v(OptionRange, wasmFunctionIndexRangeToCompile, nullptr, Normal, "wasm function index range to allow compilation on, e.g. 1:100"_s) \
    v(Bool, useEagerWasmModuleHashing, false, Normal, "Unnamed Wasm modules are identified in backtraces through their hash, if available."_s) \
    v(Bool, useArrayAllocationProfiling, true, Normal, "If true, we will use our normal array allocation profiling. If false, the allocation profile will always claim to be undecided."_s) \
    v(Bool, forcePolyProto, false, Normal, "If true, create_this will always create an object with a poly proto structure."_s) \
    v(Bool, forceMiniVMMode, false, Normal, "If true, it will force mini VM mode on."_s) \
    v(Bool, useTracePoints, false, Normal, nullptr) \
    v(Bool, useCompilerSignpost, false, Normal, nullptr) \
    v(Bool, useGCSignpost, false, Normal, nullptr) \
    v(Bool, traceLLIntExecution, false, Configurable, nullptr) \
    v(Bool, traceLLIntSlowPath, false, Configurable, nullptr) \
    v(Bool, traceBaselineJITExecution, false, Normal, nullptr) \
    v(Unsigned, thresholdForGlobalLexicalBindingEpoch, UINT_MAX, Normal, "Threshold for global lexical binding epoch. If the epoch reaches to this value, CodeBlock metadata for scope operations will be revised globally. It needs to be greater than 1."_s) \
    v(OptionString, diskCachePath, nullptr, Restricted, nullptr) \
    v(Bool, forceDiskCache, false, Restricted, nullptr) \
    v(Bool, validateAbstractInterpreterState, false, Restricted, nullptr) \
    v(Double, validateAbstractInterpreterStateProbability, 0.5, Normal, nullptr) \
    v(OptionString, dumpJITMemoryPath, nullptr, Restricted, nullptr) \
    v(Double, dumpJITMemoryFlushInterval, 10, Restricted, "Maximum time in between flushes of the JIT memory dump in seconds."_s) \
    v(Bool, useUnlinkedCodeBlockJettisoning, false, Normal, "If true, UnlinkedCodeBlock can be jettisoned."_s) \
    v(Bool, forceOSRExitToLLInt, false, Normal, "If true, we always exit to the LLInt. If false, we exit to whatever is most convenient."_s) \
    v(Unsigned, getByValICMaxNumberOfIdentifiers, 4, Normal, "Number of identifiers we see in the LLInt that could cause us to bail on generating an IC for get_by_val."_s) \
    v(Bool, useRandomizingExecutableIslandAllocation, false, Normal, "For the arm64 ExecutableAllocator, if true, select which region to use randomly. This is useful for testing that jump islands work."_s) \
    v(Bool, exposeProfilersOnGlobalObject, false, Normal, "If true, we will expose functions to enable/disable both the sampling profiler and the super sampler"_s) \
    v(Bool, allowUnsupportedTiers, false, Normal, "If true, we will not disable DFG or FTL when an experimental feature is enabled."_s) \
    v(Bool, returnEarlyFromInfiniteLoopsForFuzzing, false, Normal, nullptr) \
    v(Size, earlyReturnFromInfiniteLoopsLimit, 1300000000, Normal, "When returnEarlyFromInfiniteLoopsForFuzzing is true, this determines the number of executions a loop can run for before just returning. This is helpful for the fuzzer so it doesn't get stuck in infinite loops."_s) \
    v(Bool, useLICMFuzzing, false, Normal, nullptr) \
    v(Unsigned, seedForLICMFuzzer, 424242, Normal, nullptr) \
    v(Double, allowHoistingLICMProbability, 0.5, Normal, nullptr) \
    v(Bool, exposeCustomSettersOnGlobalObjectForTesting, false, Normal, nullptr) \
    v(Bool, useJITCage, canUseJITCage(), Normal, nullptr) \
    v(Bool, useAllocationProfiling, false, Normal, "Allows toggling of bmalloc/libPAS allocation profiling features at JSC launch."_s) \
    v(Unsigned, allocationProfilingMode, 0, Normal, "Allows custom arguments to be passed to bmalloc/libPAS allocation profiling features at JSC launch."_s) \
    v(Bool, dumpBaselineJITSizeStatistics, false, Normal, nullptr) \
    v(Bool, dumpDFGJITSizeStatistics, false, Normal, nullptr) \
    v(Bool, useLoopUnrolling, true, Normal, nullptr) \
    v(Bool, usePartialLoopUnrolling, true, Normal, nullptr) \
    v(Bool, verboseLoopUnrolling, false, Normal, nullptr) \
    v(Bool, disallowLoopUnrollingForNonInnermost, true, Normal, nullptr) \
    v(Unsigned, maxLoopUnrollingCount, 5, Normal, nullptr) \
    v(Unsigned, maxLoopUnrollingBodyNodeSize, 200, Normal, nullptr) \
    v(Unsigned, maxLoopUnrollingIterationCount, 4, Normal, nullptr) \
    v(Unsigned, maxPartialLoopUnrollingBodyNodeSize, 70, Normal, nullptr) \
    v(Unsigned, maxPartialLoopUnrollingIterationCount, 4, Normal, nullptr) \
    v(Unsigned, maxNumericHotLoopSize, 225, Normal, nullptr) \
    v(Unsigned, maxIntegerRangeOptimizationRelationshipsPerNode, 24, Normal, "How many relationships IRO keeps about any one node, 0 for no cap."_s) \
    v(Unsigned, maxIntegerRangeOptimizationWork, 50000000, Normal, "Give up threshold for IRO"_s) \
    v(Bool, printEachUnrolledLoop, false, Normal, nullptr) \
    v(Bool, verboseExecutablePoolAllocation, false, Normal, nullptr) \
    v(Bool, useHandlerICInFTL, false, Normal, nullptr) \
    v(Bool, useLLIntICs, true, Normal, "Use property and call ICs in LLInt code."_s) \
    v(Bool, useBaselineJITCodeSharing, jitEnabledByDefault(), Normal, nullptr) \
    v(Bool, libpasScavengeContinuously, false, Normal, nullptr) \
    v(Unsigned, libpasForcePGMWithRate, 0, Normal, "Forces on probablistic guard malloc and guards allocations with a rate 1/N (0 is disabled)"_s) \
    v(Bool, useWasmFaultSignalHandler, true, Normal, nullptr) \
    v(Bool, dumpUnlinkedDFGValidation, false, Normal, nullptr) \
    v(Bool, dumpWasmOpcodeStatistics, false, Normal, nullptr) \
    v(Bool, dumpWasmWarnings, false, Normal, nullptr) \
    v(Bool, useRecursiveJSONParse, true, Normal, nullptr) \
    v(Unsigned, thresholdForStringReplaceCache, 0x1000, Normal, nullptr) \
    v(Bool, useWasmIPInt, ipintEnabledByDefault(), Normal, "Use the in-place interpereter for WASM instead of LLInt."_s) \
    v(Bool, useWasmIPIntPrologueOSR, true, Normal, "Allow IPInt to tier up during function prologues"_s) \
    v(Bool, useWasmIPIntLoopOSR, true, Normal, "Allow IPInt to tier up during loop iterations"_s) \
    v(Bool, useWasmIPIntEpilogueOSR, true, Normal, "Allow IPInt to tier up during function epilogues"_s) \
    v(Bool, useWasmIPIntSIMD, true, Normal, "Allow IPInt to interpret SIMD code"_s) \
    v(Bool, traceWasmIPIntExecution, false, Normal, nullptr) \
    v(Bool, forceAllFunctionsToUseSIMD, false, Normal, "Force all functions to act conservatively w.r.t fp/vector registers for testing."_s) \
    v(Bool, useOMGInlining, true, Normal, "Use OMG inlining"_s) \
    v(Bool, freeRetiredWasmCode, true, Normal, "free BBQ/OMG-OSR wasm code once it's no longer reachable."_s) \
    v(Bool, useArrayAllocationSinking, true, Normal, nullptr) \
    v(Bool, dumpFTLCodeSize, false, Normal, nullptr) \
    v(Bool, dumpOptimizationTracing, false, Normal, nullptr) \
    v(Bool, dumpIonGraph, false, Normal, nullptr) \
    v(OptionString, ionGraphDirectory, nullptr, Normal, "Directory to place IonGraph"_s) \
    v(Unsigned, markedBlockDumpInfoCount, 0, Normal, nullptr) /* FIXME: rdar://139998916 */ \
    \
    /* Feature Flags */\
    \
    /* Feature-flag options whose source of truth is UnifiedWebPreferences.yaml. */ \
    FOR_EACH_JSC_WEB_PREFERENCE_OPTION(v) \
    /* Restricted so some app doesn't set this environment variable and start using it. */ \
    v(Bool, disallowMixedWasmExceptions, true, Restricted, "Disallow using both legacy and modern (try_table) wasm exception specs in the same module."_s) \
    /* Not sourced from UnifiedWebPreferences.yaml: force-enabled via the cross-origin-isolation path and consumed in WebCore. */ \
    v(Bool, useSharedArrayBuffer, false, Normal, nullptr) \
    /* Not sourced from UnifiedWebPreferences.yaml: shares its semantics with the WebCore-bound TrustedTypes feature. */ \
    v(Bool, useTrustedTypes, true, Normal, "Enable trusted types eval protection feature."_s) \



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
    v(dumpGraphAtEachDFGFTLPhase, dumpDFGFTLGraphAtEachPhase, SameOption) \
    v(dumpGraphAtEachDFGPhase, dumpDFGGraphAtEachPhase, SameOption) \
    v(dumpGraphAtEachB3Phase, dumpB3GraphAtEachPhase, SameOption) \
    v(dumpGraphAtEachAirPhase, dumpAirGraphAtEachPhase, SameOption) \
    v(alwaysDoFullCollection, useGenerationalGC, InvertedOption) \
    v(enableOSREntryToDFG, useOSREntryToDFG, SameOption) \
    v(enableOSREntryToFTL, useOSREntryToFTL, SameOption) \
    v(enableAccessInlining, useAccessInlining, SameOption) \
    v(enablePolyvariantDevirtualization, usePolyvariantDevirtualization, SameOption) \
    v(enablePolymorphicAccessInlining, usePolymorphicAccessInlining, SameOption) \
    v(enablePolymorphicCallInlining, usePolymorphicCallInlining, SameOption) \
    v(enableObjectAllocationSinking, useObjectAllocationSinking, SameOption) \
    v(enableConcurrentJIT, useConcurrentJIT, SameOption) \
    v(enableProfiler, useProfiler, SameOption) \
    v(enableArchitectureSpecificOptimizations, useArchitectureSpecificOptimizations, SameOption) \
    v(objectsAreImmortal, useImmortalObjects, SameOption) \
    v(disableGC, useGC, InvertedOption) \
    v(enableTypeProfiler, useTypeProfiler, SameOption) \
    v(enableControlFlowProfiler, useControlFlowProfiler, SameOption) \
    v(enableExceptionFuzz, useExceptionFuzz, SameOption) \
    v(enableExecutableAllocationFuzz, useExecutableAllocationFuzz, SameOption) \
    v(enableOSRExitFuzz, useOSRExitFuzz, SameOption) \
    v(enableDollarVM, useDollarVM, SameOption) \
    v(maximumOptimizationCandidateInstructionCount, maximumOptimizationCandidateBytecodeCost, SameOption) \
    v(maximumFTLCandidateInstructionCount, maximumFTLCandidateBytecodeCost, SameOption) \
    v(maximumInliningCallerSize, maximumInliningCallerBytecodeCost, SameOption) \
    v(validateBCE, validateBoundsCheckElimination, SameOption) \


enum ExperimentalOptionFlags {
    LLIntAndBaselineOnly = 0,
    SupportsDFG = 1 << 0,
    SupportsFTL = 1 << 1,
};

#define FOR_EACH_JSC_EXPERIMENTAL_OPTION(v) \

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
    OptionRange() = default;
    OptionRange(std::nullptr_t) { }

    bool init(const char*);
    bool NODELETE isInRange(unsigned) const;
    const char* rangeString() const { return (m_state > InitError) ? m_rangeString : s_nullRangeStr; }

    void dump(PrintStream& out) const;

private:
    JS_EXPORT_PRIVATE static const char* const s_nullRangeStr;

    RangeState m_state { Uninitialized };
    const char* m_rangeString { nullptr };
    unsigned m_lowLimit { 0 };
    unsigned m_highLimit { 0 };
};

enum class OSLogType : uint8_t {
    None,
    // These corresponds to OS_LOG_TYPE_xxx.
    Default,
    Info,
    Debug,
    Error,
    Fault
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
    using OSLogType = JSC::OSLogType;

    bool allowUnfinalizedAccess;
    bool isFinalized;

#define DECLARE_OPTION(type_, name_, defaultValue_, availability_, description_) \
    type_ name_;
FOR_EACH_JSC_OPTION(DECLARE_OPTION)
#undef DECLARE_OPTION
};

// Options::Metadata's offsetOfOption and offsetOfOptionDefault relies on this.
static_assert(sizeof(OptionsStorage) <= 16 * KB);

} // namespace JSC
