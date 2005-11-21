/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "dom_stringimpl.h"

#include <kdebug.h>

#include <string.h>
#include "dom_atomicstring.h"

using namespace khtml;

namespace DOM {

using khtml::Fixed;

DOMStringImpl* DOMStringImpl::empty()
{
    static WithOneRef w;
    static DOMStringImpl e(w);
    return &e;
}

DOMStringImpl::DOMStringImpl(const QChar *str, unsigned len)
{
    _hash = 0;
    _inTable = false;
    bool havestr = str && len;
    s = QT_ALLOC_QCHAR_VEC(havestr ? len : 1);
    if (havestr) {
        memcpy( s, str, len * sizeof(QChar) );
        l = len;
    } else {
        // crash protection
        s[0] = 0x0;
        l = 0;
    }
}

DOMStringImpl::DOMStringImpl(const QString &string)
{
    const QChar *str = string.unicode();
    unsigned len = string.length();
    _hash = 0;
    _inTable = false;
    bool havestr = str && len;
    s = QT_ALLOC_QCHAR_VEC(havestr ? len : 1);
    if (havestr) {
        memcpy( s, str, len * sizeof(QChar) );
        l = len;
    } else {
        // crash protection
        s[0] = 0x0;
        l = 0;
    }
}

DOMStringImpl::DOMStringImpl(const char *str)
{
    _hash = 0;
    _inTable = false;
    if(str && *str)
    {
        l = strlen(str);
        s = QT_ALLOC_QCHAR_VEC( l );
        int i = l;
        QChar* ptr = s;
        while( i-- )
            *ptr++ = *str++;
    }
    else
    {
        s = QT_ALLOC_QCHAR_VEC( 1 );  // crash protection
        s[0] = 0x0; // == QChar::null;
        l = 0;
    }
}

DOMStringImpl::DOMStringImpl(const char *str, unsigned int len)
{
    _hash = 0;
    _inTable = false;
    l = len;
    if (!l || !str)
        return;
    
    s = QT_ALLOC_QCHAR_VEC(l);
    int i = l;
    QChar* ptr = s;
    while( i-- )
        *ptr++ = *str++;
}

DOMStringImpl::DOMStringImpl(const QChar &ch) {
    _hash = 0;
    _inTable = false;
    s = QT_ALLOC_QCHAR_VEC( 1 );
    s[0] = ch;
    l = 1;
}

DOMStringImpl::~DOMStringImpl()
{
    if (_inTable)
        AtomicString::remove(this);
    if (s)
        QT_DELETE_QCHAR_VEC(s);
}

void DOMStringImpl::append(DOMStringImpl *str)
{
    assert(!_inTable);
    if(str && str->l != 0)
    {
        int newlen = l+str->l;
        QChar *c = QT_ALLOC_QCHAR_VEC(newlen);
        memcpy(c, s, l*sizeof(QChar));
        memcpy(c+l, str->s, str->l*sizeof(QChar));
        if(s) QT_DELETE_QCHAR_VEC(s);
        s = c;
        l = newlen;
    }
}

void DOMStringImpl::insert(DOMStringImpl *str, uint pos)
{
    assert(!_inTable);
    if(pos > l)
    {
        append(str);
        return;
    }
    if(str && str->l != 0)
    {
        int newlen = l+str->l;
        QChar *c = QT_ALLOC_QCHAR_VEC(newlen);
        memcpy(c, s, pos*sizeof(QChar));
        memcpy(c+pos, str->s, str->l*sizeof(QChar));
        memcpy(c+pos+str->l, s+pos, (l-pos)*sizeof(QChar));
        if(s) QT_DELETE_QCHAR_VEC(s);
        s = c;
        l = newlen;
    }
}

void DOMStringImpl::truncate(int len)
{
    assert(!_inTable);
    if(len > (int)l) return;

    int nl = len < 1 ? 1 : len;
    QChar *c = QT_ALLOC_QCHAR_VEC(nl);
    memcpy(c, s, nl*sizeof(QChar));
    if(s) QT_DELETE_QCHAR_VEC(s);
    s = c;
    l = len;
}

void DOMStringImpl::remove(uint pos, int len)
{
    assert(!_inTable);
    if(len <= 0) return;
    if(pos >= l ) return;
    if((unsigned)len > l - pos)
    len = l - pos;

    uint newLen = l-len;
    QChar *c = QT_ALLOC_QCHAR_VEC(newLen);
    memcpy(c, s, pos*sizeof(QChar));
    memcpy(c+pos, s+pos+len, (l-len-pos)*sizeof(QChar));
    if(s) QT_DELETE_QCHAR_VEC(s);
    s = c;
    l = newLen;
}

DOMStringImpl *DOMStringImpl::split(uint pos)
{
    assert(!_inTable);
    if( pos >=l ) return new DOMStringImpl();

    uint newLen = l-pos;
    QChar *c = QT_ALLOC_QCHAR_VEC(newLen);
    memcpy(c, s+pos, newLen*sizeof(QChar));

    DOMStringImpl *str = new DOMStringImpl(s + pos, newLen);
    truncate(pos);
    return str;
}

bool DOMStringImpl::containsOnlyWhitespace() const
{
    return containsOnlyWhitespace(0, l);
}

bool DOMStringImpl::containsOnlyWhitespace(unsigned int from, unsigned int len) const
{
    if (!s)
        return true;
    
    for (uint i = from; i < len; i++) {
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
    
DOMStringImpl *DOMStringImpl::substring(uint pos, uint len)
{
    if (pos >=l) return new DOMStringImpl();
    if (len > l - pos)
    len = l - pos;
    return new DOMStringImpl(s + pos, len);
}

static Length parseLength(QChar *s, unsigned int l)
{
    if (l == 0) {
        return Length(1, Relative);
    }

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
            const QChar* next = s+i;

            if (*next == '*')
                return Length(1, Relative);

            if (*next == '%')
                return Length(1, Relative);
        }
    }
    return Length(0, Relative);
}

Length DOMStringImpl::toLength() const
{
    return parseLength(s,l);
}

khtml::Length* DOMStringImpl::toCoordsArray(int& len) const
{
    QChar spacified[l];
    QChar space(' ');
    for(unsigned int i=0; i < l; i++) {
        QChar cc = s[i];
        if (cc > '9' || (cc < '0' && cc != '-' && cc != '*' && cc != '.'))
            spacified[i] = space;
        else
            spacified[i] = cc;
    }
    QString str(spacified, l);
    str = str.simplifyWhiteSpace();

    len = str.contains(' ') + 1;
    khtml::Length* r = new khtml::Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(' ', pos)) != -1) {
        r[i++] = parseLength((QChar *) str.unicode()+pos, pos2-pos);
        pos = pos2+1;
    }
    r[i] = parseLength((QChar *) str.unicode()+pos, str.length()-pos);

    return r;
}

