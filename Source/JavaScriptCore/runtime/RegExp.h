/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007, 2008, 2009, 2016 Apple Inc. All rights reserved.
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

JS_EXPORT_PRIVATE RegExpFlags regExpFlags(const String&);

class RegExp final : public JSCell {
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    JS_EXPORT_PRIVATE static RegExp* create(VM&, const String& pattern, RegExpFlags);
    static const bool needsDestruction = true;
    static void destroy(JSCell*);
    static size_t estimatedSize(JSCell*, VM&);
    JS_EXPORT_PRIVATE static void dumpToStream(const JSCell*, PrintStream&);

    bool global() const { return m_flags & FlagGlobal; }
    bool ignoreCase() const { return m_flags & FlagIgnoreCase; }
    bool multiline() const { return m_flags & FlagMultiline; }
    bool sticky() const { return m_flags & FlagSticky; }
    bool globalOrSticky() const { return global() || sticky(); }
    bool unicode() const { return m_flags & FlagUnicode; }
    bool dotAll() const { return m_flags & FlagDotAll; }

    const String& pattern() const { return m_patternString; }

    bool isValid() const { return !Yarr::hasError(m_constructionErrorCode) && m_flags != InvalidFlags; }
    const char* errorMessage() const { return Yarr::errorMessage(m_constructionErrorCode); }
    JSObject* errorToThrow(ExecState* exec) { return Yarr::errorToThrow(exec, m_constructionErrorCode); }

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
        return !m_captureGroupNames.isEmpty();
    }

    String getCaptureGroupName(unsigned i)
    {
        if (!i || m_captureGroupNames.size() <= i)
            return String();
        return m_captureGroupNames[i];
    }

    unsigned subpatternForName(String groupName)
    {
        auto it = m_namedGroupToParenIndex.find(groupName);
        if (it == m_namedGroupToParenIndex.end())
            return 0;
        return it->value;
    }

    bool hasCode()
    {
        return m_state != NotCompiled;
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
    RegExp(VM&, const String&, RegExpFlags);

    static RegExp* createWithoutCaching(VM&, const String&, RegExpFlags);

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

    String m_patternString;
    RegExpState m_state { NotCompiled };
    RegExpFlags m_flags;
    ConcurrentJSLock m_lock;
    Yarr::ErrorCode m_constructionErrorCode { Yarr::ErrorCode::NoError };
    unsigned m_numSubpatterns { 0 };
    Vector<String> m_captureGroupNames;
    HashMap<String, unsigned> m_namedGroupToParenIndex;
    std::unique_ptr<Yarr::BytecodePattern> m_regExpBytecode;
#if ENABLE(REGEXP_TRACING)
    double m_rtMatchOnlyTotalSubjectStringLen { 0.0 };
    double m_rtMatchTotalSubjectStringLen { 0.0 };
    unsigned m_rtMatchOnlyCallCount { 0 };
    unsigned m_rtMatchOnlyFoundCount { 0 };
    unsigned m_rtMatchCallCount { 0 };
    unsigned m_rtMatchFoundCount { 0 };
#endif

#if ENABLE(YARR_JIT)
    Yarr::YarrCodeBlock m_regExpJITCode;
#endif
};

} // namespace JSC
