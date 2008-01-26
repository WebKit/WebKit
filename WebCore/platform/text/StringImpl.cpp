/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "DeprecatedString.h"
#include "FloatConversion.h"
#include "Length.h"
#include "StringBuffer.h"
#include "StringHash.h"
#include "TextBreakIterator.h"
#include "TextEncoding.h"
#include <kjs/dtoa.h>
#include <kjs/identifier.h>
#include <wtf/Assertions.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

using KJS::Identifier;
using KJS::UString;

namespace WebCore {

static inline UChar* newUCharVector(unsigned n)
{
    return static_cast<UChar*>(fastMalloc(sizeof(UChar) * n));
}

static inline void deleteUCharVector(const UChar* p)
{
    fastFree(const_cast<UChar*>(p));
}

// This constructor is used only to create the empty string.
StringImpl::StringImpl()
    : RefCounted<StringImpl>(1)
    , m_length(0)
    , m_data(0)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
}

// This is one of the most common constructors, but it's also used for the copy()
// operation. Because of that, it's the one constructor that doesn't assert the
// length is non-zero, since we support copying the empty string.
inline StringImpl::StringImpl(const UChar* characters, unsigned length)
    : RefCounted<StringImpl>(1)
    , m_length(length)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    UChar* data = newUCharVector(length);
    memcpy(data, characters, length * sizeof(UChar));
    m_data = data;
}

inline StringImpl::StringImpl(const StringImpl& str, WithTerminatingNullCharacter)
    : RefCounted<StringImpl>(1)
    , m_length(str.m_length)
    , m_hash(str.m_hash)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(true)
{
    UChar* data = newUCharVector(str.m_length + 1);
    memcpy(data, str.m_data, str.m_length * sizeof(UChar));
    data[str.m_length] = 0;
    m_data = data;
}

inline StringImpl::StringImpl(const char* characters, unsigned length)
    : RefCounted<StringImpl>(1)
    , m_length(length)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    m_data = data;
}

inline StringImpl::StringImpl(UChar* characters, unsigned length, AdoptBuffer)
    : RefCounted<StringImpl>(1)
    , m_length(length)
    , m_data(characters)
    , m_hash(0)
    , m_inTable(false)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(characters);
    ASSERT(length);
}

// FIXME: These AtomicString constructors return objects with a refCount of 0,
// even though the others return objects with a refCount of 1. That preserves
// the historical behavior for the hash map translator call sites inside the
// AtomicString code, but is it correct?

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const UChar* characters, unsigned length, unsigned hash)
    : RefCounted<StringImpl>(0)
    , m_length(length)
    , m_hash(hash)
    , m_inTable(true)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    memcpy(data, characters, length * sizeof(UChar));
    m_data = data;
}

// This constructor is only for use by AtomicString.
StringImpl::StringImpl(const char* characters, unsigned length, unsigned hash)
    : RefCounted<StringImpl>(0)
    , m_length(length)
    , m_hash(hash)
    , m_inTable(true)
    , m_hasTerminatingNullCharacter(false)
{
    ASSERT(hash);
    ASSERT(characters);
    ASSERT(length);

    UChar* data = newUCharVector(length);
    for (unsigned i = 0; i != length; ++i) {
        unsigned char c = characters[i];
        data[i] = c;
    }
    m_data = data;
}

StringImpl::~StringImpl()
{
    if (m_inTable)
        AtomicString::remove(this);
    deleteUCharVector(m_data);
}

