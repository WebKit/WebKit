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
#include "Length.h"
#include "StringHash.h"
#include <kjs/identifier.h>
#include <kxmlcore/Assertions.h>
#include <string.h>
#include <unicode/ubrk.h>

using namespace KJS;
using namespace KXMLCore;

namespace WebCore {

static inline QChar* newQCharVector(unsigned n)
{
    return static_cast<QChar*>(fastMalloc(sizeof(QChar) * n));
}

static inline void deleteQCharVector(QChar* p)
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
    initWithQChar(str.unicode(), str.length());
}

StringImpl::StringImpl(const QChar* str, unsigned len)
{
    initWithQChar(str, len);
}

StringImpl::StringImpl(const char* str)
{
    initWithChar(str, strlen(str));
}

StringImpl::StringImpl(const char* str, unsigned len)
{
    initWithChar(str, len);
}

void StringImpl::initWithChar(const char* str, unsigned len)
{
    m_hash = 0;
    m_inTable = false;
    m_length = len;
    if (!m_length || !str) {
        m_data = 0;
        return;
    }
    
    m_data = newQCharVector(m_length);
    int i = m_length;
    QChar* ptr = m_data;
    while (i--)
        *ptr++ = *str++;
}

void StringImpl::initWithQChar(const QChar* str, unsigned len)
{
    m_hash = 0;
    m_inTable = false;
    m_length = len;
    if (!m_length || !str) {
        m_data = 0;
        return;
    }
    
    m_data = newQCharVector(len);
    memcpy(m_data, str, len * sizeof(QChar));
}

StringImpl::~StringImpl()
{
    if (m_inTable)
        AtomicString::remove(this);
    deleteQCharVector(m_data);
}

void StringImpl::append(const StringImpl* str)
{
    assert(!m_inTable);
    if(str && str->m_length != 0)
    {
        int newlen = m_length+str->m_length;
        QChar* c = newQCharVector(newlen);
        memcpy(c, m_data, m_length*sizeof(QChar));
        memcpy(c+m_length, str->m_data, str->m_length*sizeof(QChar));
        deleteQCharVector(m_data);
        m_data = c;
        m_length = newlen;
    }
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
        QChar* c = newQCharVector(newlen);
        memcpy(c, m_data, pos*sizeof(QChar));
        memcpy(c+pos, str->m_data, str->m_length*sizeof(QChar));
        memcpy(c+pos+str->m_length, m_data+pos, (m_length-pos)*sizeof(QChar));
        deleteQCharVector(m_data);
        m_data = c;
        m_length = newlen;
    }
}

void StringImpl::truncate(int len)
{
    assert(!m_inTable);
    if (len > (int)m_length)
        return;
    int nl = len < 1 ? 1 : len;
    QChar* c = newQCharVector(nl);
    memcpy(c, m_data, nl*sizeof(QChar));
    deleteQCharVector(m_data);
    m_data = c;
    m_length = len;
}

void StringImpl::remove(unsigned pos, int len)
{
    assert(!m_inTable);
    if (len <= 0)
        return;
    if (pos >= m_length )
        return;
    if ((unsigned)len > m_length - pos)
        len = m_length - pos;

    unsigned newLen = m_length-len;
    QChar* c = newQCharVector(newLen);
    memcpy(c, m_data, pos*sizeof(QChar));
    memcpy(c+pos, m_data+pos+len, (m_length-len-pos)*sizeof(QChar));
    deleteQCharVector(m_data);
    m_data = c;
    m_length = newLen;
}

