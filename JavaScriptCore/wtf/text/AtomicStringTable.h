/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef AtomicStringTable_h
#define AtomicStringTable_h

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringImpl.h>

namespace WTF {

struct CStringTranslator {
    static unsigned hash(const char* c)
    {
        return StringImpl::computeHash(c);
    }

    static bool equal(StringImpl* r, const char* s)
    {
        ASSERT(r);
        ASSERT(s);
        int length = r->length();
        const UChar* d = r->characters();
        for (int i = 0; i != length; ++i) {
            unsigned char c = s[i];
            if (d[i] != c)
                return false;
        }
        return s[length] == '\0';
    }

    static void translate(StringImpl*& location, const char* const& c, unsigned hash)
    {
        location = StringImpl::create(c).releaseRef(); 
        location->setHash(hash);
    }
};

struct UCharBuffer {
    const UChar* s;
    unsigned int length;
};

struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return StringImpl::computeHash(buf.s, buf.length);
    }

    static bool equal(StringImpl* str, const UCharBuffer& buf)
    {
        return ::equal(str, buf.s, buf.length);
    }

    static void translate(StringImpl*& location, const UCharBuffer& buf, unsigned hash)
    {
        location = StringImpl::create(buf.s, buf.length).releaseRef(); 
        location->setHash(hash);
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
    }
};

template<bool isIdentiferTable>
class IdentifierOrAtomicStringTable : public FastAllocBase {
public:
    ~IdentifierOrAtomicStringTable();

    PassRefPtr<StringImpl> add(StringImpl* value);
    PassRefPtr<StringImpl> add(const char*);
    PassRefPtr<StringImpl> add(const UChar*, unsigned);
    PassRefPtr<StringImpl> add(const UChar*, unsigned, unsigned existingHash);

    void remove(StringImpl* r) { m_table.remove(r); }

    StringImpl* find(const UChar*, unsigned, unsigned existingHash);

private:
    HashSet<StringImpl*> m_table;
};

template<bool isIdentifierTable>
IdentifierOrAtomicStringTable<isIdentifierTable>::~IdentifierOrAtomicStringTable()
{
    HashSet<StringImpl*>::iterator end = m_table.end();
    if (isIdentifierTable) {
        for (HashSet<StringImpl*>::iterator iter = m_table.begin(); iter != end; ++iter)
            (*iter)->setIsIdentifier(false);
    } else {
        for (HashSet<StringImpl*>::iterator iter = m_table.begin(); iter != end; ++iter)
            (*iter)->setIsAtomic(false);
    }
}

template<bool isIdentifierTable>
PassRefPtr<StringImpl> IdentifierOrAtomicStringTable<isIdentifierTable>::add(StringImpl* value)
{
    ASSERT(value);
    ASSERT(value->length());

    std::pair<HashSet<StringImpl*>::iterator, bool> result = m_table.add(value);
    StringImpl* identifier = *result.first;

    if (result.second) {
        if (isIdentifierTable)
            identifier->setIsIdentifier(true);
        else
            identifier->setIsAtomic(true);
    }

    return identifier;
}

template<bool isIdentifierTable>
PassRefPtr<StringImpl> IdentifierOrAtomicStringTable<isIdentifierTable>::add(const char* c)
{
    ASSERT(c);
    ASSERT(*c);

    std::pair<HashSet<StringImpl*>::iterator, bool> result = m_table.add<const char*, CStringTranslator>(c);
    StringImpl* identifier = *result.first;

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    if (result.second) {
        if (isIdentifierTable)
            identifier->setIsIdentifier(true);
        else
            identifier->setIsAtomic(true);
        return adoptRef(identifier);
    }

    return identifier;
}

template<bool isIdentifierTable>
PassRefPtr<StringImpl> IdentifierOrAtomicStringTable<isIdentifierTable>::add(const UChar* s, unsigned length)
{
    ASSERT(s);
    ASSERT(length);

    UCharBuffer buf = {s, length};
    std::pair<HashSet<StringImpl*>::iterator, bool> result = m_table.add<UCharBuffer, UCharBufferTranslator>(buf);
    StringImpl* identifier = *result.first;

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    if (result.second) {
        if (isIdentifierTable)
            identifier->setIsIdentifier(true);
        else
            identifier->setIsAtomic(true);
        return adoptRef(identifier);
    }

    return identifier;
}

template<bool isIdentifierTable>
PassRefPtr<StringImpl> IdentifierOrAtomicStringTable<isIdentifierTable>::add(const UChar* s, unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(length);
    ASSERT(existingHash);

    HashAndCharacters buffer = { existingHash, s, length }; 
    std::pair<HashSet<StringImpl*>::iterator, bool> result = m_table.add<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    StringImpl* identifier = *result.first;

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    if (result.second) {
        if (isIdentifierTable)
            identifier->setIsIdentifier(true);
        else
            identifier->setIsAtomic(true);
        return adoptRef(identifier);
    }

    return identifier;
}

template<bool isIdentifierTable>
StringImpl* IdentifierOrAtomicStringTable<isIdentifierTable>::find(const UChar* s , unsigned length, unsigned existingHash)
{
    ASSERT(s);
    ASSERT(length);
    ASSERT(existingHash);

    HashAndCharacters buffer = { existingHash, s, length }; 
    HashSet<StringImpl*>::iterator iterator = m_table.find<HashAndCharacters, HashAndCharactersTranslator>(buffer);
    if (iterator == m_table.end())
        return 0;
    return *iterator;
}

}

#if USE(JSC)
// FIXME: This is a temporary layering violation while we move more string code to WTF.
namespace JSC {
class IdentifierTable : public WTF::IdentifierOrAtomicStringTable<true> {};
}
#endif

// FIXME: This is a temporary layering violation while we move more string code to WTF.
namespace WebCore {
class AtomicStringTable : public WTF::IdentifierOrAtomicStringTable<false> {};
}
#endif // AtomicStringTable_h
