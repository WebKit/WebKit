/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
#include "Options.h"

#include "AssemblerCommon.h"
#include "CPU.h"
#include "JITOperationValidation.h"
#include "LLIntCommon.h"
#include "MacroAssembler.h"
#include "MinimumReservedZoneSize.h"
#include <algorithm>
#include <limits>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <wtf/ASCIICType.h>
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/Gigacage.h>
#include <wtf/NumberOfCores.h>
#include <wtf/OSLogPrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TranslatedProcess.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/threads/Signals.h>

#if PLATFORM(COCOA)
#include <crt_externs.h>
#endif

#if ENABLE(JIT_CAGE)
#include <machine/cpu_capabilities.h>
#include <wtf/cocoa/Entitlements.h>
#endif

namespace JSC {

bool useOSLogOptionHasChanged = false;

template<typename T>
std::optional<T> parse(const char* string);

template<>
std::optional<OptionsStorage::Bool> parse(const char* string)
{
    if (equalLettersIgnoringASCIICase(string, "true"_s) || equalLettersIgnoringASCIICase(string, "yes"_s) || !strcmp(string, "1"))
        return true;
    if (equalLettersIgnoringASCIICase(string, "false"_s) || equalLettersIgnoringASCIICase(string, "no"_s) || !strcmp(string, "0"))
        return false;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::Int32> parse(const char* string)
{
    int32_t value;
    if (sscanf(string, "%d", &value) == 1)
        return value;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::Unsigned> parse(const char* string)
{
    unsigned value;
    if (sscanf(string, "%u", &value) == 1)
        return value;
    return std::nullopt;
}

#if CPU(ADDRESS64) || OS(DARWIN)
template<>
std::optional<OptionsStorage::Size> parse(const char* string)
{
    size_t value;
    if (sscanf(string, "%zu", &value) == 1)
        return value;
    return std::nullopt;
}
#endif // CPU(ADDRESS64) || OS(DARWIN)

template<>
std::optional<OptionsStorage::Double> parse(const char* string)
{
    double value;
    if (sscanf(string, "%lf", &value) == 1)
        return value;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OptionRange> parse(const char* string)
{
    OptionRange range;
    if (range.init(string))
        return range;
    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OptionString> parse(const char* string)
{
    const char* value = nullptr;
    if (!strlen(string))
        return value;

    // FIXME <https://webkit.org/b/169057>: This could leak if this option is set more than once.
    // Given that Options are typically used for testing, this isn't considered to be a problem.
    value = WTF::fastStrDup(string);
    return value;
}

template<>
std::optional<OptionsStorage::GCLogLevel> parse(const char* string)
{
    if (equalLettersIgnoringASCIICase(string, "none"_s) || equalLettersIgnoringASCIICase(string, "no"_s) || equalLettersIgnoringASCIICase(string, "false"_s) || !strcmp(string, "0"))
        return GCLogging::None;

    if (equalLettersIgnoringASCIICase(string, "basic"_s) || equalLettersIgnoringASCIICase(string, "yes"_s) || equalLettersIgnoringASCIICase(string, "true"_s) || !strcmp(string, "1"))
        return GCLogging::Basic;

    if (equalLettersIgnoringASCIICase(string, "verbose"_s) || !strcmp(string, "2"))
        return GCLogging::Verbose;

    return std::nullopt;
}

template<>
std::optional<OptionsStorage::OSLogType> parse(const char* string)
{
    std::optional<OptionsStorage::OSLogType> result;

    if (equalLettersIgnoringASCIICase(string, "none"_s) || equalLettersIgnoringASCIICase(string, "false"_s) || !strcmp(string, "0"))
        result = OSLogType::None;
    else if (equalLettersIgnoringASCIICase(string, "true"_s) || !strcmp(string, "1"))
        result = OSLogType::Error;
    else if (equalLettersIgnoringASCIICase(string, "default"_s))
        result = OSLogType::Default;
    else if (equalLettersIgnoringASCIICase(string, "info"_s))
        result = OSLogType::Info;
    else if (equalLettersIgnoringASCIICase(string, "debug"_s))
        result = OSLogType::Debug;
    else if (equalLettersIgnoringASCIICase(string, "error"_s))
        result = OSLogType::Error;
    else if (equalLettersIgnoringASCIICase(string, "fault"_s))
        result = OSLogType::Fault;

    if (result && result.value() != Options::useOSLog())
        useOSLogOptionHasChanged = true;
    return result;
}

#if OS(DARWIN)
static os_log_type_t asDarwinOSLogType(OSLogType type)
{
    switch (type) {
    case OSLogType::None:
        RELEASE_ASSERT_NOT_REACHED();
    case OSLogType::Default:
        return OS_LOG_TYPE_DEFAULT;
    case OSLogType::Info:
        return OS_LOG_TYPE_INFO;
    case OSLogType::Debug:
        return OS_LOG_TYPE_DEBUG;
    case OSLogType::Error:
        return OS_LOG_TYPE_ERROR;
    case OSLogType::Fault:
        return OS_LOG_TYPE_FAULT;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return OS_LOG_TYPE_DEFAULT;
}

static void initializeDatafileToUseOSLog()
{
    static bool alreadyInitialized = false;
    RELEASE_ASSERT(!alreadyInitialized);
    WTF::setDataFile(OSLogPrintStream::open("com.apple.JavaScriptCore", "DataLog", asDarwinOSLogType(Options::useOSLog())));
    alreadyInitialized = true;
    // Make sure no one jumped here for nefarious reasons...
    RELEASE_ASSERT(Options::useOSLog() != OSLogType::None);
}
#endif // OS(DARWIN)

static const char* asString(OSLogType type)
{
    switch (type) {
    case OSLogType::None:
        return "none";
    case OSLogType::Default:
        return "default";
    case OSLogType::Info:
        return "info";
    case OSLogType::Debug:
        return "debug";
    case OSLogType::Error:
        return "error";
    case OSLogType::Fault:
        return "fault";
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

bool Options::isAvailable(Options::ID id, Options::Availability availability)
{
    if (availability == Availability::Restricted)
        return g_jscConfig.restrictedOptionsEnabled;
    ASSERT(availability == Availability::Configurable);
    
    UNUSED_PARAM(id);
#if !defined(NDEBUG)
    if (id == maxSingleAllocationSizeID)
        return true;
#endif
#if OS(DARWIN)
    if (id == useSigillCrashAnalyzerID)
        return true;
#endif
#if ENABLE(ASSEMBLER) && OS(LINUX)
    if (id == logJITCodeForPerfID)
        return true;
#endif
    if (id == traceLLIntExecutionID)
        return !!LLINT_TRACING;
    if (id == traceLLIntSlowPathID)
        return !!LLINT_TRACING;
    return false;
}

#if !PLATFORM(COCOA)

template<typename T>
bool overrideOptionWithHeuristic(T& variable, Options::ID id, const char* name, Options::Availability availability)
{
    bool available = (availability == Options::Availability::Normal)
        || Options::isAvailable(id, availability);

    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;
    
    if (available) {
        std::optional<T> value = parse<T>(stringValue);
        if (value) {
            variable = value.value();
            return true;
        }
    }
    
    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    return false;
}

bool Options::overrideAliasedOptionWithHeuristic(const char* name)
{
    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;

    auto aliasedOption = makeString(&name[4], '=', stringValue);
    if (Options::setOption(aliasedOption.utf8().data()))
        return true;

    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    return false;
}

#endif // !PLATFORM(COCOA)

static unsigned computeNumberOfWorkerThreads(int maxNumberOfWorkerThreads, int minimum = 1)
{
    int cpusToUse = std::min(kernTCSMAwareNumberOfProcessorCores(), maxNumberOfWorkerThreads);

    // Be paranoid, it is the OS we're dealing with, after all.
    ASSERT(cpusToUse >= 1);
    return std::max(cpusToUse, minimum);
}

static int32_t computePriorityDeltaOfWorkerThreads(int32_t twoCorePriorityDelta, int32_t multiCorePriorityDelta)
{
    if (kernTCSMAwareNumberOfProcessorCores() <= 2)
        return twoCorePriorityDelta;

    return multiCorePriorityDelta;
}

static constexpr bool jitEnabledByDefault()
{
    return is32Bit() || isAddress64Bit();
}

static unsigned computeNumberOfGCMarkers(unsigned maxNumberOfGCMarkers)
{
    return computeNumberOfWorkerThreads(maxNumberOfGCMarkers);
}

static bool defaultTCSMValue()
{
    return true;
}

const char* const OptionRange::s_nullRangeStr = "<null>";

bool OptionRange::init(const char* rangeString)
{
    // rangeString should be in the form of [!]<low>[:<high>]
    // where low and high are unsigned

    bool invert = false;

    if (!rangeString) {
        m_state = InitError;
        return false;
    }

    if (!strcmp(rangeString, s_nullRangeStr)) {
        m_state = Uninitialized;
        return true;
    }
    
    const char* p = rangeString;

    if (*p == '!') {
        invert = true;
        p++;
    }

    int scanResult = sscanf(p, " %u:%u", &m_lowLimit, &m_highLimit);

    if (!scanResult || scanResult == EOF) {
        m_state = InitError;
        return false;
    }

    if (scanResult == 1)
        m_highLimit = m_lowLimit;

    if (m_lowLimit > m_highLimit) {
        m_state = InitError;
        return false;
    }

    // FIXME <https://webkit.org/b/169057>: This could leak if this particular option is set more than once.
    // Given that these options are used for testing, this isn't considered to be problem.
    m_rangeString = WTF::fastStrDup(rangeString);
    m_state = invert ? Inverted : Normal;

    return true;
}

bool OptionRange::isInRange(unsigned count)
{
    if (m_state < Normal)
        return true;

    if ((m_lowLimit <= count) && (count <= m_highLimit))
        return m_state == Normal ? true : false;

    return m_state == Normal ? false : true;
}

void OptionRange::dump(PrintStream& out) const
{
    out.print(m_rangeString);
}

// Realize the names for each of the options:
const Options::ConstMetaData Options::s_constMetaData[NumberOfOptions] = {
#define FILL_OPTION_INFO(type_, name_, defaultValue_, availability_, description_) \
    { #name_, description_, Options::Type::type_, Availability::availability_, offsetof(OptionsStorage, name_), offsetof(OptionsStorage, name_##Default) },
    FOR_EACH_JSC_OPTION(FILL_OPTION_INFO)
#undef FILL_OPTION_INFO
};

static void scaleJITPolicy()
{
    auto& scaleFactor = Options::jitPolicyScale();
    if (scaleFactor > 1.0)
        scaleFactor = 1.0;
    else if (scaleFactor < 0.0)
        scaleFactor = 0.0;

    auto scaleOption = [&] (int32_t& optionValue, int32_t minValue) {
        optionValue *= scaleFactor;
        optionValue = std::max(optionValue, minValue);
    };

    scaleOption(Options::thresholdForJITAfterWarmUp(), 0);
    scaleOption(Options::thresholdForJITSoon(), 0);
    scaleOption(Options::thresholdForOptimizeAfterWarmUp(), 1);
    scaleOption(Options::thresholdForOptimizeAfterLongWarmUp(), 1);
    scaleOption(Options::thresholdForOptimizeSoon(), 1);
    scaleOption(Options::thresholdForFTLOptimizeSoon(), 2);
    scaleOption(Options::thresholdForFTLOptimizeAfterWarmUp(), 2);
}

static void overrideDefaults()
{
#if !PLATFORM(IOS_FAMILY)
    if (WTF::numberOfProcessorCores() < 4)
#endif
    {
        Options::maximumMutatorUtilization() = 0.6;
        Options::concurrentGCMaxHeadroom() = 1.4;
        Options::minimumGCPauseMS() = 1;
        Options::useStochasticMutatorScheduler() = false;
        if (WTF::numberOfProcessorCores() <= 1)
            Options::gcIncrementScale() = 1;
        else
            Options::gcIncrementScale() = 0;
    }

#if PLATFORM(MAC) && CPU(ARM64)
    Options::numberOfGCMarkers() = 3;
    Options::numberOfDFGCompilerThreads() = 3;
    Options::numberOfFTLCompilerThreads() = 3;
#endif

#if OS(LINUX) && CPU(ARM)
    Options::maximumFunctionForCallInlineCandidateBytecodeCost() = 77;
    Options::maximumOptimizationCandidateBytecodeCost() = 42403;
    Options::maximumFunctionForClosureCallInlineCandidateBytecodeCost() = 68;
    Options::maximumInliningCallerBytecodeCost() = 9912;
    Options::maximumInliningDepth() = 8;
    Options::maximumInliningRecursion() = 3;
#endif

#if USE(BMALLOC_MEMORY_FOOTPRINT_API)
    // On iOS and conditionally Linux, we control heap growth using process memory footprint. Therefore these values can be agressive.
    Options::smallHeapRAMFraction() = 0.8;
    Options::mediumHeapRAMFraction() = 0.9;
#endif

#if ENABLE(SIGILL_CRASH_ANALYZER)
    Options::useSigillCrashAnalyzer() = true;
#endif

#if !ENABLE(SIGNAL_BASED_VM_TRAPS)
    Options::usePollingTraps() = true;
#endif

#if !ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    Options::useWebAssemblyFastMemory() = false;
    Options::useWasmFaultSignalHandler() = false;
#endif

#if !HAVE(MACH_EXCEPTIONS)
    Options::useMachForExceptions() = false;
#endif

    if (Options::useWasmLLInt() && !Options::wasmLLIntTiersUpToBBQ()) {
        Options::thresholdForOMGOptimizeAfterWarmUp() = 1500;
        Options::thresholdForOMGOptimizeSoon() = 100;
    }
}

static inline void disableAllJITOptions()
{
    Options::useLLInt() = true;
    Options::useJIT() = false;
    Options::useBaselineJIT() = false;
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
    Options::useBBQJIT() = false;
    Options::useOMGJIT() = false;
    Options::useDOMJIT() = false;
    Options::useRegExpJIT() = false;
    Options::useJITCage() = false;
    Options::useConcurrentJIT() = false;
    Options::useSigillCrashAnalyzer() = false;

    Options::useWebAssembly() = false;

    Options::usePollingTraps() = true;
    Options::useLLInt() = true;

    Options::dumpDisassembly() = false;
    Options::asyncDisassembly() = false;
    Options::dumpDFGDisassembly() = false;
    Options::dumpFTLDisassembly() = false;
    Options::dumpRegExpDisassembly() = false;
    Options::dumpWasmDisassembly() = false;
    Options::dumpBBQDisassembly() = false;
    Options::dumpOMGDisassembly() = false;
    Options::needDisassemblySupport() = false;
}

inline void Options::dumpOptionsIfNeeded()
{
    if (LIKELY(!Options::dumpOptions()))
        return;

    DumpLevel level = static_cast<DumpLevel>(Options::dumpOptions());
    if (level > DumpLevel::Verbose)
        level = DumpLevel::Verbose;

    const char* title = nullptr;
    switch (level) {
    case DumpLevel::None:
        break;
    case DumpLevel::Overridden:
        title = "Overridden JSC options:";
        break;
    case DumpLevel::All:
        title = "All JSC options:";
        break;
    case DumpLevel::Verbose:
        title = "All JSC options with descriptions:";
        break;
    }

    StringBuilder builder;
    dumpAllOptions(builder, level, title, nullptr, "   ", "\n", DumpDefaults);
    dataLog(builder.toString());
}

void Options::notifyOptionsChanged()
{
    AllowUnfinalizedAccessScope scope;

    unsigned thresholdForGlobalLexicalBindingEpoch = Options::thresholdForGlobalLexicalBindingEpoch();
    if (thresholdForGlobalLexicalBindingEpoch == 0 || thresholdForGlobalLexicalBindingEpoch == 1)
        Options::thresholdForGlobalLexicalBindingEpoch() = UINT_MAX;

#if !defined(NDEBUG)
    Options::validateDFGExceptionHandling() = true;
#endif
#if !ENABLE(JIT)
    Options::useJIT() = false;
#endif
#if !ENABLE(CONCURRENT_JS)
    Options::useConcurrentJIT() = false;
#endif
#if !ENABLE(YARR_JIT)
    Options::useRegExpJIT() = false;
#endif
#if !ENABLE(DFG_JIT)
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(FTL_JIT)
    Options::useFTLJIT() = false;
#endif

#if CPU(RISCV64)
    // On RISCV64, JIT levels are enabled at build-time to simplify building JSC, avoiding
    // otherwise rare combinations of build-time configuration. FTL on RISCV64 is disabled
    // at runtime for now, until it gets int a proper working state.
    // https://webkit.org/b/239707
    Options::useFTLJIT() = false;
#endif
    
#if !CPU(X86_64) && !CPU(ARM64)
    Options::useConcurrentGC() = false;
    Options::forceUnlinkedDFG() = false;
    Options::useWebAssemblySIMD() = false;
#endif

    if (!Options::allowDoubleShape())
        Options::useJIT() = false; // We don't support JIT with !allowDoubleShape. So disable it.

    // At initialization time, we may decide that useJIT should be false for any
    // number of reasons (including failing to allocate JIT memory), and therefore,
    // will / should not be able to enable any JIT related services.
    if (!Options::useJIT())
        disableAllJITOptions();
    else {
        if (WTF::isX86BinaryRunningOnARM()) {
            Options::useBaselineJIT() = false;
            Options::useDFGJIT() = false;
            Options::useFTLJIT() = false;
        }

        if (Options::dumpDisassembly()
            || Options::asyncDisassembly()
            || Options::dumpDFGDisassembly()
            || Options::dumpFTLDisassembly()
            || Options::dumpRegExpDisassembly()
            || Options::dumpWasmDisassembly()
            || Options::dumpBBQDisassembly()
            || Options::dumpOMGDisassembly())
            Options::needDisassemblySupport() = true;

        if (Options::logJIT()
            || Options::needDisassemblySupport()
            || Options::dumpBytecodeAtDFGTime()
            || Options::dumpGraphAtEachPhase()
            || Options::dumpDFGGraphAtEachPhase()
            || Options::dumpDFGFTLGraphAtEachPhase()
            || Options::dumpB3GraphAtEachPhase()
            || Options::dumpAirGraphAtEachPhase()
            || Options::verboseCompilation()
            || Options::verboseFTLCompilation()
            || Options::logCompilationChanges()
            || Options::validateGraph()
            || Options::validateGraphAtEachPhase()
            || Options::verboseOSR()
            || Options::verboseCompilationQueue()
            || Options::reportCompileTimes()
            || Options::reportBaselineCompileTimes()
            || Options::reportDFGCompileTimes()
            || Options::reportFTLCompileTimes()
            || Options::logPhaseTimes()
            || Options::verboseCFA()
            || Options::verboseDFGFailure()
            || Options::verboseFTLFailure())
            Options::alwaysComputeHash() = true;

        if (Options::jitPolicyScale() != Options::jitPolicyScaleDefault())
            scaleJITPolicy();

        if (Options::forceEagerCompilation()) {
            Options::thresholdForJITAfterWarmUp() = 10;
            Options::thresholdForJITSoon() = 10;
            Options::thresholdForOptimizeAfterWarmUp() = 20;
            Options::thresholdForOptimizeAfterLongWarmUp() = 20;
            Options::thresholdForOptimizeSoon() = 20;
            Options::thresholdForFTLOptimizeAfterWarmUp() = 20;
            Options::thresholdForFTLOptimizeSoon() = 20;
            Options::maximumEvalCacheableSourceLength() = 150000;
            Options::useConcurrentJIT() = false;
        }

        // Compute the maximum value of the reoptimization retry counter. This is simply
        // the largest value at which we don't overflow the execute counter, when using it
        // to left-shift the execution counter by this amount. Currently the value ends
        // up being 18, so this loop is not so terrible; it probably takes up ~100 cycles
        // total on a 32-bit processor.
        Options::reoptimizationRetryCounterMax() = 0;
        while ((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << (Options::reoptimizationRetryCounterMax() + 1)) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
            Options::reoptimizationRetryCounterMax()++;

        ASSERT((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << Options::reoptimizationRetryCounterMax()) > 0);
        ASSERT((static_cast<int64_t>(Options::thresholdForOptimizeAfterLongWarmUp()) << Options::reoptimizationRetryCounterMax()) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()));

        if (!Options::useBBQJIT() && Options::useOMGJIT())
            Options::wasmLLIntTiersUpToBBQ() = false;

#if CPU(X86_64)
        if (!MacroAssembler::supportsAVX())
            Options::useWebAssemblySIMD() = false;
#endif

        if (Options::forceAllFunctionsToUseSIMD() && !Options::useWebAssemblySIMD())
            Options::forceAllFunctionsToUseSIMD() = false;

        if (Options::useWebAssemblySIMD() && !Options::useWasmLLInt()) {
            // The LLInt is responsible for discovering if functions use SIMD.
            // If we can't run using it, then we should be conservative.
            Options::forceAllFunctionsToUseSIMD() = true;
        }
    }

    if (Options::dumpFuzzerAgentPredictions())
        Options::alwaysComputeHash() = true;

    if (!Options::useConcurrentGC())
        Options::collectContinuously() = false;

    if (Options::useProfiler())
        Options::useConcurrentJIT() = false;

    if (Options::alwaysUseShadowChicken())
        Options::maximumInliningDepth() = 1;
        
#if !defined(NDEBUG)
    if (Options::maxSingleAllocationSize())
        fastSetMaxSingleAllocationSize(Options::maxSingleAllocationSize());
    else
        fastSetMaxSingleAllocationSize(std::numeric_limits<size_t>::max());
#endif

    if (Options::useZombieMode()) {
        Options::sweepSynchronously() = true;
        Options::scribbleFreeCells() = true;
    }

    if (Options::reservedZoneSize() < minimumReservedZoneSize)
        Options::reservedZoneSize() = minimumReservedZoneSize;
    if (Options::softReservedZoneSize() < Options::reservedZoneSize() + minimumReservedZoneSize)
        Options::softReservedZoneSize() = Options::reservedZoneSize() + minimumReservedZoneSize;

    if (!Options::useCodeCache())
        Options::diskCachePath() = nullptr;

    if (Options::randomIntegrityAuditRate() < 0)
        Options::randomIntegrityAuditRate() = 0;
    else if (Options::randomIntegrityAuditRate() > 1.0)
        Options::randomIntegrityAuditRate() = 1.0;

    if (!Options::allowUnsupportedTiers()) {
#define DISABLE_TIERS(option, flags, ...) do { \
            if (!Options::option())            \
                break;                         \
            if (!(flags & SupportsDFG))        \
                Options::useDFGJIT() = false;  \
            if (!(flags & SupportsFTL))        \
                Options::useFTLJIT() = false;  \
        } while (false);

        FOR_EACH_JSC_EXPERIMENTAL_OPTION(DISABLE_TIERS);
    }

#if OS(DARWIN)
    if (useOSLogOptionHasChanged) {
        initializeDatafileToUseOSLog();
        useOSLogOptionHasChanged = false;
    }
#endif

    if (Options::verboseVerifyGC())
        Options::verifyGC() = true;

#if ASAN_ENABLED && OS(LINUX) && ENABLE(WEBASSEMBLY_SIGNALING_MEMORY)
    if (Options::useWasmFaultSignalHandler()) {
        const char* asanOptions = getenv("ASAN_OPTIONS");
        bool okToUseWebAssemblyFastMemory = asanOptions
            && (strstr(asanOptions, "allow_user_segv_handler=1") || strstr(asanOptions, "handle_segv=0"));
        if (!okToUseWebAssemblyFastMemory) {
            dataLogLn("WARNING: ASAN interferes with JSC signal handlers; useWebAssemblyFastMemory and useWasmFaultSignalHandler will be disabled.");
            Options::useWasmFaultSignalHandler() = false;
        }
    }
#endif

    if (!Options::useWasmFaultSignalHandler())
        Options::useWebAssemblyFastMemory() = false;

    // Do range checks where needed and make corrections to the options:
    ASSERT(Options::thresholdForOptimizeAfterLongWarmUp() >= Options::thresholdForOptimizeAfterWarmUp());
    ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= 0);
    ASSERT(Options::criticalGCMemoryThreshold() > 0.0 && Options::criticalGCMemoryThreshold() < 1.0);

    // The following should only be done at the end after all options
    // have been initialized.
    dumpOptionsIfNeeded();
    assertOptionsAreCoherent();
}

inline void* Options::addressOfOption(Options::ID id)
{
    auto offset = Options::s_constMetaData[id].offsetOfOption;
    return reinterpret_cast<uint8_t*>(&g_jscConfig.options) + offset;
}

inline void* Options::addressOfOptionDefault(Options::ID id)
{
    auto offset = Options::s_constMetaData[id].offsetOfOptionDefault;
    return reinterpret_cast<uint8_t*>(&g_jscConfig.options) + offset;
}

#if OS(WINDOWS)
// FIXME: Use equalLettersIgnoringASCIICase.
inline bool strncasecmp(const char* str1, const char* str2, size_t n)
{
    return _strnicmp(str1, str2, n);
}
#endif

void Options::initialize()
{
    static std::once_flag initializeOptionsOnceFlag;
    
    std::call_once(
        initializeOptionsOnceFlag,
        [] {
            AllowUnfinalizedAccessScope scope;

            // Sanity check that options address computation is working.
            RELEASE_ASSERT(Options::addressOfOption(useKernTCSMID) ==  &Options::useKernTCSM());
            RELEASE_ASSERT(Options::addressOfOptionDefault(useKernTCSMID) ==  &Options::useKernTCSMDefault());
            RELEASE_ASSERT(Options::addressOfOption(gcMaxHeapSizeID) ==  &Options::gcMaxHeapSize());
            RELEASE_ASSERT(Options::addressOfOptionDefault(gcMaxHeapSizeID) ==  &Options::gcMaxHeapSizeDefault());
            RELEASE_ASSERT(Options::addressOfOption(forceOSRExitToLLIntID) ==  &Options::forceOSRExitToLLInt());
            RELEASE_ASSERT(Options::addressOfOptionDefault(forceOSRExitToLLIntID) ==  &Options::forceOSRExitToLLIntDefault());

#ifndef NDEBUG
            Config::enableRestrictedOptions();
#endif
            // Initialize each of the options with their default values:
#define INIT_OPTION(type_, name_, defaultValue_, availability_, description_) { \
                auto value = defaultValue_; \
                name_() = value; \
                name_##Default() = value; \
            }
            FOR_EACH_JSC_OPTION(INIT_OPTION)
#undef INIT_OPTION

            overrideDefaults();
                
            // Allow environment vars to override options if applicable.
            // The evn var should be the name of the option prefixed with
            // "JSC_".
#if PLATFORM(COCOA)
            bool hasBadOptions = false;
            for (char** envp = *_NSGetEnviron(); *envp; envp++) {
                const char* env = *envp;
                if (!strncmp("JSC_", env, 4)) {
                    if (!Options::setOption(&env[4])) {
                        dataLog("ERROR: invalid option: ", *envp, "\n");
                        hasBadOptions = true;
                    }
                }
            }
            if (hasBadOptions && Options::validateOptions())
                CRASH();
#else // PLATFORM(COCOA)
#define OVERRIDE_OPTION_WITH_HEURISTICS(type_, name_, defaultValue_, availability_, description_) \
            overrideOptionWithHeuristic(name_(), name_##ID, "JSC_" #name_, Availability::availability_);
            FOR_EACH_JSC_OPTION(OVERRIDE_OPTION_WITH_HEURISTICS)
#undef OVERRIDE_OPTION_WITH_HEURISTICS

#define OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS(aliasedName_, unaliasedName_, equivalence_) \
            overrideAliasedOptionWithHeuristic("JSC_" #aliasedName_);
            FOR_EACH_JSC_ALIASED_OPTION(OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS)
#undef OVERRIDE_ALIASED_OPTION_WITH_HEURISTICS

#endif // PLATFORM(COCOA)

#if 0
                ; // Deconfuse editors that do auto indentation
#endif

#if CPU(X86_64) && OS(DARWIN)
            Options::dumpZappedCellCrashData() =
                (hwPhysicalCPUMax() >= 4) && (hwL3CacheSize() >= static_cast<int64_t>(6 * MB));
#endif

            // No more options changes after this point. notifyOptionsChanged() will
            // do sanity checks and fix up options as needed.
            notifyOptionsChanged();

            // The code below acts on options that have been finalized.
            // Do not change any options here.
#if HAVE(MACH_EXCEPTIONS)
            if (Options::useMachForExceptions())
                handleSignalsWithMach();
#endif
    });
}

void Options::finalize()
{
    ASSERT(!g_jscConfig.options.allowUnfinalizedAccess);
    g_jscConfig.options.isFinalized = true;
}

static bool isSeparator(char c)
{
    return isASCIISpace(c) || (c == ',');
}

bool Options::setOptions(const char* optionsStr)
{
    AllowUnfinalizedAccessScope scope;
    RELEASE_ASSERT(!g_jscConfig.isPermanentlyFrozen());
    Vector<char*> options;

    size_t length = strlen(optionsStr);
    char* optionsStrCopy = WTF::fastStrDup(optionsStr);
    char* end = optionsStrCopy + length;
    char* p = optionsStrCopy;

    while (p < end) {
        // Skip separators (white space or commas).
        while (p < end && isSeparator(*p))
            p++;
        if (p == end)
            break;

        char* optionStart = p;
        p = strstr(p, "=");
        if (!p) {
            dataLogF("'=' not found in option string: %p\n", optionStart);
            WTF::fastFree(optionsStrCopy);
            return false;
        }
        p++;

        char* valueBegin = p;
        bool hasStringValue = false;
        const int minStringLength = 2; // The min is an empty string i.e. 2 double quotes.
        if ((p + minStringLength < end) && (*p == '"')) {
            p = strstr(p + 1, "\"");
            if (!p) {
                dataLogF("Missing trailing '\"' in option string: %p\n", optionStart);
                WTF::fastFree(optionsStrCopy);
                return false; // End of string not found.
            }
            hasStringValue = true;
        }

        // Find next separator (white space or commas).
        while (p < end && !isSeparator(*p))
            p++;
        if (!p)
            p = end; // No more " " separator. Hence, this is the last arg.

        // If we have a well-formed string value, strip the quotes.
        if (hasStringValue) {
            char* valueEnd = p;
            ASSERT((*valueBegin == '"') && ((valueEnd - valueBegin) >= minStringLength) && (valueEnd[-1] == '"'));
            memmove(valueBegin, valueBegin + 1, valueEnd - valueBegin - minStringLength);
            valueEnd[-minStringLength] = '\0';
        }

        // Strip leading -- if present.
        if ((p -  optionStart > 2) && optionStart[0] == '-' && optionStart[1] == '-')
            optionStart += 2;

        *p++ = '\0';
        options.append(optionStart);
    }

    bool success = true;
    for (auto& option : options) {
        bool optionSuccess = setOption(option);
        if (!optionSuccess) {
            dataLogF("Failed to set option : %s\n", option);
            success = false;
        }
    }

    notifyOptionsChanged();
    WTF::fastFree(optionsStrCopy);

    return success;
}

// Parses a single command line option in the format "<optionName>=<value>"
// (no spaces allowed) and set the specified option if appropriate.
bool Options::setOptionWithoutAlias(const char* arg, bool verify)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

    const char* valueStr = equalStr + 1;

    // For each option, check if the specified arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define SET_OPTION_IF_MATCH(type_, name_, defaultValue_, availability_, description_) \
    if (strlen(#name_) == static_cast<size_t>(equalStr - arg)      \
        && !strncasecmp(arg, #name_, equalStr - arg)) {            \
        if (Availability::availability_ != Availability::Normal    \
            && !isAvailable(name_##ID, Availability::availability_)) \
            return false;                                          \
        std::optional<OptionsStorage::type_> value;                \
        value = parse<OptionsStorage::type_>(valueStr);            \
        if (value) {                                               \
            name_() = value.value();                               \
            if (verify) notifyOptionsChanged();                    \
            return true;                                           \
        }                                                          \
        return false;                                              \
    }

    FOR_EACH_JSC_OPTION(SET_OPTION_IF_MATCH)
#undef SET_OPTION_IF_MATCH

    return false; // No option matched.
}

static const char* invertBoolOptionValue(const char* valueStr)
{
    std::optional<OptionsStorage::Bool> value = parse<OptionsStorage::Bool>(valueStr);
    if (!value)
        return nullptr;
    return value.value() ? "false" : "true";
}


bool Options::setAliasedOption(const char* arg, bool verify)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

    IGNORE_WARNINGS_BEGIN("tautological-compare")

    // For each option, check if the specify arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define FOR_EACH_OPTION(aliasedName_, unaliasedName_, equivalence) \
    if (strlen(#aliasedName_) == static_cast<size_t>(equalStr - arg)    \
        && !strncasecmp(arg, #aliasedName_, equalStr - arg)) {          \
        auto unaliasedOption = String::fromLatin1(#unaliasedName_);     \
        if (equivalence == SameOption)                                  \
            unaliasedOption = unaliasedOption + equalStr;               \
        else {                                                          \
            ASSERT(equivalence == InvertedOption);                      \
            auto* invertedValueStr = invertBoolOptionValue(equalStr + 1); \
            if (!invertedValueStr)                                      \
                return false;                                           \
            unaliasedOption = unaliasedOption + "=" + invertedValueStr; \
        }                                                               \
        return setOptionWithoutAlias(unaliasedOption.utf8().data(), verify);    \
    }

    FOR_EACH_JSC_ALIASED_OPTION(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

    IGNORE_WARNINGS_END

    return false; // No option matched.
}

bool Options::setOption(const char* arg, bool verify)
{
    AllowUnfinalizedAccessScope scope;
    bool success = setOptionWithoutAlias(arg, verify);
    if (success)
        return true;
    return setAliasedOption(arg, verify);
}


void Options::dumpAllOptions(StringBuilder& builder, DumpLevel level, const char* title,
    const char* separator, const char* optionHeader, const char* optionFooter, DumpDefaultsOption dumpDefaultsOption)
{
    AllowUnfinalizedAccessScope scope;
    if (title) {
        builder.append(title);
        builder.append('\n');
    }

    for (size_t id = 0; id < NumberOfOptions; id++) {
        if (separator && id)
            builder.append(separator);
        dumpOption(builder, level, static_cast<ID>(id), optionHeader, optionFooter, dumpDefaultsOption);
    }
}

void Options::dumpAllOptionsInALine(StringBuilder& builder)
{
    dumpAllOptions(builder, DumpLevel::All, nullptr, " ", nullptr, nullptr, DontDumpDefaults);
}

void Options::dumpAllOptions(DumpLevel level, const char* title)
{
    StringBuilder builder;
    dumpAllOptions(builder, level, title, nullptr, "   ", "\n", DumpDefaults);
    dataLog(builder.toString().utf8().data());
}

struct OptionReader {
    class Option {
    public:
        void dump(StringBuilder&) const;

        bool operator==(const Option& other) const;
        bool operator!=(const Option& other) const { return !(*this == other); }

        const char* name() const { return Options::s_constMetaData[m_id].name; }
        const char* description() const { return Options::s_constMetaData[m_id].description; }
        Options::Type type() const { return Options::s_constMetaData[m_id].type; }
        Options::Availability availability() const { return Options::s_constMetaData[m_id].availability; }
        bool isOverridden() const { return *this != OptionReader::defaultFor(m_id); }

    private:
        Option(Options::ID id, void* addressOfValue)
            : m_id(id)
        {
            initValue(addressOfValue);
        }

        void initValue(void* addressOfValue);

        Options::ID m_id;
        union {
            bool m_bool;
            unsigned m_unsigned;
            double m_double;
            int32_t m_int32;
            size_t m_size;
            OptionRange m_optionRange;
            const char* m_optionString;
            GCLogging::Level m_gcLogLevel;
            OSLogType m_osLogType;
        };

        friend struct OptionReader;
    };

    static const Option optionFor(Options::ID);
    static const Option defaultFor(Options::ID);
};

void Options::dumpOption(StringBuilder& builder, DumpLevel level, Options::ID id,
    const char* header, const char* footer, DumpDefaultsOption dumpDefaultsOption)
{
    RELEASE_ASSERT(static_cast<size_t>(id) < NumberOfOptions);

    auto option = OptionReader::optionFor(id);
    Availability availability = option.availability();
    if (availability != Availability::Normal && !isAvailable(id, availability))
        return;

    bool wasOverridden = option.isOverridden();
    bool needsDescription = (level == DumpLevel::Verbose && option.description());

    if (level == DumpLevel::Overridden && !wasOverridden)
        return;

    if (header)
        builder.append(header);
    builder.append(option.name(), '=');
    option.dump(builder);

    if (wasOverridden && (dumpDefaultsOption == DumpDefaults)) {
        auto defaultOption = OptionReader::defaultFor(id);
        builder.append(" (default: ");
        defaultOption.dump(builder);
        builder.append(")");
    }

    if (needsDescription)
        builder.append("   ... ", option.description());

    builder.append(footer);
}

void Options::assertOptionsAreCoherent()
{
    AllowUnfinalizedAccessScope scope;
    bool coherent = true;
    if (!(useLLInt() || useJIT())) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: at least one of useLLInt or useJIT must be true\n");
    }
    if (useWebAssembly() && !(useWasmLLInt() || useBBQJIT())) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: at least one of useWasmLLInt or useBBQJIT must be true\n");
    }
    if (useProfiler() && useConcurrentJIT()) {
        coherent = false;
        dataLogLn("Bytecode profiler is not concurrent JIT safe.");
    }
    if (!coherent)
        CRASH();
}

const OptionReader::Option OptionReader::optionFor(Options::ID id)
{
    return Option(id, Options::addressOfOption(id));
}

const OptionReader::Option OptionReader::defaultFor(Options::ID id)
{
    return Option(id, Options::addressOfOptionDefault(id));
}

void OptionReader::Option::initValue(void* addressOfValue)
{
    Options::Type type = Options::s_constMetaData[m_id].type;
    switch (type) {
    case Options::Type::Bool:
        memcpy(&m_bool, addressOfValue, sizeof(OptionsStorage::Bool));
        break;
    case Options::Type::Unsigned:
        memcpy(&m_unsigned, addressOfValue, sizeof(OptionsStorage::Unsigned));
        break;
    case Options::Type::Double:
        memcpy(&m_double, addressOfValue, sizeof(OptionsStorage::Double));
        break;
    case Options::Type::Int32:
        memcpy(&m_int32, addressOfValue, sizeof(OptionsStorage::Int32));
        break;
    case Options::Type::Size:
        memcpy(&m_size, addressOfValue, sizeof(OptionsStorage::Size));
        break;
    case Options::Type::OptionRange:
        memcpy(&m_optionRange, addressOfValue, sizeof(OptionsStorage::OptionRange));
        break;
    case Options::Type::OptionString:
        memcpy(&m_optionString, addressOfValue, sizeof(OptionsStorage::OptionString));
        break;
    case Options::Type::GCLogLevel:
        memcpy(&m_gcLogLevel, addressOfValue, sizeof(OptionsStorage::GCLogLevel));
        break;
    case Options::Type::OSLogType:
        memcpy(&m_osLogType, addressOfValue, sizeof(OptionsStorage::OSLogType));
        break;
    }
}

void OptionReader::Option::dump(StringBuilder& builder) const
{
    switch (type()) {
    case Options::Type::Bool:
        builder.append(m_bool ? "true" : "false");
        break;
    case Options::Type::Unsigned:
        builder.append(m_unsigned);
        break;
    case Options::Type::Size:
        builder.append(m_size);
        break;
    case Options::Type::Double:
        builder.append(m_double);
        break;
    case Options::Type::Int32:
        builder.append(m_int32);
        break;
    case Options::Type::OptionRange:
        builder.append(m_optionRange.rangeString());
        break;
    case Options::Type::OptionString:
        builder.append('"', m_optionString ? m_optionString : "", '"');
        break;
    case Options::Type::GCLogLevel:
        builder.append(GCLogging::levelAsString(m_gcLogLevel));
        break;
    case Options::Type::OSLogType:
        builder.append(asString(m_osLogType));
        break;
    }
}

bool OptionReader::Option::operator==(const Option& other) const
{
    ASSERT(type() == other.type());
    switch (type()) {
    case Options::Type::Bool:
        return m_bool == other.m_bool;
    case Options::Type::Unsigned:
        return m_unsigned == other.m_unsigned;
    case Options::Type::Size:
        return m_size == other.m_size;
    case Options::Type::Double:
        return (m_double == other.m_double) || (std::isnan(m_double) && std::isnan(other.m_double));
    case Options::Type::Int32:
        return m_int32 == other.m_int32;
    case Options::Type::OptionRange:
        return m_optionRange.rangeString() == other.m_optionRange.rangeString();
    case Options::Type::OptionString:
        return (m_optionString == other.m_optionString)
            || (m_optionString && other.m_optionString && !strcmp(m_optionString, other.m_optionString));
    case Options::Type::GCLogLevel:
        return m_gcLogLevel == other.m_gcLogLevel;
    case Options::Type::OSLogType:
        return m_osLogType == other.m_osLogType;
    }
    return false;
}

#if ENABLE(JIT_CAGE)
SUPPRESS_ASAN bool canUseJITCage()
{
    if (JSC_FORCE_USE_JIT_CAGE)
        return true;
    return JSC_JIT_CAGE_VERSION() && WTF::processHasEntitlement("com.apple.private.verified-jit"_s);
}
#else
bool canUseJITCage() { return false; }
#endif

bool canUseWebAssemblyFastMemory()
{
    // Gigacage::hasCapacityToUseLargeGigacage is determined based on EFFECTIVE_ADDRESS_WIDTH.
    // If we have enough address range to potentially use a large gigacage,
    // then we have enough address range to useWebAssemblyFastMemory.
    return Gigacage::hasCapacityToUseLargeGigacage;
}

} // namespace JSC
