/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include <wtf/unicode/UTF8.h>

namespace WTF {

using namespace Unicode;

COMPILE_ASSERT(sizeof(AtomicString) == sizeof(String), atomic_string_and_string_must_be_same_size);

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

template<typename T, typename HashTranslator>
static inline PassRefPtr<StringImpl> addToStringTable(const T& value)
{
    pair<HashSet<StringImpl*>::iterator, bool> addResult = stringTable().add<T, HashTranslator>(value);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.second ? adoptRef(*addResult.first) : *addResult.first;
}

struct CStringTranslator {
    static unsigned hash(const LChar* c)
    {
        return StringHasher::computeHash(c);
    }

    static inline bool equal(StringImpl* r, const LChar* s)
    {
        return WTF::equal(r, s);
    }

    static void translate(StringImpl*& location, const LChar* const& c, unsigned hash)
    {
        location = StringImpl::create(c).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicString::add(const LChar* c)
{
    if (!c)
        return 0;
    if (!*c)
        return StringImpl::empty();

    return addToStringTable<const LChar*, CStringTranslator>(c);
}

struct UCharBuffer {
    const UChar* s;
    unsigned length;
};

struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringHasher::computeHash(buf.s, buf.length);
    }

    static bool equal(StringImpl* const& str, const UCharBuffer& buf)
    {
        return WTF::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = StringImpl::create(buf.s, buf.length).leakRef();
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
        ASSERT(buffer.hash == StringHasher::computeHash(buffer.characters, buffer.length));
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndCharacters& buffer)
    {
        return WTF::equal(string, buffer.characters, buffer.length);
    }

    static void translate(StringImpl*& location, const HashAndCharacters& buffer, unsigned hash)
    {
        location = StringImpl::create(buffer.characters, buffer.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

struct HashAndUTF8Characters {
    unsigned hash;
    const char* characters;
    unsigned length;
    unsigned utf16Length;
};

struct HashAndUTF8CharactersTranslator {
    static unsigned hash(const HashAndUTF8Characters& buffer)
    {
        return buffer.hash;
    }

    static bool equal(StringImpl* const& string, const HashAndUTF8Characters& buffer)
    {
        if (buffer.utf16Length != string->length())
            return false;

        const UChar* stringCharacters = string->characters();

        // If buffer contains only ASCII characters UTF-8 and UTF16 length are the same.
        if (buffer.utf16Length != buffer.length)
            return equalUTF16WithUTF8(stringCharacters, stringCharacters + string->length(), buffer.characters, buffer.characters + buffer.length);

        for (unsigned i = 0; i < buffer.length; ++i) {
            ASSERT(isASCII(buffer.characters[i]));
            if (stringCharacters[i] != buffer.characters[i])
                return false;
        }

        return true;
    }

    static void translate(StringImpl*& location, const HashAndUTF8Characters& buffer, unsigned hash)
    {
        UChar* target;
        location = StringImpl::createUninitialized(buffer.utf16Length, target).leakRef();

        const char* source = buffer.characters;
        if (convertUTF8ToUTF16(&source, source + buffer.length, &target, target + buffer.utf16Length) != conversionOK)
            ASSERT_NOT_REACHED();

        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length)
{
    if (!s)
        return 0;

    if (!length)
        return StringImpl::empty();
    
    UCharBuffer buffer = { s, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(existingHash);

    if (!length)
        return StringImpl::empty();

    HashAndCharacters buffer = { existingHash, s, length };
    return addToStringTable<HashAndCharacters, HashAndCharactersTranslator>(buffer);
}

PassRefPtr<StringImpl> AtomicString::add(const UChar* s)
{
    if (!s)
        return 0;

    int length = 0;
    while (s[length] != UChar(0))
        length++;

    if (!length)
        return StringImpl::empty();

    UCharBuffer buffer = { s, length };
    return addToStringTable<UCharBuffer, UCharBufferTranslator>(buffer);
}

struct SubstringLocation {
    StringImpl* baseString;
    unsigned start;
    unsigned length;
};

struct SubstringTranslator {
    static unsigned hash(const SubstringLocation& buffer)
    {
        return StringHasher::computeHash(buffer.baseString->characters() + buffer.start, buffer.length);
    }

    static bool equal(StringImpl* const& string, const SubstringLocation& buffer)
    {
        return WTF::equal(string, buffer.baseString->characters() + buffer.start, buffer.length);
    }

    static void translate(StringImpl*& location, const SubstringLocation& buffer, unsigned hash)
    {
        location = StringImpl::create(buffer.baseString, buffer.start, buffer.length).leakRef();
        location->setHash(hash);
        location->setIsAtomic(true);
    }
};

PassRefPtr<StringImpl> AtomicString::add(StringImpl* baseString, unsigned start, unsigned length)
{
    if (!baseString)
        return 0;

    if (!length || start >= baseString->length())
        return StringImpl::empty();

    unsigned maxLength = baseString->length() - start;
    if (length >= maxLength) {
        if (!start)
            return add(baseString);
        length = maxLength;
    }

    SubstringLocation buffer = { baseString, start, length };
    return addToStringTable<SubstringLocation, SubstringTranslator>(buffer);
}

PassRefPtr<StringImpl> AtomicString::addSlowCase(StringImpl* r)
{
    if (!r->length())
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

    if (!length)
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
    if (UNLIKELY(!impl))
        return *this;
    RefPtr<StringImpl> newImpl = impl->lower();
    if (LIKELY(newImpl == impl))
        return *this;
    return AtomicString(newImpl);
}

AtomicString AtomicString::fromUTF8Internal(const char* charactersStart, const char* charactersEnd)
{
    HashAndUTF8Characters buffer;
    buffer.characters = charactersStart;
    buffer.hash = calculateStringHashAndLengthFromUTF8(charactersStart, charactersEnd, buffer.length, buffer.utf16Length);

    if (!buffer.hash)
        return nullAtom;

    AtomicString atomicString;
    atomicString.m_string = addToStringTable<HashAndUTF8Characters, HashAndUTF8CharactersTranslator>(buffer);
    return atomicString;
}

#ifndef NDEBUG
void AtomicString::show() const
{
    m_string.show();
}
#endif

} // namespace WTF
