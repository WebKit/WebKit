/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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

// For KHTML we need to avoid having static constructors.
// Our strategy is to declare the global objects with a different type (initialized to 0)
// and then use placement new to initialize the global objects later. This is not completely
// portable, and it would be good to figure out a 100% clean way that still avoids code that
// runs at init time.

#define KHTML_ATOMICSTRING_HIDE_GLOBALS 1

#include "dom_atomicstring.h"
#include "xml/dom_stringimpl.h"
#include "misc/hashset.h"

using khtml::HashSet;

namespace DOM {
   
static HashSet<DOMStringImpl *> stringTable;

inline unsigned hash(const char* const& c)
{
    return DOMStringImpl::computeHash(c);
}

inline bool equal(DOMStringImpl* const& r, const char* const& s)
{
    int length = r->l;
    const QChar *d = r->s;
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return s[length] == 0;
}

bool AtomicString::equal(const AtomicString &a, const char *b)
{ 
    DOMStringImpl *impl = a.m_string.implementation();
    if ((!impl || !impl->s) && !b)
        return true;
    if ((!impl || !impl->s) || !b)
        return false;
    return DOM::equal(impl, b); 
}

inline DOMStringImpl *convert(const char* const& c, unsigned hash)
{
    DOMStringImpl *r = new DOMStringImpl(c);
    r->_hash = hash;
    r->_inTable = true;

    return r; 
}

DOMStringImpl *AtomicString::add(const char *c)
{
    if (!c)
        return 0;
    int length = strlen(c);
    if (length == 0)
        return DOMStringImpl::empty();
    
    return *stringTable.insert<const char *, hash, DOM::equal, convert>(c).first;
}


struct QCharBuffer {
    const QChar *s;
    uint length;
};

inline unsigned hash(const QCharBuffer& buf)
{
    return DOMStringImpl::computeHash(buf.s, buf.length);
}

inline bool equal(DOMStringImpl* const& str, const QCharBuffer &buf)
{
    uint strLength = str->l;
    uint bufLength = buf.length;
    if (strLength != bufLength)
        return false;

    const uint32_t *strChars = reinterpret_cast<const uint32_t *>(str->s);
    const uint32_t *bufChars = reinterpret_cast<const uint32_t *>(buf.s);
    
    uint halfLength = strLength >> 1;
    for (uint i = 0; i != halfLength; ++i) {
        if (*strChars++ != *bufChars++)
            return false;
    }

    if (strLength & 1 && 
        *reinterpret_cast<const uint16_t *>(strChars) != *reinterpret_cast<const uint16_t *>(bufChars))
        return false;

    return true;
}

inline DOMStringImpl *convert(const QCharBuffer& buf, unsigned hash)
{
    DOMStringImpl *r = new DOMStringImpl(buf.s, buf.length);
    r->_hash = hash;
    r->_inTable = true;
    
    return r; 
}

DOMStringImpl *AtomicString::add(const QChar *s, int length)
{
    if (!s)
        return 0;

    if (length == 0)
        return DOMStringImpl::empty();
    
    QCharBuffer buf = {s, length}; 
    return *stringTable.insert<QCharBuffer, hash, DOM::equal, convert>(buf).first;
}

DOMStringImpl *AtomicString::add(DOMStringImpl *r)
{
    if (!r || r->_inTable)
        return r;

    if (r->l == 0)
        return DOMStringImpl::empty();
    
    DOMStringImpl *result = *stringTable.insert(r).first;
    if (result == r)
        r->_inTable = true;
    return result;
}

void AtomicString::remove(DOMStringImpl *r)
{
    stringTable.remove(r);
}

// Define an AtomicString-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.
#define DEFINE_GLOBAL(name) \
    void* name ## Atom[(sizeof(AtomicString) + sizeof(void*) - 1) / sizeof(void*)];

DEFINE_GLOBAL(null)
DEFINE_GLOBAL(empty)
DEFINE_GLOBAL(text)
DEFINE_GLOBAL(comment)
DEFINE_GLOBAL(star)

void AtomicString::init()
{
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        new (&nullAtom) AtomicString;
        new (&emptyAtom) AtomicString("");
        new (&textAtom) AtomicString("#text");
        new (&commentAtom) AtomicString("#comment");
        new (&starAtom) AtomicString("*");

        initialized = true;
    }
}

bool operator==(const AtomicString &a, const DOMString &b)
{
    return a.domString() == b;    
}
   
bool equalsIgnoreCase(const AtomicString &as, const DOMString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    return !strcasecmp(as.domString(), bs);
}

bool equalsIgnoreCase(const AtomicString &as, const AtomicString &bs)
{
    // returns true when equal, false otherwise (ignoring case)
    if (as == bs)
        return true;
    return !strcasecmp(as.domString(), bs.domString());
}

}
