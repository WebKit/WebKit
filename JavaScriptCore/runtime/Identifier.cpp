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
#include <wtf/WTFThreadData.h>
#include <wtf/text/AtomicStringTable.h>
#include <wtf/text/StringHash.h>

using WTF::ThreadSpecific;

namespace JSC {

class LiteralTable : public HashMap<const char*, RefPtr<StringImpl>, PtrHash<const char*> > {};

LiteralTable* createLiteralTable()
{
    return new LiteralTable;
}

void deleteLiteralTable(LiteralTable* table)
{
    delete table;
}

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

PassRefPtr<UString::Rep> Identifier::add(JSGlobalData* globalData, const char* c)
{
    if (!c)
        return UString::null().rep();
    if (!c[0])
        return UString::Rep::empty();
    if (!c[1])
        return add(globalData, globalData->smallStrings.singleCharacterStringRep(static_cast<unsigned char>(c[0])));

    LiteralTable* literalTable = globalData->literalTable;
    pair<LiteralTable::iterator, bool> result = literalTable->add(c, 0);
    if (!result.second) // pre-existing entry
        return result.first->second;

    RefPtr<StringImpl> addedString = globalData->identifierTable->add(c);
    result.first->second = addedString.get();

    return addedString.release();
}

PassRefPtr<UString::Rep> Identifier::add(ExecState* exec, const char* c)
{
    return add(&exec->globalData(), c);
}

PassRefPtr<UString::Rep> Identifier::add(JSGlobalData* globalData, const UChar* s, int length)
{
    if (length == 1) {
        UChar c = s[0];
        if (c <= 0xFF)
            return add(globalData, globalData->smallStrings.singleCharacterStringRep(c));
    }
    if (!length)
        return UString::Rep::empty();

    return globalData->identifierTable->add(s, length);
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

    return globalData->identifierTable->add(r);
}

PassRefPtr<UString::Rep> Identifier::addSlowCase(ExecState* exec, UString::Rep* r)
{
    return addSlowCase(&exec->globalData(), r);
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
    ASSERT_UNUSED(globalData, globalData->identifierTable == wtfThreadData().currentIdentifierTable());
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

} // namespace JSC
