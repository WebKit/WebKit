/*
 *  Copyright (C) 1999-2001, 2004 Harri Porten (porten@kde.org)
 *  Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RegExp.h"
#include "Lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>


#if ENABLE(YARR)

#include "yarr/RegexCompiler.h"
#if ENABLE(YARR_JIT)
#include "yarr/RegexJIT.h"
#else
#include "yarr/RegexInterpreter.h"
#endif

#else

#include <pcre/pcre.h>

#endif

namespace JSC {

struct RegExpRepresentation {
#if ENABLE(YARR_JIT)
    Yarr::RegexCodeBlock m_regExpJITCode;
#elif ENABLE(YARR)
    OwnPtr<Yarr::BytecodePattern> m_regExpBytecode;
#else
    JSRegExp* m_regExp;
#endif

#if !ENABLE(YARR)
    ~RegExpRepresentation()
    {
        jsRegExpFree(m_regExp);
    }
#endif
};

inline RegExp::RegExp(JSGlobalData* globalData, const UString& pattern, const UString& flags)
    : m_pattern(pattern)
    , m_flagBits(0)
    , m_constructionError(0)
    , m_numSubpatterns(0)
#if ENABLE(REGEXP_TRACING)
    , m_rtMatchCallCount(0)
    , m_rtMatchFoundCount(0)
#endif
    , m_representation(adoptPtr(new RegExpRepresentation))
{
    // NOTE: The global flag is handled on a case-by-case basis by functions like
    // String::match and RegExpObject::match.
    if (!flags.isNull()) {
        if (flags.find('g') != notFound)
            m_flagBits |= Global;
        if (flags.find('i') != notFound)
            m_flagBits |= IgnoreCase;
        if (flags.find('m') != notFound)
            m_flagBits |= Multiline;
    }
    compile(globalData);
}

RegExp::~RegExp()
{
}

PassRefPtr<RegExp> RegExp::create(JSGlobalData* globalData, const UString& pattern, const UString& flags)
{
    RefPtr<RegExp> res = adoptRef(new RegExp(globalData, pattern, flags));
#if ENABLE(REGEXP_TRACING)
    globalData->addRegExpToTrace(res);
#endif
    return res.release();
}

#if ENABLE(YARR)

void RegExp::compile(JSGlobalData* globalData)
{
#if ENABLE(YARR_JIT)
    Yarr::jitCompileRegex(globalData, m_representation->m_regExpJITCode, m_pattern, m_numSubpatterns, m_constructionError, ignoreCase(), multiline());
#else
    m_representation->m_regExpBytecode = Yarr::byteCompileRegex(m_pattern, m_numSubpatterns, m_constructionError, &globalData->m_regexAllocator, ignoreCase(), multiline());
#endif
}

int RegExp::match(const UString& s, int startOffset, Vector<int, 32>* ovector)
{
    if (startOffset < 0)
        startOffset = 0;
    
#if ENABLE(REGEXP_TRACING)
    m_rtMatchCallCount++;
#endif

    if (static_cast<unsigned>(startOffset) > s.length() || s.isNull())
        return -1;

#if ENABLE(YARR_JIT)
    if (!!m_representation->m_regExpJITCode) {
#else
    if (m_representation->m_regExpBytecode) {
#endif
        int offsetVectorSize = (m_numSubpatterns + 1) * 3; // FIXME: should be 2 - but adding temporary fallback to pcre.
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
        // Initialize offsetVector with the return value (index 0) and the 
        // first subpattern start indicies (even index values) set to -1.
        // No need to init the subpattern end indicies.
        for (unsigned j = 0, i = 0; i < m_numSubpatterns + 1; j += 2, i++)            
            offsetVector[j] = -1;

#if ENABLE(YARR_JIT)
        int result = Yarr::executeRegex(m_representation->m_regExpJITCode, s.characters(), startOffset, s.length(), offsetVector, offsetVectorSize);
#else
        int result = Yarr::interpretRegex(m_representation->m_regExpBytecode.get(), s.characters(), startOffset, s.length(), offsetVector);
#endif

        if (result < 0) {
#ifndef NDEBUG
            // TODO: define up a symbol, rather than magic -1
            if (result != -1)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", result);
#endif
        }
        
#if ENABLE(REGEXP_TRACING)
        if (result != -1)
            m_rtMatchFoundCount++;
#endif

        return result;
    }

    return -1;
}

#else

void RegExp::compile(JSGlobalData*)
{
    m_representation->m_regExp = 0;
    JSRegExpIgnoreCaseOption ignoreCaseOption = ignoreCase() ? JSRegExpIgnoreCase : JSRegExpDoNotIgnoreCase;
    JSRegExpMultilineOption multilineOption = multiline() ? JSRegExpMultiline : JSRegExpSingleLine;
    m_representation->m_regExp = jsRegExpCompile(reinterpret_cast<const UChar*>(m_pattern.characters()), m_pattern.length(), ignoreCaseOption, multilineOption, &m_numSubpatterns, &m_constructionError);
}

int RegExp::match(const UString& s, int startOffset, Vector<int, 32>* ovector)
{
#if ENABLE(REGEXP_TRACING)
    m_rtMatchCallCount++;
#endif
    
    if (startOffset < 0)
        startOffset = 0;
    if (ovector)
        ovector->clear();

    if (static_cast<unsigned>(startOffset) > s.length() || s.isNull())
        return -1;

    if (m_representation->m_regExp) {
        // Set up the offset vector for the result.
        // First 2/3 used for result, the last third used by PCRE.
        int* offsetVector;
        int offsetVectorSize;
        int fixedSizeOffsetVector[3];
        if (!ovector) {
            offsetVectorSize = 3;
            offsetVector = fixedSizeOffsetVector;
        } else {
            offsetVectorSize = (m_numSubpatterns + 1) * 3;
            ovector->resize(offsetVectorSize);
            offsetVector = ovector->data();
        }

        int numMatches = jsRegExpExecute(m_representation->m_regExp, reinterpret_cast<const UChar*>(s.characters()), s.length(), startOffset, offsetVector, offsetVectorSize);
    
        if (numMatches < 0) {
#ifndef NDEBUG
            if (numMatches != JSRegExpErrorNoMatch)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", numMatches);
#endif
            if (ovector)
                ovector->clear();
            return -1;
        }

#if ENABLE(REGEXP_TRACING)
        m_rtMatchFoundCount++;
#endif
        
        return offsetVector[0];
    }

    return -1;
}
    
#endif

#if ENABLE(REGEXP_TRACING)
    void RegExp::printTraceData()
    {
        char formattedPattern[41];
        char rawPattern[41];
        
        strncpy(rawPattern, m_pattern.utf8().data(), 40);
        rawPattern[40]= '\0';
        
        int pattLen = strlen(rawPattern);
        
        snprintf(formattedPattern, 41, (pattLen <= 38) ? "/%.38s/" : "/%.36s...", rawPattern);

#if ENABLE(YARR_JIT)
        Yarr::RegexCodeBlock& codeBlock = m_representation->m_regExpJITCode;

        char jitAddr[20];
        if (codeBlock.getFallback())
            sprintf(jitAddr, "fallback");
        else
            sprintf(jitAddr, "0x%014lx", (uintptr_t)codeBlock.getAddr());
#else
        const char* jitAddr = "JIT Off";
#endif
        
        printf("%-40.40s %16.16s %10d %10d\n", formattedPattern, jitAddr, m_rtMatchCallCount, m_rtMatchFoundCount);
    }
#endif
    
} // namespace JSC