StringImpl* StringImpl::empty()
{
    static StringImpl e;
    return &e;
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

PassRefPtr<StringImpl> StringImpl::substring(unsigned pos, unsigned len)
{
    if (pos >= m_length)
        return empty();
    if (len > m_length - pos)
        len = m_length - pos;
    return create(m_data + pos, len);
}

UChar32 StringImpl::characterStartingAt(unsigned i)
{
    if (U16_IS_SINGLE(m_data[i]))
        return m_data[i];
    if (i + 1 < m_length && U16_IS_LEAD(m_data[i]) && U16_IS_TRAIL(m_data[i + 1]))
        return U16_GET_SUPPLEMENTARY(m_data[i], m_data[i + 1]);
    return 0;
}

static Length parseLength(const UChar* data, unsigned length)
{
    if (length == 0)
        return Length(1, Relative);

    unsigned i = 0;
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;
    if (i < length && (data[i] == '+' || data[i] == '-'))
        ++i;
    while (i < length && Unicode::isDigit(data[i]))
        ++i;

    bool ok;
    int r = DeprecatedConstString(reinterpret_cast<const DeprecatedChar*>(data), i).string().toInt(&ok);

    /* Skip over any remaining digits, we are not that accurate (5.5% => 5%) */
    while (i < length && (Unicode::isDigit(data[i]) || data[i] == '.'))
        ++i;

    /* IE Quirk: Skip any whitespace (20 % => 20%) */
    while (i < length && isSpaceOrNewline(data[i]))
        ++i;

    if (ok) {
        if (i < length) {
            UChar next = data[i];
            if (next == '%')
                return Length(static_cast<double>(r), Percent);
            if (next == '*')
                return Length(r, Relative);
        }
        return Length(r, Fixed);
    } else {
        if (i < length) {
            UChar next = data[i];
            if (next == '*')
                return Length(1, Relative);
            if (next == '%')
                return Length(1, Relative);
        }
    }
    return Length(0, Relative);
}

Length StringImpl::toLength()
{
    return parseLength(m_data, m_length);
}

static int countCharacter(StringImpl* string, UChar character)
{
    int count = 0;
    int length = string->length();
    for (int i = 0; i < length; ++i)
            count += (*string)[i] == character;
    return count;
}

Length* StringImpl::toCoordsArray(int& len)
{
    StringBuffer spacified(m_length);
    for (unsigned i = 0; i < m_length; i++) {
        UChar cc = m_data[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = ' ';
        else
            spacified[i] = cc;
    }
    RefPtr<StringImpl> str = adopt(spacified);

    str = str->simplifyWhiteSpace();

    len = countCharacter(str.get(), ' ') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while ((pos2 = str->find(' ', pos)) != -1) {
        r[i++] = parseLength(str->characters() + pos, pos2 - pos);
        pos = pos2+1;
    }
    r[i] = parseLength(str->characters() + pos, str->length() - pos);

    ASSERT(i == len - 1);

    return r;
}

Length* StringImpl::toLengthArray(int& len)
{
    RefPtr<StringImpl> str = simplifyWhiteSpace();
    if (!str->length()) {
        len = 1;
        return 0;
    }

    len = countCharacter(str.get(), ',') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while ((pos2 = str->find(',', pos)) != -1) {
        r[i++] = parseLength(str->characters() + pos, pos2 - pos);
        pos = pos2+1;
    }

    ASSERT(i == len - 1);

    /* IE Quirk: If the last comma is the last char skip it and reduce len by one */
    if (str->length()-pos > 0)
        r[i] = parseLength(str->characters() + pos, str->length() - pos);
    else
        len--;

    return r;
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
    StringBuffer data(m_length);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return adopt(data);

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::toLower(data.characters(), length, m_data, m_length, &error);
    if (!error && realLength == length)
        return adopt(data);
    data.resize(realLength);
    Unicode::toLower(data.characters(), length, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::upper()
{
    bool error;
    int32_t length = Unicode::toUpper(0, 0, m_data, m_length, &error);
    StringBuffer data(length);
    Unicode::toUpper(data.characters(), length, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::secure(UChar aChar)
{
    int length = m_length;
    StringBuffer data(length);
    for (int i = 0; i < length; ++i)
        data[i] = aChar;
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::foldCase()
{
    StringBuffer data(m_length);
    int32_t length = m_length;

    // Do a faster loop for the case where all the characters are ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = toASCIILower(c);
    }
    if (!(ored & ~0x7F))
        return adopt(data);

    // Do a slower implementation for cases that include non-ASCII characters.
    bool error;
    int32_t realLength = Unicode::foldCase(data.characters(), length, m_data, m_length, &error);
    if (!error && realLength == length)
        return adopt(data);
    data.resize(realLength);
    Unicode::foldCase(data.characters(), length, m_data, m_length, &error);
    if (error)
        return this;
    return adopt(data);
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

int StringImpl::toInt(bool* ok)
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != m_length; ++i)
        if (!isSpaceOrNewline(m_data[i]))
            break;
    
    // Allow sign.
    if (i != m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    
    // Allow digits.
    for (; i != m_length; ++i)
        if (!Unicode::isDigit(m_data[i]))
            break;
    
    return DeprecatedConstString(reinterpret_cast<const DeprecatedChar*>(m_data), i).string().toInt(ok);
}

int64_t StringImpl::toInt64(bool* ok)
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != m_length; ++i)
        if (!isSpaceOrNewline(m_data[i]))
            break;
    
    // Allow sign.
    if (i != m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    
    // Allow digits.
    for (; i != m_length; ++i)
        if (!Unicode::isDigit(m_data[i]))
            break;
    
    return DeprecatedConstString(reinterpret_cast<const DeprecatedChar*>(m_data), i).string().toInt64(ok);
}

uint64_t StringImpl::toUInt64(bool* ok)
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != m_length; ++i)
        if (!isSpaceOrNewline(m_data[i]))
            break;

    // Allow digits.
    for (; i != m_length; ++i)
        if (!Unicode::isDigit(m_data[i]))
            break;
    
    return DeprecatedConstString(reinterpret_cast<const DeprecatedChar*>(m_data), i).string().toUInt64(ok);
}

double StringImpl::toDouble(bool* ok)
{
    if (!m_length) {
        if (ok)
            *ok = false;
        return 0;
    }
    char *end;
    CString latin1String = Latin1Encoding().encode(characters(), length());
    double val = kjs_strtod(latin1String.data(), &end);
    if (ok)
        *ok = end == 0 || *end == '\0';
    return val;
}

float StringImpl::toFloat(bool* ok)
{
    // FIXME: This will return ok even when the string fits into a double but not a float.
    return narrowPrecisionToFloat(toDouble(ok));
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

static bool equalIgnoringCase(const UChar* a, const char* b, int length)
{
    ASSERT(length >= 0);
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
    unsigned index = start;
    if (index >= m_length )
        return -1;
    while(index < m_length) {
        if (m_data[index] == c)
            return index;
        index++;
    }
    return -1;
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
    if (index >= (int)m_length || m_length == 0)
        return -1;

    if (index < 0)
        index += m_length;
    while (1) {
        if (m_data[index] == c)
            return index;
        if (index == 0)
            return -1;
        index--;
    }
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

    StringBuffer data(m_length);
    for (i = 0; i != m_length; ++i) {
        UChar ch = m_data[i];
        if (ch == oldC)
            ch = newC;
        data[i] = ch;
    }
    return adopt(data);
}

PassRefPtr<StringImpl> StringImpl::replace(unsigned position, unsigned lengthToReplace, StringImpl* str)
{
    position = min(position, length());
    lengthToReplace = min(lengthToReplace, length() - position);
    unsigned lengthToInsert = str ? str->length() : 0;
    if (!lengthToReplace && !lengthToInsert)
        return this;
    StringBuffer buffer(length() - lengthToReplace + lengthToInsert);
    memcpy(buffer.characters(), characters(), position * sizeof(UChar));
    if (str)
        memcpy(buffer.characters() + position, str->characters(), lengthToInsert * sizeof(UChar));
    memcpy(buffer.characters() + position + lengthToInsert, characters() + position + lengthToReplace,
        (length() - position - lengthToReplace) * sizeof(UChar));
    return adopt(buffer);
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
    
    StringBuffer data(m_length - matchCount + (matchCount * repStrLength));
    
    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data.characters() + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + 1;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(data.length()));

    return adopt(data);
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
    
    StringBuffer data(m_length + matchCount * (repStrLength - patternLength));
    
    // Construct the new data
    int srcSegmentEnd;
    int srcSegmentLength;
    srcSegmentStart = 0;
    int dstOffset = 0;
    
    while ((srcSegmentEnd = find(pattern, srcSegmentStart)) >= 0) {
        srcSegmentLength = srcSegmentEnd - srcSegmentStart;
        memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));
        dstOffset += srcSegmentLength;
        memcpy(data.characters() + dstOffset, replacement->m_data, repStrLength * sizeof(UChar));
        dstOffset += repStrLength;
        srcSegmentStart = srcSegmentEnd + patternLength;
    }

    srcSegmentLength = m_length - srcSegmentStart;
    memcpy(data.characters() + dstOffset, m_data + srcSegmentStart, srcSegmentLength * sizeof(UChar));

    ASSERT(dstOffset + srcSegmentLength == static_cast<int>(data.length()));

    return adopt(data);
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
PassRefPtr<StringImpl> StringImpl::createStrippingNullCharacters(const UChar* characters, unsigned length)
{
    ASSERT(characters);
    ASSERT(length);

    StringBuffer strippedCopy(length);
    int foundNull = 0;
    for (unsigned i = 0; i < length; i++) {
        int c = characters[i]; // more efficient than using UChar here (at least on Intel Mac OS)
        strippedCopy[i] = c;
        foundNull |= ~c;
    }
    if (!foundNull)
        return adoptRef(new StringImpl(strippedCopy.release(), length, AdoptBuffer()));
    unsigned strippedLength = 0;
    for (unsigned i = 0; i < length; i++) {
        if (int c = characters[i])
            strippedCopy[strippedLength++] = c;
    }
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

PassRefPtr<StringImpl> StringImpl::create(const UChar* characters, unsigned length)
{
    if (!characters || !length)
        return empty();
    return adoptRef(new StringImpl(characters, length));
}

PassRefPtr<StringImpl> StringImpl::create(const char* characters, unsigned length)
{
    if (!characters || !length)
        return empty();
    return adoptRef(new StringImpl(characters, length));
}

PassRefPtr<StringImpl> StringImpl::create(const char* string)
{
    if (!string)
        return empty();
    unsigned length = strlen(string);
    if (!length)
        return empty();
    return adoptRef(new StringImpl(string, length));
}

PassRefPtr<StringImpl> StringImpl::createWithTerminatingNullCharacter(const StringImpl& string)
{
    return adoptRef(new StringImpl(string, WithTerminatingNullCharacter()));
}

PassRefPtr<StringImpl> StringImpl::copy()
{
    return adoptRef(new StringImpl(m_data, m_length));
}

} // namespace WebCore
