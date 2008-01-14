/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifdef AVOID_STATIC_CONSTRUCTORS
#define ATOMICSTRING_HIDE_GLOBALS 1
#endif

#include "AtomicString.h"

#include "DeprecatedString.h"
#include "StaticConstructors.h"
#include "StringHash.h"
#include <kjs/identifier.h>
#include <wtf/HashSet.h>

using KJS::Identifier;
using KJS::UString;

namespace WebCore {

static HashSet<StringImpl*>* stringTable;

struct CStringTranslator 
{
    static unsigned hash(const char* c)
    {
        return StringImpl::computeHash(c);
    }

    static bool equal(StringImpl* r, const char* s)
    {
        int length = r->length();
        const UChar* d = r->characters();
        for (int i = 0; i != length; ++i) {
            unsigned char c = s[i];
            if (d[i] != c)
                return false;
        }
        return s[length] == 0;
    }

    static void translate(StringImpl*& location, const char* const& c, unsigned hash)
    {
        location = new StringImpl(c, strlen(c), hash); 
    }
};

bool operator==(const AtomicString& a, const char* b)
{ 
    StringImpl* impl = a.impl();
    if ((!impl || !impl->characters()) && !b)
        return true;
    if ((!impl || !impl->characters()) || !b)
        return false;
    return CStringTranslator::equal(impl, b); 
}

StringImpl* AtomicString::add(const char* c)
{
    if (!c)
        return 0;
    if (!*c)
        return StringImpl::empty();    
    return *stringTable->add<const char*, CStringTranslator>(c).first;
}

struct UCharBuffer {
    const UChar* s;
    unsigned length;
};

struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringImpl::computeHash(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        unsigned strLength = str->length();
        unsigned bufLength = buf.length;
        if (strLength != bufLength)
            return false;

#if PLATFORM(ARM)
        const UChar* strChars = str->characters();
        const UChar* bufChars = buf.s;

        for (unsigned i = 0; i != strLength; ++i) {
            if (*strChars++ != *bufChars++)
                return false;
        }
        return true;
#else
        /* Do it 4-bytes-at-a-time on architectures where it's safe */
        const uint32_t* strChars = reinterpret_cast<const uint32_t*>(str->characters());
        const uint32_t* bufChars = reinterpret_cast<const uint32_t*>(buf.s);
        
        unsigned halfLength = strLength >> 1;
        for (unsigned i = 0; i != halfLength; ++i) {
            if (*strChars++ != *bufChars++)
                return false;
        }
        
        if (strLength & 1 && 
            *reinterpret_cast<const uint16_t *>(strChars) != *reinterpret_cast<const uint16_t *>(bufChars))
            return false;
        
        return true;
#endif
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = new StringImpl(buf.s, buf.length, hash); 
    }
};

StringImpl* AtomicString::add(const UChar* s, int length)
{
    if (!s)
        return 0;

    if (length == 0)
        return StringImpl::empty();
    
    UCharBuffer buf = {s, length}; 
    return *stringTable->add<UCharBuffer, UCharBufferTranslator>(buf).first;
}

StringImpl* AtomicString::add(const UChar* s)
{
    if (!s)
        return 0;

    int length = 0;
    while (s[length] != UChar(0))
        length++;

    if (length == 0)
        return StringImpl::empty();

    UCharBuffer buf = {s, length}; 
    return *stringTable->add<UCharBuffer, UCharBufferTranslator>(buf).first;
}

StringImpl* AtomicString::add(StringImpl* r)
{
    if (!r || r->m_inTable)
        return r;

    if (r->length() == 0)
        return StringImpl::empty();
    
    StringImpl* result = *stringTable->add(r).first;
    if (result == r)
        r->m_inTable = true;
    return result;
}

void AtomicString::remove(StringImpl* r)
{
    stringTable->remove(r);
}

StringImpl* AtomicString::add(const KJS::Identifier& str)
{
    return add(reinterpret_cast<const UChar*>(str.data()), str.size());
}

StringImpl* AtomicString::add(const KJS::UString& str)
{
    return add(reinterpret_cast<const UChar*>(str.data()), str.size());
}

AtomicString::operator Identifier() const
{
    return m_string;
}

AtomicString::operator UString() const
{
    return m_string;
}

AtomicString::AtomicString(const DeprecatedString& s)
    : m_string(add(reinterpret_cast<const UChar*>(s.unicode()), s.length()))
{
}

DeprecatedString AtomicString::deprecatedString() const
{
    return m_string.deprecatedString();
}

DEFINE_GLOBAL(AtomicString, nullAtom)
DEFINE_GLOBAL(AtomicString, emptyAtom, "")
DEFINE_GLOBAL(AtomicString, textAtom, "#text")
DEFINE_GLOBAL(AtomicString, commentAtom, "#comment")
DEFINE_GLOBAL(AtomicString, starAtom, "*")

void AtomicString::init()
{
    static bool initialized;
    if (!initialized) {
        stringTable = new HashSet<StringImpl*>;

        // Use placement new to initialize the globals.
        new ((void*)&nullAtom) AtomicString;
        new ((void*)&emptyAtom) AtomicString("");
        new ((void*)&textAtom) AtomicString("#text");
        new ((void*)&commentAtom) AtomicString("#comment");
        new ((void*)&starAtom) AtomicString("*");

        initialized = true;
    }
}

}