khtml::Length* DOMStringImpl::toLengthArray(int& len) const
{
    QString str(s, l);
    str = str.simplifyWhiteSpace();

    len = str.contains(',') + 1;
    khtml::Length* r = new khtml::Length[len];

    int i = 0;
    int pos = 0;
    int pos2;

    while((pos2 = str.find(',', pos)) != -1) {
        r[i++] = parseLength((QChar *) str.unicode()+pos, pos2-pos);
        pos = pos2+1;
    }

    /* IE Quirk: If the last comma is the last char skip it and reduce len by one */
    if (str.length()-pos > 0)
        r[i] = parseLength((QChar *) str.unicode()+pos, str.length()-pos);
    else
        len--;

    return r;
}

bool DOMStringImpl::isLower() const
{
    unsigned int i;
    for (i = 0; i < l; i++)
	if (s[i].lower() != s[i])
	    return false;
    return true;
}

DOMStringImpl *DOMStringImpl::lower() const
{
    DOMStringImpl *c = new DOMStringImpl;
    if(!l) return c;

    c->s = QT_ALLOC_QCHAR_VEC(l);
    c->l = l;

    for (unsigned int i = 0; i < l; i++)
	c->s[i] = s[i].lower();

    return c;
}

DOMStringImpl *DOMStringImpl::upper() const
{
    DOMStringImpl *c = new DOMStringImpl;
    if(!l) return c;

    c->s = QT_ALLOC_QCHAR_VEC(l);
    c->l = l;

    for (unsigned int i = 0; i < l; i++)
	c->s[i] = s[i].upper();

    return c;
}

DOMStringImpl *DOMStringImpl::capitalize() const
{
    DOMStringImpl *c = new DOMStringImpl;
    bool haveCapped = false;
    if(!l) return c;

    c->s = QT_ALLOC_QCHAR_VEC(l);
    c->l = l;

    if ( l ) c->s[0] = s[0].upper();
    
    // This patch takes care of a lot of the text_transform: capitalize problems, particularly
    // with the apostrophe. But it is just a temporary fix until we implement UBreakIterator as a 
    // way to determine when to break for words.
    for (unsigned int i = 0; i < l; i++) {
        if (haveCapped) {
            if (s[i].isSpace()) 
                haveCapped = false;
            c->s[i] = s[i];
        } else if (s[i].isLetterOrNumber()) {
            c->s[i] = s[i].upper();
            haveCapped = true;
        } else 
            c->s[i] = s[i];
    }
    
    return c;
}

int DOMStringImpl::toInt(bool *ok) const
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

DOMStringImpl *DOMStringImpl::replace(QChar oldC, QChar newC)
{
    if (oldC == newC)
        return this;
    unsigned i;
    for (i = 0; i != l; ++i)
	if (s[i] == oldC)
            break;
    if (i == l)
        return this;

    DOMStringImpl *c = new DOMStringImpl;

    c->s = QT_ALLOC_QCHAR_VEC(l);
    c->l = l;

    for (i = 0; i != l; ++i) {
        QChar ch = s[i];
	if (ch == oldC)
            ch = newC;
        c->s[i] = ch;
    }

    return c;
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned DOMStringImpl::computeHash(const QChar *s, int len)
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
unsigned DOMStringImpl::computeHash(const char *s)
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

const char *DOMStringImpl::ascii() const
{
    char *buffer = new char[l + 1];
    char *p = buffer;
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

} // namespace DOM