StringImpl* StringImpl::split(unsigned pos)
{
    assert(!m_inTable);
    if( pos >=m_length ) return new StringImpl();

    unsigned newLen = m_length-pos;
    QChar* c = newQCharVector(newLen);
    memcpy(c, m_data+pos, newLen*sizeof(QChar));

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
    
    for (unsigned i = from; i < len; i++) {
        QChar c = m_data[i];
        if (c.unicode() <= 0x7F) {
            if (!isspace(c.unicode()))
                return false;
        } 
        else
            return false;
    }
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

static Length parseLength(const QChar* m_data, unsigned int m_length)
{
    if (m_length == 0)
        return Length(1, Relative);

    unsigned i = 0;
    while (i < m_length && m_data[i].isSpace())
        ++i;
    if (i < m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    while (i < m_length && m_data[i].isDigit())
        ++i;

    bool ok;
    int r = QConstString(m_data, i).string().toInt(&ok);

    /* Skip over any remaining digits, we are not that accurate (5.5% => 5%) */
    while (i < m_length && (m_data[i].isDigit() || m_data[i] == '.'))
        ++i;

    /* IE Quirk: Skip any whitespace (20 % => 20%) */
    while (i < m_length && m_data[i].isSpace())
        ++i;

    if (ok) {
        if (i == m_length)
            return Length(r, Fixed);
        else {
            const QChar* next = m_data+i;

            if (*next == '%')
                return Length(r, Percent);

            if (*next == '*')
                return Length(r, Relative);
        }
        return Length(r, Fixed);
    } else {
        if (i < m_length) {
            const QChar* next = m_data + i;

            if (*next == '*')
                return Length(1, Relative);

            if (*next == '%')
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
    QChar* spacified = newQCharVector(m_length);
    QChar space(' ');
    for(unsigned int i=0; i < m_length; i++) {
        QChar cc = m_data[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = space;
        else
            spacified[i] = cc;
    }
    DeprecatedString str(spacified, m_length);
    deleteQCharVector(spacified);

    str = str.simplifyWhiteSpace();

    len = str.contains(' ') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(' ', pos)) != -1) {
        r[i++] = parseLength(str.unicode() + pos, pos2-pos);
        pos = pos2+1;
    }
    r[i] = parseLength(str.unicode() + pos, str.length()-pos);

    return r;
}

Length* StringImpl::toLengthArray(int& len) const
{
    DeprecatedString str(m_data, m_length);
    str = str.simplifyWhiteSpace();

    len = str.contains(',') + 1;
    Length* r = new Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(',', pos)) != -1) {
        r[i++] = parseLength(str.unicode() + pos, pos2 - pos);
        pos = pos2+1;
    }

    /* IE Quirk: If the last comma is the last char skip it and reduce len by one */
    if (str.length()-pos > 0)
        r[i] = parseLength(str.unicode() + pos, str.length() - pos);
    else
        len--;

    return r;
}

bool StringImpl::isLower() const
{
    unsigned int i;
    for (i = 0; i < m_length; i++)
        if (m_data[i].lower() != m_data[i])
            return false;
    return true;
}

StringImpl* StringImpl::lower() const
{
    StringImpl* c = new StringImpl;
    if (!m_length)
        return c;

    c->m_data = newQCharVector(m_length);
    c->m_length = m_length;

    for (unsigned int i = 0; i < m_length; i++)
        c->m_data[i] = m_data[i].lower();

    return c;
}

StringImpl* StringImpl::upper() const
{
    StringImpl* c = new StringImpl;
    if (!m_length)
        return c;

    c->m_data = newQCharVector(m_length);
    c->m_length = m_length;

    for (unsigned int i = 0; i < m_length; i++)
        c->m_data[i] = m_data[i].upper();

    return c;
}

static UBreakIterator* getWordBreakIterator(const QChar* string, int length)
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
    if (status != U_ZERO_ERROR)
        return 0;

    return iterator;
}

StringImpl* StringImpl::capitalize(QChar previous) const
{
    StringImpl* capitalizedString = new StringImpl;
    if (!m_length)
        return capitalizedString;
    
    QChar* stringWithPrevious = newQCharVector(m_length + 1);
    stringWithPrevious[0] = previous;
    const char nonBreakingSpace = '\xa0';
    for (unsigned i = 1; i < m_length + 1; i++) {
        // Replace &nbsp with a real space since ICU no longer treats &nbsp as a word separator.
        if (m_data[i - 1] == nonBreakingSpace)
            stringWithPrevious[i] = ' ';
        else
            stringWithPrevious[i] = m_data[i - 1];
    }
    
    UBreakIterator* boundary = getWordBreakIterator(stringWithPrevious, m_length + 1);
    if (!boundary) {
        deleteQCharVector(stringWithPrevious);
        return capitalizedString;
    }
    
    capitalizedString->m_data = newQCharVector(m_length);
    capitalizedString->m_length = m_length;
    
    int32_t endOfWord;
    int32_t startOfWord = ubrk_first(boundary);
    for (endOfWord = ubrk_next(boundary); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(boundary)) {
        if (startOfWord != 0) // Ignore first char of previous string
            capitalizedString->m_data[startOfWord - 1] = stringWithPrevious[startOfWord].upper();
        for (int i = startOfWord + 1; i < endOfWord; i++)
            capitalizedString->m_data[i - 1] = stringWithPrevious[i];
    }
    
    deleteQCharVector(stringWithPrevious);
    return capitalizedString;
}

int StringImpl::toInt(bool* ok) const
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != m_length; ++i) {
        if (!m_data[i].isSpace())
            break;
    }
    
    // Allow sign.
    if (i != m_length && (m_data[i] == '+' || m_data[i] == '-'))
        ++i;
    
    // Allow digits.
    for (; i != m_length; ++i) {
        if (!m_data[i].isDigit())
            break;
    }
    
    return QConstString(m_data, i).string().toInt(ok);
}

