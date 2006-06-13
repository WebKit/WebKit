/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "StringImpl.h"

#include "AtomicString.h"
#include "DeprecatedString.h"
#include "Length.h"
#include "StringHash.h"
#include <kjs/identifier.h>
#include <wtf/Assertions.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <assert.h>

using namespace WTF;

using KJS::Identifier;
using KJS::UString;

namespace WebCore {

const UChar nonBreakingSpace = 0xA0;

static inline UChar* newUCharVector(unsigned n)
{
    return static_cast<UChar*>(fastMalloc(sizeof(UChar) * n));
}

static inline void deleteUCharVector(UChar* p)
{
    fastFree(p);
}

StringImpl* StringImpl::empty()
{
    static WithOneRef w;
    static StringImpl e(w);
    return &e;
}

StringImpl::StringImpl(const DeprecatedString& str)
{
    init(reinterpret_cast<const UChar*>(str.unicode()), str.length());
}

StringImpl::StringImpl(const UChar* str, unsigned len)
{
    init(str, len);
}

StringImpl::StringImpl(const char* str)
{
    init(str, strlen(str));
}

StringImpl::StringImpl(const char* str, unsigned len)
{
    init(str, len);
}

void StringImpl::init(const char* str, unsigned len)
{
    m_hash = 0;
    m_inTable = false;
    m_length = len;
    if (!m_length || !str) {
        m_data = 0;
        return;
    }
    
    m_data = newUCharVector(m_length);
    int i = m_length;
    UChar* ptr = m_data;
    while (i--) {
        unsigned char c = *str++;
        *ptr++ = c;
    }
}

void StringImpl::init(const UChar* str, unsigned len)
{
    m_hash = 0;
    m_inTable = false;
    m_length = len;
    if (!m_length || !str) {
        m_data = 0;
        return;
    }
    
    m_data = newUCharVector(len);
    memcpy(m_data, str, len * sizeof(UChar));
}

StringImpl::~StringImpl()
{
    if (m_inTable)
        AtomicString::remove(this);
    deleteUCharVector(m_data);
}

void StringImpl::append(const StringImpl* str)
{
    assert(!m_inTable);
    if(str && str->m_length != 0)
    {
        int newlen = m_length+str->m_length;
        UChar* c = newUCharVector(newlen);
        memcpy(c, m_data, m_length * sizeof(UChar));
        memcpy(c + m_length, str->m_data, str->m_length * sizeof(UChar));
        deleteUCharVector(m_data);
        m_data = c;
        m_length = newlen;
    }
}

void StringImpl::append(char c)
{
    append(UChar(c));
}

void StringImpl::append(UChar c)
{
    assert(!m_inTable);
    UChar* nc = newUCharVector(m_length + 1);
    memcpy(nc, m_data, m_length * sizeof(UChar));
    nc[m_length] = c;
    deleteUCharVector(m_data);
    m_data = nc;
    m_length++;
}

void StringImpl::insert(const StringImpl* str, unsigned pos)
{
    assert(!m_inTable);
    if (pos >= m_length) {
        append(str);
        return;
    }
    if (str && str->m_length != 0) {
        int newlen = m_length + str->m_length;
        UChar* c = newUCharVector(newlen);
        memcpy(c, m_data, pos * sizeof(UChar));
        memcpy(c + pos, str->m_data, str->m_length * sizeof(UChar));
        memcpy(c + pos + str->m_length, m_data + pos, (m_length - pos) * sizeof(UChar));
        deleteUCharVector(m_data);
        m_data = c;
        m_length = newlen;
    }
}

void StringImpl::truncate(int len)
{
    assert(!m_inTable);
    if (len >= (int)m_length)
        return;
    int nl = len < 1 ? 1 : len;
    UChar* c = newUCharVector(nl);
    memcpy(c, m_data, nl * sizeof(UChar));
    deleteUCharVector(m_data);
    m_data = c;
    m_length = len;
}

void StringImpl::remove(unsigned pos, int len)
{
    assert(!m_inTable);
    if (len <= 0)
        return;
    if (pos >= m_length)
        return;
    if ((unsigned)len > m_length - pos)
        len = m_length - pos;

    unsigned newLen = m_length-len;
    UChar* c = newUCharVector(newLen);
    memcpy(c, m_data, pos * sizeof(UChar));
    memcpy(c + pos, m_data + pos + len, (m_length - len - pos) * sizeof(UChar));
    deleteUCharVector(m_data);
    m_data = c;
    m_length = newLen;
}

StringImpl* StringImpl::split(unsigned pos)
{
    assert(!m_inTable);
    if( pos >=m_length ) return new StringImpl();

    unsigned newLen = m_length-pos;
    UChar* c = newUCharVector(newLen);
    memcpy(c, m_data + pos, newLen * sizeof(UChar));

    StringImpl* str = new StringImpl(m_data + pos, newLen);
    truncate(pos);
    return str;
}

bool StringImpl::containsOnlyWhitespace() const
{
    return containsOnlyWhitespace(0, m_length);
}

bool StringImpl::containsOnlyWhitespace(unsigned from, unsigned len) const
{
    if (!m_data)
        return true;
    // FIXME: Both the definition of what whitespace is, and the definition of what
    // the "len" parameter means are different here from what's done in RenderText.
    // FIXME: No range checking here.
    for (unsigned i = from; i < len; i++)
        if (m_data[i] > 0x7F || !isspace(m_data[i]))
            return false;
    return true;
}
    
StringImpl* StringImpl::substring(unsigned pos, unsigned len)
{
    if (pos >= m_length) return
        new StringImpl;
    if (len > m_length - pos)
        len = m_length - pos;
    return new StringImpl(m_data + pos, len);
}

static Length parseLength(const UChar* m_data, unsigned int m_length)
{
    if (m_length == 0)
        return Length(1, Relative);

    unsigned i = 0;
    while (i < m_length && QChar(m_data[i]).isSpace())
        ++i;
    if (i < m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    while (i < m_length && u_isdigit(m_data[i]))
        ++i;

    bool ok;
    int r = QConstString(reinterpret_cast<const QChar*>(m_data), i).string().toInt(&ok);

    /* Skip over any remaining digits, we are not that accurate (5.5% => 5%) */
    while (i < m_length && (u_isdigit(m_data[i]) || m_data[i] == '.'))
        ++i;

    /* IE Quirk: Skip any whitespace (20 % => 20%) */
    while (i < m_length && QChar(m_data[i]).isSpace())
        ++i;

    if (ok) {
        if (i < m_length) {
            UChar next = m_data[i];
            if (next == '%')
                return Length(r, Percent);
            if (next == '*')
                return Length(r, Relative);
        }
        return Length(r, Fixed);
    } else {
        if (i < m_length) {
            UChar next = m_data[i];
            if (next == '*')
                return Length(1, Relative);
            if (next == '%')
                return Length(1, Relative);
        }
    }
    return Length(0, Relative);
}

Length StringImpl::toLength() const
{
    return parseLength(m_data, m_length);
}

Length* StringImpl::toCoordsArray(int& len) const
{
    UChar* spacified = newUCharVector(m_length);
    for (unsigned i = 0; i < m_length; i++) {
        UChar cc = m_data[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = ' ';
        else
            spacified[i] = cc;
    }
    DeprecatedString str(reinterpret_cast<const QChar*>(spacified), m_length);
    deleteUCharVector(spacified);

    str = str.simplifyWhiteSpace();

    len = str.contains(' ') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(' ', pos)) != -1) {
        r[i++] = parseLength(reinterpret_cast<const UChar*>(str.unicode()) + pos, pos2 - pos);
        pos = pos2+1;
    }
    r[i] = parseLength(reinterpret_cast<const UChar*>(str.unicode()) + pos, str.length() - pos);

    return r;
}

Length* StringImpl::toLengthArray(int& len) const
{
    if (!length()) {
        len = 1;
        return 0;
    }
    DeprecatedString str(reinterpret_cast<const QChar*>(m_data), m_length);
    str = str.simplifyWhiteSpace();

    len = str.contains(',') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while ((pos2 = str.find(',', pos)) != -1) {
        r[i++] = parseLength(reinterpret_cast<const UChar*>(str.unicode()) + pos, pos2 - pos);
        pos = pos2+1;
    }

    /* IE Quirk: If the last comma is the last char skip it and reduce len by one */
    if (str.length()-pos > 0)
        r[i] = parseLength(reinterpret_cast<const UChar*>(str.unicode()) + pos, str.length() - pos);
    else
        len--;

    return r;
}

bool StringImpl::isLower() const
{
    // Do a quick check for the case where it's all ASCII.
    int allLower = true;
    UChar ored = 0;
    for (unsigned i = 0; i < m_length; i++) {
        UChar c = m_data[i];
        allLower &= islower(c);
        ored |= c;
    }
    if (!(ored & ~0x7F))
        return allLower;

    // Do a slower check for the other cases.
    UBool allLower2 = true;
    for (unsigned i = 0; i < m_length; i++)
        allLower2 &= u_islower(m_data[i]);
    return allLower2;
}

StringImpl* StringImpl::lower() const
{
    StringImpl* c = new StringImpl;
    if (!m_length)
        return c;

    UChar* data = newUCharVector(m_length);
    int length = m_length;

    c->m_data = data;
    c->m_length = length;

    // Do a faster loop for the case where it's all ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++) {
        UChar c = m_data[i];
        ored |= c;
        data[i] = tolower(c);
    }
    if (!(ored & ~0x7F))
        return c;

