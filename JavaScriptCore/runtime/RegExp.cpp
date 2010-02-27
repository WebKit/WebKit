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

inline RegExp::RegExp(JSGlobalData* globalData, const UString& pattern)
    : m_pattern(pattern)
    , m_flagBits(0)
    , m_constructionError(0)
    , m_numSubpatterns(0)
{
    compile(globalData);
}

inline RegExp::RegExp(JSGlobalData* globalData, const UString& pattern, const UString& flags)
    : m_pattern(pattern)
    , m_flagBits(0)
    , m_constructionError(0)
    , m_numSubpatterns(0)
{
    // NOTE: The global flag is handled on a case-by-case basis by functions like
    // String::match and RegExpObject::match.
    if (flags.find('g') != UString::NotFound)
        m_flagBits |= Global;
    if (flags.find('i') != UString::NotFound)
        m_flagBits |= IgnoreCase;
    if (flags.find('m') != UString::NotFound)
        m_flagBits |= Multiline;

    compile(globalData);
}

#if !ENABLE(YARR)
RegExp::~RegExp()
{
    jsRegExpFree(m_regExp);
}
#endif

PassRefPtr<RegExp> RegExp::create(JSGlobalData* globalData, const UString& pattern)
{
    return adoptRef(new RegExp(globalData, pattern));
}

PassRefPtr<RegExp> RegExp::create(JSGlobalData* globalData, const UString& pattern, const UString& flags)
{
    return adoptRef(new RegExp(globalData, pattern, flags));
}

#if ENABLE(YARR)

void RegExp::compile(JSGlobalData* globalData)
{
#if ENABLE(YARR_JIT)
    Yarr::jitCompileRegex(globalData, m_regExpJITCode, m_pattern, m_numSubpatterns, m_constructionError, ignoreCase(), multiline());
#else
    UNUSED_PARAM(globalData);
    m_regExpBytecode.set(Yarr::byteCompileRegex(m_pattern, m_numSubpatterns, m_constructionError, ignoreCase(), multiline()));
#endif
}

int RegExp::match(const UString& s, int startOffset, Vector<int, 32>* ovector)
{
    if (startOffset < 0)
        startOffset = 0;
    if (ovector)
        ovector->clear();

    if (static_cast<unsigned>(startOffset) > s.size() || s.isNull())
        return -1;

#if ENABLE(YARR_JIT)
    if (!!m_regExpJITCode) {
#else
    if (m_regExpBytecode) {
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
        for (int j = 0; j < offsetVectorSize; ++j)
            offsetVector[j] = -1;


#if ENABLE(YARR_JIT)
        int result = Yarr::executeRegex(m_regExpJITCode, s.data(), startOffset, s.size(), offsetVector, offsetVectorSize);
#else
        int result = Yarr::interpretRegex(m_regExpBytecode.get(), s.data(), startOffset, s.size(), offsetVector);
#endif

        if (result < 0) {
#ifndef NDEBUG
            // TODO: define up a symbol, rather than magic -1
            if (result != -1)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", result);
#endif
            if (ovector)
                ovector->clear();
        }
        return result;
    }

    return -1;
}

#else

void RegExp::compile(JSGlobalData*)
{
    m_regExp = 0;
    JSRegExpIgnoreCaseOption ignoreCaseOption = ignoreCase() ? JSRegExpIgnoreCase : JSRegExpDoNotIgnoreCase;
    JSRegExpMultilineOption multilineOption = multiline() ? JSRegExpMultiline : JSRegExpSingleLine;
    m_regExp = jsRegExpCompile(reinterpret_cast<const UChar*>(m_pattern.data()), m_pattern.size(), ignoreCaseOption, multilineOption, &m_numSubpatterns, &m_constructionError);
}

int RegExp::match(const UString& s, int startOffset, Vector<int, 32>* ovector)
{
    if (startOffset < 0)
        startOffset = 0;
    if (ovector)
        ovector->clear();

    if (static_cast<unsigned>(startOffset) > s.size() || s.isNull())
        return -1;

    if (m_regExp) {
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

        int numMatches = jsRegExpExecute(m_regExp, reinterpret_cast<const UChar*>(s.data()), s.size(), startOffset, offsetVector, offsetVectorSize);
    
        if (numMatches < 0) {
#ifndef NDEBUG
            if (numMatches != JSRegExpErrorNoMatch)
                fprintf(stderr, "jsRegExpExecute failed with result %d\n", numMatches);
#endif
            if (ovector)
                ovector->clear();
            return -1;
        }

        return offsetVector[0];
    }

    return -1;
}

#endif

} // namespace JSC