static bool equal(const QChar* a, const char* b, int m_length)
{
    ASSERT(m_length >= 0);
    while (m_length--) {
        if (*a != *b)
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar* a, const char* b, int m_length)
{
    ASSERT(m_length >= 0);
    while (m_length--) {
        if (tolower(a->unicode()) != tolower(*b))
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar* a, const QChar* b, int m_length)
{
    ASSERT(m_length >= 0);
    while (m_length--) {
        if (tolower(a->unicode()) != tolower(b->unicode()))
            return false;
        a++; b++;
    }
    return true;
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
    
    const QChar* ptr = m_data + index - 1;
    if (caseSensitive) {
        QChar c = *chs;
        do {
            if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    } else {
        int lc = tolower((unsigned char)*chs);
        do {
            if (tolower((++ptr)->unicode()) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return m_length - chsLength - n + 1;
        } while (--n);
    }

    return -1;
}

int StringImpl::find(const QChar c, int start) const
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

      The hash value of a string is the sum of the cells of its
      QChars.
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

    const QChar* uthis = m_data + index;
    const QChar* ustr = str->m_data;
    unsigned hthis = 0;
    unsigned hstr = 0;
    if (caseSensitive) {
        for (int i = 0; i < lstr; i++) {
            hthis += uthis[i].unicode();
            hstr += ustr[i].unicode();
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(QChar)) == 0)
                return index + i;
            if (i == delta)
                return -1;
            hthis += uthis[i + lstr].unicode();
            hthis -= uthis[i].unicode();
            i++;
        }
    } else {
        for (int i = 0; i < lstr; i++ ) {
            hthis += tolower(uthis[i].unicode());
            hstr += tolower(ustr[i].unicode());
        }
        int i = 0;
        while (1) {
            if (hthis == hstr && equalCaseInsensitive(uthis + i, ustr, lstr))
                return index + i;
            if (i == delta)
                return -1;
            hthis += tolower(uthis[i + lstr].unicode());
            hthis -= tolower(uthis[i].unicode());
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

StringImpl* StringImpl::replace(QChar oldC, QChar newC)
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

    c->m_data = newQCharVector(m_length);
    c->m_length = m_length;

    for (i = 0; i != m_length; ++i) {
        QChar ch = m_data[i];
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

StringImpl* StringImpl::replace(QChar pattern, const StringImpl* str)
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
    const QChar* as = a->unicode();
    for (unsigned i = 0; i != length; ++i) {
        char c = b[i];
        if (!c)
            return false;
        if (as[i] != c)
            return false;
    }

    return !b[length];
}

bool equal(const char* a, const StringImpl* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = b->length();
    const QChar* bs = b->unicode();
    for (unsigned i = 0; i != length; ++i) {
        char c = a[i];
        if (!c)
            return false;
        if (c != bs[i])
            return false;
    }

    return !a[length];
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
    const QChar* as = a->unicode();
    for (unsigned i = 0; i != length; ++i) {
        char c = b[i];
        if (!c)
            return false;
        if (as[i].lower() != QChar(c).lower())
            return false;
    }

    return !b[length];
}

bool equalIgnoringCase(const char* a, const StringImpl* b)
{
    if (!a)
        return !b;
    if (!b)
        return !a;

    unsigned length = b->length();
    const QChar* bs = b->unicode();
    for (unsigned i = 0; i != length; ++i) {
        char c = a[i];
        if (!c)
            return false;
        if (QChar(c).lower() != bs[i].lower())
            return false;
    }

    return !a[length];
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned StringImpl::computeHash(const QChar* m_data, unsigned len)
{
    unsigned m_length = len;
    uint32_t hash = PHI;
    uint32_t tmp;
    
    int rem = m_length & 1;
    m_length >>= 1;
    
    // Main loop
    for (; m_length > 0; m_length--) {
        hash += m_data[0].unicode();
        tmp = (m_data[1].unicode() << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        m_data += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += m_data[0].unicode();
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

const char* StringImpl::ascii() const
{
    char* buffer = new char[m_length + 1];
    char* p = buffer;
    for (unsigned i = 0; i != m_length; ++i) {
        unsigned short c = m_data[i].unicode();
        if (c >= 0x20 && c < 0x7F)
            *p++ = c;
        else
            *p++ = '?';
    }
    *p++ = '\0';
    return buffer;
}

StringImpl::StringImpl(const Identifier& str)
{
    initWithQChar(reinterpret_cast<const QChar*>(str.data()), str.size());
}

StringImpl::StringImpl(const UString& str)
{
    initWithQChar(reinterpret_cast<const QChar*>(str.data()), str.size());
}

} // namespace WebCore