    UErrorCode status = U_ZERO_ERROR;
    int32_t realLength = u_strToLower(data, length, m_data, length, "", &status);
    if (U_SUCCESS(status) && realLength == length)
        return c;

    if (realLength > length) {
        deleteUCharVector(data);
        data = newUCharVector(realLength);
    }
    length = realLength;

    c->m_data = data;
    c->m_length = length;

    status = U_ZERO_ERROR;
    u_strToLower(data, length, m_data, m_length, "", &status);
    if (U_FAILURE(status)) {
        c->ref();
        c->deref();
        return copy();
    }
    return c;
}

StringImpl* StringImpl::upper() const
{
    StringImpl* c = new StringImpl;
    if (!m_length)
        return c;
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = u_strToUpper(0, 0, m_data, m_length, "", &status);
    c->m_data = newUCharVector(length);
    c->m_length = length;
    status = U_ZERO_ERROR;
    u_strToUpper(c->m_data, length, m_data, m_length, "", &status);
    if (U_FAILURE(status)) {
        c->ref();
        c->deref();
        return copy();
    }
    return c;
}

StringImpl* StringImpl::foldCase() const
{
    StringImpl* c = new StringImpl;
    if (!m_length)
        return c;
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = u_strFoldCase(0, 0, m_data, m_length, U_FOLD_CASE_DEFAULT, &status);
    c->m_data = newUCharVector(length);
    c->m_length = length;
    status = U_ZERO_ERROR;
    u_strFoldCase(c->m_data, length, m_data, m_length, U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status)) {
        c->ref();
        c->deref();
        return copy();
    }
    return c;
}

