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
#include "Identifier.h"

#include "CallFrame.h"
#include <new> // for placement new
#include <string.h> // for strlen
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>

namespace JSC {

typedef HashMap<const char*, RefPtr<UString::Rep>, PtrHash<const char*> > LiteralIdentifierTable;

class IdentifierTable {
public:
    ~IdentifierTable()
    {
        HashSet<UString::Rep*>::iterator end = m_table.end();
        for (HashSet<UString::Rep*>::iterator iter = m_table.begin(); iter != end; ++iter)
            (*iter)->setIdentifierTable(0);
    }
    
    std::pair<HashSet<UString::Rep*>::iterator, bool> add(UString::Rep* value)
    {
        std::pair<HashSet<UString::Rep*>::iterator, bool> result = m_table.add(value);
        (*result.first)->setIdentifierTable(this);
        return result;
    }

    template<typename U, typename V>
    std::pair<HashSet<UString::Rep*>::iterator, bool> add(U value)
    {
        std::pair<HashSet<UString::Rep*>::iterator, bool> result = m_table.add<U, V>(value);
        (*result.first)->setIdentifierTable(this);
        return result;
    }

    void remove(UString::Rep* r) { m_table.remove(r); }

    LiteralIdentifierTable& literalTable() { return m_literalTable; }

private:
    HashSet<UString::Rep*> m_table;
    LiteralIdentifierTable m_literalTable;
};

IdentifierTable* createIdentifierTable()
{
    return new IdentifierTable;
}

void deleteIdentifierTable(IdentifierTable* table)
{
    delete table;
}

bool Identifier::equal(const UString::Rep* r, const char* s)
{
    int length = r->len;
    const UChar* d = r->data();
    for (int i = 0; i != length; ++i)
        if (d[i] != (unsigned char)s[i])
            return false;
    return s[length] == 0;
}

bool Identifier::equal(const UString::Rep* r, const UChar* s, int length)
{
    if (r->len != length)
        return false;
    const UChar* d = r->data();
    for (int i = 0; i != length; ++i)
        if (d[i] != s[i])
            return false;
    return true;
}

struct CStringTranslator {
    static unsigned hash(const char* c)
    {
        return UString::Rep::computeHash(c);
    }

    static bool equal(UString::Rep* r, const char* s)
    {
        return Identifier::equal(r, s);
    }

    static void translate(UString::Rep*& location, const char* c, unsigned hash)
    {
        size_t length = strlen(c);
        UChar* d = static_cast<UChar*>(fastMalloc(sizeof(UChar) * length));
        for (size_t i = 0; i != length; i++)
            d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
        
        UString::Rep* r = UString::Rep::create(d, static_cast<int>(length)).releaseRef();
        r->_hash = hash;

        location = r;
    }
};

PassRefPtr<UString::Rep> Identifier::add(JSGlobalData* globalData, const char* c)
{
    if (!c) {
        UString::Rep::null().hash();
        return &UString::Rep::null();
    }
    if (!c[0]) {
        UString::Rep::empty().hash();
        return &UString::Rep::empty();
    }
    if (!c[1])
        return add(globalData, globalData->smallStrings.singleCharacterStringRep(static_cast<unsigned char>(c[0])));

    IdentifierTable& identifierTable = *globalData->identifierTable;
    LiteralIdentifierTable& literalIdentifierTable = identifierTable.literalTable();

    const LiteralIdentifierTable::iterator& iter = literalIdentifierTable.find(c);
    if (iter != literalIdentifierTable.end())
        return iter->second;

    pair<HashSet<UString::Rep*>::iterator, bool> addResult = identifierTable.add<const char*, CStringTranslator>(c);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    RefPtr<UString::Rep> addedString = addResult.second ? adoptRef(*addResult.first) : *addResult.first;

    literalIdentifierTable.add(c, addedString.get());

    return addedString.release();
}

PassRefPtr<UString::Rep> Identifier::add(ExecState* exec, const char* c)
{
    return add(&exec->globalData(), c);
}

struct UCharBuffer {
    const UChar* s;
    unsigned int length;
};

struct UCharBufferTranslator {
    static unsigned hash(const UCharBuffer& buf)
    {
        return UString::Rep::computeHash(buf.s, buf.length);
    }

    static bool equal(UString::Rep* str, const UCharBuffer& buf)
    {
        return Identifier::equal(str, buf.s, buf.length);
    }

    static void translate(UString::Rep*& location, const UCharBuffer& buf, unsigned hash)
    {
        UChar* d = static_cast<UChar*>(fastMalloc(sizeof(UChar) * buf.length));
        for (unsigned i = 0; i != buf.length; i++)
            d[i] = buf.s[i];
        
        UString::Rep* r = UString::Rep::create(d, buf.length).releaseRef();
        r->_hash = hash;
        
        location = r; 
    }
};

PassRefPtr<UString::Rep> Identifier::add(JSGlobalData* globalData, const UChar* s, int length)
{
    if (length == 1) {
        UChar c = s[0];
        if (c <= 0xFF)
            return add(globalData, globalData->smallStrings.singleCharacterStringRep(c));
    }
    if (!length) {
        UString::Rep::empty().hash();
        return &UString::Rep::empty();
    }
    UCharBuffer buf = {s, length}; 
    pair<HashSet<UString::Rep*>::iterator, bool> addResult = globalData->identifierTable->add<UCharBuffer, UCharBufferTranslator>(buf);

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.second ? adoptRef(*addResult.first) : *addResult.first;
}

PassRefPtr<UString::Rep> Identifier::add(ExecState* exec, const UChar* s, int length)
{
    return add(&exec->globalData(), s, length);
}

PassRefPtr<UString::Rep> Identifier::addSlowCase(JSGlobalData* globalData, UString::Rep* r)
{
    ASSERT(!r->identifierTable());
    if (r->len == 1) {
        UChar c = r->data()[0];
        if (c <= 0xFF)
            r = globalData->smallStrings.singleCharacterStringRep(c);
            if (r->identifierTable()) {
#ifndef NDEBUG
                checkSameIdentifierTable(globalData, r);
#endif
                return r;
            }
    }
    if (!r->len) {
        UString::Rep::empty().hash();
        return &UString::Rep::empty();
    }
    return *globalData->identifierTable->add(r).first;
}

PassRefPtr<UString::Rep> Identifier::addSlowCase(ExecState* exec, UString::Rep* r)
{
    return addSlowCase(&exec->globalData(), r);
}

void Identifier::remove(UString::Rep* r)
{
    r->identifierTable()->remove(r);
}

#ifndef NDEBUG

void Identifier::checkSameIdentifierTable(ExecState* exec, UString::Rep* rep)
{
    ASSERT(rep->identifierTable() == exec->globalData().identifierTable);
}

void Identifier::checkSameIdentifierTable(JSGlobalData* globalData, UString::Rep* rep)
{
    ASSERT(rep->identifierTable() == globalData->identifierTable);
}

#else

void Identifier::checkSameIdentifierTable(ExecState*, UString::Rep*)
{
}

void Identifier::checkSameIdentifierTable(JSGlobalData*, UString::Rep*)
{
}

#endif

} // namespace JSC
