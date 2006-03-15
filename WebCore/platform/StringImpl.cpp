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
#include <kxmlcore/Assertions.h>
#include <string.h>
#include <unicode/ubrk.h>

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

StringImpl::StringImpl(const QString& str)
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

StringImpl::StringImpl(const char* str, unsigned int len)
{
    initWithChar(str, len);
}

void StringImpl::initWithChar(const char* str, unsigned int len)
{
    _hash = 0;
    _inTable = false;
    l = len;
    if (!l || !str) {
        s = 0;
        return;
    }
    
    s = newQCharVector(l);
    int i = l;
    QChar* ptr = s;
    while (i--)
        *ptr++ = *str++;
}

void StringImpl::initWithQChar(const QChar* str, unsigned int len)
{
    _hash = 0;
    _inTable = false;
    l = len;
    if (!l || !str) {
        s = 0;
        return;
    }
    
    s = newQCharVector(len);
    memcpy(s, str, len * sizeof(QChar));
}

StringImpl::~StringImpl()
{
    if (_inTable)
        AtomicString::remove(this);
    deleteQCharVector(s);
}

void StringImpl::append(const StringImpl* str)
{
    assert(!_inTable);
    if(str && str->l != 0)
    {
        int newlen = l+str->l;
        QChar* c = newQCharVector(newlen);
        memcpy(c, s, l*sizeof(QChar));
        memcpy(c+l, str->s, str->l*sizeof(QChar));
        deleteQCharVector(s);
        s = c;
        l = newlen;
    }
}

void StringImpl::insert(const StringImpl* str, unsigned pos)
{
    assert(!_inTable);
    if (pos >= l) {
        append(str);
        return;
    }
    if (str && str->l != 0) {
        int newlen = l + str->l;
        QChar* c = newQCharVector(newlen);
        memcpy(c, s, pos*sizeof(QChar));
        memcpy(c+pos, str->s, str->l*sizeof(QChar));
        memcpy(c+pos+str->l, s+pos, (l-pos)*sizeof(QChar));
        deleteQCharVector(s);
        s = c;
        l = newlen;
    }
}

void StringImpl::truncate(int len)
{
    assert(!_inTable);
    if (len > (int)l)
        return;
    int nl = len < 1 ? 1 : len;
    QChar* c = newQCharVector(nl);
    memcpy(c, s, nl*sizeof(QChar));
    deleteQCharVector(s);
    s = c;
    l = len;
}

void StringImpl::remove(unsigned pos, int len)
{
    assert(!_inTable);
    if(len <= 0) return;
    if(pos >= l ) return;
    if((unsigned)len > l - pos)
    len = l - pos;

    unsigned newLen = l-len;
    QChar* c = newQCharVector(newLen);
    memcpy(c, s, pos*sizeof(QChar));
    memcpy(c+pos, s+pos+len, (l-len-pos)*sizeof(QChar));
    deleteQCharVector(s);
    s = c;
    l = newLen;
}

StringImpl* StringImpl::split(unsigned pos)
{
    assert(!_inTable);
    if( pos >=l ) return new StringImpl();

    unsigned newLen = l-pos;
    QChar* c = newQCharVector(newLen);
    memcpy(c, s+pos, newLen*sizeof(QChar));

    StringImpl* str = new StringImpl(s + pos, newLen);
    truncate(pos);
    return str;
}

bool StringImpl::containsOnlyWhitespace() const
{
    return containsOnlyWhitespace(0, l);
}