static UBreakIterator* getWordBreakIterator(const UChar* string, int length)
{
    // The locale is currently ignored when determining character cluster breaks.
    // This may change in the future, according to Deborah Goldsmith.
    static bool createdIterator = false;
    static UBreakIterator* iterator;
    UErrorCode status;
    if (!createdIterator) {
        status = U_ZERO_ERROR;
        iterator = ubrk_open(UBRK_WORD, "en_us", 0, 0, &status);
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    status = U_ZERO_ERROR;
    ubrk_setText(iterator, reinterpret_cast<const ::UChar*>(string), length, &status);
    if (U_FAILURE(status))
        return 0;

    return iterator;
}

StringImpl* StringImpl::capitalize(UChar previous) const
{
    StringImpl* capitalizedString = new StringImpl;
    if (!m_length)
        return capitalizedString;
    
    UChar* stringWithPrevious = newUCharVector(m_length + 1);
    stringWithPrevious[0] = previous;
    for (unsigned i = 1; i < m_length + 1; i++) {
        // Replace &nbsp with a real space since ICU no longer treats &nbsp as a word separator.
        if (m_data[i - 1] == nonBreakingSpace)
            stringWithPrevious[i] = ' ';
        else
            stringWithPrevious[i] = m_data[i - 1];
    }

    UBreakIterator* boundary = getWordBreakIterator(stringWithPrevious, m_length + 1);
    if (!boundary) {
        deleteUCharVector(stringWithPrevious);
        return capitalizedString;
    }
    
    capitalizedString->m_data = newUCharVector(m_length);
    capitalizedString->m_length = m_length;
    
    int32_t endOfWord;
    int32_t startOfWord = ubrk_first(boundary);
    for (endOfWord = ubrk_next(boundary); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(boundary)) {
        if (startOfWord != 0) // Ignore first char of previous string
            capitalizedString->m_data[startOfWord - 1] = u_totitle(stringWithPrevious[startOfWord]);
        for (int i = startOfWord + 1; i < endOfWord; i++)
            capitalizedString->m_data[i - 1] = stringWithPrevious[i];
    }
    
    deleteUCharVector(stringWithPrevious);
    return capitalizedString;
}

int StringImpl::toInt(bool* ok) const
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != m_length; ++i)
        if (!QChar(m_data[i]).isSpace())
            break;
    
    // Allow sign.
    if (i != m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    
    // Allow digits.
    for (; i != m_length; ++i)
        if (!u_isdigit(m_data[i]))
            break;
    
    return QConstString(reinterpret_cast<const QChar*>(m_data), i).string().toInt(ok);
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
        if (u_foldCase(*a++, U_FOLD_CASE_DEFAULT) != u_foldCase(bc, U_FOLD_CASE_DEFAULT))
            return false;
    }
    return true;
}

