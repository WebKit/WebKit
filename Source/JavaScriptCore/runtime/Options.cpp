/*
 * Copyright (C) 2011-2012, 2014-2015 Apple Inc. All rights reserved.
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

#include "HeapStatistics.h"
#include <algorithm>
#include <limits>
#include <math.h>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <wtf/DataLog.h>
#include <wtf/NumberOfCores.h>
#include <wtf/PageBlock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>

#if OS(DARWIN) && ENABLE(PARALLEL_GC)
#include <sys/sysctl.h>
#endif

#if OS(WINDOWS)
#include "MacroAssemblerX86.h"
#endif

namespace JSC {

static bool parse(const char* string, bool& value)
{
    if (!strcasecmp(string, "true") || !strcasecmp(string, "yes") || !strcmp(string, "1")) {
        value = true;
        return true;
    }
    if (!strcasecmp(string, "false") || !strcasecmp(string, "no") || !strcmp(string, "0")) {
        value = false;
        return true;
    }
    return false;
}

static bool parse(const char* string, int32_t& value)
{
    return sscanf(string, "%d", &value) == 1;
}

static bool parse(const char* string, unsigned& value)
{
    return sscanf(string, "%u", &value) == 1;
}

static bool parse(const char* string, double& value)
{
    return sscanf(string, "%lf", &value) == 1;
}

static bool parse(const char* string, OptionRange& value)
{
    return value.init(string);
}

static bool parse(const char* string, const char*& value)
{
    value = string;
    return true;
}

static bool parse(const char* string, GCLogging::Level& value)
{
    if (!strcasecmp(string, "none") || !strcasecmp(string, "no") || !strcasecmp(string, "false") || !strcmp(string, "0")) {
        value = GCLogging::None;
        return true;
    }

    if (!strcasecmp(string, "basic") || !strcasecmp(string, "yes") || !strcasecmp(string, "true") || !strcmp(string, "1")) {
        value = GCLogging::Basic;
        return true;
    }

    if (!strcasecmp(string, "verbose") || !strcmp(string, "2")) {
        value = GCLogging::Verbose;
        return true;
    }

    return false;
}

template<typename T>
bool overrideOptionWithHeuristic(T& variable, const char* name)
{
    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;
    
    if (parse(stringValue, variable))
        return true;
    
    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    return false;
}

static unsigned computeNumberOfWorkerThreads(int maxNumberOfWorkerThreads, int minimum = 1)
{
    int cpusToUse = std::min(WTF::numberOfProcessorCores(), maxNumberOfWorkerThreads);

    // Be paranoid, it is the OS we're dealing with, after all.
    ASSERT(cpusToUse >= 1);
    return std::max(cpusToUse, minimum);
}

static int32_t computePriorityDeltaOfWorkerThreads(int32_t twoCorePriorityDelta, int32_t multiCorePriorityDelta)
{
    if (WTF::numberOfProcessorCores() <= 2)
        return twoCorePriorityDelta;

    return multiCorePriorityDelta;
}

static unsigned computeNumberOfGCMarkers(unsigned maxNumberOfGCMarkers)
{
#if ENABLE(PARALLEL_GC)
    return computeNumberOfWorkerThreads(maxNumberOfGCMarkers);
#else
    UNUSED_PARAM(maxNumberOfGCMarkers);
    return 1;
#endif
}

bool OptionRange::init(const char* rangeString)
{
    // rangeString should be in the form of [!]<low>[:<high>]
    // where low and high are unsigned

    bool invert = false;

    if (m_state > Uninitialized)
        return true;

    if (!rangeString) {
        m_state = InitError;
        return false;
    }

    m_rangeString = rangeString;

    if (*rangeString == '!') {
        invert = true;
        rangeString++;
    }

    int scanResult = sscanf(rangeString, " %u:%u", &m_lowLimit, &m_highLimit);

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

Options::Entry Options::s_options[Options::numberOfOptions];
Options::Entry Options::s_defaultOptions[Options::numberOfOptions];

// Realize the names for each of the options:
const Options::EntryInfo Options::s_optionsInfo[Options::numberOfOptions] = {
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_) \
    { #name_, description_, Options::Type::type_##Type },
    JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION
};

static void scaleJITPolicy()
{
    auto& scaleFactor = Options::jitPolicyScale();
    if (scaleFactor > 1.0)
        scaleFactor = 1.0;
    else if (scaleFactor < 0.0)
        scaleFactor = 0.0;

    struct OptionToScale {
        Options::OptionID id;
        int32_t minVal;
    };

    static const OptionToScale optionsToScale[] = {
        { Options::thresholdForJITAfterWarmUpID, 0 },
        { Options::thresholdForJITSoonID, 0 },
        { Options::thresholdForOptimizeAfterWarmUpID, 1 },
        { Options::thresholdForOptimizeAfterLongWarmUpID, 1 },
        { Options::thresholdForOptimizeSoonID, 1 },
        { Options::thresholdForFTLOptimizeSoonID, 2 },
        { Options::thresholdForFTLOptimizeAfterWarmUpID, 2 }
    };

    const int numberOfOptionsToScale = sizeof(optionsToScale) / sizeof(OptionToScale);
    for (int i = 0; i < numberOfOptionsToScale; i++) {
        Option option(optionsToScale[i].id);
        ASSERT(option.type() == Options::Type::int32Type);
        option.int32Val() *= scaleFactor;
        option.int32Val() = std::max(option.int32Val(), optionsToScale[i].minVal);
    }
}

static void recomputeDependentOptions()
{
#if !ENABLE(JIT)
    Options::useLLInt() = true;
    Options::useJIT() = false;
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(YARR_JIT)
    Options::useRegExpJIT() = false;
#endif
#if !ENABLE(CONCURRENT_JIT)
    Options::enableConcurrentJIT() = false;
#endif
#if !ENABLE(DFG_JIT)
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(FTL_JIT)
    Options::useFTLJIT() = false;
#endif
#if OS(WINDOWS) && CPU(X86) 
    // Disable JIT on Windows if SSE2 is not present 
    if (!MacroAssemblerX86::supportsFloatingPoint())
        Options::useJIT() = false;
#endif
    if (Options::showDisassembly()
        || Options::showDFGDisassembly()
        || Options::showFTLDisassembly()
        || Options::dumpBytecodeAtDFGTime()
        || Options::dumpGraphAtEachPhase()
        || Options::verboseCompilation()
        || Options::verboseFTLCompilation()
        || Options::logCompilationChanges()
        || Options::validateGraph()
        || Options::validateGraphAtEachPhase()
        || Options::verboseOSR()
        || Options::verboseCompilationQueue()
        || Options::reportCompileTimes()
        || Options::reportFTLCompileTimes()
        || Options::verboseCFA()
        || Options::verboseFTLFailure())
        Options::alwaysComputeHash() = true;

    if (Option(Options::jitPolicyScaleID).isOverridden())
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
        Options::enableConcurrentJIT() = false;
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
}

void Options::initialize()
{
    static std::once_flag initializeOptionsOnceFlag;
    
    std::call_once(
        initializeOptionsOnceFlag,
        [] {
            // Initialize each of the options with their default values:
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_)      \
            name_() = defaultValue_;                                    \
            name_##Default() = defaultValue_;
            JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION
    
                // It *probably* makes sense for other platforms to enable this.
#if PLATFORM(IOS) && CPU(ARM64)
                enableLLVMFastISel() = true;
#endif
        
            // Allow environment vars to override options if applicable.
            // The evn var should be the name of the option prefixed with
            // "JSC_".
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_)      \
            overrideOptionWithHeuristic(name_(), "JSC_" #name_);
            JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

#if 0
                ; // Deconfuse editors that do auto indentation
#endif
    
            recomputeDependentOptions();

            // Do range checks where needed and make corrections to the options:
            ASSERT(Options::thresholdForOptimizeAfterLongWarmUp() >= Options::thresholdForOptimizeAfterWarmUp());
            ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= Options::thresholdForOptimizeSoon());
            ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= 0);

            if (Options::showOptions()) {
                DumpLevel level = static_cast<DumpLevel>(Options::showOptions());
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
                dumpAllOptions(level, title);
            }

            ensureOptionsAreCoherent();
        });
}

// Parses a single command line option in the format "<optionName>=<value>"
// (no spaces allowed) and set the specified option if appropriate.
bool Options::setOption(const char* arg)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

    const char* valueStr = equalStr + 1;

    // For each option, check if the specify arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_) \
    if (!strncmp(arg, #name_, equalStr - arg)) {        \
        type_ value;                                    \
        value = (defaultValue_);                        \
        bool success = parse(valueStr, value);          \
        if (success) {                                  \
            name_() = value;                            \
            recomputeDependentOptions();                \
            return true;                                \
        }                                               \
        return false;                                   \
    }

    JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

    return false; // No option matched.
}

void Options::dumpAllOptions(DumpLevel level, const char* title, FILE* stream)
{
    if (title)
        fprintf(stream, "%s\n", title);
    for (int id = 0; id < numberOfOptions; id++)
        dumpOption(level, static_cast<OptionID>(id), stream, "   ", "\n");
}

void Options::dumpOption(DumpLevel level, OptionID id, FILE* stream, const char* header, const char* footer)
{
    if (id >= numberOfOptions)
        return; // Illegal option.

    Option option(id);
    bool wasOverridden = option.isOverridden();
    bool needsDescription = (level == DumpLevel::Verbose && option.description());

    if (level == DumpLevel::Overridden && !wasOverridden)
        return;

    fprintf(stream, "%s%s: ", header, option.name());
    option.dump(stream);

    if (wasOverridden) {
        fprintf(stream, " (default: ");
        option.defaultOption().dump(stream);
        fprintf(stream, ")");
    }

    if (needsDescription)
        fprintf(stream, "   ... %s", option.description());

    fprintf(stream, "%s", footer);
}

void Options::ensureOptionsAreCoherent()
{
    bool coherent = true;
    if (!(useLLInt() || useJIT())) {
        coherent = false;
        dataLog("INCOHERENT OPTIONS: at least one of useLLInt or useJIT must be true\n");
    }
    if (!coherent)
        CRASH();
}

void Option::dump(FILE* stream) const
{
    switch (type()) {
    case Options::Type::boolType:
        fprintf(stream, "%s", m_entry.boolVal ? "true" : "false");
        break;
    case Options::Type::unsignedType:
        fprintf(stream, "%u", m_entry.unsignedVal);
        break;
    case Options::Type::doubleType:
        fprintf(stream, "%lf", m_entry.doubleVal);
        break;
    case Options::Type::int32Type:
        fprintf(stream, "%d", m_entry.int32Val);
        break;
    case Options::Type::optionRangeType:
        fprintf(stream, "%s", m_entry.optionRangeVal.rangeString());
        break;
    case Options::Type::optionStringType: {
        const char* option = m_entry.optionStringVal;
        if (!option)
            option = "";
        fprintf(stream, "%s", option);
        break;
    }
    case Options::Type::gcLogLevelType: {
        fprintf(stream, "%s", GCLogging::levelAsString(m_entry.gcLogLevelVal));
        break;
    }
    }
}

bool Option::operator==(const Option& other) const
{
    switch (type()) {
    case Options::Type::boolType:
        return m_entry.boolVal == other.m_entry.boolVal;
    case Options::Type::unsignedType:
        return m_entry.unsignedVal == other.m_entry.unsignedVal;
    case Options::Type::doubleType:
        return (m_entry.doubleVal == other.m_entry.doubleVal) || (isnan(m_entry.doubleVal) && isnan(other.m_entry.doubleVal));
    case Options::Type::int32Type:
        return m_entry.int32Val == other.m_entry.int32Val;
    case Options::Type::optionRangeType:
        return m_entry.optionRangeVal.rangeString() == other.m_entry.optionRangeVal.rangeString();
    case Options::Type::optionStringType:
        return (m_entry.optionStringVal == other.m_entry.optionStringVal)
            || (m_entry.optionStringVal && other.m_entry.optionStringVal && !strcmp(m_entry.optionStringVal, other.m_entry.optionStringVal));
    case Options::Type::gcLogLevelType:
        return m_entry.gcLogLevelVal == other.m_entry.gcLogLevelVal;
    }
    return false;
}

} // namespace JSC

