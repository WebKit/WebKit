/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StringImpl.h"

#include "AtomicString.h"
#include "CString.h"
#include "CharacterNames.h"
#include "FloatConversion.h"
#include "StringBuffer.h"
#include "StringHash.h"
#include "TextBreakIterator.h"
#include "TextEncoding.h"
#include "ThreadGlobalData.h"
#include <wtf/dtoa.h>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

namespace WebCore {

static const unsigned minLengthToShare = 20;

static inline UChar* newUCharVector(unsigned n)
{
    return static_cast<UChar*>(fastMalloc(sizeof(UChar) * n));
}

static inline void deleteUCharVector(const UChar* p)
{
    fastFree(const_cast<UChar*>(p));
}

// Some of the factory methods create buffers using fastMalloc.
// We must ensure that all allocations of StringImpl are allocated using
// fastMalloc so that we don't have mis-matched frees. We accomplish 
// this by overriding the new and delete operators.
void* StringImpl::operator new(size_t size, void* address)
{
    if (address)
        return address;  // Allocating using an internal buffer
    return fastMalloc(size);
}

void* StringImpl::operator new(size_t size)
{
    return fastMalloc(size);
}

void StringImpl::operator delete(void* address)
{
    fastFree(address);
}

// This constructor is used only to create the empty string.
StringImpl::StringImpl()
    : m_data(0)
    , m_length(0)
    , m_hash(0)
    , m_buffer(0)
{
    // Ensure that the hash is computed so that AtomicStringHash can call existingHash()
    // with impunity. The empty string is special because it is never entered into
    // AtomicString's HashKey, but still needs to compare correctly.
    hash();
}

inline StringImpl::StringImpl(UChar* characters, unsigned length, AdoptBuffer)
    : m_data(characters)
    , m_length(length)
    , m_hash(0)
    , m_buffer(0)
{
    ASSERT(characters);
    ASSERT(length);
}

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const UChar* characters, unsigned length, unsigned hash)
    : m_data(0)
    , m_length(length)
    , m_hash(hash)
    , m_buffer(0)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    setInTable();
    UChar* data = newUCharVector(length);
    memcpy(data, characters, length * sizeof(UChar));
    m_data = data;
}

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const char* characters, unsigned length, unsigned hash)
    : m_data(0)
    , m_length(length)
    , m_hash(hash)
    , m_buffer(0)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    setInTable();
    UChar* data = newUCharVector(length);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    m_data = data;
}

StringImpl::~StringImpl()
{
    if (inTable())
        AtomicString::remove(this);
    if (!bufferIsInternal()) {
        SharedUChar* sharedBuffer = m_sharedBufferAndFlags.get();
        if (sharedBuffer)
            sharedBuffer->deref();
        else
            deleteUCharVector(m_data);
    }
}

StringImpl* StringImpl::empty()
{
    return threadGlobalData().emptyString();
}

bool StringImpl::containsOnlyWhitespace()
{
    // FIXME: The definition of whitespace here includes a number of characters
    // that are not whitespace from the point of view of RenderText; I wonder if
    // that's a problem in practice.
    for (unsigned i = 0; i < m_length; i++)
        if (!isASCIISpace(m_data[i]))
            return false;
    return true;
}

PassRefPtr<StringImpl> StringImpl::substring(unsigned start, unsigned length)
{
    if (start >= m_length)
        return empty();
    unsigned maxLength = m_length - start;
    if (length >= maxLength) {
        if (!start)
            return this;
        length = maxLength;
    }
    return create(m_data + start, length);
}

UChar32 StringImpl::characterStartingAt(unsigned i)
{
    if (U16_IS_SINGLE(m_data[i]))
        return m_data[i];
    if (i + 1 < m_length && U16_IS_LEAD(m_data[i]) && U16_IS_TRAIL(m_data[i + 1]))
        return U16_GET_SUPPLEMENTARY(m_data[i], m_data[i + 1]);
    return 0;
}

bool StringImpl::isLower()
{
    // Do a faster loop for the case where all the characters are ASCII.
    bool allLower = true;
    UChar ored = 0;
    for (unsigned i = 0; i < m_length; i++) {
        UChar c = m_data[i];
        allLower = allLower && isASCIILower(c);
        ored |= c;
    }
    if (!(ored & ~0x7F))
        return allLower;

    // Do a slower check for cases that include non-ASCII characters.
    allLower = true;
    unsigned i = 0;
    while (i < m_length) {
        UChar32 character;
        U16_NEXT(m_data, i, m_length, character)
        allLower = allLower && Unicode::isLower(character);
    }
    return allLower;
}