bool StringImpl::containsOnlyWhitespace(unsigned int from, unsigned int len) const
{
    if (!s)
        return true;
    
    for (unsigned i = from; i < len; i++) {
        QChar c = s[i];
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
    if (pos >= l) return
        new StringImpl;
    if (len > l - pos)
        len = l - pos;
    return new StringImpl(s + pos, len);
}

static Length parseLength(const QChar* s, unsigned int l)
{
    if (l == 0)
        return Length(1, Relative);

    unsigned i = 0;
    while (i < l && s[i].isSpace())
        ++i;
    if (i < l && (s[i] == '+' || s[i] == '-'))
        ++i;
    while (i < l && s[i].isDigit())
        ++i;

    bool ok;
    int r = QConstString(s, i).string().toInt(&ok);

    /* Skip over any remaining digits, we are not that accurate (5.5% => 5%) */
    while (i < l && (s[i].isDigit() || s[i] == '.'))
        ++i;

    /* IE Quirk: Skip any whitespace (20 % => 20%) */
    while (i < l && s[i].isSpace())
        ++i;

    if (ok) {
        if (i == l) {
            return Length(r, Fixed);
        } else {
            const QChar* next = s+i;

            if (*next == '%')
                return Length(r, Percent);

            if (*next == '*')
                return Length(r, Relative);
        }
        return Length(r, Fixed);
    } else {
        if (i < l) {
            const QChar* next = s + i;

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
    return parseLength(s, l);
}

Length* StringImpl::toCoordsArray(int& len) const
{
    QChar* spacified = newQCharVector(l);
    QChar space(' ');
    for(unsigned int i=0; i < l; i++) {
        QChar cc = s[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = space;
        else
            spacified[i] = cc;
    }
    QString str(spacified, l);
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
    QString str(s, l);
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
    for (i = 0; i < l; i++)
	if (s[i].lower() != s[i])
	    return false;
    return true;
}

StringImpl* StringImpl::lower() const
{
    StringImpl* c = new StringImpl;
    if (!l)
        return c;

    c->s = newQCharVector(l);
    c->l = l;

    for (unsigned int i = 0; i < l; i++)
	c->s[i] = s[i].lower();

    return c;
}

StringImpl* StringImpl::upper() const
{
    StringImpl* c = new StringImpl;
    if (!l)
        return c;

    c->s = newQCharVector(l);
    c->l = l;

    for (unsigned int i = 0; i < l; i++)
	c->s[i] = s[i].upper();

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
    ubrk_setText(iterator, reinterpret_cast<const UChar*>(string), length, &status);
    if (status != U_ZERO_ERROR)
        return 0;

    return iterator;
}

StringImpl* StringImpl::capitalize(QChar previous) const
{
    StringImpl* capitalizedString = new StringImpl;
    if (!l)
        return capitalizedString;
    
    QChar* stringWithPrevious = newQCharVector(l + 1);
    stringWithPrevious[0] = previous;
    for (unsigned i = 1; i < l + 1; i++)
        stringWithPrevious[i] = s[i - 1];
    
    UBreakIterator* boundary = getWordBreakIterator(stringWithPrevious, l + 1);
    if (!boundary) {
        deleteQCharVector(stringWithPrevious);
        return capitalizedString;
    }
    
    capitalizedString->s = newQCharVector(l);
    capitalizedString->l = l;
    
    int32_t endOfWord;
    int32_t startOfWord = ubrk_first(boundary);
    for (endOfWord = ubrk_next(boundary); endOfWord != UBRK_DONE; startOfWord = endOfWord, endOfWord = ubrk_next(boundary)) {
        if (startOfWord != 0) // Ignore first char of previous string
            capitalizedString->s[startOfWord - 1] = stringWithPrevious[startOfWord].upper();
        for (int i = startOfWord + 1; i < endOfWord; i++)
            capitalizedString->s[i - 1] = stringWithPrevious[i];
    }
    
    deleteQCharVector(stringWithPrevious);
    return capitalizedString;
}

int StringImpl::toInt(bool* ok) const
{
    unsigned i = 0;

    // Allow leading spaces.
    for (; i != l; ++i) {
        if (!s[i].isSpace()) {
            break;
        }
    }
    
    // Allow sign.
    if (i != l) {
        if (s[i] == '+' || s[i] == '-') {
            ++i;
        }
    }
    
    // Allow digits.
    for (; i != l; ++i) {
        if (!s[i].isDigit()) {
            break;
        }
    }
    
    return QConstString(s, i).string().toInt(ok);
}

static bool equal(const QChar* a, const char* b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (*a != *b)
            return false;
	a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar* a, const char* b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (tolower(a->unicode()) != tolower(*b))
            return false;
	a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar* a, const QChar* b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
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
// NOTE: Those time trials were done when this function was part of KWQ's QString
// It was copied here and changed slightly since.
int StringImpl::find(const char* chs, int index, bool caseSensitive) const
{
    if (!chs || index < 0)
        return -1;

    int chsLength = strlen(chs);
    int n = l - index;
    if (n < 0)
        return -1;
    n -= chsLength - 1;
    if (n <= 0)
        return -1;

    const char* chsPlusOne = chs + 1;
    int chsLengthMinusOne = chsLength - 1;
    
    const QChar* ptr = s + index - 1;
    if (caseSensitive) {
        QChar c = *chs;
        do {
            if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return l - chsLength - n + 1;
        } while (--n);
    } else {
        int lc = tolower((unsigned char)*chs);
        do {
            if (tolower((++ptr)->unicode()) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne))
                return l - chsLength - n + 1;
        } while (--n);
    }

    return -1;
}

int StringImpl::find(const QChar c, int start) const
{
    unsigned int index = start;
    if (index >= l )
        return -1;
    while(index < l) {
	if (s[index] == c)
            return index;
	index++;
    }
    return -1;
}

// This was copied from KWQ's QString and made to work here w/ small modifications.
// FIXME comments were from the QString version.
int StringImpl::find(const StringImpl* str, int index, bool caseSensitive) const
{
    // FIXME, use the first character algorithm
    /*
      We use some weird hashing for efficiency's sake.  Instead of
      comparing strings, we compare the sum of str with that of
      a part of this QString.  Only if that matches, we call memcmp
      or ucstrnicmp.

      The hash value of a string is the sum of the cells of its
      QChars.
    */
    ASSERT(str);
    if (index < 0)
	index += l;
    int lstr = str->l;
    int lthis = l - index;
    if ((unsigned)lthis > l)
	return -1;
    int delta = lthis - lstr;
    if (delta < 0)
	return -1;

    const QChar* uthis = s + index;
    const QChar* ustr = str->s;
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

bool StringImpl::endsWith(const StringImpl* s, bool caseSensitive) const
{
    ASSERT(s);
    int start = l - s->l;
    if (start >= 0)
        return (find(s, start, caseSensitive) == start);
    return false;
}

StringImpl* StringImpl::replace(QChar oldC, QChar newC)
{
    if (oldC == newC)
        return this;
    unsigned i;
    for (i = 0; i != l; ++i)
	if (s[i] == oldC)
            break;
    if (i == l)
        return this;

    StringImpl* c = new StringImpl;

    c->s = newQCharVector(l);
    c->l = l;

    for (i = 0; i != l; ++i) {
        QChar ch = s[i];
	if (ch == oldC)
            ch = newC;
        c->s[i] = ch;
    }

    return c;
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

    unsigned length = a->l;
    const QChar* as = a->s;
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

    unsigned length = b->l;
    const QChar* bs = b->s;
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

    unsigned length = a->l;
    const QChar* as = a->s;
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

    unsigned length = b->l;
    const QChar* bs = b->s;
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
unsigned StringImpl::computeHash(const QChar* s, unsigned len)
{
    unsigned l = len;
    uint32_t hash = PHI;
    uint32_t tmp;
    
    int rem = l & 1;
    l >>= 1;
    
    // Main loop
    for (; l > 0; l--) {
        hash += s[0].unicode();
        tmp = (s[1].unicode() << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += s[0].unicode();
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
unsigned StringImpl::computeHash(const char* s)
{
    // This hash is designed to work on 16-bit chunks at a time. But since the normal case
    // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    // were 16-bit chunks, which should give matching results

    unsigned l = strlen(s);
    uint32_t hash = PHI;
    uint32_t tmp;
    
    int rem = l & 1;
    l >>= 1;
    
    // Main loop
    for (; l > 0; l--) {
        hash += (unsigned char)s[0];
        tmp = ((unsigned char)s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }
    
    // Handle end case
    if (rem) {
        hash += (unsigned char)s[0];
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
    char* buffer = new char[l + 1];
    char* p = buffer;
    for (unsigned i = 0; i != l; ++i) {
        unsigned short c = s[i].unicode();
        if (c >= 0x20 && c < 0x7F)
            *p++ = c;
        else
            *p++ = '?';
    }
    *p++ = '\0';
    return buffer;
}

} // namespace WebCore

namespace KXMLCore {

const RefPtr<WebCore::StringImpl> HashTraits<RefPtr<WebCore::StringImpl> >::_deleted = new WebCore::StringImpl(static_cast<char*>(0), 0);

}
