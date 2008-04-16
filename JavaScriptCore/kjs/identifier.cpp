/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "identifier.h"

#include "JSLock.h"
#include <new> // for placement new
#include <string.h> // for strlen
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#if USE(MULTIPLE_THREADS)
#include <wtf/ThreadSpecific.h>
using namespace WTF;
#endif

namespace WTF {

    template<typename T> struct DefaultHash;
    template<typename T> struct StrHash;

    template<> struct StrHash<KJS::UString::Rep *> {
        static unsigned hash(const KJS::UString::Rep *key) { return key->hash(); }
        static bool equal(const KJS::UString::Rep *a, const KJS::UString::Rep *b) { return KJS::Identifier::equal(a, b); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<> struct DefaultHash<KJS::UString::Rep *> {
        typedef StrHash<KJS::UString::Rep *> Hash;
    };

}

namespace KJS {

typedef HashSet<UString::Rep*> IdentifierTable;
typedef HashMap<const char*, UString::Rep*, PtrHash<const char*> > LiteralIdentifierTable;

static inline IdentifierTable& identifierTable()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<IdentifierTable> table;
    return *table;
#else
    static IdentifierTable table;
    return table;
#endif
}

static inline LiteralIdentifierTable& literalIdentifierTable()
{
#if USE(MULTIPLE_THREADS)
    static ThreadSpecific<LiteralIdentifierTable> table;
    return *table;
#else
    static LiteralIdentifierTable table;
    return table;
#endif
}

void Identifier::initializeIdentifierThreading()
{
    identifierTable();
    literalIdentifierTable();
}

bool Identifier::equal(const UString::Rep *r, const char *s)
{
    int length = r->len;
    const UChar *d = r->data();
    for (int i = 0; i != length; ++i)
        if (d[i] != (unsigned char)s[i])
            return false;
    return s[length] == 0;
}

bool Identifier::equal(const UString::Rep *r, const UChar *s, int length)
{
    if (r->len != length)
        return false;
    const UChar *d = r->data();
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

bool Identifier::equal(const UString::Rep *r, const UString::Rep *b)
{
    int length = r->len;
    if (length != b->len)
        return false;
    const UChar *d = r->data();
    const UChar *s = b->data();
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

struct CStringTranslator
{
    static unsigned hash(const char *c)
    {
        return UString::Rep::computeHash(c);
    }

    static bool equal(UString::Rep *r, const char *s)
    {
        return Identifier::equal(r, s);
    }

    static void translate(UString::Rep*& location, const char *c, unsigned hash)
    {
        size_t length = strlen(c);
        UChar *d = static_cast<UChar *>(fastMalloc(sizeof(UChar) * length));
        for (size_t i = 0; i != length; i++)
            d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
        
        UString::Rep *r = UString::Rep::create(d, static_cast<int>(length)).releaseRef();
        r->isIdentifier = true;
        r->rc = 0;
        r->_hash = hash;

        location = r;
    }
};

PassRefPtr<UString::Rep> Identifier::add(const char* c)
{
    if (!c) {
        UString::Rep::null.hash();
        return &UString::Rep::null;
    }

    if (!c[0]) {
        UString::Rep::empty.hash();
        return &UString::Rep::empty;
    }

    LiteralIdentifierTable& literalTableLocalRef = literalIdentifierTable();

    const LiteralIdentifierTable::iterator& iter = literalTableLocalRef.find(c);
    if (iter != literalTableLocalRef.end())
        return iter->second;

    UString::Rep* addedString = *identifierTable().add<const char*, CStringTranslator>(c).first;
    literalTableLocalRef.add(c, addedString);
    addedString->ref();

    return addedString;
}

struct UCharBuffer {
    const UChar *s;
    unsigned int length;
};

struct UCharBufferTranslator
{
    static unsigned hash(const UCharBuffer& buf)
    {
        return UString::Rep::computeHash(buf.s, buf.length);
    }

    static bool equal(UString::Rep *str, const UCharBuffer& buf)
    {
        return Identifier::equal(str, buf.s, buf.length);
    }

    static void translate(UString::Rep *& location, const UCharBuffer& buf, unsigned hash)
    {
        UChar *d = static_cast<UChar *>(fastMalloc(sizeof(UChar) * buf.length));
        for (unsigned i = 0; i != buf.length; i++)
            d[i] = buf.s[i];
        
        UString::Rep *r = UString::Rep::create(d, buf.length).releaseRef();
        r->isIdentifier = true;
        r->rc = 0;
        r->_hash = hash;
        
        location = r; 
    }
};

PassRefPtr<UString::Rep> Identifier::add(const UChar *s, int length)
{
    if (!length) {
        UString::Rep::empty.hash();
        return &UString::Rep::empty;
    }
    
    UCharBuffer buf = {s, length}; 
    return *identifierTable().add<UCharBuffer, UCharBufferTranslator>(buf).first;
}

PassRefPtr<UString::Rep> Identifier::addSlowCase(UString::Rep *r)
{
    ASSERT(!r->isIdentifier);

    if (r->len == 0) {
        UString::Rep::empty.hash();
        return &UString::Rep::empty;
    }

    UString::Rep *result = *identifierTable().add(r).first;
    if (result == r)
        r->isIdentifier = true;
    return result;
}

void Identifier::remove(UString::Rep *r)
{
    identifierTable().remove(r);
}

} // namespace KJS