static inline bool equalIgnoringCase(const UChar* a, const UChar* b, int length)
{
    ASSERT(length >= 0);
    return u_memcasecmp(a, b, length, U_FOLD_CASE_DEFAULT) == 0;
}

// This function should be as fast as possible, every little bit helps.
// Our usage patterns are typically small strings.  In time trials
// this simplistic algorithm is much faster than Boyer-Moore or hash
// based algorithms.
// NOTE: Those time trials were done when this function was part of KWQ's DeprecatedString
// It was copied here and changed slightly since.
int StringImpl::find(const char* chs, int index, bool caseSensitive) const
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
        UChar lc = u_foldCase(*chs, U_FOLD_CASE_DEFAULT);
        do {
            if (u_foldCase(*++ptr, U_FOLD_CASE_DEFAULT) == lc
                    && equalIgnoringCase(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    }

    return -1;
}

int StringImpl::find(const UChar c, int start) const
{
    unsigned int index = start;
    if (index >= m_length )
        return -1;
    while(index < m_length) {
        if (m_data[index] == c)
            return index;
        index++;
    }
    return -1;
}

// This was copied from DeprecatedString and made to work here w/ small modifications.
// FIXME comments were from the DeprecatedString version.
int StringImpl::find(const StringImpl* str, int index, bool caseSensitive) const
{
    // FIXME, use the first character algorithm
    /*
      We use some weird hashing for efficiency's sake.  Instead of
      comparing strings, we compare the sum of str with that of
      a part of this DeprecatedString.  Only if that matches, we call memcmp
      or ucstrnicmp.

      The hash value of a string is the sum of its characters.
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
            hthis += tolower(uthis[i]);
            hstr += tolower(ustr[i]);
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && equalIgnoringCase(uthis + i, ustr, lstr))
                return index + i;
            if (i == delta)
                return -1;
            hthis += tolower(uthis[i + lstr]);
            hthis -= tolower(uthis[i]);
            i++;
        }
    }
}

bool StringImpl::endsWith(const StringImpl* m_data, bool caseSensitive) const
{
    ASSERT(m_data);
    int start = m_length - m_data->m_length;
    if (start >= 0)
        return (find(m_data, start, caseSensitive) == start);
    return false;
}

StringImpl* StringImpl::replace(UChar oldC, UChar newC)
{
    if (oldC == newC)
        return this;
    unsigned i;
    for (i = 0; i != m_length; ++i)
        if (m_data[i] == oldC)
            break;
    if (i == m_length)
        return this;

    StringImpl* c = new StringImpl;

    c->m_data = newUCharVector(m_length);
    c->m_length = m_length;

    for (i = 0; i != m_length; ++i) {
        UChar ch = m_data[i];
        if (ch == oldC)
            ch = newC;
        c->m_data[i] = ch;
    }

    return c;
}

StringImpl* StringImpl::replace(unsigned index, unsigned len, const StringImpl* str)
{
    StringImpl* m_data = copy();
    m_data->remove(index, len);
    m_data->insert(str, index);
    return m_data;
}

StringImpl* StringImpl::replace(UChar pattern, const StringImpl* str)
{
    int slen = str ? str->length() : 0;
    int index = 0;
    StringImpl* result = this;
    while ((index = result->find(pattern, index)) >= 0) {
        result = result->replace(index, 1, str);
        index += slen;
    }
    return result;
}

bool equal(const StringImpl* a, const StringImpl* b)
{
    return StrHash<StringImpl*>::equal(a, b);
}

bool equal(const StringImpl* a, const char* b)
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

bool equalIgnoringCase(const StringImpl* a, const StringImpl* b)
{
    return CaseInsensitiveHash::equal(a, b);
}

bool equalIgnoringCase(const StringImpl* a, const char* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = a->length();
    const UChar* as = a->characters();

    // Do a faster loop for the case where it's all ASCII.
    UChar ored = 0;
    bool equal = true;
    for (unsigned i = 0; i != length; ++i) {
        unsigned char bc = b[i];
        if (!bc)
            return false;
        UChar ac = as[i];
        ored |= ac;
        equal &= tolower(ac) == tolower(bc);
    }

    if (ored & ~0x7F) {
        equal = true;
        for (unsigned i = 0; i != length; ++i) {
            unsigned char bc = b[i];
            equal &= u_foldCase(as[i], U_FOLD_CASE_DEFAULT) == u_foldCase(bc, U_FOLD_CASE_DEFAULT);
        }
    }

    return equal && !b[length];
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned StringImpl::computeHash(const UChar* m_data, unsigned len)
{
    unsigned m_length = len;
    uint32_t hash = PHI;
    uint32_t tmp;
    
    int rem = m_length & 1;
    m_length >>= 1;
    
    // Main loop
    for (; m_length > 0; m_length--) {
        hash += m_data[0];
        tmp = (m_data[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        m_data += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += m_data[0];
        hash ^= hash << 11;
        hash += hash >> 17;
    }

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;
    
    return hash;
}

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned StringImpl::computeHash(const char* m_data)
{
    // This hash is designed to work on 16-bit chunks at a time. But since the normal case
    // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    // were 16-bit chunks, which should give matching results

    unsigned m_length = strlen(m_data);
    uint32_t hash = PHI;
    uint32_t tmp;
    
    int rem = m_length & 1;
    m_length >>= 1;
    
    // Main loop
    for (; m_length > 0; m_length--) {
        hash += (unsigned char)m_data[0];
        tmp = ((unsigned char)m_data[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        m_data += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += (unsigned char)m_data[0];
        hash ^= hash << 11;
        hash += hash >> 17;
    }

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // this avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;
    
    return hash;
}

Vector<char> StringImpl::ascii() const
{
    Vector<char> buffer(m_length + 1);
    
    unsigned i;
    for (i = 0; i != m_length; ++i) {
        UChar c = m_data[i];
        if ((c >= 0x20 && c < 0x7F) || c == 0x00)
            buffer[i] = c;
        else
            buffer[i] = '?';
    }
    buffer[i] = '\0';
    return buffer;
}

StringImpl::StringImpl(const Identifier& str)
{
    init(reinterpret_cast<const UChar*>(str.data()), str.size());
}

StringImpl::StringImpl(const UString& str)
{
    init(reinterpret_cast<const UChar*>(str.data()), str.size());
}

} // namespace WebCore
