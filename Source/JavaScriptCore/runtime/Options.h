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
    enum class DumpLevel : uint8_t {
        None = 0,
        Overridden,
        All,
        Verbose
    };

    enum class Availability : uint8_t {
        Normal = 0,
        Restricted,
        Configurable
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

// FIXME: This is a workaround for MSVC's inability to handle large sources.
// See https://developercommunity.visualstudio.com/content/problem/653301/fatal-error-c1002-compiler-is-out-of-heap-space-in.html
#if COMPILER(MSVC)
#define OPTION_ACCESSOR_LINKAGE JS_EXPORT_PRIVATE
#else
#define OPTION_ACCESSOR_LINKAGE ALWAYS_INLINE
#endif

#define DECLARE_OPTION_ACCESSORS(type_, name_, defaultValue_, availability_, description_) \
    OPTION_ACCESSOR_LINKAGE static OptionTypes::type_& name_(); \
    OPTION_ACCESSOR_LINKAGE static OptionTypes::type_& name_##Default();
    FOR_EACH_JSC_OPTION(DECLARE_OPTION_ACCESSORS)
#undef DECLARE_OPTION_ACCESSORS

    static bool isAvailable(OptionID, Availability);

private:

    // For storing constant meta data about each option:
    struct EntryInfo {
        const char* name;
        const char* description;
        OptionTypeID type;
        Availability availability;
        unsigned typeSpecificIndex;
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

    static const EntryInfo s_optionsInfo[NumberOfOptions];

    friend class Option;
};

class Option {
public:
    Option(OptionID);

    void dump(StringBuilder&) const;

    bool operator==(const Option& other) const;
    bool operator!=(const Option& other) const { return !(*this == other); }
    
    OptionID id() const { return m_id; }
    const char* name() const { return Options::s_optionsInfo[idIndex()].name; }
    const char* description() const { return Options::s_optionsInfo[idIndex()].description; }
    OptionTypeID type() const { return Options::s_optionsInfo[idIndex()].type; }
    Options::Availability availability() const { return Options::s_optionsInfo[idIndex()].availability; }
    bool isOverridden() const { return *this != defaultOption(); }
    const Option defaultOption() const;

#define DECLARE_ACCESSOR(OptionType_, type_) \
    type_ val##OptionType_() const { return m_val##OptionType_; }
    FOR_EACH_JSC_OPTION_TYPE(DECLARE_ACCESSOR)
#undef DECLARE_ACCESSOR

private:
    Option() { }

    size_t idIndex() const { return static_cast<size_t>(m_id); }

    OptionID m_id;
    union {

#define DECLARE_MEMBER(OptionType_, type_) \
        type_ m_val##OptionType_;
    FOR_EACH_JSC_OPTION_TYPE(DECLARE_MEMBER)
#undef DECLARE_MEMBER

    };
};

// FIXME: This is a workaround for MSVC's inability to handle large sources.
// Once the MSVC bug has been fixed, we can make the following unconditional and
// remove its counterpart MSVC version in Options.cpp.
// See https://developercommunity.visualstudio.com/content/problem/653301/fatal-error-c1002-compiler-is-out-of-heap-space-in.html
#if !COMPILER(MSVC)

template<OptionTypeID type, OptionID id>
constexpr size_t optionTypeSpecificIndex()
{
    size_t index = 0;
#define COUNT_INDEX_AND_FIND_MATCH(type_, name_, defaultValue_, availability_, description_) \
    if (id == OptionID::name_) \
        return index; \
    if (type == OptionTypeID::type_) \
        index++;

    FOR_EACH_JSC_OPTION(COUNT_INDEX_AND_FIND_MATCH);
#undef COUNT_INDEX_AND_FIND_MATCH
    return InvalidOptionIndex;
}

#define DEFINE_OPTION_ACCESSORS(type_, name_, defaultValue_, availability_, description_) \
    ALWAYS_INLINE OptionTypes::type_& Options::name_() \
    { \
        return g_jscConfig.type##type_##Options[optionTypeSpecificIndex<OptionTypeID::type_, OptionID::name_>()]; \
    }  \
    ALWAYS_INLINE OptionTypes::type_& Options::name_##Default() \
    { \
        return g_jscConfig.type##type_##DefaultOptions[optionTypeSpecificIndex<OptionTypeID::type_, OptionID::name_>()]; \
    }
    FOR_EACH_JSC_OPTION(DEFINE_OPTION_ACCESSORS)
#undef DEFINE_OPTION_ACCESSORS

#endif // !COMPILER(MSVC)

} // namespace JSC
