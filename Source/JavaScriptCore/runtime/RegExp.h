/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include "ConcurrentJSLock.h"
#include "MatchResult.h"
#include "RegExpKey.h"
#include "Structure.h"
#include "Yarr.h"
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

#if ENABLE(YARR_JIT)
#include "YarrJIT.h"
#endif

namespace JSC {

struct RegExpRepresentation;
class VM;

class RegExp final : public JSCell {
    friend class CachedRegExp;

public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;
    static constexpr bool needsDestruction = true;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.regExpSpace;
    }

    JS_EXPORT_PRIVATE static RegExp* create(VM&, const String& pattern, OptionSet<Yarr::Flags>);
    static void destroy(JSCell*);
    static size_t estimatedSize(JSCell*, VM&);
    JS_EXPORT_PRIVATE static void dumpToStream(const JSCell*, PrintStream&);

    bool global() const { return m_flags.contains(Yarr::Flags::Global); }
    bool ignoreCase() const { return m_flags.contains(Yarr::Flags::IgnoreCase); }
    bool multiline() const { return m_flags.contains(Yarr::Flags::Multiline); }
    bool sticky() const { return m_flags.contains(Yarr::Flags::Sticky); }
    bool globalOrSticky() const { return global() || sticky(); }
    bool unicode() const { return m_flags.contains(Yarr::Flags::Unicode); }
    bool dotAll() const { return m_flags.contains(Yarr::Flags::DotAll); }

    const String& pattern() const { return m_patternString; }

    bool isValid() const { return !Yarr::hasError(m_constructionErrorCode); }
    const char* errorMessage() const { return Yarr::errorMessage(m_constructionErrorCode); }
    JSObject* errorToThrow(JSGlobalObject* globalObject) { return Yarr::errorToThrow(globalObject, m_constructionErrorCode); }
    void reset()
    {
        m_state = NotCompiled;
        m_constructionErrorCode = Yarr::ErrorCode::NoError;
    }

    JS_EXPORT_PRIVATE int match(VM&, const String&, unsigned startOffset, Vector<int>& ovector);

    // Returns false if we couldn't run the regular expression for any reason.
    bool matchConcurrently(VM&, const String&, unsigned startOffset, int& position, Vector<int>& ovector);
    
    JS_EXPORT_PRIVATE MatchResult match(VM&, const String&, unsigned startOffset);

    bool matchConcurrently(VM&, const String&, unsigned startOffset, MatchResult&);

    // Call these versions of the match functions if you're desperate for performance.
    template<typename VectorType>
    int matchInline(VM&, const String&, unsigned startOffset, VectorType& ovector);
    MatchResult matchInline(VM&, const String&, unsigned startOffset);
    
    unsigned numSubpatterns() const { return m_numSubpatterns; }

    bool hasNamedCaptures()
    {
        return m_rareData && !m_rareData->m_captureGroupNames.isEmpty();
    }

    String getCaptureGroupName(unsigned i)
    {
        if (!i || !m_rareData || m_rareData->m_captureGroupNames.size() <= i)
            return String();
        ASSERT(m_rareData);
        return m_rareData->m_captureGroupNames[i];
    }

    unsigned subpatternForName(String groupName)
    {
        if (!m_rareData)
            return 0;
        auto it = m_rareData->m_namedGroupToParenIndex.find(groupName);
        if (it == m_rareData->m_namedGroupToParenIndex.end())
            return 0;
        return it->value;
    }

    bool hasCode()
    {
        return m_state == JITCode || m_state == ByteCode;
    }

    bool hasCodeFor(Yarr::YarrCharSize);
    bool hasMatchOnlyCodeFor(Yarr::YarrCharSize);

    void deleteCode();

#if ENABLE(REGEXP_TRACING)
    void printTraceData();
#endif

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
    }

    DECLARE_INFO;

    RegExpKey key() { return RegExpKey(m_flags, m_patternString); }

protected:
    void finishCreation(VM&);

private:
    friend class RegExpCache;
    RegExp(VM&, const String&, OptionSet<Yarr::Flags>);

    static RegExp* createWithoutCaching(VM&, const String&, OptionSet<Yarr::Flags>);

    enum RegExpState : uint8_t {
        ParseError,
        JITCode,
        ByteCode,
        NotCompiled
    };

    void byteCodeCompileIfNecessary(VM*);

    void compile(VM*, Yarr::YarrCharSize);
    void compileIfNecessary(VM&, Yarr::YarrCharSize);

    void compileMatchOnly(VM*, Yarr::YarrCharSize);
    void compileIfNecessaryMatchOnly(VM&, Yarr::YarrCharSize);

#if ENABLE(YARR_JIT_DEBUG)
    void matchCompareWithInterpreter(const String&, int startOffset, int* offsetVector, int jitResult);
#endif

#if ENABLE(YARR_JIT)
    Yarr::YarrCodeBlock& ensureRegExpJITCode()
    {
        if (!m_regExpJITCode)
            m_regExpJITCode = makeUnique<Yarr::YarrCodeBlock>();
        return *m_regExpJITCode.get();
    }
#endif

    struct RareData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Vector<String> m_captureGroupNames;
        HashMap<String, unsigned> m_namedGroupToParenIndex;
    };

    String m_patternString;
    RegExpState m_state { NotCompiled };
    OptionSet<Yarr::Flags> m_flags;
    Yarr::ErrorCode m_constructionErrorCode { Yarr::ErrorCode::NoError };
    unsigned m_numSubpatterns { 0 };
    std::unique_ptr<Yarr::BytecodePattern> m_regExpBytecode;
#if ENABLE(YARR_JIT)
    std::unique_ptr<Yarr::YarrCodeBlock> m_regExpJITCode;
#endif
    std::unique_ptr<RareData> m_rareData;
#if ENABLE(REGEXP_TRACING)
    double m_rtMatchOnlyTotalSubjectStringLen { 0.0 };
    double m_rtMatchTotalSubjectStringLen { 0.0 };
    unsigned m_rtMatchOnlyCallCount { 0 };
    unsigned m_rtMatchOnlyFoundCount { 0 };
    unsigned m_rtMatchCallCount { 0 };
    unsigned m_rtMatchFoundCount { 0 };
#endif
};

} // namespace JSC
