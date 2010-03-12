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
#include "NumericStrings.h"
#include <new> // for placement new
#include <string.h> // for strlen
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>

using WTF::ThreadSpecific;

namespace JSC {

typedef HashMap<const char*, RefPtr<UString::Rep>, PtrHash<const char*> > LiteralIdentifierTable;

class IdentifierTable : public FastAllocBase {
public:
    ~IdentifierTable()
    {
        HashSet<UString::Rep*>::iterator end = m_table.end();
        for (HashSet<UString::Rep*>::iterator iter = m_table.begin(); iter != end; ++iter)
            (*iter)->setIsIdentifier(false);
    }

    std::pair<HashSet<UString::Rep*>::iterator, bool> add(UString::Rep* value)
    {
        std::pair<HashSet<UString::Rep*>::iterator, bool> result = m_table.add(value);
        (*result.first)->setIsIdentifier(true);
        return result;
    }

    template<typename U, typename V>
    std::pair<HashSet<UString::Rep*>::iterator, bool> add(U value)
    {
        std::pair<HashSet<UString::Rep*>::iterator, bool> result = m_table.add<U, V>(value);
        (*result.first)->setIsIdentifier(true);
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
    int length = r->length();
    const UChar* d = r->characters();
    for (int i = 0; i != length; ++i)
        if (d[i] != (unsigned char)s[i])
            return false;
    return s[length] == 0;
}

bool Identifier::equal(const UString::Rep* r, const UChar* s, unsigned length)
{
    if (r->length() != length)
        return false;
    const UChar* d = r->characters();
    for (unsigned i = 0; i != length; ++i)
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
        UChar* d;
        UString::Rep* r = UString::Rep::createUninitialized(length, d).releaseRef();
        for (size_t i = 0; i != length; i++)
            d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
        r->setHash(hash);
        location = r;
    }
};

PassRefPtr<UString::Rep> Identifier::add(JSGlobalData* globalData, const char* c)
{
    if (!c)
        return UString::null().rep();
    if (!c[0])
        return UString::Rep::empty();
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
        UChar* d;
        UString::Rep* r = UString::Rep::createUninitialized(buf.length, d).releaseRef();
        for (unsigned i = 0; i != buf.length; i++)
            d[i] = buf.s[i];
        r->setHash(hash);
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
    if (!length)
        return UString::Rep::empty();
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
    ASSERT(!r->isIdentifier());
    // The empty & null strings are static singletons, and static strings are handled
    // in ::add() in the header, so we should never get here with a zero length string.
    ASSERT(r->length());

    if (r->length() == 1) {
        UChar c = r->characters()[0];
        if (c <= 0xFF)
            r = globalData->smallStrings.singleCharacterStringRep(c);
            if (r->isIdentifier())
                return r;
    }

    return *globalData->identifierTable->add(r).first;
}

PassRefPtr<UString::Rep> Identifier::addSlowCase(ExecState* exec, UString::Rep* r)
{
    return addSlowCase(&exec->globalData(), r);
}

void Identifier::remove(UString::Rep* r)
{
    currentIdentifierTable()->remove(r);
}
    
Identifier Identifier::from(ExecState* exec, unsigned value)
{
    return Identifier(exec, exec->globalData().numericStrings.add(value));
}

Identifier Identifier::from(ExecState* exec, int value)
{
    return Identifier(exec, exec->globalData().numericStrings.add(value));
}

Identifier Identifier::from(ExecState* exec, double value)
{
    return Identifier(exec, exec->globalData().numericStrings.add(value));
}

#ifndef NDEBUG

void Identifier::checkCurrentIdentifierTable(JSGlobalData* globalData)
{
    // Check the identifier table accessible through the threadspecific matches the
    // globalData's identifier table.
    ASSERT_UNUSED(globalData, globalData->identifierTable == currentIdentifierTable());
}

void Identifier::checkCurrentIdentifierTable(ExecState* exec)
{
    checkCurrentIdentifierTable(&exec->globalData());
}

#else

// These only exists so that our exports are the same for debug and release builds.
// This would be an ASSERT_NOT_REACHED(), but we're in NDEBUG only code here!
void Identifier::checkCurrentIdentifierTable(JSGlobalData*) { CRASH(); }
void Identifier::checkCurrentIdentifierTable(ExecState*) { CRASH(); }

#endif

ThreadSpecific<ThreadIdentifierTableData>* g_identifierTableSpecific = 0;

#if ENABLE(JSC_MULTIPLE_THREADS)

pthread_once_t createIdentifierTableSpecificOnce = PTHREAD_ONCE_INIT;
static void createIdentifierTableSpecificCallback()
{
    ASSERT(!g_identifierTableSpecific);
    g_identifierTableSpecific = new ThreadSpecific<ThreadIdentifierTableData>();
}
void createIdentifierTableSpecific()
{
    pthread_once(&createIdentifierTableSpecificOnce, createIdentifierTableSpecificCallback);
    ASSERT(g_identifierTableSpecific);
}

#else 

void createIdentifierTableSpecific()
{
    ASSERT(!g_identifierTableSpecific);
    g_identifierTableSpecific = new ThreadSpecific<ThreadIdentifierTableData>();
}

#endif

} // namespace JSC
