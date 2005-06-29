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

#if APPLE_CHANGES
#define AVOID_STATIC_CONSTRUCTORS 1
#endif

#if AVOID_STATIC_CONSTRUCTORS
#define KHTML_ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "dom_atomicstring.h"
#include "xml/dom_stringimpl.h"
#include "hashset.h"

using khtml::HashSet;

namespace DOM {
   
inline unsigned hash(DOMStringImpl* const& s) 
{
    return s->hash();
}

bool equal(DOMStringImpl* const& r, DOMStringImpl* const& b)
{
    if (r == b) return true;
    if (!r || !b) return false;
    uint length = r->l;
    if (length != b->l)
        return false;
    const QChar *d = r->s;
    const QChar *s = b->s;
    for (uint i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

static HashSet<DOMStringImpl *, hash, equal> stringTable;

inline unsigned hash(const char* const& c)
{
    return DOMStringImpl::computeHash(c);
}

inline bool equal(DOMStringImpl* const& r, const char* const& s)
{
    if (!r && !s) return true;
    if (!r || !s) return false;

    int length = r->l;
    const QChar *d = r->s;
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return s[length] == 0;
}

bool AtomicString::equal(const AtomicString &a, const char *b)
{ 
    return DOM::equal(a.m_string.implementation(), b); 
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
    
    return *stringTable.insert<const char *, hash, DOM::equal, convert>(c);
}


struct QCharBuffer {
    const QChar *s;
    uint length;
};

inline unsigned hash(const QCharBuffer& buf)
{
    return DOMStringImpl::computeHash(buf.s, buf.length);
}

inline bool equal(DOMStringImpl* const&r, const QCharBuffer &buf)
{
    if (!r && !buf.s) return true;
    if (!r || !buf.s) return false;
    
    if (r->l != buf.length)
        return false;
    const QChar *d = r->s;
    for (uint i = 0; i != buf.length; ++i)
        if (d[i] != buf.s[i])
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
    return *stringTable.insert<QCharBuffer, hash, DOM::equal, convert>(buf);
}

DOMStringImpl *AtomicString::add(DOMStringImpl *r)
{
    if (!r || r->_inTable)
        return r;

    if (r->l == 0)
        return DOMStringImpl::empty();
    
    DOMStringImpl *result = *stringTable.insert(r);
    if (result == r)
        r->_inTable = true;
    return result;
}

void AtomicString::remove(DOMStringImpl *r)
{
    stringTable.remove(r);
}

// Global constants for property name strings.

#if !AVOID_STATIC_CONSTRUCTORS
    // Define an AtomicString in the normal way.
    #define DEFINE_GLOBAL(name, string) extern const AtomicString name ## Atom(string);

    extern const AtomicString nullAtom;
    extern const AtomicString emptyAtom;
#else
    // Define an AtomicString-sized array of pointers to avoid static initialization.
    // Use an array of pointers instead of an array of char in case there is some alignment issue.
    #define DEFINE_GLOBAL(name, string) \
        void * name ## Atom[(sizeof(AtomicString) + sizeof(void *) - 1) / sizeof(void *)];

    DEFINE_GLOBAL(null, ignored)
    DEFINE_GLOBAL(empty, ignored)
#endif

#define CALL_DEFINE_GLOBAL(name) DEFINE_GLOBAL(name, #name)
KHTML_ATOMICSTRING_EACH_GLOBAL(CALL_DEFINE_GLOBAL)

void AtomicString::init()
{
#if AVOID_STATIC_CONSTRUCTORS
    static bool initialized;
    if (!initialized) {
        // Use placement new to initialize the globals.
        new (&nullAtom) AtomicString;
        new (&emptyAtom) AtomicString("");
        
        #define PLACEMENT_NEW_GLOBAL(name, string) new (&name ## PropertyName) AtomicString(string);
        #define CALL_PLACEMENT_NEW_GLOBAL(name) PLACEMENT_NEW_GLOBAL(name, #name)
        KHTML_ATOMICSTRING_EACH_GLOBAL(CALL_PLACEMENT_NEW_GLOBAL)
        initialized = true;
    }
#endif
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
