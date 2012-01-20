/*
 *  Copyright (C) 1999-2001, 2004 Harri Porten (porten@kde.org)
 *  Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *  Copyright (C) 2010 Peter Varga (pvarga@inf.u-szeged.hu), University of Szeged
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

#include "config.h"
#include "RegExp.h"

#include "Lexer.h"
#include "RegExpCache.h"
#include "yarr/Yarr.h"
#include "yarr/YarrJIT.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>


#define REGEXP_FUNC_TEST_DATA_GEN 0

namespace JSC {

const ClassInfo RegExp::s_info = { "RegExp", 0, 0, 0, CREATE_METHOD_TABLE(RegExp) };

RegExpFlags regExpFlags(const UString& string)
{
    RegExpFlags flags = NoFlags;

    for (unsigned i = 0; i < string.length(); ++i) {
        switch (string[i]) {
        case 'g':
            if (flags & FlagGlobal)
                return InvalidFlags;
            flags = static_cast<RegExpFlags>(flags | FlagGlobal);
            break;

        case 'i':
            if (flags & FlagIgnoreCase)
                return InvalidFlags;
            flags = static_cast<RegExpFlags>(flags | FlagIgnoreCase);
            break;

        case 'm':
            if (flags & FlagMultiline)
                return InvalidFlags;
            flags = static_cast<RegExpFlags>(flags | FlagMultiline);
            break;

        default:
            return InvalidFlags;
        }
    }

    return flags;
}

#if REGEXP_FUNC_TEST_DATA_GEN
class RegExpFunctionalTestCollector {
    // This class is not thread safe.
protected:
    static const char* const s_fileName;

public:
    static RegExpFunctionalTestCollector* get();

    ~RegExpFunctionalTestCollector();

    void outputOneTest(RegExp*, UString, int, int*, int);
    void clearRegExp(RegExp* regExp)
    {
        if (regExp == m_lastRegExp)
            m_lastRegExp = 0;
    }

private:
    RegExpFunctionalTestCollector();

    void outputEscapedUString(const UString&, bool escapeSlash = false);

    static RegExpFunctionalTestCollector* s_instance;
    FILE* m_file;
    RegExp* m_lastRegExp;
};

const char* const RegExpFunctionalTestCollector::s_fileName = "/tmp/RegExpTestsData";
RegExpFunctionalTestCollector* RegExpFunctionalTestCollector::s_instance = 0;

RegExpFunctionalTestCollector* RegExpFunctionalTestCollector::get()
{
    if (!s_instance)
        s_instance = new RegExpFunctionalTestCollector();

    return s_instance;
}

void RegExpFunctionalTestCollector::outputOneTest(RegExp* regExp, UString s, int startOffset, int* ovector, int result)
{
    if ((!m_lastRegExp) || (m_lastRegExp != regExp)) {
        m_lastRegExp = regExp;
        fputc('/', m_file);
        outputEscapedUString(regExp->pattern(), true);
        fputc('/', m_file);
        if (regExp->global())
            fputc('g', m_file);
        if (regExp->ignoreCase())
            fputc('i', m_file);
        if (regExp->multiline())
            fputc('m', m_file);
        fprintf(m_file, "\n");
    }

    fprintf(m_file, " \"");
    outputEscapedUString(s);
    fprintf(m_file, "\", %d, %d, (", startOffset, result);
    for (unsigned i = 0; i <= regExp->numSubpatterns(); i++) {
        int subPatternBegin = ovector[i * 2];
        int subPatternEnd = ovector[i * 2 + 1];
        if (subPatternBegin == -1)
            subPatternEnd = -1;
        fprintf(m_file, "%d, %d", subPatternBegin, subPatternEnd);
        if (i < regExp->numSubpatterns())
            fputs(", ", m_file);
    }

    fprintf(m_file, ")\n");
    fflush(m_file);
}

RegExpFunctionalTestCollector::RegExpFunctionalTestCollector()
{
    m_file = fopen(s_fileName, "r+");
    if  (!m_file)
        m_file = fopen(s_fileName, "w+");

    fseek(m_file, 0L, SEEK_END);
}

RegExpFunctionalTestCollector::~RegExpFunctionalTestCollector()
{
    fclose(m_file);
    s_instance = 0;
}

void RegExpFunctionalTestCollector::outputEscapedUString(const UString& s, bool escapeSlash)
{
    int len = s.length();
    
    for (int i = 0; i < len; ++i) {
        UChar c = s[i];

        switch (c) {
        case '\0':
            fputs("\\0", m_file);
            break;
        case '\a':
            fputs("\\a", m_file);
            break;
        case '\b':
            fputs("\\b", m_file);
            break;
        case '\f':
            fputs("\\f", m_file);
            break;
        case '\n':
            fputs("\\n", m_file);
            break;
        case '\r':
            fputs("\\r", m_file);
            break;
        case '\t':
            fputs("\\t", m_file);
            break;
        case '\v':
            fputs("\\v", m_file);
            break;
        case '/':
            if (escapeSlash)
                fputs("\\/", m_file);
            else
                fputs("/", m_file);
            break;
        case '\"':
            fputs("\\\"", m_file);
            break;
        case '\\':
            fputs("\\\\", m_file);
            break;
        case '\?':
            fputs("\?", m_file);
            break;
        default:
            if (c > 0x7f)
                fprintf(m_file, "\\u%04x", c);
            else 
                fputc(c, m_file);
            break;
        }
    }
}
#endif

struct RegExpRepresentation {
#if ENABLE(YARR_JIT)
    Yarr::YarrCodeBlock m_regExpJITCode;
#endif
    OwnPtr<Yarr::BytecodePattern> m_regExpBytecode;
};

RegExp::RegExp(JSGlobalData& globalData, const UString& patternString, RegExpFlags flags)
    : JSCell(globalData, globalData.regExpStructure.get())
    , m_state(NotCompiled)
    , m_patternString(patternString)
    , m_flags(flags)
    , m_constructionError(0)
    , m_numSubpatterns(0)
#if ENABLE(REGEXP_TRACING)
    , m_rtMatchCallCount(0)
    , m_rtMatchFoundCount(0)
#endif
{
}

void RegExp::finishCreation(JSGlobalData& globalData)
{
    Base::finishCreation(globalData);
    Yarr::YarrPattern pattern(m_patternString, ignoreCase(), multiline(), &m_constructionError);
    if (m_constructionError)
        m_state = ParseError;
    else
        m_numSubpatterns = pattern.m_numSubpatterns;
}

void RegExp::destroy(JSCell* cell)
{
    RegExp* thisObject = jsCast<RegExp*>(cell);
#if REGEXP_FUNC_TEST_DATA_GEN
    RegExpFunctionalTestCollector::get()->clearRegExp(this);
#endif
    thisObject->RegExp::~RegExp();
}

RegExp* RegExp::createWithoutCaching(JSGlobalData& globalData, const UString& patternString, RegExpFlags flags)
{
    RegExp* regExp = new (NotNull, allocateCell<RegExp>(globalData.heap)) RegExp(globalData, patternString, flags);
    regExp->finishCreation(globalData);
    return regExp;
}

RegExp* RegExp::create(JSGlobalData& globalData, const UString& patternString, RegExpFlags flags)
{
    return globalData.regExpCache()->lookupOrCreate(patternString, flags);
}

void RegExp::compile(JSGlobalData* globalData, Yarr::YarrCharSize charSize)
{
    Yarr::YarrPattern pattern(m_patternString, ignoreCase(), multiline(), &m_constructionError);
    if (m_constructionError) {
        ASSERT_NOT_REACHED();
        m_state = ParseError;
        return;
    }
    ASSERT(m_numSubpatterns == pattern.m_numSubpatterns);

    if (!m_representation) {
        ASSERT(m_state == NotCompiled);
        m_representation = adoptPtr(new RegExpRepresentation);
        globalData->regExpCache()->addToStrongCache(this);
        m_state = ByteCode;
    }

#if ENABLE(YARR_JIT)
    if (!pattern.m_containsBackreferences && globalData->canUseJIT()) {
        Yarr::jitCompile(pattern, charSize, globalData, m_representation->m_regExpJITCode);
#if ENABLE(YARR_JIT_DEBUG)
        if (!m_representation->m_regExpJITCode.isFallBack())
            m_state = JITCode;
        else
            m_state = ByteCode;
#else
        if (!m_representation->m_regExpJITCode.isFallBack()) {
            m_state = JITCode;
            return;
        }
#endif
    }
#else
    UNUSED_PARAM(charSize);
#endif

    m_representation->m_regExpBytecode = Yarr::byteCompile(pattern, &globalData->m_regExpAllocator);
}

void RegExp::compileIfNecessary(JSGlobalData& globalData, Yarr::YarrCharSize charSize)
{
    // If the state is NotCompiled or ParseError, then there is no representation.
    // If there is a representation, and the state must be either JITCode or ByteCode.
    ASSERT(!!m_representation == (m_state == JITCode || m_state == ByteCode));
    
    if (m_representation) {
#if ENABLE(YARR_JIT)
        if (m_state != JITCode)
            return;
        if ((charSize == Yarr::Char8) && (m_representation->m_regExpJITCode.has8BitCode()))
            return;
        if ((charSize == Yarr::Char16) && (m_representation->m_regExpJITCode.has16BitCode()))
            return;
#else
        return;
#endif
    }

    compile(&globalData, charSize);
}

int RegExp::match(JSGlobalData& globalData, const UString& s, unsigned startOffset, Vector<int, 32>* ovector)
{
#if ENABLE(REGEXP_TRACING)
    m_rtMatchCallCount++;
#endif

    ASSERT(m_state != ParseError);
    compileIfNecessary(globalData, s.is8Bit() ? Yarr::Char8 : Yarr::Char16);

    int offsetVectorSize = (m_numSubpatterns + 1) * 2;
    int* offsetVector;
    Vector<int, 32> nonReturnedOvector;
    if (ovector) {
        ovector->resize(offsetVectorSize);
        offsetVector = ovector->data();
    } else {
        nonReturnedOvector.resize(offsetVectorSize);
        offsetVector = nonReturnedOvector.data();
    }
    ASSERT(offsetVector);

    int result;
#if ENABLE(YARR_JIT)
    if (m_state == JITCode) {
        if (s.is8Bit())
            result = Yarr::execute(m_representation->m_regExpJITCode, s.characters8(), startOffset, s.length(), offsetVector);
        else
            result = Yarr::execute(m_representation->m_regExpJITCode, s.characters16(), startOffset, s.length(), offsetVector);
#if ENABLE(YARR_JIT_DEBUG)
        matchCompareWithInterpreter(s, startOffset, offsetVector, result);
#endif
    } else
#endif
        result = Yarr::interpret(m_representation->m_regExpBytecode.get(), s, startOffset, s.length(), offsetVector);
    ASSERT(result >= -1);

#if REGEXP_FUNC_TEST_DATA_GEN
    RegExpFunctionalTestCollector::get()->outputOneTest(this, s, startOffset, offsetVector, result);
#endif

#if ENABLE(REGEXP_TRACING)
    if (result != -1)
        m_rtMatchFoundCount++;
#endif

    return result;
}

void RegExp::invalidateCode()
{
    if (!m_representation)
        return;
    m_state = NotCompiled;
    m_representation.clear();
}

#if ENABLE(YARR_JIT_DEBUG)
void RegExp::matchCompareWithInterpreter(const UString& s, int startOffset, int* offsetVector, int jitResult)
{
    int offsetVectorSize = (m_numSubpatterns + 1) * 2;
    Vector<int, 32> interpreterOvector;
    interpreterOvector.resize(offsetVectorSize);
    int* interpreterOffsetVector = interpreterOvector.data();
    int interpreterResult = 0;
    int differences = 0;

    // Initialize interpreterOffsetVector with the return value (index 0) and the 
    // first subpattern start indicies (even index values) set to -1.
    // No need to init the subpattern end indicies.
    for (unsigned j = 0, i = 0; i < m_numSubpatterns + 1; j += 2, i++)
        interpreterOffsetVector[j] = -1;

    interpreterResult = Yarr::interpret(m_representation->m_regExpBytecode.get(), s, startOffset, s.length(), interpreterOffsetVector);

    if (jitResult != interpreterResult)
        differences++;

    for (unsigned j = 2, i = 0; i < m_numSubpatterns; j +=2, i++)
        if ((offsetVector[j] != interpreterOffsetVector[j])
            || ((offsetVector[j] >= 0) && (offsetVector[j+1] != interpreterOffsetVector[j+1])))
            differences++;

    if (differences) {
        fprintf(stderr, "RegExp Discrepency for /%s/\n    string input ", pattern().utf8().data());
        unsigned segmentLen = s.length() - static_cast<unsigned>(startOffset);

        fprintf(stderr, (segmentLen < 150) ? "\"%s\"\n" : "\"%148s...\"\n", s.utf8().data() + startOffset);

        if (jitResult != interpreterResult) {
            fprintf(stderr, "    JIT result = %d, blah interpreted result = %d\n", jitResult, interpreterResult);
            differences--;
        } else {
            fprintf(stderr, "    Correct result = %d\n", jitResult);
        }

        if (differences) {
            for (unsigned j = 2, i = 0; i < m_numSubpatterns; j +=2, i++) {
                if (offsetVector[j] != interpreterOffsetVector[j])
                    fprintf(stderr, "    JIT offset[%d] = %d, interpreted offset[%d] = %d\n", j, offsetVector[j], j, interpreterOffsetVector[j]);
                if ((offsetVector[j] >= 0) && (offsetVector[j+1] != interpreterOffsetVector[j+1]))
                    fprintf(stderr, "    JIT offset[%d] = %d, interpreted offset[%d] = %d\n", j+1, offsetVector[j+1], j+1, interpreterOffsetVector[j+1]);
            }
        }
    }
}
#endif

#if ENABLE(REGEXP_TRACING)
    void RegExp::printTraceData()
    {
        char formattedPattern[41];
        char rawPattern[41];

        strncpy(rawPattern, pattern().utf8().data(), 40);
        rawPattern[40]= '\0';

        int pattLen = strlen(rawPattern);

        snprintf(formattedPattern, 41, (pattLen <= 38) ? "/%.38s/" : "/%.36s...", rawPattern);

#if ENABLE(YARR_JIT)
        Yarr::YarrCodeBlock& codeBlock = m_representation->m_regExpJITCode;

        const size_t jitAddrSize = 20;
        char jitAddr[jitAddrSize];
        if (m_state == JITCode)
            snprintf(jitAddr, jitAddrSize, "fallback");
        else
            snprintf(jitAddr, jitAddrSize, "0x%014lx", reinterpret_cast<unsigned long int>(codeBlock.getAddr()));
#else
        const char* jitAddr = "JIT Off";
#endif

        printf("%-40.40s %16.16s %10d %10d\n", formattedPattern, jitAddr, m_rtMatchCallCount, m_rtMatchFoundCount);
    }
#endif

} // namespace JSC
