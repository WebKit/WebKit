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

#include <algorithm>
#include <limits>
#include <math.h>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <wtf/ASCIICType.h>
#include <wtf/Compiler.h>
#include <wtf/DataLog.h>
#include <wtf/NumberOfCores.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <crt_externs.h>
#endif

#if OS(WINDOWS)
#include "MacroAssemblerX86.h"
#endif

#define USE_OPTIONS_FILE 0
#define OPTIONS_FILENAME "/tmp/jsc.options"

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
    if (!strlen(string))
        string = nullptr;
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

bool Options::overrideAliasedOptionWithHeuristic(const char* name)
{
    const char* stringValue = getenv(name);
    if (!stringValue)
        return false;

    String aliasedOption;
    aliasedOption = String(&name[4]) + "=" + stringValue;
    if (Options::setOption(aliasedOption.utf8().data()))
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
    return computeNumberOfWorkerThreads(maxNumberOfGCMarkers);
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
#if !defined(NDEBUG)
    Options::validateDFGExceptionHandling() = true;
#endif
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
    Options::useConcurrentJIT() = false;
#endif
#if !ENABLE(DFG_JIT)
    Options::useDFGJIT() = false;
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(FTL_JIT)
    Options::useFTLJIT() = false;
#endif
#if !ENABLE(SEPARATED_WX_HEAP)
    Options::useSeparatedWXHeap() = false;
#endif
#if OS(WINDOWS) && CPU(X86) 
    // Disable JIT on Windows if SSE2 is not present 
    if (!MacroAssemblerX86::supportsFloatingPoint())
        Options::useJIT() = false;
#endif
    if (Options::dumpDisassembly()
        || Options::dumpDFGDisassembly()
        || Options::dumpFTLDisassembly()
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
        Options::useConcurrentJIT() = false;
    }
    if (Options::useMaximalFlushInsertionPhase()) {
        Options::useOSREntryToDFG() = false;
        Options::useOSREntryToFTL() = false;
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
#define FOR_EACH_OPTION(type_, name_, defaultValue_, description_)      \
            overrideOptionWithHeuristic(name_(), "JSC_" #name_);
            JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION
#endif // PLATFORM(COCOA)

#define FOR_EACH_OPTION(aliasedName_, unaliasedName_, equivalence_) \
            overrideAliasedOptionWithHeuristic("JSC_" #aliasedName_);
            JSC_ALIASED_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

#if 0
                ; // Deconfuse editors that do auto indentation
#endif
    
            recomputeDependentOptions();

#if USE(OPTIONS_FILE)
            {
                const char* filename = OPTIONS_FILENAME;
                FILE* optionsFile = fopen(filename, "r");
                if (!optionsFile) {
                    dataLogF("Failed to open file %s. Did you add the file-read-data entitlement to WebProcess.sb?\n", filename);
                    return;
                }
                
                StringBuilder builder;
                char* line;
                char buffer[BUFSIZ];
                while ((line = fgets(buffer, sizeof(buffer), optionsFile)))
                    builder.append(buffer);
                
                const char* optionsStr = builder.toString().utf8().data();
                dataLogF("Setting options: %s\n", optionsStr);
                setOptions(optionsStr);
                
                int result = fclose(optionsFile);
                if (result)
                    dataLogF("Failed to close file %s: %s\n", filename, strerror(errno));
            }
#endif

            // Do range checks where needed and make corrections to the options:
            ASSERT(Options::thresholdForOptimizeAfterLongWarmUp() >= Options::thresholdForOptimizeAfterWarmUp());
            ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= Options::thresholdForOptimizeSoon());
            ASSERT(Options::thresholdForOptimizeAfterWarmUp() >= 0);

            dumpOptionsIfNeeded();
            ensureOptionsAreCoherent();
        });
}

