/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include "JSCConfig.h"
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

class Options {
public:
    enum class DumpLevel {
        None = 0,
        Overridden,
        All,
        Verbose
    };

    enum class Availability {
        Normal = 0,
        Restricted,
        Configurable
    };

#define DECLARE_OPTION_ID(type_, name_, defaultValue_, availability_, description_) \
    name_##ID,

    enum ID {
        FOR_EACH_JSC_OPTION(DECLARE_OPTION_ID)
        numberOfOptions
    };
#undef DECLARE_OPTION_ID

    enum class Type {
        Bool,
        Unsigned,
        Double,
        Int32,
        Size,
        OptionRange,
        OptionString,
        GCLogLevel,
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

#define DECLARE_OPTION_ACCESSORS(type_, name_, defaultValue_, availability_, description_) \
    ALWAYS_INLINE static OptionEntry::type_& name_() { return g_jscConfig.options[name_##ID].val##type_; }  \
    ALWAYS_INLINE static OptionEntry::type_& name_##Default() { return g_jscConfig.defaultOptions[name_##ID].val##type_; }

    FOR_EACH_JSC_OPTION(DECLARE_OPTION_ACCESSORS)
#undef DECLARE_OPTION_ACCESSORS

    static bool isAvailable(ID, Availability);

private:

    // For storing constant meta data about each option:
    struct EntryInfo {
        const char* name;
        const char* description;
        Type type;
        Availability availability;
    };

    Options();

    enum DumpDefaultsOption {
        DontDumpDefaults,
        DumpDefaults
    };
    static void dumpOptionsIfNeeded();
    static void dumpAllOptions(StringBuilder&, DumpLevel, const char* title,
        const char* separator, const char* optionHeader, const char* optionFooter, DumpDefaultsOption);
    static void dumpOption(StringBuilder&, DumpLevel, ID,
        const char* optionHeader, const char* optionFooter, DumpDefaultsOption);

    static bool setOptionWithoutAlias(const char* arg);
    static bool setAliasedOption(const char* arg);
    static bool overrideAliasedOptionWithHeuristic(const char* name);

    static const EntryInfo s_optionsInfo[numberOfOptions];

    friend class Option;
};

class Option {
public:
    Option(Options::ID id)
        : m_id(id)
        , m_entry(g_jscConfig.options[m_id])
    {
    }
    
    void dump(StringBuilder&) const;

    bool operator==(const Option& other) const;
    bool operator!=(const Option& other) const { return !(*this == other); }
    
    Options::ID id() const { return m_id; }
    const char* name() const;
    const char* description() const;
    Options::Type type() const;
    Options::Availability availability() const;
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
    Option(Options::ID id, OptionEntry& entry)
        : m_id(id)
        , m_entry(entry)
    {
    }
    
    Options::ID m_id;
    OptionEntry& m_entry;
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

inline Options::Availability Option::availability() const
{
    return Options::s_optionsInfo[m_id].availability;
}

inline bool Option::isOverridden() const
{
    return *this != defaultOption();
}

inline const Option Option::defaultOption() const
{
    return Option(m_id, g_jscConfig.defaultOptions[m_id]);
}

inline bool& Option::boolVal()
{
    return m_entry.valBool;
}

inline unsigned& Option::unsignedVal()
{
    return m_entry.valUnsigned;
}

inline double& Option::doubleVal()
{
    return m_entry.valDouble;
}

inline int32_t& Option::int32Val()
{
    return m_entry.valInt32;
}

inline OptionRange Option::optionRangeVal()
{
    return m_entry.valOptionRange;
}

inline const char* Option::optionStringVal()
{
    return m_entry.valOptionString;
}

inline GCLogging::Level& Option::gcLogLevelVal()
{
    return m_entry.valGCLogLevel;
}

} // namespace JSC
