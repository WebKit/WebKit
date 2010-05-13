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

#include "AtomicString.h"

#include "StringHash.h"
#include <wtf/HashSet.h>
#include <wtf/Threading.h>
#include <wtf/WTFThreadData.h>

namespace WebCore {

class AtomicStringTable {
public:
    static AtomicStringTable* create()
    {
        AtomicStringTable* table = new AtomicStringTable;

        WTFThreadData& data = wtfThreadData();
        data.m_atomicStringTable = table;
        data.m_atomicStringTableDestructor = AtomicStringTable::destroy;

        return table;
    }

    HashSet<StringImpl*>& table()
    {
        return m_table;
    }

private:
    static void destroy(AtomicStringTable* table)
    {
        HashSet<StringImpl*>::iterator end = table->m_table.end();
        for (HashSet<StringImpl*>::iterator iter = table->m_table.begin(); iter != end; ++iter)
            (*iter)->setIsAtomic(false);
        delete table;
    }

    HashSet<StringImpl*> m_table;
};

static inline HashSet<StringImpl*>& stringTable()
{
    // Once possible we should make this non-lazy (constructed in WTFThreadData's constructor).
    AtomicStringTable* table = wtfThreadData().atomicStringTable();
    if (UNLIKELY(!table))
        table = AtomicStringTable::create();
    return table->table();
}

struct CStringTranslator {
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
        location = StringImpl::create(c).releaseRef(); 
        location->setHash(hash);
        location->setIsAtomic(true);
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

PassRefPtr<StringImpl> AtomicString::add(const char* c)
{
    if (!c)
        return 0;
    if (!*c)
        return StringImpl::empty();    
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable().add<const char*, CStringTranslator>(c);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

struct UCharBuffer {
    const UChar* s;
    unsigned length;
};

static inline bool equal(StringImpl* string, const UChar* characters, unsigned length)
{
    if (string->length() != length)
        return false;

    // FIXME: perhaps we should have a more abstract macro that indicates when
    // going 4 bytes at a time is unsafe
#if CPU(ARM) || CPU(SH4)
    const UChar* stringCharacters = string->characters();
    for (unsigned i = 0; i != length; ++i) {
        if (*stringCharacters++ != *characters++)
            return false;
    }
    return true;
#else
    /* Do it 4-bytes-at-a-time on architectures where it's safe */

    const uint32_t* stringCharacters = reinterpret_cast<const uint32_t*>(string->characters());
    const uint32_t* bufferCharacters = reinterpret_cast<const uint32_t*>(characters);

    unsigned halfLength = length >> 1;
    for (unsigned i = 0; i != halfLength; ++i) {
        if (*stringCharacters++ != *bufferCharacters++)
            return false;
    }

    if (length & 1 &&  *reinterpret_cast<const uint16_t*>(stringCharacters) != *reinterpret_cast<const uint16_t*>(bufferCharacters))
        return false;

    return true;
#endif
}

struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringImpl::computeHash(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        return WebCore::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = StringImpl::create(buf.s, buf.length).releaseRef(); 
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

struct HashAndCharacters {
    unsigned hash;
    const UChar* characters;
    unsigned length;
};

struct HashAndCharactersTranslator {
    static unsigned hash(const HashAndCharacters& buffer)
    {
        ASSERT(buffer.hash == StringImpl::computeHash(buffer.characters, buffer.length));
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndCharacters& buffer)
    {
        return WebCore::equal(string, buffer.characters, buffer.length);
    }

    static void translate(StringImpl*& location, const HashAndCharacters& buffer, unsigned hash)
    {
        location = StringImpl::create(buffer.characters, buffer.length).releaseRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length)
{
    if (!s)
        return 0;

    if (length == 0)
        return StringImpl::empty();
    
    UCharBuffer buf = { s, length }; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable().add<UCharBuffer, UCharBufferTranslator>(buf);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.second ? adoptRef(*addResult.first) : *addResult.first;
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(existingHash);

    if (length == 0)
        return StringImpl::empty();
    
    HashAndCharacters buffer = { existingHash, s, length }; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable().add<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (!addResult.second)
        return *addResult.first;
    return adoptRef(*addResult.first);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s)
{
    if (!s)
        return 0;

    int length = 0;
    while (s[length] != UChar(0))
        length++;

    if (length == 0)
        return StringImpl::empty();

    UCharBuffer buf = {s, length}; 
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable().add<UCharBuffer, UCharBufferTranslator>(buf);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.second ? adoptRef(*addResult.first) : *addResult.first;
}

PassRefPtr<StringImpl> AtomicString::addSlowCase(StringImpl* r)
{
    if (!r || r->isAtomic())
        return r;

    if (r->length() == 0)
        return StringImpl::empty();

    StringImpl* result = *stringTable().add(r).first;
    if (result == r)
        r->setIsAtomic(true);
    return result;
}

AtomicStringImpl* AtomicString::find(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(existingHash);

    if (length == 0)
        return static_cast<AtomicStringImpl*>(StringImpl::empty());

    HashAndCharacters buffer = { existingHash, s, length }; 
    HashSet<StringImpl*>::iterator iterator = stringTable().find<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (iterator == stringTable().end())
        return 0;
    return static_cast<AtomicStringImpl*>(*iterator);
}

void AtomicString::remove(StringImpl* r)
{
    stringTable().remove(r);
}
    
AtomicString AtomicString::lower() const
{
    // Note: This is a hot function in the Dromaeo benchmark.
    StringImpl* impl = this->impl();
    RefPtr<StringImpl> newImpl = impl->lower();
    if (LIKELY(newImpl == impl))
        return *this;
    return AtomicString(newImpl);
}

}