void Options::dumpOptionsIfNeeded()
{
    if (Options::dumpOptions()) {
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
}

bool Options::setOptions(const char* optionsStr)
{
    Vector<char*> options;

    size_t length = strlen(optionsStr);
    char* optionsStrCopy = WTF::fastStrDup(optionsStr);
    char* end = optionsStrCopy + length;
    char* p = optionsStrCopy;

    while (p < end) {
        // Skip white space.
        while (p < end && isASCIISpace(*p))
            p++;
        if (p == end)
            break;

        char* optionStart = p;
        p = strstr(p, "=");
        if (!p) {
            dataLogF("'=' not found in option string: %p\n", optionStart);
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
                return false; // End of string not found.
            }
            hasStringValue = true;
        }

        // Find next white space.
        while (p < end && !isASCIISpace(*p))
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

    dumpOptionsIfNeeded();
    return success;
}

// Parses a single command line option in the format "<optionName>=<value>"
// (no spaces allowed) and set the specified option if appropriate.
bool Options::setOptionWithoutAlias(const char* arg)
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
    if (strlen(#name_) == static_cast<size_t>(equalStr - arg)      \
        && !strncmp(arg, #name_, equalStr - arg)) {                \
        type_ value;                                               \
        value = (defaultValue_);                                   \
        bool success = parse(valueStr, value);                     \
        if (success) {                                             \
            name_() = value;                                       \
            recomputeDependentOptions();                           \
            return true;                                           \
        }                                                          \
        return false;                                              \
    }

    JSC_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

    return false; // No option matched.
}

static bool invertBoolOptionValue(const char* valueStr, const char*& invertedValueStr)
{
    bool boolValue;
    if (!parse(valueStr, boolValue))
        return false;
    invertedValueStr = boolValue ? "false" : "true";
    return true;
}


bool Options::setAliasedOption(const char* arg)
{
    // arg should look like this:
    //   <jscOptionName>=<appropriate value>
    const char* equalStr = strchr(arg, '=');
    if (!equalStr)
        return false;

#if COMPILER(CLANG)
#if __has_warning("-Wtautological-compare")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
#endif

    // For each option, check if the specify arg is a match. If so, set the arg
    // if the value makes sense. Otherwise, move on to checking the next option.
#define FOR_EACH_OPTION(aliasedName_, unaliasedName_, equivalence) \
    if (strlen(#aliasedName_) == static_cast<size_t>(equalStr - arg)    \
        && !strncmp(arg, #aliasedName_, equalStr - arg)) {              \
        String unaliasedOption(#unaliasedName_);                        \
        if (equivalence == SameOption)                                  \
            unaliasedOption = unaliasedOption + equalStr;               \
        else {                                                          \
            ASSERT(equivalence == InvertedOption);                      \
            const char* invertedValueStr = nullptr;                     \
            if (!invertBoolOptionValue(equalStr + 1, invertedValueStr)) \
                return false;                                           \
            unaliasedOption = unaliasedOption + "=" + invertedValueStr; \
        }                                                               \
        return setOptionWithoutAlias(unaliasedOption.utf8().data());   \
    }

    JSC_ALIASED_OPTIONS(FOR_EACH_OPTION)
#undef FOR_EACH_OPTION

#if COMPILER(CLANG)
#if __has_warning("-Wtautological-compare")
#pragma clang diagnostic pop
#endif
#endif

    return false; // No option matched.
}

bool Options::setOption(const char* arg)
{
    bool success = setOptionWithoutAlias(arg);
    if (success)
        return true;
    return setAliasedOption(arg);
}


void Options::dumpAllOptions(StringBuilder& builder, DumpLevel level, const char* title,
    const char* separator, const char* optionHeader, const char* optionFooter, DumpDefaultsOption dumpDefaultsOption)
{
    if (title) {
        builder.append(title);
        builder.append('\n');
    }

    for (int id = 0; id < numberOfOptions; id++) {
        if (separator && id)
            builder.append(separator);
        dumpOption(builder, level, static_cast<OptionID>(id), optionHeader, optionFooter, dumpDefaultsOption);
    }
}

void Options::dumpAllOptionsInALine(StringBuilder& builder)
{
    dumpAllOptions(builder, DumpLevel::All, nullptr, " ", nullptr, nullptr, DontDumpDefaults);
}

void Options::dumpAllOptions(FILE* stream, DumpLevel level, const char* title)
{
    StringBuilder builder;
    dumpAllOptions(builder, level, title, nullptr, "   ", "\n", DumpDefaults);
    fprintf(stream, "%s", builder.toString().utf8().data());
}

void Options::dumpOption(StringBuilder& builder, DumpLevel level, OptionID id,
    const char* header, const char* footer, DumpDefaultsOption dumpDefaultsOption)
{
    if (id >= numberOfOptions)
        return; // Illegal option.

    Option option(id);
    bool wasOverridden = option.isOverridden();
    bool needsDescription = (level == DumpLevel::Verbose && option.description());

    if (level == DumpLevel::Overridden && !wasOverridden)
        return;

    if (header)
        builder.append(header);
    builder.append(option.name());
    builder.append('=');
    option.dump(builder);

    if (wasOverridden && (dumpDefaultsOption == DumpDefaults)) {
        builder.append(" (default: ");
        option.defaultOption().dump(builder);
        builder.append(")");
    }

    if (needsDescription) {
        builder.append("   ... ");
        builder.append(option.description());
    }

    builder.append(footer);
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

void Option::dump(StringBuilder& builder) const
{
    switch (type()) {
    case Options::Type::boolType:
        builder.append(m_entry.boolVal ? "true" : "false");
        break;
    case Options::Type::unsignedType:
        builder.appendNumber(m_entry.unsignedVal);
        break;
    case Options::Type::doubleType:
        builder.appendNumber(m_entry.doubleVal);
        break;
    case Options::Type::int32Type:
        builder.appendNumber(m_entry.int32Val);
        break;
    case Options::Type::optionRangeType:
        builder.append(m_entry.optionRangeVal.rangeString());
        break;
    case Options::Type::optionStringType: {
        const char* option = m_entry.optionStringVal;
        if (!option)
            option = "";
        builder.append('"');
        builder.append(option);
        builder.append('"');
        break;
    }
    case Options::Type::gcLogLevelType: {
        builder.append(GCLogging::levelAsString(m_entry.gcLogLevelVal));
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
        return (m_entry.doubleVal == other.m_entry.doubleVal) || (std::isnan(m_entry.doubleVal) && std::isnan(other.m_entry.doubleVal));
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

