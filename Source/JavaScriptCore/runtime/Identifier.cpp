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
#include "JSObject.h"
#include "NumericStrings.h"
#include "ScopeChain.h"
#include <new> // for placement new
#include <string.h> // for strlen
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

using WTF::ThreadSpecific;

namespace JSC {

IdentifierTable* createIdentifierTable()
{
    return new IdentifierTable;
}

void deleteIdentifierTable(IdentifierTable* table)
{
    delete table;
}

struct IdentifierCStringTranslator {
    static unsigned hash(const LChar* c)
    {
        return StringHasher::computeHash<LChar>(c);
    }

    static bool equal(StringImpl* r, const LChar* s)
    {
        return Identifier::equal(r, s);
    }

    static void translate(StringImpl*& location, const LChar* c, unsigned hash)
    {
        size_t length = strlen(reinterpret_cast<const char*>(c));
        LChar* d;
        StringImpl* r = StringImpl::createUninitialized(length, d).leakRef();
        for (size_t i = 0; i != length; i++)
            d[i] = c[i];
        r->setHash(hash);
        location = r;
    }
};

struct IdentifierLCharFromUCharTranslator {
    static unsigned hash(const CharBuffer<UChar>& buf)
    {
        return StringHasher::computeHash<UChar>(buf.s, buf.length);
    }
    
    static bool equal(StringImpl* str, const CharBuffer<UChar>& buf)
    {
        return Identifier::equal(str, buf.s, buf.length);
    }
    
    static void translate(StringImpl*& location, const CharBuffer<UChar>& buf, unsigned hash)
    {
        LChar* d;
        StringImpl* r = StringImpl::createUninitialized(buf.length, d).leakRef();
        for (unsigned i = 0; i != buf.length; i++) {
            UChar c = buf.s[i];
            ASSERT(c <= 0xff);
            d[i] = c;
        }
        r->setHash(hash);
        location = r; 
    }
};

PassRefPtr<StringImpl> Identifier::add(JSGlobalData* globalData, const char* c)
{
    if (!c)
        return 0;
    if (!c[0])
        return StringImpl::empty();
    if (!c[1])
        return add(globalData, globalData->smallStrings.singleCharacterStringRep(c[0]));

    IdentifierTable& identifierTable = *globalData->identifierTable;
    LiteralIdentifierTable& literalIdentifierTable = identifierTable.literalTable();

    const LiteralIdentifierTable::iterator& iter = literalIdentifierTable.find(c);
    if (iter != literalIdentifierTable.end())
        return iter->second;

    HashSet<StringImpl*>::AddResult addResult = identifierTable.add<const LChar*, IdentifierCStringTranslator>(reinterpret_cast<const LChar*>(c));

    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    RefPtr<StringImpl> addedString = addResult.isNewEntry ? adoptRef(*addResult.iterator) : *addResult.iterator;

    literalIdentifierTable.add(c, addedString.get());

    return addedString.release();
}

PassRefPtr<StringImpl> Identifier::add(ExecState* exec, const char* c)
{
    return add(&exec->globalData(), c);
}

PassRefPtr<StringImpl> Identifier::add8(JSGlobalData* globalData, const UChar* s, int length)
{
    if (length == 1) {
        UChar c = s[0];
        ASSERT(c <= 0xff);
        if (canUseSingleCharacterString(c))
            return add(globalData, globalData->smallStrings.singleCharacterStringRep(c));
    }
    
    if (!length)
        return StringImpl::empty();
    CharBuffer<UChar> buf = {s, length}; 
    HashSet<StringImpl*>::AddResult addResult = globalData->identifierTable->add<CharBuffer<UChar>, IdentifierLCharFromUCharTranslator >(buf);
    
    // If the string is newly-translated, then we need to adopt it.
    // The boolean in the pair tells us if that is so.
    return addResult.isNewEntry ? adoptRef(*addResult.iterator) : *addResult.iterator;
}

template <typename CharType>
ALWAYS_INLINE uint32_t Identifier::toUInt32FromCharacters(const CharType* characters, unsigned length, bool& ok)
{
    // Get the first character, turning it into a digit.
    uint32_t value = characters[0] - '0';
    if (value > 9)
        return 0;
    
    // Check for leading zeros. If the first characher is 0, then the
    // length of the string must be one - e.g. "042" is not equal to "42".
    if (!value && length > 1)
        return 0;
    
    while (--length) {
        // Multiply value by 10, checking for overflow out of 32 bits.
        if (value > 0xFFFFFFFFU / 10)
            return 0;
        value *= 10;
        
        // Get the next character, turning it into a digit.
        uint32_t newValue = *(++characters) - '0';
        if (newValue > 9)
            return 0;
        
        // Add in the old value, checking for overflow out of 32 bits.
        newValue += value;
        if (newValue < value)
            return 0;
        value = newValue;
    }
    
    ok = true;
    return value;
}

uint32_t Identifier::toUInt32(const UString& string, bool& ok)
{
    ok = false;

    unsigned length = string.length();

    // An empty string is not a number.
    if (!length)
        return 0;

    if (string.is8Bit())
        return toUInt32FromCharacters(string.characters8(), length, ok);
    return toUInt32FromCharacters(string.characters16(), length, ok);
}

PassRefPtr<StringImpl> Identifier::addSlowCase(JSGlobalData* globalData, StringImpl* r)
{
    ASSERT(!r->isIdentifier());
    // The empty & null strings are static singletons, and static strings are handled
    // in ::add() in the header, so we should never get here with a zero length string.
    ASSERT(r->length());

    if (r->length() == 1) {
        UChar c = (*r)[0];
        if (c <= maxSingleCharacterString)
            r = globalData->smallStrings.singleCharacterStringRep(c);
            if (r->isIdentifier())
                return r;
    }

    return *globalData->identifierTable->add(r).iterator;
}

PassRefPtr<StringImpl> Identifier::addSlowCase(ExecState* exec, StringImpl* r)
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

Identifier Identifier::from(JSGlobalData* globalData, unsigned value)
{
    return Identifier(globalData, globalData->numericStrings.add(value));
}

Identifier Identifier::from(JSGlobalData* globalData, int value)
{
    return Identifier(globalData, globalData->numericStrings.add(value));
}

Identifier Identifier::from(JSGlobalData* globalData, double value)
{
    return Identifier(globalData, globalData->numericStrings.add(value));
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
NO_RETURN_DUE_TO_CRASH void Identifier::checkCurrentIdentifierTable(JSGlobalData*) { CRASH(); }
NO_RETURN_DUE_TO_CRASH void Identifier::checkCurrentIdentifierTable(ExecState*) { CRASH(); }

#endif

} // namespace JSC