PassRefPtr<StringImpl> StringImpl::lower()
{
    UChar* data;
    PassRefPtr<StringImpl> newImpl = createUninitialized(m_length, data);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::toLower(data, length, m_data, m_length, &error);
    if (!error && realLength == length)
        return newImpl;
    newImpl = createUninitialized(realLength, data);
    Unicode::toLower(data, realLength, m_data, m_length, &error);
    if (error)
        return this;
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::upper()
{
    UChar* data;
    PassRefPtr<StringImpl> newImpl = createUninitialized(m_length, data);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIIUpper(c);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::toUpper(data, length, m_data, m_length, &error);
    if (!error && realLength == length)
        return newImpl;
    newImpl = createUninitialized(realLength, data);
    Unicode::toUpper(data, realLength, m_data, m_length, &error);
    if (error)
        return this;
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::secure(UChar aChar)
{
    UChar* data;
    PassRefPtr<StringImpl> newImpl = createUninitialized(m_length, data);
    int32_t length = m_length;
    for (int i = 0; i < length; ++i)
        data[i] = aChar;
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::foldCase()
{
    UChar* data;
    PassRefPtr<StringImpl> newImpl = createUninitialized(m_length, data);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return newImpl;

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::foldCase(data, length, m_data, m_length, &error);
    if (!error && realLength == length)
        return newImpl;
    newImpl = createUninitialized(realLength, data);
    Unicode::foldCase(data, realLength, m_data, m_length, &error);
    if (error)
        return this;
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::stripWhiteSpace()
{
    if (!m_length)
        return empty();

    unsigned start = 0;
    unsigned end = m_length - 1;
    
    // skip white space from start
    while (start <= end && isSpaceOrNewline(m_data[start]))
        start++;
    
    // only white space
    if (start > end) 
        return empty();

    // skip white space from end
    while (end && isSpaceOrNewline(m_data[end]))
        end--;

    return create(m_data + start, end + 1 - start);
}

PassRefPtr<StringImpl> StringImpl::removeCharacters(CharacterMatchFunctionPtr findMatch)
{
    const UChar* from = m_data;
    const UChar* fromend = from + m_length;

    // Assume the common case will not remove any characters
    while (from != fromend && !findMatch(*from))
        from++;
    if (from == fromend)
        return this;

    StringBuffer data(m_length);
    UChar* to = data.characters();
    unsigned outc = from - m_data;

    if (outc)
        memcpy(to, m_data, outc * sizeof(UChar));

    while (true) {
        while (from != fromend && findMatch(*from))
            from++;
        while (from != fromend && !findMatch(*from))
            to[outc++] = *from++;
        if (from == fromend)
            break;
    }

    data.shrink(outc);

    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::simplifyWhiteSpace()
{
    StringBuffer data(m_length);

    const UChar* from = m_data;
    const UChar* fromend = from + m_length;
    int outc = 0;
    
    UChar* to = data.characters();
    
    while (true) {
        while (from != fromend && isSpaceOrNewline(*from))
            from++;
        while (from != fromend && !isSpaceOrNewline(*from))
            to[outc++] = *from++;
        if (from != fromend)
            to[outc++] = ' ';
        else
            break;
    }
    
    if (outc > 0 && to[outc - 1] == ' ')
        outc--;
    
    data.shrink(outc);
    
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::capitalize(UChar previous)
{
    StringBuffer stringWithPrevious(m_length + 1);
    stringWithPrevious[0] = previous == noBreakSpace ? ' ' : previous;
    for (unsigned i = 1; i < m_length + 1; i++) {
        // Replace &nbsp with a real space since ICU no longer treats &nbsp as a word separator.
        if (m_data[i - 1] == noBreakSpace)
            stringWithPrevious[i] = ' ';
        else
            stringWithPrevious[i] = m_data[i - 1];
    }

    TextBreakIterator* boundary = wordBreakIterator(stringWithPrevious.characters(), m_length + 1);
    if (!boundary)
        return this;

    StringBuffer data(m_length);

    int32_t endOfWord;
    int32_t startOfWord = textBreakFirst(boundary);
    for (endOfWord = textBreakNext(boundary); endOfWord != TextBreakDone; startOfWord = endOfWord, endOfWord = textBreakNext(boundary)) {
        if (startOfWord != 0) // Ignore first char of previous string
            data[startOfWord - 1] = m_data[startOfWord - 1] == noBreakSpace ? noBreakSpace : toTitleCase(stringWithPrevious[startOfWord]);
        for (int i = startOfWord + 1; i < endOfWord; i++)
            data[i - 1] = m_data[i - 1];
    }

    return adopt(data);
}

int StringImpl::toIntStrict(bool* ok, int base)
{
    return charactersToIntStrict(m_data, m_length, ok, base);
}

unsigned StringImpl::toUIntStrict(bool* ok, int base)
{
    return charactersToUIntStrict(m_data, m_length, ok, base);
}

int64_t StringImpl::toInt64Strict(bool* ok, int base)
{
    return charactersToInt64Strict(m_data, m_length, ok, base);
}

uint64_t StringImpl::toUInt64Strict(bool* ok, int base)
{
    return charactersToUInt64Strict(m_data, m_length, ok, base);
}

intptr_t StringImpl::toIntPtrStrict(bool* ok, int base)
{
    return charactersToIntPtrStrict(m_data, m_length, ok, base);
}

int StringImpl::toInt(bool* ok)
{
    return charactersToInt(m_data, m_length, ok);
}

unsigned StringImpl::toUInt(bool* ok)
{
    return charactersToUInt(m_data, m_length, ok);
}

int64_t StringImpl::toInt64(bool* ok)
{
    return charactersToInt64(m_data, m_length, ok);
}

uint64_t StringImpl::toUInt64(bool* ok)
{
    return charactersToUInt64(m_data, m_length, ok);
}

intptr_t StringImpl::toIntPtr(bool* ok)
{
    return charactersToIntPtr(m_data, m_length, ok);
}

double StringImpl::toDouble(bool* ok)
{
    return charactersToDouble(m_data, m_length, ok);
}

float StringImpl::toFloat(bool* ok)
{
    return charactersToFloat(m_data, m_length, ok);
}

static bool equal(const UChar* a, const char* b, int length)
{
    ASSERT(length >= 0);
    while (length--) {
        unsigned char bc = *b++;
        if (*a++ != bc)
            return false;
    }
    return true;
}

bool equalIgnoringCase(const UChar* a, const char* b, unsigned length)
{
    while (length--) {
        unsigned char bc = *b++;
        if (foldCase(*a++) != foldCase(bc))
            return false;
    }
    return true;
}

static inline bool equalIgnoringCase(const UChar* a, const UChar* b, int length)
{
    ASSERT(length >= 0);
    return umemcasecmp(a, b, length) == 0;
}

int StringImpl::find(const char* chs, int index, bool caseSensitive)
{
    if (!chs || index < 0)
        return -1;

    int chsLength = strlen(chs);
    int n = m_length - index;
    if (n < 0)
        return -1;
    n -= chsLength - 1;
    if (n <= 0)
        return -1;

    const char* chsPlusOne = chs + 1;
    int chsLengthMinusOne = chsLength - 1;
    
    const UChar* ptr = m_data + index - 1;
    if (caseSensitive) {
        UChar c = *chs;
        do {
            if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    } else {
        UChar lc = Unicode::foldCase(*chs);
        do {
            if (Unicode::foldCase(*++ptr) == lc && equalIgnoringCase(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    }

    return -1;
}

int StringImpl::find(UChar c, int start)
{
    return WebCore::find(m_data, m_length, c, start);
}

int StringImpl::find(CharacterMatchFunctionPtr matchFunction, int start)
{
    return WebCore::find(m_data, m_length, matchFunction, start);
}

int StringImpl::find(StringImpl* str, int index, bool caseSensitive)
{
    /*
      We use a simple trick for efficiency's sake. Instead of
      comparing strings, we compare the sum of str with that of
      a part of this string. Only if that matches, we call memcmp
      or ucstrnicmp.
    */
    ASSERT(str);
    if (index < 0)
        index += m_length;
    int lstr = str->m_length;
    int lthis = m_length - index;
    if ((unsigned)lthis > m_length)
        return -1;
    int delta = lthis - lstr;
    if (delta < 0)
        return -1;

    const UChar* uthis = m_data + index;
    const UChar* ustr = str->m_data;
    unsigned hthis = 0;
    unsigned hstr = 0;
    if (caseSensitive) {
        for (int i = 0; i < lstr; i++) {
            hthis += uthis[i];
            hstr += ustr[i];
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(UChar)) == 0)
                return index + i;
            if (i == delta)
                return -1;
            hthis += uthis[i + lstr];
            hthis -= uthis[i];
            i++;
        }
    } else {
        for (int i = 0; i < lstr; i++ ) {
            hthis += toASCIILower(uthis[i]);
            hstr += toASCIILower(ustr[i]);
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && equalIgnoringCase(uthis + i, ustr, lstr))
                return index + i;
            if (i == delta)
                return -1;
            hthis += toASCIILower(uthis[i + lstr]);
            hthis -= toASCIILower(uthis[i]);
            i++;
        }
    }
}

int StringImpl::reverseFind(UChar c, int index)
{
    return WebCore::reverseFind(m_data, m_length, c, index);
}

int StringImpl::reverseFind(StringImpl* str, int index, bool caseSensitive)
{
    /*
     See StringImpl::find() for explanations.
     */
    ASSERT(str);
    int lthis = m_length;
    if (index < 0)
        index += lthis;
    
    int lstr = str->m_length;
    int delta = lthis - lstr;
    if ( index < 0 || index > lthis || delta < 0 )
        return -1;
    if ( index > delta )
        index = delta;
    
    const UChar *uthis = m_data;
    const UChar *ustr = str->m_data;
    unsigned hthis = 0;
    unsigned hstr = 0;
    int i;
    if (caseSensitive) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i];
            hstr += ustr[i];
        }
        i = index;
        while (1) {
            if (hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(UChar)) == 0)
                return i;
            if (i == 0)
                return -1;
            i--;
            hthis -= uthis[i + lstr];
            hthis += uthis[i];
        }
    } else {
        for (i = 0; i < lstr; i++) {
            hthis += toASCIILower(uthis[index + i]);
            hstr += toASCIILower(ustr[i]);
        }
        i = index;
        while (1) {
            if (hthis == hstr && equalIgnoringCase(uthis + i, ustr, lstr) )
                return i;
            if (i == 0)
                return -1;
            i--;
            hthis -= toASCIILower(uthis[i + lstr]);
            hthis += toASCIILower(uthis[i]);
        }
    }
    
    // Should never get here.
    return -1;
}

bool StringImpl::endsWith(StringImpl* m_data, bool caseSensitive)
{
    ASSERT(m_data);
    int start = m_length - m_data->m_length;
    if (start >= 0)
        return (find(m_data, start, caseSensitive) == start);
    return false;
}

PassRefPtr<StringImpl> StringImpl::replace(UChar oldC, UChar newC)
{
    if (oldC == newC)
        return this;
    unsigned i;
    for (i = 0; i != m_length; ++i)
        if (m_data[i] == oldC)
            break;
    if (i == m_length)
        return this;

    UChar* data;
    PassRefPtr<StringImpl> newImpl = createUninitialized(m_length, data);

    for (i = 0; i != m_length; ++i) {
        UChar ch = m_data[i];
        if (ch == oldC)
            ch = newC;
        data[i] = ch;
    }
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::replace(unsigned position, unsigned lengthToReplace, StringImpl* str)
{
    position = min(position, length());
    lengthToReplace = min(lengthToReplace, length() - position);
    unsigned lengthToInsert = str ? str->length() : 0;
    if (!lengthToReplace && !lengthToInsert)
        return this;
    UChar* data;
    PassRefPtr<StringImpl> newImpl =
        createUninitialized(length() - lengthToReplace + lengthToInsert, data);
    memcpy(data, characters(), position * sizeof(UChar));
    if (str)
        memcpy(data + position, str->characters(), lengthToInsert * sizeof(UChar));
    memcpy(data + position + lengthToInsert, characters() + position + lengthToReplace,
        (length() - position - lengthToReplace) * sizeof(UChar));
    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::replace(UChar pattern, StringImpl* replacement)
{
    if (!replacement)
        return this;
        
    int repStrLength = replacement->length();
    int srcSegmentStart = 0;
    int matchCount = 0;
    
    // Count the matches
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) >= 0) {
        ++matchCount;
        ++srcSegmentStart;
    }
    
    // If we have 0 matches, we don't have to do any more work
    if (!matchCount)
        return this;
    
    UChar* data;
    PassRefPtr<StringImpl> newImpl =
        createUninitialized(m_length - matchCount + (matchCount * repStrLength), data);

    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(newImpl->length()));

    return newImpl;
}

PassRefPtr<StringImpl> StringImpl::replace(StringImpl* pattern, StringImpl* replacement)
{
    if (!pattern || !replacement)
        return this;

    int patternLength = pattern->length();
    if (!patternLength)
        return this;
        
    int repStrLength = replacement->length();
    int srcSegmentStart = 0;
    int matchCount = 0;
    
    // Count the matches
    while ((srcSegmentStart = find(pattern, srcSegmentStart)) >= 0) {
        ++matchCount;
        srcSegmentStart += patternLength;
    }
    
    // If we have 0 matches, we don't have to do any more work
    if (!matchCount)
        return this;
    
    UChar* data;
    PassRefPtr<StringImpl> newImpl =
        createUninitialized(m_length + matchCount * (repStrLength - patternLength), data);
    
    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + patternLength;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(newImpl->length()));

    return newImpl;
}

bool equal(StringImpl* a, StringImpl* b)
{
    return StringHash::equal(a, b);
}

bool equal(StringImpl* a, const char* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();
    const UChar* as = a->characters();
    for (unsigned i = 0; i != length; ++i) {
        unsigned char bc = b[i];
        if (!bc)
            return false;
        if (as[i] != bc)
            return false;
    }

    return !b[length];
}

bool equalIgnoringCase(StringImpl* a, StringImpl* b)
{
    return CaseFoldingHash::equal(a, b);
}

bool equalIgnoringCase(StringImpl* a, const char* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();
    const UChar* as = a->characters();

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    bool equal = true;
    for (unsigned i = 0; i != length; ++i) {
        char bc = b[i];
        if (!bc)
            return false;
        UChar ac = as[i];
        ored |= ac;
        equal = equal && (toASCIILower(ac) == toASCIILower(bc));
    }

    // Do a slower implementation for cases that include non-ASCII characters.
    if (ored & ~0x7F) {
        equal = true;
        for (unsigned i = 0; i != length; ++i) {
            unsigned char bc = b[i];
            equal = equal && (foldCase(as[i]) == foldCase(bc));
        }
    }

    return equal && !b[length];
}

bool equalIgnoringNullity(StringImpl* a, StringImpl* b)
{
    if (StringHash::equal(a, b))
        return true;
    if (!a && b && !b->length())
        return true;
    if (!b && a && !a->length())
        return true;

    return false;
}

Vector<char> StringImpl::ascii()
{
    Vector<char> buffer(m_length + 1);
    for (unsigned i = 0; i != m_length; ++i) {
        UChar c = m_data[i];
        if ((c >= 0x20 && c < 0x7F) || c == 0x00)
            buffer[i] = c;
        else
            buffer[i] = '?';
    }
    buffer[m_length] = '\0';
    return buffer;
}

WTF::Unicode::Direction StringImpl::defaultWritingDirection()
{
    for (unsigned i = 0; i < m_length; ++i) {
        WTF::Unicode::Direction charDirection = WTF::Unicode::direction(m_data[i]);
        if (charDirection == WTF::Unicode::LeftToRight)
            return WTF::Unicode::LeftToRight;
        if (charDirection == WTF::Unicode::RightToLeft || charDirection == WTF::Unicode::RightToLeftArabic)
            return WTF::Unicode::RightToLeft;
    }
    return WTF::Unicode::LeftToRight;
}

// This is a hot function because it's used when parsing HTML.
PassRefPtr<StringImpl> StringImpl::createStrippingNullCharactersSlowCase(const UChar* characters, unsigned length)
{
    StringBuffer strippedCopy(length);
    unsigned strippedLength = 0;
    for (unsigned i = 0; i < length; i++) {
        if (int c = characters[i])
            strippedCopy[strippedLength++] = c;
    }
    ASSERT(strippedLength < length);  // Only take the slow case when stripping.
    strippedCopy.shrink(strippedLength);
    return adopt(strippedCopy);
}

PassRefPtr<StringImpl> StringImpl::adopt(StringBuffer& buffer)
{
    unsigned length = buffer.length();
    if (length == 0)
        return empty();
    return adoptRef(new StringImpl(buffer.release(), length, AdoptBuffer()));
}

PassRefPtr<StringImpl> StringImpl::adopt(Vector<UChar>& vector)
{
    size_t size = vector.size();
    if (size == 0)
        return empty();
    return adoptRef(new StringImpl(vector.releaseBuffer(), size, AdoptBuffer()));
}

PassRefPtr<StringImpl> StringImpl::createUninitialized(unsigned length, UChar*& data)
{
    if (!length) {
        data = 0;
        return empty();
    }

    // Allocate a single buffer large enough to contain the StringImpl
    // struct as well as the data which it contains. This removes one 
    // heap allocation from this call.
    size_t size = OBJECT_OFFSETOF(StringImpl, m_buffer) + length * sizeof(UChar);
    StringImpl* string = static_cast<StringImpl*>(fastMalloc(size));
    data = const_cast<UChar*>(&string->m_buffer[0]);
    string = new (string) StringImpl(data, length, AdoptBuffer());
    return adoptRef(string);
}

PassRefPtr<StringImpl> StringImpl::create(const UChar* characters, unsigned length)
{
    if (!characters || !length)
        return empty();

    UChar* data;
    PassRefPtr<StringImpl> string = createUninitialized(length, data);
    memcpy(data, characters, length * sizeof(UChar));
    return string;
}

PassRefPtr<StringImpl> StringImpl::create(const char* characters, unsigned length)
{
    if (!characters || !length)
        return empty();

    UChar* data;
    PassRefPtr<StringImpl> string = createUninitialized(length, data);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    return string;
}

PassRefPtr<StringImpl> StringImpl::create(const char* string)
{
    if (!string)
        return empty();
    return create(string, strlen(string));
}

#if USE(JSC)
PassRefPtr<StringImpl> StringImpl::create(const JSC::UString& str)
{
    SharedUChar* sharedBuffer = const_cast<JSC::UString*>(&str)->rep()->sharedBuffer();
    if (sharedBuffer) {
        PassRefPtr<StringImpl> impl = adoptRef(new StringImpl(const_cast<UChar*>(str.data()), str.size(), AdoptBuffer()));
        sharedBuffer->ref();
        impl->m_sharedBufferAndFlags.set(sharedBuffer);
        return impl;
    }
    return StringImpl::create(str.data(), str.size());
}

JSC::UString StringImpl::ustring()
{
    SharedUChar* sharedBuffer = this->sharedBuffer();
    if (sharedBuffer)
        return JSC::UString::Rep::create(const_cast<UChar*>(m_data), m_length, sharedBuffer);

    return JSC::UString(m_data, m_length);
}
#endif

PassRefPtr<StringImpl> StringImpl::createWithTerminatingNullCharacter(const StringImpl& string)
{
    // Use createUninitialized instead of 'new StringImpl' so that the string and its buffer
    // get allocated in a single malloc block.
    UChar* data;
    int length = string.m_length;
    RefPtr<StringImpl> terminatedString = createUninitialized(length + 1, data);
    memcpy(data, string.m_data, length * sizeof(UChar));
    data[length] = 0;
    terminatedString->m_length--;
    terminatedString->m_hash = string.m_hash;
    terminatedString->m_sharedBufferAndFlags.setFlag(HasTerminatingNullCharacter);
    return terminatedString.release();
}

PassRefPtr<StringImpl> StringImpl::threadsafeCopy() const
{
    // Special-case empty strings to make sure that per-thread empty string instance isn't returned.
    if (m_length == 0)
        return adoptRef(new StringImpl);
    return create(m_data, m_length);
}

PassRefPtr<StringImpl> StringImpl::crossThreadString()
{
    SharedUChar* shared = sharedBuffer();
    if (shared) {
        RefPtr<StringImpl> impl = adoptRef(new StringImpl(const_cast<UChar*>(m_data), m_length, AdoptBuffer()));
        impl->m_sharedBufferAndFlags.set(shared->crossThreadCopy().releaseRef());
        return impl.release();
    }

    // If no shared buffer is available, create a copy.
    return threadsafeCopy();
}

StringImpl::SharedUChar* StringImpl::sharedBuffer()
{
    if (m_length < minLengthToShare || bufferIsInternal())
        return 0;

    if (!m_sharedBufferAndFlags.get())
        m_sharedBufferAndFlags.set(SharedUChar::create(new OwnFastMallocPtr<UChar>(const_cast<UChar*>(m_data))).releaseRef());
    return m_sharedBufferAndFlags.get();
}


} // namespace WebCore
